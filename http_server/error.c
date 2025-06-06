#include "http_server_internal.h"

#include <assert.h>
#include <stdlib.h>

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



