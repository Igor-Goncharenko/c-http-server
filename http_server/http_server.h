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
