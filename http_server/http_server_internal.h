#ifndef HTTP_SERVER_INTERNAL_H_
#define HTTP_SERVER_INTERNAL_H_

#include <assert.h>
#include <errno.h>
#include <stdbool.h>

#include "http_server.h"

/*
 ********************************************
 *                  ERRORS                  *
 ********************************************
 */

typedef enum {
    HS_OK = 0,
    HS_ROUTE_ERR,
    HS_PTHREAD_SIGMASK_ERR,
    HS_SIGNALFD_ERR,
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
    int errno_;
    int line;
} hs_err_t;

HS_LIB const char *
hs_strerror(hs_err_e err);

#define HS_STATIC_ASSERT(expr) typedef char static_assert_##__LINE__[(expr) ? 1 : -1]

#define HS_ERROR_CHECK(_hs_err_var_t, _hs_function)                                     \
    ({                                                                                  \
        HS_STATIC_ASSERT(__builtin_types_compatible_p(typeof(_hs_function), hs_err_t)); \
        hs_err_t _hs_err_tmp = (_hs_function);                                          \
        (_hs_err_var_t) = _hs_err_tmp;                                                  \
        (_hs_err_var_t.code != HS_OK);                                                  \
    })

#define HS_CREATE_ERR(_err_e)       \
    ({                              \
        int _saved_errno = errno;   \
        (hs_err_t){                 \
            .code = (_err_e),       \
            .line = __LINE__,       \
            .errno_ = _saved_errno, \
        };                          \
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
        void *mem;
        char *data;
    };
    size_t len;
    size_t cap;

    bool allow_realloc;
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

HS_LIB const char *
hs_get_http_code_str(const int code);

HS_LIB hs_err_t
hs_form_response(const hs_server_t *server, const hs_request_data_t *request, char **resp_dest,
                 int *resp_len);

HS_LIB hs_err_t
hs_create_error_response(char **dest, int *dest_size, const int code);

/*
 ********************************************
 *              SERVER ROUTE                *
 ********************************************
 */

HS_LIB hs_err_t
hs_cpy_init_routes_to_server(hs_server_t *self, const hs_server_route_t *routes,
                             const int n_routes);

#endif /* HTTP_SERVER_INTERNAL_H_ */
