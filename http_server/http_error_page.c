#include "http_server.h"
#include "http_server_internal.h"

#include <stdio.h>
#include <stdlib.h>

static const char 
ERROR_FMT[] = 
"HTTP1/1 %3d %20s\n\r"
"Content-Length: 33\r\n"
"Content-Type: text/html\r\n"
"Connection: Closed\r\n"
"\r\n"
"<h1>%3d %20s</h1>";

static const int 
ERR_RESP_MAX_SIZE = sizeof(ERROR_FMT) + 3 + 20 + 3 + 20;

HTTP_SERVER_LIB http_server_err_t 
create_error_response(char **dest, int *dest_size, const int code) {
    if ((*dest = malloc(ERR_RESP_MAX_SIZE)) == NULL) {
        *dest = NULL;
        *dest_size = 0;
        return HS_CREATE_ERR(HTTP_SERVER_MALLOC_ERR);
    }

    const char *code_str = get_http_code_str(code);
    *dest_size = snprintf(*dest, ERR_RESP_MAX_SIZE, ERROR_FMT, code, code_str, code, code_str);

    if (*dest_size <= 0) {
        free(*dest);
        *dest = NULL;
        *dest_size = 0;
        return HS_CREATE_ERR(HTTP_SERVER_STDIO_ERR);
    }

    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

