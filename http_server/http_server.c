#include "http_server.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "http_server_internal.h"

#define MAX_EVENTS 64

HS_API const char *
hs_get_header(const hs_request_data_t *request, const char *header) {
    for (int i = 0; i < request->n_headers; i++)
        if (strcmp(header, request->headers[i].key) == 0) return request->headers[i].value;
    return NULL;
}

HS_API int
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

    if (strcpy(resp->headers.data + resp->headers.len, key) == NULL ||
        strcpy(resp->headers.data + resp->headers.len + key_len, ": ") == NULL ||
        strcpy(resp->headers.data + resp->headers.len + key_len + 2, value) == NULL ||
        strcpy(resp->headers.data + resp->headers.len + total_len - 2, "\r\n") == NULL)
        return -2;

    resp->headers.len += total_len;

    return 0;
}

HS_API void
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

HS_API size_t
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
 *                  SERVER                  *
 ********************************************
 */

HS_STATIC hs_err_t
_hs_setup_signalfd(hs_server_t *server) {
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGQUIT);

    if (pthread_sigmask(SIG_BLOCK, &mask, NULL) == -1) {
        LOG_ERROR("pthread_sigmask: %s.", strerror(errno));
        return HS_CREATE_ERR(HS_PTHREAD_SIGMASK_ERR);
    }

    server->signal_fd = signalfd(-1, &mask, SFD_NONBLOCK);
    if (server->signal_fd == -1) {
        LOG_ERROR("signalfd: %s.", strerror(errno));
        return HS_CREATE_ERR(HS_SIGNALFD_ERR);
    }

    return HS_CREATE_ERR(HS_OK);
}

HS_STATIC void
_hs_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

HS_STATIC hs_err_t
_hs_init_server_epoll(hs_server_t *self) {
    hs_err_t err = HS_CREATE_ERR(HS_OK);
    struct epoll_event ev;

    if ((self->epoll_fd = epoll_create1(0)) == -1) {
        err = HS_CREATE_ERR(HS_EPOLL_CREATE_ERR);
        goto end;
    }

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = self->fd;
    if (epoll_ctl(self->epoll_fd, EPOLL_CTL_ADD, self->fd, &ev) == -1) {
        err = HS_CREATE_ERR(HS_EPOLL_CTL_ERR);
        goto end;
    }

    ev.events = EPOLLIN;
    ev.data.fd = self->signal_fd;
    if (epoll_ctl(self->epoll_fd, EPOLL_CTL_ADD, self->signal_fd, &ev) == -1) {
        err = HS_CREATE_ERR(HS_EPOLL_CTL_ERR);
        goto end;
    }

end:
    return err;
}

HS_API void
hs_server_destroy(hs_server_t *self) {
    self->running = false;

    for (int i = 0; i < self->n_routes; i++) {
        regfree(&self->routes[i]._re);
    }

    if (self->fd > 0) close(self->fd);
    self->fd = -1;

    if (self->epoll_fd > 0) close(self->epoll_fd);
    if (self->signal_fd > 0) close(self->signal_fd);

    if (self->mem != NULL) free(self->mem);
}

HS_API int
hs_init_server(hs_server_t *self, const int port, const int to_listen,
               const hs_server_route_t *routes, const int n_routes) {
    hs_err_t err;

    self->epoll_fd = -1;
    self->fd = -1;
    self->signal_fd = -1;
    self->mem = NULL;

    self->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (self->fd == 0) {
        err = HS_CREATE_ERR(HS_SOCKET_CREATE_ERR);
        goto failed;
    }

    int opt = 1;
    setsockopt(self->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    self->addr.sin_family = AF_INET;
    self->addr.sin_addr.s_addr = INADDR_ANY;
    self->addr.sin_port = htons(port);

    if (HS_ERROR_CHECK(err, hs_cpy_init_routes_to_server(self, routes, n_routes))) goto failed;

    if (bind(self->fd, (struct sockaddr *)&self->addr, sizeof(self->addr)) != 0) {
        err = HS_CREATE_ERR(HS_BIND_ERR);
        goto failed;
    }

    _hs_set_nonblocking(self->fd);

    if (listen(self->fd, to_listen) != 0) {
        err = HS_CREATE_ERR(HS_LISTEN_ERR);
        goto failed;
    }

    self->running = true;

    if (HS_ERROR_CHECK(err, _hs_setup_signalfd(self))) goto failed;
    if (HS_ERROR_CHECK(err, _hs_init_server_epoll(self))) goto failed;

    return HS_OK;

failed:
    hs_server_destroy(self);
    LOG_ERROR("Failed to init server, " HS_ERROR_FORMAT, HS_ERROR_ARGS(err));
    return -1;
}

HS_STATIC hs_err_t
_hs_client_acceptor(const int fd, const int epoll_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (1) {
        int client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                return HS_CREATE_ERR(HS_ACCEPT_ERR);
            }
        }

        _hs_set_nonblocking(client_fd);

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            LOG_ERROR("Epoll ctl error: client_socket: '%s'(%d).", strerror(errno), errno);
            close(client_fd);
            return HS_CREATE_ERR(HS_EPOLL_CTL_ERR);
        }
    }

    return HS_CREATE_ERR(HS_OK);
}

HS_API int
hs_start_server(hs_server_t *self) {
    hs_err_t err;
    struct epoll_event events[MAX_EVENTS];

    while (self->running) {
        int n = epoll_wait(self->epoll_fd, events, MAX_EVENTS, -1);
        if (n == -1) {
            LOG_ERROR("Epoll wait error: '%s'(%d).", strerror(errno), errno);
            return -1;
        }

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == self->signal_fd) {
                struct signalfd_siginfo info;
                if (read(self->signal_fd, &info, sizeof(info)) == sizeof(info)) {
                    LOG_INFO("Recieved signal %d, shutting down.", info.ssi_signo);
                    self->running = false;
                }
                continue;
            }

            if (events[i].data.fd == self->fd) {
                if (HS_ERROR_CHECK(err, _hs_client_acceptor(self->fd, self->epoll_fd)))
                    LOG_ERROR("Acceptor error: " HS_ERROR_FORMAT, HS_ERROR_ARGS(err));

            } else {
                if (HS_ERROR_CHECK(err, hs_handle_client(self, events[i].data.fd)))
                    LOG_ERROR("Failed to handle client: " HS_ERROR_FORMAT, HS_ERROR_ARGS(err));
                if (epoll_ctl(self->epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL) != 0)
                    LOG_ERROR("epoll_ctl error: errno='%s'(%d)", strerror(errno), errno);
                if (close(events[i].data.fd) != 0)
                    LOG_ERROR("close error: errno='%s'(%d)", strerror(errno), errno);
            }
        }
    }

    return 0;
}
