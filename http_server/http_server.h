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
 *              HTTP REQUEST                *
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


/*
 ********************************************
 *              HTTP RESPONSE               *
 ********************************************
 */

#define HS_CONTENT_TYPE_BUF_MAX 20

typedef struct {
    char                content_type[HS_CONTENT_TYPE_BUF_MAX];
    char                *content;
    int                 content_len;
} hs_response_t;

/*
 ********************************************
 *                  SERVER                  *
 ********************************************
 */

typedef int (*route_callback)(const request_data_t*, const va_list, hs_response_t*);

typedef struct {
    http_method_e       method;
    char                *route_tmp;
    route_callback      cb;
    va_list             args;
    int                 n_args;
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
