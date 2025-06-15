#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http_server.h"
#include "http_server_internal.h"

static const char *HTTP_CODE_STR[] = {
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

HS_LIB const char *get_http_code_str(const int code) {
    if (code < 0 || code > sizeof(HTTP_CODE_STR) / 8) return "UNKNOWN";
    const char *res = HTTP_CODE_STR[code];
    return (res != NULL) ? res : "UNKNOWN";
}

/*
 ********************************************
 *                  ROUTE                   *
 ********************************************
 */

HS_STATIC hs_server_route_t *_find_route(const hs_server_t *server,
                                         const hs_request_data_t *request) {
    for (int i = 0; i < server->n_routes; i++) {
        hs_server_route_t *route = &server->routes[i];
        if (request->method == route->method &&
            regexec(&route->_re, request->route, 0, NULL, 0) == 0) {
            return route;
        }
    }
    return NULL;
}

HS_STATIC hs_err_t _hs_parse_route_matches(const hs_request_data_t *req,
                                           const hs_server_route_t *route, char ***matches) {
    hs_err_t err;
    hs_buffer_t buf;
    regmatch_t *rematches;

    rematches = malloc(sizeof(regmatch_t) * (1 + route->_n_matches));

    const int arr_size = sizeof(char *) * route->_n_matches;
    const int route_len = strlen(req->route);

    if (HS_ERROR_CHECK(err, hs_buffer_init_with_size(&buf, arr_size + route_len))) goto failed;
    buf.len += arr_size;

    if (regexec(&route->_re, req->route, route->_n_matches + 1, rematches, 0) == 0) {
        for (int i = 0; i < route->_n_matches; i++) {
            err = hs_buffer_append_sentence(&buf, req->route + rematches[i + 1].rm_so,
                                            rematches[i + 1].rm_eo - rematches[i + 1].rm_so,
                                            buf.mem + i * sizeof(char *));
            if (err.code != HS_OK) goto failed;
        }
    }

    *matches = buf.mem;
    free(rematches);
    return HS_CREATE_ERR(HS_OK);
failed:
    free(rematches);
    hs_buffer_free(&buf);
    return err;
}

HS_STATIC hs_err_t _process_route(hs_server_route_t *route, const hs_request_data_t *request,
                                  char **resp_dest, int *resp_len) {
    static const char resp_fmt[] =
        "HTTP/1.1 %3d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "%s"
        "\r\n";

    hs_err_t err = HS_CREATE_ERR(HS_OK);
    const char *type_str, *subtype_str;
    hs_response_t resp = {0};
    char **matches = NULL;

    if (route->_n_matches > 0 &&
        HS_ERROR_CHECK(err, _hs_parse_route_matches(request, route, &matches))) {
        // TODO: process err
        goto cleanup;
    }

    if (route->cb(&resp, request, matches, route->_n_matches) != 0) {
        err = HS_CREATE_ERR(HS_ROUTE_ERR);
        goto cleanup;
    }

    const char *code_str = get_http_code_str(resp.code);
    const int code_str_len = strlen(code_str);

    *resp_dest = malloc(resp.content_len + code_str_len + sizeof(resp_fmt) + 256);
    if (*resp_dest == NULL) {
        err = HS_CREATE_ERR(HS_MALLOC_ERR);
        goto cleanup;
    }

    *resp_len = sprintf(*resp_dest, resp_fmt, resp.code, code_str, resp.content_type,
                        resp.content_len, (resp.headers.len <= 0) ? "" : resp.headers.data);
    if (*resp_len <= 0) {
        err = HS_CREATE_ERR(HS_STDIO_ERR);
        goto cleanup;
    }

    if (resp.content_len > 0) {
        if (memcpy((*resp_dest) + (*resp_len), resp.content, resp.content_len) == NULL) {
            err = HS_CREATE_ERR(HS_MEMCPY_ERR);
            goto cleanup;
        }
        *resp_len += resp.content_len;
    }

cleanup:
    hs_response_free(&resp);
    free(matches);
    return err;
}

HS_LIB hs_err_t hs_form_response(const hs_server_t *server, const hs_request_data_t *request,
                                 char **resp_dest, int *resp_len) {
    hs_err_t err;
    hs_server_route_t *found_route = _find_route(server, request);

    *resp_dest = NULL;
    *resp_len = 0;

    if (found_route == NULL) {
        if (HS_ERROR_CHECK(err, hs_create_error_response(resp_dest, resp_len, 404))) return err;
    } else {
        if (HS_ERROR_CHECK(err, _process_route(found_route, request, resp_dest, resp_len))) {
            *resp_dest = NULL;
            *resp_len = 0;
            return err;
        }
    }
    return HS_CREATE_ERR(HS_OK);
}

/*
 ********************************************
 *              ERROR PAGE                  *
 ********************************************
 */

static const char ERROR_FMT[] =
    "HTTP/1.1 %3d %s\r\n"
    "Content-Length: %d\r\n"
    "Content-Type: text/html\r\n"
    "Connection: Closed\r\n"
    "\r\n";

static const char ERROR_PAGE_BODY[] =
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

HS_LIB hs_err_t hs_create_error_response(char **dest, int *dest_size, const int code) {
    hs_err_e err_e;

    *dest = NULL;
    *dest_size = 0;

    const char *code_str = get_http_code_str(code);
    const int code_len = strlen(code_str);
    const int header_len = sizeof(ERROR_FMT) + code_len;
    const int content_len = sizeof(ERROR_PAGE_BODY) + code_len * 2;

    if ((*dest = malloc(header_len + content_len + 10)) == NULL) {
        err_e = HS_MALLOC_ERR;
        goto failed;
    }

    const int fin_content_len = snprintf(NULL, 0, ERROR_PAGE_BODY, code, code_str, code, code_str);
    if (fin_content_len <= 0) {
        err_e = HS_STDIO_ERR;
        goto failed;
    }

    const int fin_header_len =
        snprintf(*dest, header_len, ERROR_FMT, code, code_str, fin_content_len);
    if (fin_content_len <= 0) {
        err_e = HS_STDIO_ERR;
        goto failed;
    }

    if (snprintf(*dest + fin_header_len, content_len, ERROR_PAGE_BODY, code, code_str, code,
                 code_str) <= 0) {
        err_e = HS_STDIO_ERR;
        goto failed;
    }

    *dest_size = fin_header_len + fin_content_len;

    return HS_CREATE_ERR(HS_OK);

failed:
    if (*dest != NULL) {
        free(*dest);
        *dest = NULL;
    }
    *dest_size = 0;
    return HS_CREATE_ERR(err_e);
}
