#include <assert.h>

#include "http_server_internal.h"

HS_LIB const char *
hs_strerror(hs_err_e err) {
    static const char *hs_error_str[] = {
        [HS_OK] = "Ok",
        [HS_ROUTE_ERR] = "Route_err",
        [HS_PTHREAD_SIGMASK_ERR] = "Sigmask_err",
        [HS_SIGNALFD_ERR] = "Signalfd_err",
        /* General */
        [HS_MALLOC_ERR] = "Malloc_err",
        [HS_STDIO_ERR] = "Stdio_err",
        /* string errors */
        [HS_STRTOK_ERR] = "Strtok_err",
        [HS_STRCPY_ERR] = "Strcpy_err",
        [HS_STRCAT_ERR] = "Strcat_err",
        [HS_MEMCPY_ERR] = "Memcpy_err",
        [HS_MEMSET_ERR] = "Memset_err",
        /* Server init errors */
        [HS_SOCKET_CREATE_ERR] = "Socket_create_err",
        [HS_ACCEPT_ERR] = "Accept_err",
        [HS_BIND_ERR] = "Bind_err",
        [HS_LISTEN_ERR] = "Listen_err",
        [HS_EPOLL_CREATE_ERR] = "Epoll_create_err",
        [HS_EPOLL_CTL_ERR] = "Epoll_ctl_err",
        [HS_WRITE_ERR] = "Write_err",
        /* Socket errors */
        [HS_SOCKET_READ_ERR] = "Socket_read_err",
        /* buffer errors */
        [HS_BUFFER_OVERFLOW_ERR] = "Buffer_overflow_err",
    };

    return hs_error_str[err];
}
