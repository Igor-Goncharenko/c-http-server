#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "http_server.h"
#include "http_server_internal.h"

#define READ_HEADERS_CHARS_LIMIT        8192
#define READ_BUFFER_LEN                 1024

#define MAX_EVENTS                      64

HTTP_SERVER_API const char*
get_header(const request_data_t *request, const char *header) {
    for (int i = 0; i < request->n_headers; i++)
        if (strcmp(header, request->headers[i].key) == 0)
            return request->headers[i].value;
    return NULL;
}

HTTP_SERVER_API int 
hs_response_add_header(hs_response_t *resp, const char *key, const char *value) {
    const int key_len = strlen(key), value_len = strlen(value);
    const int total_len = key_len + value_len + 4;  // 4: ": " + "\r\n" symbols

    if (resp->headers.cap == 0) {
        resp->headers.len = 0;
        resp->headers.cap = 64;
        resp->headers.data = malloc(resp->headers.cap);
        if (resp->headers.data == NULL) return -1;
    } else if (resp->headers.cap < resp->headers.len + total_len + 1) {
        resp->headers.cap *= 2;
        resp->headers.data = realloc(resp->headers.data, resp->headers.cap);
        if (resp->headers.data == NULL) return -1;
    }

    if (    strcpy(resp->headers.data + resp->headers.len, key) == NULL ||
            strcpy(resp->headers.data + resp->headers.len + key_len, ": ") == NULL ||
            strcpy(resp->headers.data + resp->headers.len + key_len + 2, value) == NULL ||
            strcpy(resp->headers.data + resp->headers.len + total_len - 2, "\r\n") == NULL)
        return -2;

    resp->headers.len += total_len;

    return 0;
}

HTTP_SERVER_API void 
hs_response_free(hs_response_t *resp) {
    if (resp->headers.data != NULL && resp->headers.cap > 0) {
        resp->headers.cap = 0;
        resp->headers.len = 0;
        free(resp->headers.data);
    }
    if (resp->content != NULL) {
        resp->content_len = 0;
        free(resp->content);
    }
}

HTTP_SERVER_API size_t 
hs_load_file(const char *filename, char **dest) {
    FILE *fd;
    struct stat fd_stat;
    char *buf = NULL;

    if (stat(filename, &fd_stat)) {
        LOG_ERROR("File not found: '%s'.", filename);
        return -1;
    }

    if ((fd = fopen(filename, "rb")) == NULL) {
        LOG_ERROR("Failed to open file '%s'.", filename);
        return -1;
    }

    if ((buf = malloc(fd_stat.st_size + 1)) == NULL) {
        LOG_ERROR("Memory allocation failed for file '%s'.", filename);
        fclose(fd);
        return -1;
    }

    size_t bytes_read = fread(buf, 1, fd_stat.st_size, fd);
    if (bytes_read != fd_stat.st_size) {
        LOG_ERROR("Read error for file '%s'.", filename);
        free(buf);
        fclose(fd);
        return -1;
    }

    fclose(fd);

    *dest = buf;

    return fd_stat.st_size;
}

/*
 ********************************************
 *              CLIENT HANDLER              *
 ********************************************
 */

HTTP_SERVER_STATIC http_server_err_t 
_read_headers_raw(const int client_fd, buffer_t *dest) {
    http_server_err_t err;
    int chars_count = 0;
    int new_line_count = 0;
    char ch;

    char buffer[READ_BUFFER_LEN];
    int buffer_len = 0;

    if (HS_ERROR_CHECK(err, buffer_init(dest)))
        goto failed;
    dest->allow_realloc = true;

    while (new_line_count < 2 && chars_count < READ_HEADERS_CHARS_LIMIT) {
        chars_count++;

        int n = read(client_fd, &ch, 1);
        if (n <= 0) {
            err = HS_CREATE_ERR(HTTP_SERVER_SOCKET_READ_ERR);
            goto failed;
        }

        buffer[buffer_len++] = ch;
        if (buffer_len == READ_BUFFER_LEN) {
            if (HS_ERROR_CHECK(err, buffer_append_mem(dest, 1, 1024, buffer, NULL)))
                goto failed;
            buffer_len = 0;
        }

        if (ch == '\r') 
            continue;
        else if (ch == '\n')
            new_line_count++;
        else
            new_line_count = 0;
    }

    if (HS_ERROR_CHECK(err, buffer_append_mem(dest, 1, buffer_len, buffer, NULL)))
        goto failed;

    return HS_CREATE_ERR(HTTP_SERVER_OK);

failed:
    buffer_free(dest);
    return err;
}

HTTP_SERVER_STATIC http_server_err_t 
_read_body(const int client_fd, buffer_t *dest, const int size) {
    http_server_err_t err;
    char buffer[READ_BUFFER_LEN];
    int total_len = 0;

    if (HS_ERROR_CHECK(err, buffer_init_with_size(dest, size)))
        goto failed;

    while (total_len < size) {
        int to_read = (size - total_len < READ_BUFFER_LEN) ? size - total_len : READ_BUFFER_LEN;

        int bytes_read = read(client_fd, buffer, to_read);
        if (bytes_read <= 0 || bytes_read != to_read) {
            err = HS_CREATE_ERR(HTTP_SERVER_SOCKET_READ_ERR);
            goto failed;
        }

        if (HS_ERROR_CHECK(err, buffer_append_mem(dest, 1, bytes_read, buffer, NULL)))
            goto failed;
        total_len += bytes_read;
    }
    return HS_CREATE_ERR(HTTP_SERVER_OK);

failed:
    buffer_free(dest);
    return err;
}

HTTP_SERVER_STATIC int
_get_content_len(const request_data_t *request) {
    const char *value_str = get_header(request, "Content-Length");
    return (value_str == NULL) ? 0 : atoi(value_str);
}

HTTP_SERVER_STATIC http_server_err_t 
_send_internal_error(const int fd) {
    http_server_err_t err;
    char *resp = NULL;
    int resp_len;
    if (HS_ERROR_CHECK(err, create_error_response(&resp, &resp_len, 500))) {
        return err;
    }

    write(fd, resp, resp_len);
    if (resp != NULL) {
        free(resp);
    }

    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

HTTP_SERVER_STATIC http_server_err_t 
_send_response(const int fd, const char *response, const int response_len) {
    if (response_len <= 0 || response == NULL) return HS_CREATE_ERR(HTTP_SERVER_WRITE_ERR);
    int bytes_sent = 0;

    while (bytes_sent < response_len) {
        int n = write(fd, response + bytes_sent, response_len - bytes_sent);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return HS_CREATE_ERR(HTTP_SERVER_WRITE_ERR);
        }
        bytes_sent += n;
    }
    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

HTTP_SERVER_STATIC http_server_err_t 
_handle_client(const server_t *server, const int fd) {
    http_server_err_t err = HS_CREATE_ERR(HTTP_SERVER_OK);
    request_data_t request = { 0 };
    buffer_t headers_raw = { 0 }, content_buf = { 0 };

    char *response = NULL;
    int response_len = 0;

    fprintf(stdout, "New client: %d\n", fd);    // TODO: delete print

    if (HS_ERROR_CHECK(err, _read_headers_raw(fd, &headers_raw)))
        goto cleanup;

    if (HS_ERROR_CHECK(err, parse_http_request(&request, &headers_raw)))
        goto cleanup;

    int content_len = _get_content_len(&request);
    printf("content_len = %d\n", content_len);  // TODO: delete print

    if (content_len > 0) {
        if (HS_ERROR_CHECK(err, _read_body(fd, &content_buf, content_len)))
            goto cleanup;
        request.content_len = content_len;
        request.content = content_buf.data;
    }

    if (HS_ERROR_CHECK(err, form_response(server, &request, &response, &response_len)))
        goto cleanup;

    if (HS_ERROR_CHECK(err, _send_response(fd, response, response_len))) 
        goto cleanup;

cleanup:
    buffer_free(&headers_raw);
    buffer_free(&content_buf);
    if (request.mem != NULL) free(request.mem);
    if (response != NULL) free(response);
    if (err.code != HTTP_SERVER_OK) _send_internal_error(fd);

    return err;
}

/*
 ********************************************
 *                  SERVER                  *
 ********************************************
 */

/**/
HTTP_SERVER_STATIC void 
_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

HTTP_SERVER_STATIC http_server_err_t 
_cpy_init_routes_to_server(server_t *self, const server_route_t *routes, const int n_routes) {
    http_server_err_t err;
    buffer_t buf;


    /* count mem */
    const int routes_list_mem = n_routes * sizeof(server_route_t);
    int total_mem = routes_list_mem;
    for (int i = 0; i < n_routes; i++)
        total_mem += strlen(routes[i].route_tmp) + 1;

    if (    HS_ERROR_CHECK(err, buffer_init_with_size(&buf, total_mem)) ||
            HS_ERROR_CHECK(err, buffer_append_mem(&buf, sizeof(server_route_t), 
                    n_routes, NULL, NULL)))
        goto failed;

    self->routes = (server_route_t*)buf.mem;

    if (memcpy(self->routes, routes, routes_list_mem) == NULL) {
        err = HS_CREATE_ERR(HTTP_SERVER_MEMCPY_ERR);
        goto failed;
    }

    for (int i = 0; i < n_routes; i++) {
        if (HS_ERROR_CHECK(err, buffer_append_sentence(&buf, routes[i].route_tmp, 
                        strlen(routes[i].route_tmp), &self->routes[i].route_tmp)))
            goto failed;
    }

    self->mem = buf.mem;
    self->n_routes = n_routes;

    return HS_CREATE_ERR(HTTP_SERVER_OK);
failed:
    buffer_free(&buf);
    return err;
}

HTTP_SERVER_STATIC http_server_err_t 
_init_server_epoll(server_t *self) {
    http_server_err_t err = HS_CREATE_ERR(HTTP_SERVER_OK);

    if ((self->epoll_fd = epoll_create1(0)) == -1) {
        err = HS_CREATE_ERR(HTTP_SERVER_EPOLL_CREATE_ERR);
        goto end;
    }

    self->event.events = EPOLLIN | EPOLLET;
    self->event.data.fd = self->fd;
    if (epoll_ctl(self->epoll_fd, EPOLL_CTL_ADD, self->fd, &self->event) == -1) {
        err = HS_CREATE_ERR(HTTP_SERVER_EPOLL_CTL_ERR);
        goto end;
    }
end:
    return err;
}

HTTP_SERVER_API void
server_destroy(server_t *self) {
    self->running = false;

    if (self->fd > 0)
        close(self->fd);
    self->fd = -1;

    if (self->epoll_fd > 0)
        close(self->epoll_fd);

    if (self->mem != NULL)
        free(self->mem);
}

HTTP_SERVER_API int
init_server(server_t *self, const int port, const int to_listen, const server_route_t *routes, 
        const int n_routes) {
    http_server_err_t err;

    self->epoll_fd = -1;
    self->fd = -1;
    self->mem = NULL;

    self->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (self->fd == 0) {
        err = HS_CREATE_ERR(HTTP_SERVER_SOCKET_CREATE_ERR);
        goto failed;
    }

    int opt = 1;
    setsockopt(self->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    self->addr.sin_family = AF_INET;
    self->addr.sin_addr.s_addr = INADDR_ANY;
    self->addr.sin_port = htons(port);

    if (HS_ERROR_CHECK(err, _cpy_init_routes_to_server(self, routes, n_routes)))
        goto failed;

    if (bind(self->fd, (struct sockaddr*)&self->addr, sizeof(self->addr)) != 0) {
        err = HS_CREATE_ERR(HTTP_SERVER_BIND_ERR);
        goto failed;
    }

    _set_nonblocking(self->fd);

    if (listen(self->fd, to_listen) != 0) {
        err = HS_CREATE_ERR(HTTP_SERVER_LISTEN_ERR);
        goto failed;
    }

    self->running = true;

    if (HS_ERROR_CHECK(err, _init_server_epoll(self)))
        goto failed;

    return HTTP_SERVER_OK;

failed:
    server_destroy(self);
    LOG_ERROR("Failed to init server, "HS_ERROR_FORMAT, HS_ERROR_ARGS(err));
    return -1;
}

HTTP_SERVER_API int 
start_server(server_t *self) {
    http_server_err_t err;
    struct epoll_event events[MAX_EVENTS];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (self->running) {
        int n = epoll_wait(self->epoll_fd, events, MAX_EVENTS, -1);
        if (n == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == self->fd) {
                while (1) {
                    int client_fd = accept(self->fd, (struct sockaddr*)&client_addr, &client_len);
                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        } else {
                            LOG_ERROR("accept error: errno='%s'(%d)", strerror(errno), errno);
                            perror("accept");
                            break;
                        }
                    }

                    _set_nonblocking(client_fd);

                    self->event.events = EPOLLIN | EPOLLET;
                    self->event.data.fd = client_fd;
                    if (epoll_ctl(self->epoll_fd, EPOLL_CTL_ADD, client_fd, &self->event) == -1) {
                        perror("epoll_ctl: client_socket");
                        close(client_fd);
                    }
                }
            } else {
                if (HS_ERROR_CHECK(err, _handle_client(self, events[i].data.fd)))
                    LOG_ERROR("Failed to handle client: "HS_ERROR_FORMAT, HS_ERROR_ARGS(err));
                if (epoll_ctl(self->epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL) != 0)
                    LOG_ERROR("epoll_ctl error: errno='%s'(%d)", strerror(errno), errno);
                if (close(events[i].data.fd) != 0)
                    LOG_ERROR("close error: errno='%s'(%d)", strerror(errno), errno);
            }
        }
    }

    return 0;
}
