#ifndef HTTP_SERVER_INTERNAL_H_
#define HTTP_SERVER_INTERNAL_H_

#include "http_server.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>

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


/*
 ********************************************
 *              ERROR PAGE                  *
 ********************************************
 */

HTTP_SERVER_LIB http_server_err_t 
create_error_response(char **dest, int *dest_size, const int code);

#endif /* HTTP_SERVER_INTERNAL_H_ */
