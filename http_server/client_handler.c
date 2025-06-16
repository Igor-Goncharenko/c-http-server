#include <stdlib.h>
#include <unistd.h>

#include "http_server.h"
#include "http_server_internal.h"

#define READ_HEADERS_CHARS_LIMIT 8192
#define READ_BUFFER_LEN 1024

HS_STATIC hs_err_t
_hs_read_headers_raw(const int client_fd, hs_buffer_t *dest) {
    hs_err_t err;
    int chars_count = 0;
    int new_line_count = 0;
    char ch;

    char buffer[READ_BUFFER_LEN];
    int buffer_len = 0;

    if (HS_ERROR_CHECK(err, hs_buffer_init(dest))) goto failed;
    dest->allow_realloc = true;

    while (new_line_count < 2 && chars_count < READ_HEADERS_CHARS_LIMIT) {
        chars_count++;

        int n = read(client_fd, &ch, 1);
        if (n <= 0) {
            err = HS_CREATE_ERR(HS_SOCKET_READ_ERR);
            goto failed;
        }

        buffer[buffer_len++] = ch;
        if (buffer_len == READ_BUFFER_LEN) {
            if (HS_ERROR_CHECK(err, hs_buffer_append_mem(dest, 1, 1024, buffer, NULL))) goto failed;
            buffer_len = 0;
        }

        if (ch == '\r')
            continue;
        else if (ch == '\n')
            new_line_count++;
        else
            new_line_count = 0;
    }

    if (HS_ERROR_CHECK(err, hs_buffer_append_mem(dest, 1, buffer_len, buffer, NULL))) goto failed;

    return HS_CREATE_ERR(HS_OK);

failed:
    hs_buffer_free(dest);
    return err;
}

HS_STATIC hs_err_t
_hs_read_body(const int client_fd, hs_buffer_t *dest, const int size) {
    hs_err_t err;
    char buffer[READ_BUFFER_LEN];
    int total_len = 0;

    if (HS_ERROR_CHECK(err, hs_buffer_init_with_size(dest, size))) goto failed;

    while (total_len < size) {
        int to_read = (size - total_len < READ_BUFFER_LEN) ? size - total_len : READ_BUFFER_LEN;

        int bytes_read = read(client_fd, buffer, to_read);
        if (bytes_read <= 0 || bytes_read != to_read) {
            err = HS_CREATE_ERR(HS_SOCKET_READ_ERR);
            goto failed;
        }

        if (HS_ERROR_CHECK(err, hs_buffer_append_mem(dest, 1, bytes_read, buffer, NULL)))
            goto failed;
        total_len += bytes_read;
    }
    return HS_CREATE_ERR(HS_OK);

failed:
    hs_buffer_free(dest);
    return err;
}

HS_STATIC int
_hs_get_content_len(const hs_request_data_t *request) {
    const char *value_str = hs_get_header(request, "Content-Length");
    return (value_str == NULL) ? 0 : atoi(value_str);
}

HS_STATIC hs_err_t
_hs_send_internal_error(const int fd) {
    hs_err_t err;
    char *resp = NULL;
    int resp_len;
    if (HS_ERROR_CHECK(err, hs_create_error_response(&resp, &resp_len, 500))) {
        return err;
    }

    write(fd, resp, resp_len);
    if (resp != NULL) {
        free(resp);
    }

    return HS_CREATE_ERR(HS_OK);
}

HS_STATIC hs_err_t
_hs_send_response(const int fd, const char *response, const int response_len) {
    if (response_len <= 0 || response == NULL) return HS_CREATE_ERR(HS_WRITE_ERR);
    int bytes_sent = 0;

    while (bytes_sent < response_len) {
        int n = write(fd, response + bytes_sent, response_len - bytes_sent);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return HS_CREATE_ERR(HS_WRITE_ERR);
        }
        bytes_sent += n;
    }
    return HS_CREATE_ERR(HS_OK);
}

HS_API hs_err_t
hs_handle_client(const hs_server_t *server, const int fd) {
    hs_err_t err = HS_CREATE_ERR(HS_OK);
    hs_request_data_t request = {0};
    hs_buffer_t headers_raw = {0}, content_buf = {0};

    char *response = NULL;
    int response_len = 0;

    LOG_DEBUG("New client handled: %d.", fd);

    if (HS_ERROR_CHECK(err, _hs_read_headers_raw(fd, &headers_raw))) goto cleanup;

    if (HS_ERROR_CHECK(err, hs_parse_http_request(&request, &headers_raw))) goto cleanup;

    int content_len = _hs_get_content_len(&request);

    LOG_DEBUG("Client %d: path=\"%s\".", fd, request.route);

    if (content_len > 0) {
        if (HS_ERROR_CHECK(err, _hs_read_body(fd, &content_buf, content_len))) goto cleanup;
        request.content_len = content_len;
        request.content = content_buf.data;
    }

    if (HS_ERROR_CHECK(err, hs_form_response(server, &request, &response, &response_len)))
        goto cleanup;

    if (HS_ERROR_CHECK(err, _hs_send_response(fd, response, response_len))) goto cleanup;

cleanup:
    hs_buffer_free(&headers_raw);
    hs_buffer_free(&content_buf);
    if (request.mem != NULL) free(request.mem);
    if (response != NULL) free(response);
    if (err.code != HS_OK) _hs_send_internal_error(fd);

    return err;
}
