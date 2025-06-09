#include "http_server.h"
#include "http_server_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
