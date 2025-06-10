#include <string.h>
#include <time.h>

#include "http_server.h"
#include "http_server_internal.h"

static const char
*HTTP_VERSION_STR[HTTP_VERSION_LAST] = {
    [HTTP_VERSION_1_1] = "HTTP/1.1",
};

static const char
*HTTP_METHODS_STR[] = {
    [HTTP_METHOD_GET]       = "GET",
    [HTTP_METHOD_HEAD]      = "HEAD",
    [HTTP_METHOD_POST]      = "POST",
    [HTTP_METHOD_PUT]       = "PUT",
    [HTTP_METHOD_DELETE]    = "DELETE",
    [HTTP_METHOD_CONNECT]   = "CONNECT",
    [HTTP_METHOD_OPTIONS]   = "OPTIONS",
    [HTTP_METHOD_TRACE]     = "TRACE",
};

HTTP_SERVER_STATIC http_version_e
_parse_http_version(const char *version_str) {
    if (version_str == NULL)
        return HTTP_VERSION_UNKNOWN;

    for (int i = 0; i < HTTP_VERSION_LAST; i++)
        if (strcmp(version_str, HTTP_VERSION_STR[i]) == 0)
            return (http_version_e)i;

    return HTTP_VERSION_UNKNOWN;
}

HTTP_SERVER_STATIC http_method_e
_parse_http_method(const char *method_str) {
    if (method_str == NULL)
        return HTTP_METHOD_UNKNOWN;

    for (int i = 0; i < HTTP_METHOD_LAST; i++)
        if (strcmp(method_str, HTTP_METHODS_STR[i]) == 0)
            return (http_method_e)i;

    return HTTP_METHOD_UNKNOWN;
}

HTTP_SERVER_STATIC http_server_err_t
_parse_request_line(request_data_t *dest, buffer_t *buf, char *line) {
    char *save_ptr, *token;
    http_server_err_t err;

    if ((token = strtok_r(line, " ", &save_ptr)) == NULL)
        return HS_CREATE_ERR(HTTP_SERVER_STRTOK_ERR);
    dest->method = _parse_http_method(token);

    if ((token = strtok_r(NULL, " ", &save_ptr)) == NULL)
        return HS_CREATE_ERR(HTTP_SERVER_STRTOK_ERR);
    int token_len = strlen(token);
    if (HS_ERROR_CHECK(err, buffer_append_sentence(buf, token, token_len, &dest->route)))
        return err;

    if ((token = strtok_r(NULL, " ", &save_ptr)) == NULL)
        return HS_CREATE_ERR(HTTP_SERVER_STRTOK_ERR);
    dest->version = _parse_http_version(token);

    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

HTTP_SERVER_STATIC int
_count_raw_lines(buffer_t *headers_raw) {
    int counter = 1;
    for (int i = 0; i < headers_raw->len; i++) 
        if (headers_raw->data[i] == '\n')
            counter++;
    return counter;
}

HTTP_SERVER_STATIC http_server_err_t
_parse_header(header_t *dest, buffer_t *buf, char *line) {
    http_server_err_t err;

    char *delim_ptr = strchr(line, ':');
    *delim_ptr = '\0';

    /* printf("%s (%d)\n", line, strlen(line));fflush(stdout); */
    dest->key_len = strlen(line);
    if (HS_ERROR_CHECK(err, buffer_append_sentence(buf, line, dest->key_len, &dest->key)))
        return err;

    delim_ptr += 2; // ignore ": " ("\0 ")
    dest->value_len = strlen(delim_ptr);
    if (HS_ERROR_CHECK(err, buffer_append_sentence(buf, delim_ptr, dest->value_len, &dest->value)))
        return err;

    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

HTTP_SERVER_LIB http_server_err_t
parse_http_request(request_data_t *dest, buffer_t *headers_raw) {
    http_server_err_t err = HS_CREATE_ERR(HTTP_SERVER_OK);
    buffer_t req_buf;
    char *save_ptr, *token;

    {   /* exclude request line, empty line and line after empty line */
        dest->n_headers = _count_raw_lines(headers_raw) - 3;
        size_t headers_size = dest->n_headers * sizeof(header_t);
        size_t buf_size = headers_raw->len + headers_size;
        void *ptr;
        if (    HS_ERROR_CHECK(err, buffer_init_with_size(&req_buf, buf_size)) ||
                HS_ERROR_CHECK(err, buffer_append_mem(&req_buf, 1, headers_size, NULL, &ptr)))
            goto cleanup;
        dest->headers = (header_t*)ptr;
    }

    dest->time = time(NULL);

    if ((token = strtok_r(headers_raw->data, "\r", &save_ptr)) == NULL) {
        err = HS_CREATE_ERR(HTTP_SERVER_STRTOK_ERR);
        goto cleanup;
    }
    if (HS_ERROR_CHECK(err, _parse_request_line(dest, &req_buf, token)))
        goto cleanup;

    /* We are splitting by '\r', therefore, at the beginning of the line there is '\n' */
    token = strtok_r(NULL, "\r", &save_ptr) + 1;    /* Ignore '\n' */
    for (int i = 0; i < dest->n_headers && token != NULL && strlen(token) > 4; i++) {
        /* header_t new_header; */
        if (HS_ERROR_CHECK(err, _parse_header(&dest->headers[i], &req_buf, token)))
            goto cleanup;

        token = strtok_r(NULL, "\r", &save_ptr) + 1;
    }

    dest->mem = req_buf.data;
    dest->mem_len = req_buf.len;

cleanup:
    if (err.code != HTTP_SERVER_OK) {
        memset(dest, 0, sizeof(request_data_t));
        buffer_free(&req_buf);
    }
    return err;
}

