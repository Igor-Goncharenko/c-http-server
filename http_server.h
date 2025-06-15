#ifndef HS_SINGLE_FILE
# define HS_SINGLE_FILE
#endif

#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(__GNUC__) && !defined(__clang__)
#error "GCC or Clang required"
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#define HS_API extern
#define HS_STATIC static
#ifdef HS_SINGLE_FILE
# define HS_LIB static
#else
# define HS_LIB extern
#endif /* HS_SINGLE_FILE */

/*
 ********************************************
 *              HTTP REQUEST                *
 ********************************************
 */

typedef struct {
    char            *key;
    int             key_len;

    char            *value;
    int             value_len;
} hs_header_t;

#define HS_VERSION_LAST (HS_VERSION_UNKNOWN)
typedef enum {
    HS_VERSION_1_1 = 0,
    HS_VERSION_UNKNOWN
} hs_http_version_e;

#define HS_METHOD_LAST (HS_METHOD_UNKNOWN)
typedef enum {
    HS_METHOD_GET = 0,
    HS_METHOD_HEAD,
    HS_METHOD_POST,
    HS_METHOD_PUT,
    HS_METHOD_DELETE,
    HS_METHOD_CONNECT,
    HS_METHOD_OPTIONS,
    HS_METHOD_TRACE,
    HS_METHOD_UNKNOWN,
} hs_http_method_e;

typedef struct {
    char                *mem;
    size_t              mem_len;

    time_t              time;

    hs_http_method_e       method;
    char                *route;
    hs_http_version_e      version;
    int                 code;

    hs_header_t            *headers;
    int                 n_headers;

    char                *content;
    size_t              content_len;
} hs_request_data_t;


/*
 ********************************************
 *              HTTP RESPONSE               *
 ********************************************
 */

#define HS_CONTENT_TYPE_BUF_MAX 20

typedef struct {
    int                 code;

    char                content_type[HS_CONTENT_TYPE_BUF_MAX];
    char                *content;
    int                 content_len;

    struct {
        char            *data;
        int             len;
        int             cap;
    } headers;
} hs_response_t;

/*
 ********************************************
 *                  SERVER                  *
 ********************************************
 */

typedef int (*hs_route_callback)(const hs_request_data_t*, hs_response_t*);

typedef struct {
    hs_http_method_e    method;
    char                *route_tmp;
    hs_route_callback   cb;
} hs_server_route_t;

typedef struct {
    void                *mem;

    int                 fd;
    struct sockaddr_in  addr;

    int                 epoll_fd;
    struct epoll_event  event;

    bool                running;

    hs_server_route_t   *routes;
    int                 n_routes;
} hs_server_t;

/*
 ********************************************
 *              API FUNCTIONS               *
 ********************************************
 */

/**
 *
 */
HS_API int
hs_init_server(hs_server_t *self, const int port, const int to_listen, const hs_server_route_t *routes, 
        const int n_routes);

/**
 *
 */
HS_API int
hs_start_server(hs_server_t *self);

/**
 *
 */
HS_API void 
hs_server_destroy(hs_server_t *self);

/**
 *
 */
HS_API const char*
hs_get_header(const hs_request_data_t *request, const char *header);

/**
 *
 */
HS_API int 
hs_response_add_header(hs_response_t *resp, const char *key, const char *value);

/**
 *
 */
HS_API void 
hs_response_free(hs_response_t *resp);

/**
 *
 */
HS_API size_t 
hs_load_file(const char *filename, char **dest);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_SERVER_H_ */

#ifdef HS_IMPLEMENTATION

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>


/* http_server/http_server_internal.h */
#ifndef HTTP_SERVER_INTERNAL_H_
#define HTTP_SERVER_INTERNAL_H_


/*
 ********************************************
 *                  ERRORS                  *
 ********************************************
 */

typedef enum {
    HS_OK = 0,
    HS_ROUTE_ERR,
    /* General */
    HS_MALLOC_ERR,
    HS_STDIO_ERR,
    /* string errors */
    HS_STRTOK_ERR,
    HS_STRCPY_ERR,
    HS_STRCAT_ERR,
    HS_MEMCPY_ERR,
    HS_MEMSET_ERR,
    /* Server init errors */
    HS_SOCKET_CREATE_ERR,
    HS_BIND_ERR,
    HS_LISTEN_ERR,
    HS_EPOLL_CREATE_ERR,
    HS_EPOLL_CTL_ERR,
    HS_WRITE_ERR,
    /* Socket errors */
    HS_SOCKET_READ_ERR,
    /* buffer errors */
    HS_BUFFER_OVERFLOW_ERR,
} hs_err_e;

typedef struct {
    hs_err_e code;
    int             errno_;
    int             line;
} hs_err_t;

HS_LIB const char*
hs_strerror(hs_err_e err);

#define HS_STATIC_ASSERT(expr) typedef char static_assert_##__LINE__[(expr) ? 1 : -1]

#define HS_ERROR_CHECK(_hs_err_var_t, _hs_function)                             \
    ({                                                                          \
        HS_STATIC_ASSERT(                                                       \
                __builtin_types_compatible_p(typeof(_hs_function), hs_err_t));  \
        hs_err_t _hs_err_tmp = (_hs_function);                                  \
        (_hs_err_var_t) = _hs_err_tmp;                                          \
        (_hs_err_var_t.code != HS_OK);                                          \
    })

#define HS_CREATE_ERR(_err_e)           \
    ({                                  \
        int _saved_errno = errno;       \
        (hs_err_t){                     \
            .code = (_err_e),           \
            .line = __LINE__,           \
            .errno_ = _saved_errno,     \
        };                              \
    })

#define HS_ERROR_FORMAT "Error(hse='%s'(%d), errno='%s'(%d), line=%d"
#define HS_ERROR_ARGS(err) \
    hs_strerror((err).code), (err).code, strerror((err).errno_), (err).errno_, (err).line


/*
 ********************************************
 *                  LOGGER                  *
 ********************************************
 */

typedef enum {
    HS_LOG_LEVEL_TRACE = 0,
    HS_LOG_LEVEL_DEBUG,
    HS_LOG_LEVEL_INFO,
    HS_LOG_LEVEL_WARN,
    HS_LOG_LEVEL_ERROR,
} log_level_e;

HS_LIB void 
hs_log_log(const log_level_e level, const int line, const char *file, const char *func, 
        const char *fmt, ...);

#define LOG_TRACE(...) hs_log_log(HS_LOG_LEVEL_TRACE, __LINE__, __FILE__, __FUNCTION__, __VA_ARGS__)
#define LOG_DEBUG(...) hs_log_log(HS_LOG_LEVEL_DEBUG, __LINE__, __FILE__, __FUNCTION__, __VA_ARGS__)
#define LOG_INFO(...) hs_log_log(HS_LOG_LEVEL_INFO, __LINE__, __FILE__, __FUNCTION__, __VA_ARGS__)
#define LOG_WARN(...) hs_log_log(HS_LOG_LEVEL_WARN, __LINE__, __FILE__, __FUNCTION__, __VA_ARGS__)
#define LOG_ERROR(...) hs_log_log(HS_LOG_LEVEL_ERROR, __LINE__, __FILE__, __FUNCTION__, __VA_ARGS__)

/*
 ********************************************
 *                  BUFFER                  *
 ********************************************
 */

typedef struct {
    union {
        void            *mem;
        char            *data;
    };
    size_t              len;
    size_t              cap;

    bool                allow_realloc;
} hs_buffer_t;

/** @brief Inits buffer structure */
HS_LIB hs_err_t 
hs_buffer_init(hs_buffer_t *self);

/** @brief Inits buffer with set size */
HS_LIB hs_err_t 
hs_buffer_init_with_size(hs_buffer_t *self, size_t size);

/**
 * @brief Appends `nitems` with size `item_size` from `src` to buffer.
 * @warning This function may throw Assertion if `size_t` max limit reached.
 */
HS_LIB hs_err_t 
hs_buffer_append_mem(hs_buffer_t *self, const int item_size, const int nitems, const void *src,
        void **beginning_ptr);

/** @brief Frees mem from buffer structure and sets cap and len to zero */
HS_LIB void 
hs_buffer_free(hs_buffer_t *self);

/**  */
HS_LIB hs_err_t
hs_buffer_append_sentence(hs_buffer_t *self, const char *sentence, const size_t sentence_len, 
        char **beginning_ptr);

/**  */
HS_LIB hs_err_t
hs_buffer_join_buffer(hs_buffer_t *self, const hs_buffer_t *other, void **beginning_ptr);


/*
 ********************************************
 *          HTTP REQUEST PARSER             *
 ********************************************
 */

HS_LIB hs_err_t
hs_parse_http_request(hs_request_data_t *dest, hs_buffer_t *headers_raw);


/*
 ********************************************
 *              HTTP RESPONSE               *
 ********************************************
 */

HS_LIB const char*
hs_get_http_code_str(const int code);

HS_LIB hs_err_t 
hs_form_response(const hs_server_t *server, const hs_request_data_t *request, char **resp_dest, 
        int *resp_len);

HS_LIB hs_err_t 
hs_create_error_response(char **dest, int *dest_size, const int code);

#endif /* HTTP_SERVER_INTERNAL_H_ */

/* http_server/http_server.c */
#define READ_HEADERS_CHARS_LIMIT        8192
#define READ_BUFFER_LEN                 1024

#define MAX_EVENTS                      64

HS_API const char*
hs_get_header(const hs_request_data_t *request, const char *header) {
    for (int i = 0; i < request->n_headers; i++)
        if (strcmp(header, request->headers[i].key) == 0)
            return request->headers[i].value;
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

    if (    strcpy(resp->headers.data + resp->headers.len, key) == NULL ||
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
 *              CLIENT HANDLER              *
 ********************************************
 */

HS_STATIC hs_err_t
_hs_read_headers_raw(const int client_fd, hs_buffer_t *dest) {
    hs_err_t err;
    int chars_count = 0;
    int new_line_count = 0;
    char ch;

    char buffer[READ_BUFFER_LEN];
    int buffer_len = 0;

    if (HS_ERROR_CHECK(err, hs_buffer_init(dest)))
        goto failed;
    dest->allow_realloc = true;

    while (new_line_count < 2 && chars_count < READ_HEADERS_CHARS_LIMIT) {
        chars_count++;

        int n = read(client_fd, &ch, 1);
        if (n <= 0) {
            err = HS_CREATE_ERR(HS_SOCKET_READ_ERR);
            goto failed;
        }

        buffer[buffer_len++] = ch;
        if (buffer_len == READ_BUFFER_LEN) {
            if (HS_ERROR_CHECK(err, hs_buffer_append_mem(dest, 1, 1024, buffer, NULL)))
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

    if (HS_ERROR_CHECK(err, hs_buffer_append_mem(dest, 1, buffer_len, buffer, NULL)))
        goto failed;

    return HS_CREATE_ERR(HS_OK);

failed:
    hs_buffer_free(dest);
    return err;
}

HS_STATIC hs_err_t
_hs_read_body(const int client_fd, hs_buffer_t *dest, const int size) {
    hs_err_t err;
    char buffer[READ_BUFFER_LEN];
    int total_len = 0;

    if (HS_ERROR_CHECK(err, hs_buffer_init_with_size(dest, size)))
        goto failed;

    while (total_len < size) {
        int to_read = (size - total_len < READ_BUFFER_LEN) ? size - total_len : READ_BUFFER_LEN;

        int bytes_read = read(client_fd, buffer, to_read);
        if (bytes_read <= 0 || bytes_read != to_read) {
            err = HS_CREATE_ERR(HS_SOCKET_READ_ERR);
            goto failed;
        }

        if (HS_ERROR_CHECK(err, hs_buffer_append_mem(dest, 1, bytes_read, buffer, NULL)))
            goto failed;
        total_len += bytes_read;
    }
    return HS_CREATE_ERR(HS_OK);

failed:
    hs_buffer_free(dest);
    return err;
}

HS_STATIC int
_hs_get_content_len(const hs_request_data_t *request) {
    const char *value_str = hs_get_header(request, "Content-Length");
    return (value_str == NULL) ? 0 : atoi(value_str);
}

HS_STATIC hs_err_t
_hs_send_internal_error(const int fd) {
    hs_err_t err;
    char *resp = NULL;
    int resp_len;
    if (HS_ERROR_CHECK(err, hs_create_error_response(&resp, &resp_len, 500))) {
        return err;
    }

    write(fd, resp, resp_len);
    if (resp != NULL) {
        free(resp);
    }

    return HS_CREATE_ERR(HS_OK);
}

HS_STATIC hs_err_t
_hs_send_response(const int fd, const char *response, const int response_len) {
    if (response_len <= 0 || response == NULL) return HS_CREATE_ERR(HS_WRITE_ERR);
    int bytes_sent = 0;

    while (bytes_sent < response_len) {
        int n = write(fd, response + bytes_sent, response_len - bytes_sent);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return HS_CREATE_ERR(HS_WRITE_ERR);
        }
        bytes_sent += n;
    }
    return HS_CREATE_ERR(HS_OK);
}

HS_STATIC hs_err_t
_hs_handle_client(const hs_server_t *server, const int fd) {
    hs_err_t err = HS_CREATE_ERR(HS_OK);
    hs_request_data_t request = { 0 };
    hs_buffer_t headers_raw = { 0 }, content_buf = { 0 };

    char *response = NULL;
    int response_len = 0;

    LOG_DEBUG("New client handled: %d.", fd);

    if (HS_ERROR_CHECK(err, _hs_read_headers_raw(fd, &headers_raw)))
        goto cleanup;

    if (HS_ERROR_CHECK(err, hs_parse_http_request(&request, &headers_raw)))
        goto cleanup;

    int content_len = _hs_get_content_len(&request);

    LOG_DEBUG("Client %d: path=\"%s\".", fd, request.route);

    if (content_len > 0) {
        if (HS_ERROR_CHECK(err, _hs_read_body(fd, &content_buf, content_len)))
            goto cleanup;
        request.content_len = content_len;
        request.content = content_buf.data;
    }

    if (HS_ERROR_CHECK(err, hs_form_response(server, &request, &response, &response_len)))
        goto cleanup;

    if (HS_ERROR_CHECK(err, _hs_send_response(fd, response, response_len))) 
        goto cleanup;

cleanup:
    hs_buffer_free(&headers_raw);
    hs_buffer_free(&content_buf);
    if (request.mem != NULL) free(request.mem);
    if (response != NULL) free(response);
    if (err.code != HS_OK) _hs_send_internal_error(fd);

    return err;
}

/*
 ********************************************
 *                  SERVER                  *
 ********************************************
 */

/**/
HS_STATIC void 
_hs_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

HS_STATIC hs_err_t
_hs_cpy_init_routes_to_server(hs_server_t *self, const hs_server_route_t *routes, const int n_routes) {
    hs_err_t err;
    hs_buffer_t buf;


    /* count mem */
    const int routes_list_mem = n_routes * sizeof(hs_server_route_t);
    int total_mem = routes_list_mem;
    for (int i = 0; i < n_routes; i++)
        total_mem += strlen(routes[i].route_tmp) + 1;

    if (    HS_ERROR_CHECK(err, hs_buffer_init_with_size(&buf, total_mem)) ||
            HS_ERROR_CHECK(err, hs_buffer_append_mem(&buf, sizeof(hs_server_route_t), 
                    n_routes, NULL, NULL)))
        goto failed;

    self->routes = (hs_server_route_t*)buf.mem;

    if (memcpy(self->routes, routes, routes_list_mem) == NULL) {
        err = HS_CREATE_ERR(HS_MEMCPY_ERR);
        goto failed;
    }

    for (int i = 0; i < n_routes; i++) {
        if (HS_ERROR_CHECK(err, hs_buffer_append_sentence(&buf, routes[i].route_tmp, 
                        strlen(routes[i].route_tmp), &self->routes[i].route_tmp)))
            goto failed;
    }

    self->mem = buf.mem;
    self->n_routes = n_routes;

    return HS_CREATE_ERR(HS_OK);
failed:
    hs_buffer_free(&buf);
    return err;
}

HS_STATIC hs_err_t
_hs_init_server_epoll(hs_server_t *self) {
    hs_err_t err = HS_CREATE_ERR(HS_OK);

    if ((self->epoll_fd = epoll_create1(0)) == -1) {
        err = HS_CREATE_ERR(HS_EPOLL_CREATE_ERR);
        goto end;
    }

    self->event.events = EPOLLIN | EPOLLET;
    self->event.data.fd = self->fd;
    if (epoll_ctl(self->epoll_fd, EPOLL_CTL_ADD, self->fd, &self->event) == -1) {
        err = HS_CREATE_ERR(HS_EPOLL_CTL_ERR);
        goto end;
    }
end:
    return err;
}

HS_API void
hs_server_destroy(hs_server_t *self) {
    self->running = false;

    if (self->fd > 0)
        close(self->fd);
    self->fd = -1;

    if (self->epoll_fd > 0)
        close(self->epoll_fd);

    if (self->mem != NULL)
        free(self->mem);
}

HS_API int
hs_init_server(hs_server_t *self, const int port, const int to_listen, const hs_server_route_t *routes, 
        const int n_routes) {
    hs_err_t err;

    self->epoll_fd = -1;
    self->fd = -1;
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

    if (HS_ERROR_CHECK(err, _hs_cpy_init_routes_to_server(self, routes, n_routes)))
        goto failed;

    if (bind(self->fd, (struct sockaddr*)&self->addr, sizeof(self->addr)) != 0) {
        err = HS_CREATE_ERR(HS_BIND_ERR);
        goto failed;
    }

    _hs_set_nonblocking(self->fd);

    if (listen(self->fd, to_listen) != 0) {
        err = HS_CREATE_ERR(HS_LISTEN_ERR);
        goto failed;
    }

    self->running = true;

    if (HS_ERROR_CHECK(err, _hs_init_server_epoll(self)))
        goto failed;

    return HS_OK;

failed:
    hs_server_destroy(self);
    LOG_ERROR("Failed to init server, "HS_ERROR_FORMAT, HS_ERROR_ARGS(err));
    return -1;
}

HS_API int 
hs_start_server(hs_server_t *self) {
    hs_err_t err;
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

                    _hs_set_nonblocking(client_fd);

                    self->event.events = EPOLLIN | EPOLLET;
                    self->event.data.fd = client_fd;
                    if (epoll_ctl(self->epoll_fd, EPOLL_CTL_ADD, client_fd, &self->event) == -1) {
                        perror("epoll_ctl: client_socket");
                        close(client_fd);
                    }
                }
            } else {
                if (HS_ERROR_CHECK(err, _hs_handle_client(self, events[i].data.fd)))
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

/* http_server/buffer.c */
#define BUFFER_START_CAP    64

HS_LIB hs_err_t
hs_buffer_init(hs_buffer_t *self) {
    if ((self->mem = malloc(BUFFER_START_CAP)) == NULL)
        return HS_CREATE_ERR(HS_MALLOC_ERR);
    self->cap = BUFFER_START_CAP;
    self->len = 0;
    self->allow_realloc = false;
    return HS_CREATE_ERR(HS_OK);
}

HS_LIB hs_err_t
hs_buffer_init_with_size(hs_buffer_t *self, size_t size) {
    if ((self->mem = malloc(size)) == NULL)
        return HS_CREATE_ERR(HS_MALLOC_ERR);
    self->cap = size;
    self->len = 0;
    self->allow_realloc = false;
    return HS_CREATE_ERR(HS_OK);
}

HS_LIB hs_err_t
hs_buffer_append_mem(hs_buffer_t *self, const int item_size, const int nitems, const void *src, 
        void **beginning_ptr) {
    assert(!(SIZE_MAX / item_size < nitems));
    size_t add_size = item_size * nitems;

    if (add_size == 0)
        return HS_CREATE_ERR(HS_OK);

    assert(SIZE_MAX - self->len >= add_size);
    size_t new_size = self->len + add_size;

    if (new_size > self->cap) {
        if (self->allow_realloc == false) {
            fprintf(stderr, "Buffer overflow: required size = %lu, avaliable size = %lu\n",
                    new_size, self->cap);
            fprintf(stderr, "%s\n", self->data);
            fflush(stderr);
            return HS_CREATE_ERR(HS_BUFFER_OVERFLOW_ERR);
        }

        if (SIZE_MAX / 2 < new_size) {
            new_size = SIZE_MAX;
        } else {
            while (self->cap < new_size)
                self->cap *= 2;
        }
        if ((self->mem = realloc(self->mem, self->cap)) == NULL)
            return HS_CREATE_ERR(HS_MALLOC_ERR);
    }

    if (src != NULL && memcpy(self->mem + self->len, src, add_size) == NULL)
        return HS_CREATE_ERR(HS_MEMCPY_ERR);

    if (beginning_ptr != NULL)
        *beginning_ptr = self->mem + self->len;

    self->len = new_size;

    return HS_CREATE_ERR(HS_OK);
}

HS_LIB void 
hs_buffer_free(hs_buffer_t *self) {
    if (self != NULL) {
        if (self->mem != NULL) 
            free(self->mem);
        self->len = 0;
        self->cap = 0;
        self->mem = NULL;
    }
}

HS_LIB hs_err_t
hs_buffer_append_sentence(hs_buffer_t *self, const char *sentence, const size_t sentence_len, 
        char **beginning_ptr) {
    hs_err_t err;
    if (HS_ERROR_CHECK(err, hs_buffer_append_mem(self, 1, sentence_len, sentence, NULL)))
        return err;
    if (HS_ERROR_CHECK(err, hs_buffer_append_mem(self, 1, 1, "\0", NULL)))
        return err;
    if (beginning_ptr != NULL)
        *beginning_ptr = self->data + self->len - sentence_len - 1;
    return HS_CREATE_ERR(HS_OK);
}

HS_LIB hs_err_t
hs_buffer_join_buffer(hs_buffer_t *self, const hs_buffer_t *other, void **beginning_ptr) {
    hs_err_t err;
    if (HS_ERROR_CHECK(err, hs_buffer_append_mem(self, other->len, 1, other->mem, NULL)))
        return err;
    if (beginning_ptr != NULL)
        *beginning_ptr = self->mem + self->len - other->len;
    return HS_CREATE_ERR(HS_OK);
}

/* http_server/http_parser.c */
static const char
*HTTP_VERSION_STR[HS_VERSION_LAST] = {
    [HS_VERSION_1_1] = "HTTP/1.1",
};

static const char
*HTTP_METHODS_STR[] = {
    [HS_METHOD_GET]       = "GET",
    [HS_METHOD_HEAD]      = "HEAD",
    [HS_METHOD_POST]      = "POST",
    [HS_METHOD_PUT]       = "PUT",
    [HS_METHOD_DELETE]    = "DELETE",
    [HS_METHOD_CONNECT]   = "CONNECT",
    [HS_METHOD_OPTIONS]   = "OPTIONS",
    [HS_METHOD_TRACE]     = "TRACE",
};

HS_STATIC hs_http_version_e
_hs_parse_http_version(const char *version_str) {
    if (version_str == NULL)
        return HS_VERSION_UNKNOWN;

    for (int i = 0; i < HS_VERSION_LAST; i++)
        if (strcmp(version_str, HTTP_VERSION_STR[i]) == 0)
            return (hs_http_version_e)i;

    return HS_VERSION_UNKNOWN;
}

HS_STATIC hs_http_method_e
_hs_parse_http_method(const char *method_str) {
    if (method_str == NULL)
        return HS_METHOD_UNKNOWN;

    for (int i = 0; i < HS_METHOD_LAST; i++)
        if (strcmp(method_str, HTTP_METHODS_STR[i]) == 0)
            return (hs_http_method_e)i;

    return HS_METHOD_UNKNOWN;
}

HS_STATIC hs_err_t
_hs_parse_request_line(hs_request_data_t *dest, hs_buffer_t *buf, char *line) {
    char *save_ptr, *token;
    hs_err_t err;

    if ((token = strtok_r(line, " ", &save_ptr)) == NULL)
        return HS_CREATE_ERR(HS_STRTOK_ERR);
    dest->method = _hs_parse_http_method(token);

    if ((token = strtok_r(NULL, " ", &save_ptr)) == NULL)
        return HS_CREATE_ERR(HS_STRTOK_ERR);
    int token_len = strlen(token);
    if (HS_ERROR_CHECK(err, hs_buffer_append_sentence(buf, token, token_len, &dest->route)))
        return err;

    if ((token = strtok_r(NULL, " ", &save_ptr)) == NULL)
        return HS_CREATE_ERR(HS_STRTOK_ERR);
    dest->version = _hs_parse_http_version(token);

    return HS_CREATE_ERR(HS_OK);
}

HS_STATIC int
_hs_count_raw_lines(hs_buffer_t *headers_raw) {
    int counter = 1;
    for (int i = 0; i < headers_raw->len; i++) 
        if (headers_raw->data[i] == '\n')
            counter++;
    return counter;
}

HS_STATIC hs_err_t
_hs_parse_header(hs_header_t *dest, hs_buffer_t *buf, char *line) {
    hs_err_t err;

    char *delim_ptr = strchr(line, ':');
    *delim_ptr = '\0';

    /* printf("%s (%d)\n", line, strlen(line));fflush(stdout); */
    dest->key_len = strlen(line);
    if (HS_ERROR_CHECK(err, hs_buffer_append_sentence(buf, line, dest->key_len, &dest->key)))
        return err;

    delim_ptr += 2; // ignore ": " ("\0 ")
    dest->value_len = strlen(delim_ptr);
    if (HS_ERROR_CHECK(err, hs_buffer_append_sentence(buf, delim_ptr, dest->value_len, &dest->value)))
        return err;

    return HS_CREATE_ERR(HS_OK);
}

HS_LIB hs_err_t
hs_parse_http_request(hs_request_data_t *dest, hs_buffer_t *headers_raw) {
    hs_err_t err = HS_CREATE_ERR(HS_OK);
    hs_buffer_t req_buf;
    char *save_ptr, *token;

    {   /* exclude request line, empty line and line after empty line */
        dest->n_headers = _hs_count_raw_lines(headers_raw) - 3;
        size_t headers_size = dest->n_headers * sizeof(hs_header_t);
        size_t buf_size = headers_raw->len + headers_size;
        void *ptr;
        if (    HS_ERROR_CHECK(err, hs_buffer_init_with_size(&req_buf, buf_size)) ||
                HS_ERROR_CHECK(err, hs_buffer_append_mem(&req_buf, 1, headers_size, NULL, &ptr)))
            goto cleanup;
        dest->headers = (hs_header_t*)ptr;
    }

    dest->time = time(NULL);

    if ((token = strtok_r(headers_raw->data, "\r", &save_ptr)) == NULL) {
        err = HS_CREATE_ERR(HS_STRTOK_ERR);
        goto cleanup;
    }
    if (HS_ERROR_CHECK(err, _hs_parse_request_line(dest, &req_buf, token)))
        goto cleanup;

    /* We are splitting by '\r', therefore, at the beginning of the line there is '\n' */
    token = strtok_r(NULL, "\r", &save_ptr) + 1;    /* Ignore '\n' */
    for (int i = 0; i < dest->n_headers && token != NULL && strlen(token) > 4; i++) {
        /* header_t new_header; */
        if (HS_ERROR_CHECK(err, _hs_parse_header(&dest->headers[i], &req_buf, token)))
            goto cleanup;

        token = strtok_r(NULL, "\r", &save_ptr) + 1;
    }

    dest->mem = req_buf.data;
    dest->mem_len = req_buf.len;

cleanup:
    if (err.code != HS_OK) {
        memset(dest, 0, sizeof(hs_request_data_t));
        hs_buffer_free(&req_buf);
    }
    return err;
}

/* http_server/logger.c */

static const char
*_level_strings[] = {
    [HS_LOG_LEVEL_TRACE] = "TRACE",
    [HS_LOG_LEVEL_DEBUG] = "DEBUG",
    [HS_LOG_LEVEL_INFO]  = "INFO",
    [HS_LOG_LEVEL_WARN]  = "WARN",
    [HS_LOG_LEVEL_ERROR] = "ERROR",
};

static const int 
_level_colors[] = {
    [HS_LOG_LEVEL_TRACE] = 39,     /* default terminal color */
    [HS_LOG_LEVEL_DEBUG] = 37,     /* white */
    [HS_LOG_LEVEL_INFO]  = 32,     /* green */
    [HS_LOG_LEVEL_WARN]  = 33,     /* yellow */
    [HS_LOG_LEVEL_ERROR] = 31,     /* red */
};

HS_LIB void 
hs_log_log(const log_level_e level, const int line, const char *file, const char *func, 
        const char *fmt, ...) {
    char time_buf[16], date_buf[16];
    va_list args;
    time_t t = time(NULL);

    struct tm *l_time = localtime(&t);
    time_buf[strftime(time_buf, 16, "%H:%M:%S", l_time)] = '\0';
    date_buf[strftime(date_buf, 16, "%Y-%m-%d", l_time)] = '\0';

    va_start(args, fmt);

    printf("\033[1;%dm", _level_colors[level]);
    printf("%s/%s %-7s %s:%s:%d ", time_buf, date_buf, _level_strings[level], file, func, line);
    vprintf(fmt, args);
    printf("\033[0m\n");
    fflush(stdout);

    va_end(args);
}

/* http_server/error.c */

HS_LIB const char*
hs_strerror(hs_err_e err) {
    static const char *hs_error_str[] = {
        [HS_OK]                    = "Ok",
        [HS_ROUTE_ERR]             = "Route_err",
        /* General */
        [HS_MALLOC_ERR]            = "Malloc_err",
        [HS_STDIO_ERR]             = "Stdio_err",
        /* string errors */
        [HS_STRTOK_ERR]            = "Strtok_err",
        [HS_STRCPY_ERR]            = "Strcpy_err",
        [HS_STRCAT_ERR]            = "Strcat_err",
        [HS_MEMCPY_ERR]            = "Memcpy_err",
        [HS_MEMSET_ERR]            = "Memset_err",
        /* Server init errors */
        [HS_SOCKET_CREATE_ERR]     = "Socket_create_err",
        [HS_BIND_ERR]              = "Bind_err",
        [HS_LISTEN_ERR]            = "Listen_err",
        [HS_EPOLL_CREATE_ERR]      = "Epoll_create_err",
        [HS_EPOLL_CTL_ERR]         = "Epoll_ctl_err",
        [HS_WRITE_ERR]             = "Write_err",
        /* Socket errors */
        [HS_SOCKET_READ_ERR]       = "Socket_read_err",
        /* buffer errors */
        [HS_BUFFER_OVERFLOW_ERR]   = "Buffer_overflow_err",
    };

    return hs_error_str[err];
}

/* http_server/http_response.c */

static const char
*HTTP_CODE_STR[] = {
    [100] = "CONTINUE",
    [101] = "SWITCHING PROTOCOLS",
    [200] = "OK",
    [201] = "CREATED",
    [202] = "ACCEPTED",
    [203] = "NON-AUTHORITATIVE INFORMATION",
    [204] = "NO CONTENT",
    [205] = "RESET CONTENT",
    [206] = "PARTIAL CONTENT",
    [300] = "MULTIPLE CHOICES",
    [301] = "MOVED PERMANENTLY",
    [302] = "FOUND",
    [303] = "SEE OTHER",
    [304] = "NOT MODIFIED",
    [305] = "USE PROXY",
    [306] = "UNUSED",
    [307] = "TEMPORARY REDIRECT",
    [400] = "BAD REQUEST",
    [401] = "UNAUTHORIZED",
    [402] = "PAYMENT REQUIRED",
    [403] = "FORBIDDEN",
    [404] = "NOT FOUND",
    [405] = "METHOD NOT ALLOWED",
    [406] = "NOT ACCEPTABLE",
    [407] = "PROXY AUTHENTICATION REQUIRED",
    [408] = "REQUEST TIMEOUT",
    [409] = "CONFLICT",
    [410] = "GONE",
    [411] = "LENGTH REQUIRED",
    [412] = "PRECONDITION FAILED",
    [413] = "REQUEST ENTITY TOO LARGE",
    [414] = "REQUEST-URL TOO LONG",
    [415] = "UNSUPPORTED MEDIA TYPE",
    [416] = "REQUESTED RANGE NOT SATISFIABLE",
    [417] = "EXPECTATION FAILED",
    [500] = "INTERNAL SERVER ERROR",
    [501] = "NOT IMPLEMENTED",
    [502] = "BAD GATEWAY",
    [503] = "SERVICE UNAVAILABLE",
    [504] = "GATEWAY TIMEOUT",
    [505] = "HTTP VERSION NOT SUPPORTED",
};

HS_LIB const char*
get_http_code_str(const int code) {
    if (code < 0 || code > sizeof(HTTP_CODE_STR) / 8)
        return "UNKNOWN";
    const char *res = HTTP_CODE_STR[code];
    return (res != NULL) ? res : "UNKNOWN";
}

/*
 ********************************************
 *                  ROUTE                   *
 ********************************************
 */

HS_STATIC hs_server_route_t* 
_find_route(const hs_server_t *server, const hs_request_data_t *request) {
    hs_server_route_t *found_route = NULL;
    va_list args_cpy;

    for (int i = 0; i < server->n_routes; i++) {
        hs_server_route_t *route = &server->routes[i];
        // if (route->n_args > 0) {
        //     va_copy(args_cpy, route->args);
        //     if (vsscanf(request->route, route->route_tmp, args_cpy) == route->n_args) {
        //         found_route = route;
        //         break;
        //     }
        //} else 
        if (strcmp(route->route_tmp, request->route) == 0) {
            found_route = route;
            break;
        }
    }

    return found_route;
}

HS_STATIC hs_err_t
_process_route(hs_server_route_t *route, const hs_request_data_t *request, char **resp_dest, 
        int *resp_len) {
    static const char resp_fmt[] = 
        "HTTP/1.1 %3d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "%s"
        "\r\n";

    hs_err_e err_e = HS_OK;
    const char *type_str, *subtype_str;
    hs_response_t resp = { 0 };

    if (route->cb(request, &resp) != 0) {
        err_e = HS_ROUTE_ERR;
        goto cleanup;
    }

    const char *code_str = get_http_code_str(resp.code);
    const int code_str_len = strlen(code_str);

    *resp_dest = malloc(resp.content_len + code_str_len + sizeof(resp_fmt) + 256);
    if (*resp_dest == NULL) {
        err_e = HS_MALLOC_ERR;
        goto cleanup;
    }

    *resp_len = sprintf(*resp_dest, resp_fmt, resp.code, code_str, resp.content_type, 
            resp.content_len, (resp.headers.len <= 0) ? "" : resp.headers.data);
    if (*resp_len <= 0) {
        err_e = HS_STDIO_ERR;
        goto cleanup;
    }

    if (resp.content_len > 0) {
        if (memcpy((*resp_dest) + (*resp_len), resp.content, resp.content_len) == NULL) {
            err_e = HS_MEMCPY_ERR;
            goto cleanup;
        }
        *resp_len += resp.content_len;
    }

cleanup:
    hs_response_free(&resp);
    return HS_CREATE_ERR(err_e);
}

HS_LIB hs_err_t
hs_form_response(const hs_server_t *server, const hs_request_data_t *request, char **resp_dest, 
        int *resp_len) {
    hs_err_t err;
    hs_server_route_t *found_route = _find_route(server, request);

    *resp_dest = NULL;
    *resp_len = 0;

    if (found_route == NULL) {
        if (HS_ERROR_CHECK(err, hs_create_error_response(resp_dest, resp_len, 404)))
            return err;
    } else {
        if (HS_ERROR_CHECK(err, _process_route(found_route, request, resp_dest, resp_len))) {
            *resp_dest = NULL;
            *resp_len = 0;
            return err;
        }
    }
    return HS_CREATE_ERR(HS_OK);
}

/*
 ********************************************
 *              ERROR PAGE                  *
 ********************************************
 */

static const char 
ERROR_FMT[] = 
"HTTP/1.1 %3d %s\r\n"
"Content-Length: %d\r\n"
"Content-Type: text/html\r\n"
"Connection: Closed\r\n"
"\r\n";

static const char 
ERROR_PAGE_BODY[] = 
"<!DOCTYPE html>"
"<html lang=\"en\">"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"<meta http-equiv=\"X-UA-Compatible\" content=\"ie=edge\">"
"<title>%3d %s</title>"
"</head>"
"<body>"
"<h1>Failed to load page</h1>"
"<h1>%3d %s</h1>"
"</body>"
"</html>";

HS_LIB hs_err_t
hs_create_error_response(char **dest, int *dest_size, const int code) {
    hs_err_e err_e;

    *dest = NULL;
    *dest_size = 0;

    const char *code_str = get_http_code_str(code);
    const int code_len = strlen(code_str);
    const int header_len = sizeof(ERROR_FMT) + code_len;
    const int content_len = sizeof(ERROR_PAGE_BODY) + code_len * 2;

    if ((*dest = malloc(header_len + content_len + 10)) == NULL) {
        err_e = HS_MALLOC_ERR;
        goto failed;
    }

    const int fin_content_len = snprintf(NULL, 0, ERROR_PAGE_BODY, code, code_str, code, code_str);
    if (fin_content_len <= 0) {
        err_e = HS_STDIO_ERR;
        goto failed;
    }

    const int fin_header_len = snprintf(*dest, header_len, ERROR_FMT, code, code_str, 
            fin_content_len);
    if (fin_content_len <= 0) {
        err_e = HS_STDIO_ERR;
        goto failed;
    }

    if (snprintf(*dest + fin_header_len, content_len, ERROR_PAGE_BODY, code, code_str, code, 
                code_str) <= 0) {
        err_e = HS_STDIO_ERR;
        goto failed;
    }

    *dest_size = fin_header_len + fin_content_len;

    return HS_CREATE_ERR(HS_OK);

failed:
    if (*dest != NULL) {
        free(*dest);
        *dest = NULL;
    }
    *dest_size = 0;
    return HS_CREATE_ERR(err_e);
}

#endif /* HS_IMPLEMENTATION */
