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

typedef int (*hs_route_callback)(const hs_request_data_t*, const va_list, hs_response_t*);

typedef struct {
    hs_http_method_e       method;
    char                *route_tmp;
    hs_route_callback      cb;
    va_list             args;
    int                 n_args;
} hs_server_route_t;

typedef struct {
    void                *mem;

    int                 fd;
    struct sockaddr_in  addr;

    int                 epoll_fd;
    struct epoll_event  event;

    bool                running;

    hs_server_route_t      *routes;
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
