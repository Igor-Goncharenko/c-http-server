#ifndef HTTP_SERVER_SINGLE_FILE
# define HTTP_SERVER_SINGLE_FILE
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

#define HTTP_SERVER_API         extern
#define HTTP_SERVER_STATIC      static
#ifdef HTTP_SERVER_SINGLE_FILE
# define HTTP_SERVER_LIB        static
#else
# define HTTP_SERVER_LIB        extern
#endif /* HTTP_SERVER_SINGLE_FILE */

/*
 ********************************************
 *              CONTENT TYPES               *
 ********************************************
 */

typedef enum {
    HS_CONTENT_TYPE_TEXT = 0,
    HS_CONTENT_TYPE_IMAGE,
    HS_CONTENT_TYPE_APPLICATION,
} hs_content_main_type_e;

typedef enum {
    HS_SUBTYPE_TEXT_PLAIN = 0,
    HS_SUBTYPE_TEXT_HTML,
    HS_SUBTYPE_TEXT_CSS,
    HS_SUBTYPE_TEXT_CSV,
    HS_SUBTYPE_TEXT_XML,
} hs_content_subtype_text_e;

typedef enum {
    HS_SUBTYPE_IMAGE_JPEG = 0,
    HS_SUBTYPE_IMAGE_PNG,
    HS_SUBTYPE_IMAGE_GIF,
    HS_SUBTYPE_IMAGE_WEBP,
    HS_SUBTYPE_IMAGE_SVG,
    HS_SUBTYPE_IMAGE_AVIF,
} hs_content_subtype_image_e;

typedef enum {
    HS_SUBTYPE_APPLICATION_JSON = 0,
    HS_SUBTYPE_APPLICATION_XML,
    HS_SUBTYPE_APPLICATION_PDF,
    HS_SUBTYPE_APPLICATION_ZIP,
    HS_SUBTYPE_APPLICATION_JAVASCRIPT,
    HS_SUBTYPE_APPLICATION_WASM,
} hs_content_subtype_application_e;

typedef struct {
    hs_content_main_type_e type;
    union {
        hs_content_subtype_text_e text;
        hs_content_subtype_image_e image;
        hs_content_subtype_application_e application;
    } subtype;
} hs_content_type_t;

/*
 ********************************************
 *          HTTP REQUEST/RESPONSE           *
 ********************************************
 */

typedef struct {
    char            *key;
    int             key_len;

    char            *value;
    int             value_len;
} header_t;

#define HTTP_VERSION_LAST   HTTP_VERSION_UNKNOWN
typedef enum {
    HTTP_VERSION_1_1 = 0,
    HTTP_VERSION_UNKNOWN
} http_version_e;

#define HTTP_METHOD_LAST    HTTP_METHOD_UNKNOWN
typedef enum {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_CONNECT,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_TRACE,
    HTTP_METHOD_UNKNOWN,
} http_method_e;

typedef struct {
    char                *mem;
    size_t              mem_len;

    time_t              time;

    http_method_e       method;
    char                *route;
    http_version_e      version;
    int                 code;

    header_t            *headers;
    int                 n_headers;

    char                *content;
    size_t              content_len;
} request_data_t;

typedef int (*route_callback)(const request_data_t*, const va_list, char**, int*);

/*
 ********************************************
 *                  SERVER                  *
 ********************************************
 */

typedef struct {
    http_method_e       method;
    char                *route_tmp;
    route_callback      cb;
    va_list             args;
    int                 n_args;

    hs_content_type_t   content_type;
} server_route_t;

typedef struct {
    void                *mem;

    int                 fd;
    struct sockaddr_in  addr;

    int                 epoll_fd;
    struct epoll_event  event;

    bool                running;

    server_route_t      *routes;
    int                 n_routes;
} server_t;

/*
 ********************************************
 *              API FUNCTIONS               *
 ********************************************
 */

/**
 *
 */
HTTP_SERVER_API int
init_server(server_t *self, const int port, const int to_listen, const server_route_t *routes, 
        const int n_routes);

/**
 *
 */
HTTP_SERVER_API int
start_server(server_t *self);

/**
 *
 */
HTTP_SERVER_API void 
server_destroy(server_t *self);

/**
 *
 */
HTTP_SERVER_API const char*
get_header(const request_data_t *request, const char *header);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_SERVER_H_ */

#ifdef HTTP_SERVER_IMPLEMENTATION

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


/* http_server/http_server_internal.h */
#ifndef HTTP_SERVER_INTERNAL_H_
#define HTTP_SERVER_INTERNAL_H_


/*
 ********************************************
 *                  ERRORS                  *
 ********************************************
 */

typedef enum {
    HTTP_SERVER_OK = 0,
    HTTP_SERVER_ROUTE_ERR,
    /* General */
    HTTP_SERVER_MALLOC_ERR,
    HTTP_SERVER_STDIO_ERR,
    /* string errors */
    HTTP_SERVER_STRTOK_ERR,
    HTTP_SERVER_STRCPY_ERR,
    HTTP_SERVER_STRCAT_ERR,
    HTTP_SERVER_MEMCPY_ERR,
    HTTP_SERVER_MEMSET_ERR,
    /* Server init errors */
    HTTP_SERVER_SOCKET_CREATE_ERR,
    HTTP_SERVER_BIND_ERR,
    HTTP_SERVER_LISTEN_ERR,
    HTTP_SERVER_EPOLL_CREATE_ERR,
    HTTP_SERVER_EPOLL_CTL_ERR,
    HTTP_SERVER_WRITE_ERR,
    /* Socket errors */
    HTTP_SERVER_SOCKET_READ_ERR,
    /* buffer errors */
    HTTP_SERVER_BUFFER_OVERFLOW_ERR,
} http_server_err_e;

typedef struct {
    http_server_err_e code;
    int             errno_;
    int             line;
} http_server_err_t;

HTTP_SERVER_LIB const char*
hs_strerror(http_server_err_e err);

#define HS_STATIC_ASSERT(expr) typedef char static_assert_##__LINE__[(expr) ? 1 : -1]

#define HS_ERROR_CHECK(_hs_err_var_t, _hs_function)                                     \
    ({                                                                                  \
        HS_STATIC_ASSERT(                                                               \
                __builtin_types_compatible_p(typeof(_hs_function), http_server_err_t)); \
        http_server_err_t _hs_err_tmp = (_hs_function);                                 \
        (_hs_err_var_t) = _hs_err_tmp;                                                  \
        (_hs_err_var_t.code != HTTP_SERVER_OK);                                         \
    })

#define HS_CREATE_ERR(_err_e)           \
    ({                                  \
        int _saved_errno = errno;       \
        (http_server_err_t){            \
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
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
} log_level_e;

HTTP_SERVER_LIB void 
log_log(const log_level_e level, const int line, const char *file, const char *func, 
        const char *fmt, ...);

#define LOG_TRACE(...)  log_log(LOG_LEVEL_TRACE, __LINE__, __FILE__, __FUNCTION__, __VA_ARGS__)
#define LOG_DEBUG(...)  log_log(LOG_LEVEL_DEBUG, __LINE__, __FILE__, __FUNCTION__, __VA_ARGS__)
#define LOG_INFO(...)   log_log(LOG_LEVEL_INFO, __LINE__, __FILE__, __FUNCTION__, __VA_ARGS__)
#define LOG_WARN(...)   log_log(LOG_LEVEL_WARN, __LINE__, __FILE__, __FUNCTION__, __VA_ARGS__)
#define LOG_ERROR(...)  log_log(LOG_LEVEL_ERROR, __LINE__, __FILE__, __FUNCTION__, __VA_ARGS__)

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
} buffer_t;

/** @brief Inits buffer structure */
HTTP_SERVER_LIB http_server_err_t 
buffer_init(buffer_t *self);

/** @brief Inits buffer with set size */
HTTP_SERVER_LIB http_server_err_t 
buffer_init_with_size(buffer_t *self, size_t size);

/**
 * @brief Appends `nitems` with size `item_size` from `src` to buffer.
 * @warning This function may throw Assertion if `size_t` max limit reached.
 */
HTTP_SERVER_LIB http_server_err_t 
buffer_append_mem(buffer_t *self, const int item_size, const int nitems, const void *src,
        void **beginning_ptr);

/** @brief Frees mem from buffer structure and sets cap and len to zero */
HTTP_SERVER_LIB void 
buffer_free(buffer_t *self);

/**  */
HTTP_SERVER_LIB http_server_err_t
buffer_append_sentence(buffer_t *self, const char *sentence, const size_t sentence_len, 
        char **beginning_ptr);

/**  */
HTTP_SERVER_LIB http_server_err_t
buffer_join_buffer(buffer_t *self, const buffer_t *other, void **beginning_ptr);


/*
 ********************************************
 *          HTTP REQUEST PARSER             *
 ********************************************
 */

HTTP_SERVER_LIB http_server_err_t
parse_http_request(request_data_t *dest, buffer_t *headers_raw);


/*
 ********************************************
 *              HTTP RESPONSE               *
 ********************************************
 */

HTTP_SERVER_LIB const char*
get_http_code_str(const int code);

HTTP_SERVER_LIB http_server_err_t 
form_response(const server_t *server, const request_data_t *request, char **resp_dest, 
        int *resp_len);


/*
 ********************************************
 *              ERROR PAGE                  *
 ********************************************
 */

HTTP_SERVER_LIB http_server_err_t 
create_error_response(char **dest, int *dest_size, const int code);

#endif /* HTTP_SERVER_INTERNAL_H_ */

/* http_server/http_server.c */
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

/* http_server/buffer.c */
#define BUFFER_START_CAP    64

HTTP_SERVER_LIB http_server_err_t
buffer_init(buffer_t *self) {
    if ((self->mem = malloc(BUFFER_START_CAP)) == NULL)
        return HS_CREATE_ERR(HTTP_SERVER_MALLOC_ERR);
    self->cap = BUFFER_START_CAP;
    self->len = 0;
    self->allow_realloc = false;
    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

HTTP_SERVER_LIB http_server_err_t 
buffer_init_with_size(buffer_t *self, size_t size) {
    if ((self->mem = malloc(size)) == NULL)
        return HS_CREATE_ERR(HTTP_SERVER_MALLOC_ERR);
    self->cap = size;
    self->len = 0;
    self->allow_realloc = false;
    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

HTTP_SERVER_LIB http_server_err_t 
buffer_append_mem(buffer_t *self, const int item_size, const int nitems, const void *src, 
        void **beginning_ptr) {
    assert(!(SIZE_MAX / item_size < nitems));
    size_t add_size = item_size * nitems;

    if (add_size == 0)
        return HS_CREATE_ERR(HTTP_SERVER_OK);

    assert(SIZE_MAX - self->len >= add_size);
    size_t new_size = self->len + add_size;

    if (new_size > self->cap) {
        if (self->allow_realloc == false) {
            fprintf(stderr, "Buffer overflow: required size = %lu, avaliable size = %lu\n",
                    new_size, self->cap);
            fprintf(stderr, "%s\n", self->data);
            fflush(stderr);
            return HS_CREATE_ERR(HTTP_SERVER_BUFFER_OVERFLOW_ERR);
        }

        if (SIZE_MAX / 2 < new_size) {
            new_size = SIZE_MAX;
        } else {
            while (self->cap < new_size)
                self->cap *= 2;
        }
        if ((self->mem = realloc(self->mem, self->cap)) == NULL)
            return HS_CREATE_ERR(HTTP_SERVER_MALLOC_ERR);
    }

    if (src != NULL && memcpy(self->mem + self->len, src, add_size) == NULL)
        return HS_CREATE_ERR(HTTP_SERVER_MEMCPY_ERR);

    if (beginning_ptr != NULL)
        *beginning_ptr = self->mem + self->len;

    self->len = new_size;

    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

HTTP_SERVER_LIB void 
buffer_free(buffer_t *self) {
    if (self != NULL) {
        if (self->mem != NULL) 
            free(self->mem);
        self->len = 0;
        self->cap = 0;
        self->mem = NULL;
    }
}

HTTP_SERVER_LIB http_server_err_t
buffer_append_sentence(buffer_t *self, const char *sentence, const size_t sentence_len, 
        char **beginning_ptr) {
    http_server_err_t err;
    if (HS_ERROR_CHECK(err, buffer_append_mem(self, 1, sentence_len, sentence, NULL)))
        return err;
    if (HS_ERROR_CHECK(err, buffer_append_mem(self, 1, 1, "\0", NULL)))
        return err;
    if (beginning_ptr != NULL)
        *beginning_ptr = self->data + self->len - sentence_len - 1;
    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

HTTP_SERVER_LIB http_server_err_t
buffer_join_buffer(buffer_t *self, const buffer_t *other, void **beginning_ptr) {
    http_server_err_t err;
    if (HS_ERROR_CHECK(err, buffer_append_mem(self, other->len, 1, other->mem, NULL)))
        return err;
    if (beginning_ptr != NULL)
        *beginning_ptr = self->mem + self->len - other->len;
    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

/* http_server/http_parser.c */
static const char
*HTTP_VERSION_STR[HTTP_VERSION_LAST] = {
    [HTTP_VERSION_1_1] = "HTTP/1.1",
};

static const char
*HTTP_METHODS_STR[] = {
    [HTTP_METHOD_GET]       = "GET",
    [HTTP_METHOD_HEAD]      = "HEAD",
    [HTTP_METHOD_POST]      = "POST",
    [HTTP_METHOD_PUT]       = "PUT",
    [HTTP_METHOD_DELETE]    = "DELETE",
    [HTTP_METHOD_CONNECT]   = "CONNECT",
    [HTTP_METHOD_OPTIONS]   = "OPTIONS",
    [HTTP_METHOD_TRACE]     = "TRACE",
};

HTTP_SERVER_STATIC http_version_e
_parse_http_version(const char *version_str) {
    if (version_str == NULL)
        return HTTP_VERSION_UNKNOWN;

    for (int i = 0; i < HTTP_VERSION_LAST; i++)
        if (strcmp(version_str, HTTP_VERSION_STR[i]) == 0)
            return (http_version_e)i;

    return HTTP_VERSION_UNKNOWN;
}

HTTP_SERVER_STATIC http_method_e
_parse_http_method(const char *method_str) {
    if (method_str == NULL)
        return HTTP_METHOD_UNKNOWN;

    for (int i = 0; i < HTTP_METHOD_LAST; i++)
        if (strcmp(method_str, HTTP_METHODS_STR[i]) == 0)
            return (http_method_e)i;

    return HTTP_METHOD_UNKNOWN;
}

HTTP_SERVER_STATIC http_server_err_t
_parse_request_line(request_data_t *dest, buffer_t *buf, char *line) {
    char *save_ptr, *token;
    http_server_err_t err;

    if ((token = strtok_r(line, " ", &save_ptr)) == NULL)
        return HS_CREATE_ERR(HTTP_SERVER_STRTOK_ERR);
    dest->method = _parse_http_method(token);

    if ((token = strtok_r(NULL, " ", &save_ptr)) == NULL)
        return HS_CREATE_ERR(HTTP_SERVER_STRTOK_ERR);
    int token_len = strlen(token);
    if (HS_ERROR_CHECK(err, buffer_append_sentence(buf, token, token_len, &dest->route)))
        return err;

    if ((token = strtok_r(NULL, " ", &save_ptr)) == NULL)
        return HS_CREATE_ERR(HTTP_SERVER_STRTOK_ERR);
    dest->version = _parse_http_version(token);

    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

HTTP_SERVER_STATIC int
_count_raw_lines(buffer_t *headers_raw) {
    int counter = 1;
    for (int i = 0; i < headers_raw->len; i++) 
        if (headers_raw->data[i] == '\n')
            counter++;
    return counter;
}

HTTP_SERVER_STATIC http_server_err_t
_parse_header(header_t *dest, buffer_t *buf, char *line) {
    http_server_err_t err;

    char *delim_ptr = strchr(line, ':');
    *delim_ptr = '\0';

    /* printf("%s (%d)\n", line, strlen(line));fflush(stdout); */
    dest->key_len = strlen(line);
    if (HS_ERROR_CHECK(err, buffer_append_sentence(buf, line, dest->key_len, &dest->key)))
        return err;

    delim_ptr += 2; // ignore ": " ("\0 ")
    dest->value_len = strlen(delim_ptr);
    if (HS_ERROR_CHECK(err, buffer_append_sentence(buf, delim_ptr, dest->value_len, &dest->value)))
        return err;

    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

HTTP_SERVER_LIB http_server_err_t
parse_http_request(request_data_t *dest, buffer_t *headers_raw) {
    http_server_err_t err = HS_CREATE_ERR(HTTP_SERVER_OK);
    buffer_t req_buf;
    char *save_ptr, *token;

    {   /* exclude request line, empty line and line after empty line */
        dest->n_headers = _count_raw_lines(headers_raw) - 3;
        size_t headers_size = dest->n_headers * sizeof(header_t);
        size_t buf_size = headers_raw->len + headers_size;
        void *ptr;
        if (    HS_ERROR_CHECK(err, buffer_init_with_size(&req_buf, buf_size)) ||
                HS_ERROR_CHECK(err, buffer_append_mem(&req_buf, 1, headers_size, NULL, &ptr)))
            goto cleanup;
        dest->headers = (header_t*)ptr;
    }

    dest->time = time(NULL);

    if ((token = strtok_r(headers_raw->data, "\r", &save_ptr)) == NULL) {
        err = HS_CREATE_ERR(HTTP_SERVER_STRTOK_ERR);
        goto cleanup;
    }
    if (HS_ERROR_CHECK(err, _parse_request_line(dest, &req_buf, token)))
        goto cleanup;

    /* We are splitting by '\r', therefore, at the beginning of the line there is '\n' */
    token = strtok_r(NULL, "\r", &save_ptr) + 1;    /* Ignore '\n' */
    for (int i = 0; i < dest->n_headers && token != NULL && strlen(token) > 4; i++) {
        /* header_t new_header; */
        if (HS_ERROR_CHECK(err, _parse_header(&dest->headers[i], &req_buf, token)))
            goto cleanup;

        token = strtok_r(NULL, "\r", &save_ptr) + 1;
    }

    dest->mem = req_buf.data;
    dest->mem_len = req_buf.len;

cleanup:
    if (err.code != HTTP_SERVER_OK) {
        memset(dest, 0, sizeof(request_data_t));
        buffer_free(&req_buf);
    }
    return err;
}

/* http_server/logger.c */

static const char
*_level_strings[] = {
    [LOG_LEVEL_TRACE] = "TRACE",
    [LOG_LEVEL_DEBUG] = "DEBUG",
    [LOG_LEVEL_INFO]  = "INFO",
    [LOG_LEVEL_WARN]  = "WARN",
    [LOG_LEVEL_ERROR] = "ERROR",
};

static const int 
_level_colors[] = {
    [LOG_LEVEL_TRACE] = 39,     /* default terminal color */
    [LOG_LEVEL_DEBUG] = 37,     /* white */
    [LOG_LEVEL_INFO]  = 32,     /* green */
    [LOG_LEVEL_WARN]  = 33,     /* yellow */
    [LOG_LEVEL_ERROR] = 31,     /* red */
};

HTTP_SERVER_LIB void 
log_log(const log_level_e level, const int line, const char *file, const char *func, 
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

HTTP_SERVER_LIB const char*
hs_strerror(http_server_err_e err) {
    static const char *hs_error_str[] = {
        [HTTP_SERVER_OK]                    = "Ok",
        /* General */
        [HTTP_SERVER_MALLOC_ERR]            = "Malloc_err",
        /* string errors */
        [HTTP_SERVER_STRTOK_ERR]            = "Strtok_err",
        [HTTP_SERVER_STRCPY_ERR]            = "Strcpy_err",
        [HTTP_SERVER_STRCAT_ERR]            = "Strcat_err",
        [HTTP_SERVER_MEMCPY_ERR]            = "Memcpy_err",
        [HTTP_SERVER_MEMSET_ERR]            = "Memset_err",
        /* Server init errors */
        [HTTP_SERVER_SOCKET_CREATE_ERR]     = "Socket_create_err",
        [HTTP_SERVER_BIND_ERR]              = "Bind_err",
        [HTTP_SERVER_LISTEN_ERR]            = "Listen_err",
        [HTTP_SERVER_EPOLL_CREATE_ERR]      = "Epoll_create_err",
        [HTTP_SERVER_EPOLL_CTL_ERR]         = "Epoll_ctl_err",
        /* Socket errors */
        [HTTP_SERVER_SOCKET_READ_ERR]       = "Socket_read_err",
        /* buffer errors */
        [HTTP_SERVER_BUFFER_OVERFLOW_ERR]   = "Buffer_overflow_err",
    };

    assert(err > sizeof(hs_error_str) / 8);

    const char *res = hs_error_str[err];

    if (res == NULL) {
        LOG_ERROR("Error with code %d not set", err);
        exit(EXIT_FAILURE);
    }

    return res;
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

HTTP_SERVER_LIB const char*
get_http_code_str(const int code) {
    if (code < 0 || code > sizeof(HTTP_CODE_STR) / 8)
        return "UNKNOWN";
    const char *res = HTTP_CODE_STR[code];
    return (res != NULL) ? res : "UNKNOWN";
}

/*
 ********************************************
 *              CONTENT TYPES               *
 ********************************************
 */

HTTP_SERVER_STATIC const char *
_get_content_main_type_str(hs_content_main_type_e t) {
    static const char *MAIN_TYPE_STR[] = {
        [HS_CONTENT_TYPE_TEXT] = "text",
        [HS_CONTENT_TYPE_IMAGE] = "image",
        [HS_CONTENT_TYPE_APPLICATION] = "application",
    };
    const char *res = MAIN_TYPE_STR[t];
    if (res == NULL) {
        LOG_ERROR("Unknown content main type '%d'. Return just 'text'.\n", t);
        return MAIN_TYPE_STR[HS_CONTENT_TYPE_TEXT];
    }
    return res;
}

HTTP_SERVER_STATIC const char *
_get_content_subtype_text_str(hs_content_subtype_text_e t) {
    static const char *SUBTYPE_TEXT[] = {
        [HS_SUBTYPE_TEXT_PLAIN] = "plain",
        [HS_SUBTYPE_TEXT_HTML] = "html",
        [HS_SUBTYPE_TEXT_CSS] = "css",
        [HS_SUBTYPE_TEXT_CSV] = "csv",
        [HS_SUBTYPE_TEXT_XML] = "xml",
    };
    const char *res = SUBTYPE_TEXT[t];
    if (res == NULL) {
        LOG_ERROR("Unknown content text subtype '%d'. Return just 'text'.\n", t);
        return SUBTYPE_TEXT[HS_SUBTYPE_TEXT_PLAIN];
    }
    return res;
}

HTTP_SERVER_STATIC const char *
_get_content_subtype_image_str(hs_content_subtype_image_e t) {
    static const char *SUBTYPE_IMAGE[] = {
        [HS_SUBTYPE_IMAGE_JPEG] = "jpeg",
        [HS_SUBTYPE_IMAGE_PNG] = "png",
        [HS_SUBTYPE_IMAGE_GIF] = "gif",
        [HS_SUBTYPE_IMAGE_WEBP] = "webp",
        [HS_SUBTYPE_IMAGE_SVG] = "svg+xml",
        [HS_SUBTYPE_IMAGE_AVIF] = "avif",
    };
    const char *res = SUBTYPE_IMAGE[t];
    if (res == NULL) {
        LOG_ERROR("Unknown content image subtype '%d'. Return just 'text'.\n", t);
        return SUBTYPE_IMAGE[HS_SUBTYPE_IMAGE_JPEG];
    }
    return res;
}

HTTP_SERVER_STATIC const char *
_get_content_subtype_application_str(hs_content_subtype_application_e t) {
    static const char *SUBTYPE_APPLICATION[] = {
        [HS_SUBTYPE_APPLICATION_JSON] = "json",
        [HS_SUBTYPE_APPLICATION_XML] = "xml",
        [HS_SUBTYPE_APPLICATION_PDF] = "pdf",
        [HS_SUBTYPE_APPLICATION_ZIP] = "zip",
        [HS_SUBTYPE_APPLICATION_JAVASCRIPT] = "javascript",
        [HS_SUBTYPE_APPLICATION_WASM] = "wasm",
    };
    const char *res = SUBTYPE_APPLICATION[t];
    if (res == NULL) {
        LOG_ERROR("Unknown content application subtype '%d'. Return just 'text'.\n", t);
        return SUBTYPE_APPLICATION[HS_SUBTYPE_APPLICATION_JSON];
    }
    return res;
}

HTTP_SERVER_STATIC void 
_get_content_type_str(hs_content_type_t t, const char **type, const char **subtype) {
    *type = _get_content_main_type_str(t.type);

    switch (t.type) {
        case HS_CONTENT_TYPE_TEXT:
            *subtype = _get_content_subtype_text_str(t.subtype.text);
            break;
        case HS_CONTENT_TYPE_IMAGE:
            *subtype = _get_content_subtype_image_str(t.subtype.image);
            break;
        case HS_CONTENT_TYPE_APPLICATION:
            *subtype = _get_content_subtype_application_str(t.subtype.application);
            break;
    }
}

/*
 ********************************************
 *                  ROUTE                   *
 ********************************************
 */

HTTP_SERVER_STATIC server_route_t* 
_find_route(const server_t *server, const request_data_t *request) {
    server_route_t *found_route = NULL;
    va_list args_cpy;

    for (int i = 0; i < server->n_routes; i++) {
        server_route_t *route = &server->routes[i];
        if (route->n_args > 0) {
            va_copy(args_cpy, route->args);
            if (vsscanf(request->route, route->route_tmp, args_cpy) == route->n_args) {
                found_route = route;
                break;
            }
        } else if (strcmp(route->route_tmp, request->route) == 0) {
            found_route = route;
            break;
        }
    }

    return found_route;
}

HTTP_SERVER_STATIC http_server_err_t 
_process_route(server_route_t *route, const request_data_t *request, char **resp_dest, 
        int *resp_len) {
    static const char resp_fmt[] = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s/%s\r\n"
        "Connection: close\r\n"
        "Content-Length: %d\r\n"
        "\r\n";

    http_server_err_e err_e = HTTP_SERVER_OK;
    const char *type_str, *subtype_str;
    char *resp = NULL;
    int re_len;
    va_list args_cpy;

    va_copy(args_cpy, route->args);

    _get_content_type_str(route->content_type, &type_str, &subtype_str);

    if (route->cb(request, args_cpy, &resp, &re_len) != 0) {
        err_e = HTTP_SERVER_ROUTE_ERR;
        goto cleanup;
    }

    if ((*resp_dest = malloc(re_len + sizeof(resp_fmt) + 256)) == NULL) {
        err_e = HTTP_SERVER_MALLOC_ERR;
        goto cleanup;
    }
    if ((*resp_len = sprintf(*resp_dest, resp_fmt, type_str, subtype_str, re_len)) <= 0) {
        err_e = HTTP_SERVER_STDIO_ERR;
        goto cleanup;
    }
    if (strcat(*resp_dest, resp) == NULL) {
        err_e = HTTP_SERVER_STRCAT_ERR;
        goto cleanup;
    }

    *resp_len += re_len;

cleanup:
    if (resp != NULL) free(resp);
    return HS_CREATE_ERR(err_e);
}

HTTP_SERVER_LIB http_server_err_t 
form_response(const server_t *server, const request_data_t *request, char **resp_dest, 
        int *resp_len) {
    http_server_err_t err;
    server_route_t *found_route = _find_route(server, request);

    *resp_dest = NULL;
    *resp_len = 0;

    if (found_route == NULL) {
        if (HS_ERROR_CHECK(err, create_error_response(resp_dest, resp_len, 404)))
            return err;
    } else {
        if (HS_ERROR_CHECK(err, _process_route(found_route, request, resp_dest, resp_len))) {
            *resp_dest = NULL;
            *resp_len = 0;
            return err;
        }
    }
    return HS_CREATE_ERR(HTTP_SERVER_OK);
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

HTTP_SERVER_LIB http_server_err_t 
create_error_response(char **dest, int *dest_size, const int code) {
    http_server_err_e err_e;

    *dest = NULL;
    *dest_size = 0;

    const char *code_str = get_http_code_str(code);
    const int code_len = strlen(code_str);
    const int header_len = sizeof(ERROR_FMT) + code_len;
    const int content_len = sizeof(ERROR_PAGE_BODY) + code_len * 2;

    if ((*dest = malloc(header_len + content_len + 10)) == NULL) {
        err_e = HTTP_SERVER_MALLOC_ERR;
        goto failed;
    }

    const int fin_content_len = snprintf(NULL, 0, ERROR_PAGE_BODY, code, code_str, code, code_str);
    if (fin_content_len <= 0) {
        err_e = HTTP_SERVER_STDIO_ERR;
        goto failed;
    }

    const int fin_header_len = snprintf(*dest, header_len, ERROR_FMT, code, code_str, 
            fin_content_len);
    if (fin_content_len <= 0) {
        err_e = HTTP_SERVER_STDIO_ERR;
        goto failed;
    }

    if (snprintf(*dest + fin_header_len, content_len, ERROR_PAGE_BODY, code, code_str, code, 
                code_str) <= 0) {
        err_e = HTTP_SERVER_STDIO_ERR;
        goto failed;
    }

    *dest_size = fin_header_len + fin_content_len;

    return HS_CREATE_ERR(HTTP_SERVER_OK);

failed:
    if (*dest != NULL) {
        free(*dest);
        *dest = NULL;
    }
    *dest_size = 0;
    return HS_CREATE_ERR(err_e);
}

#endif /* HTTP_SERVER_IMPLEMENTATION */
