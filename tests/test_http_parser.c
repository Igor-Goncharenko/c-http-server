#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "tests.c"

#include "buffer.c"
#include "http_parser.c"

int header_t_cmp(header_t v1, header_t v2) {
    if (v1.key_len != v2.key_len)       return 1;
    if (v1.value_len != v2.value_len)   return 1;
    if (strcmp(v1.key, v2.key))         return 1;
    if (strcmp(v2.value, v2.value))     return 1;

    return 0;
}

int request_data_cmp(request_data_t v1, request_data_t v2) {
    if (v1.version != v2.version) return 1;
    if (v1.code != v2.code) return 1;
    if (strcmp(v1.route, v2.route)) return 1;
    if (v1.n_headers != v2.n_headers) return 1;
    for (int i = 0; i < v1.n_headers; i++)
        if (header_t_cmp(v1.headers[i], v2.headers[i]))
            return 1;

    return 0;
}

int main(int argc, char *argv[]) {
    tests_counter_t tc = { 0 };
    http_server_err_e err;
    buffer_t buf;

    buffer_init(&buf);
    buf.allow_realloc = true;

    /* test http version parser */
    TEST(
            tc, 
            "http_version_1_1", 
            http_version_e, 
            HTTP_VERSION_1_1, 
            int_cmp,
            _parse_http_version, 
            "HTTP/1.1"
        );

    TEST(
            tc, 
            "http_version_unknown", 
            http_version_e, 
            HTTP_VERSION_UNKNOWN,
            int_cmp,
            _parse_http_version, 
            "HTTP/0.1"
        );

    TEST(
            tc, 
            "http_version_null", 
            http_version_e, 
            HTTP_VERSION_UNKNOWN,
            int_cmp,
            _parse_http_version, 
            NULL
        );

    /* test http code parser */
    TEST(
            tc,
            "http_code_100",
            const char*,
            "CONTINUE",
            strcmp,
            _parse_http_code,
            100
        );
    
    TEST(
            tc,
            "http_code_505",
            const char*,
            "HTTP VERSION NOT SUPPORTED",
            strcmp,
            _parse_http_code,
            505
        );

    TEST(
            tc,
            "http_code_out_of_range",
            const char*,
            "UNKNOWN",
            strcmp,
            _parse_http_code,
            10000
        );

    TEST(
            tc,
            "http_code_negative",
            const char*,
            "UNKNOWN",
            strcmp,
            _parse_http_code,
            -99
        );
     
    /* test http method parser */
    TEST(
            tc,
            "http_method_get",
            http_method_e,
            HTTP_METHOD_GET,
            int_cmp,
            _parse_http_method,
            "GET"
        );

    TEST(
            tc,
            "http_method_options",
            http_method_e,
            HTTP_METHOD_OPTIONS,
            int_cmp,
            _parse_http_method,
            "OPTIONS"
        );

    TEST(
            tc,
            "http_method_unknown",
            http_method_e,
            HTTP_METHOD_UNKNOWN,
            int_cmp,
            _parse_http_method,
            "54\n\r"
        );

    TEST(
            tc,
            "http_method_null",
            http_method_e,
            HTTP_METHOD_UNKNOWN,
            int_cmp,
            _parse_http_method,
            NULL
        );

    TEST(
            tc,
            "http_method_empty_string",
            http_method_e,
            HTTP_METHOD_UNKNOWN,
            int_cmp,
            _parse_http_method,
            ""
        );

    /* test request line parser */
    {
        request_data_t dest = { 0 };
        char *line = create_test_string("GET /home HTTP/1.1");
        TEST(
                tc,
                "request_line_parser_1",
                http_server_err_e,
                HTTP_SERVER_OK,
                int_cmp,
                _parse_request_line,
                &dest,
                &buf,
                line
            );

        request_data_t test_req_data = {
            .method = HTTP_METHOD_GET,
            .route = "/home",
            .version = HTTP_VERSION_1_1,
        };

        TEST_EQ(
                tc,
                "request_line_parser_1_eq",
                request_data_t,
                test_req_data,
                dest,
                request_data_cmp
               );

        free(line);
    }
    {
        request_data_t dest = { 0 };
        char *line = create_test_string("OPTIONS /some-options?name=igor&age=10 HTTP/1.1");

        TEST(
                tc,
                "request_line_parser_2",
                http_server_err_e,
                HTTP_SERVER_OK,
                int_cmp,
                _parse_request_line,
                &dest,
                &buf,
                line
            );

        request_data_t test_req_data = {
            .method = HTTP_METHOD_OPTIONS,
            .route = "/some-options?name=igor&age=10",
            .version = HTTP_VERSION_1_1,
        };

        TEST_EQ(
                tc,
                "request_line_parser_2_eq",
                request_data_t,
                test_req_data,
                dest,
                request_data_cmp
               );

        free(line);
    }

    /* test header line parser */
    {
        header_t dest = { 0 };
        char *line = create_test_string("User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:138.0) "
                "Gecko/20100101 Firefox/138.0");
        TEST(
                tc,
                "header_line_parser_1",
                http_server_err_e,
                HTTP_SERVER_OK,
                int_cmp,
                _parse_header,
                &dest,
                &buf,
                line
            );
        free(line);

        header_t test_header = {
            .key_len = 10,
            .key = "User-Agent",
            .value_len = 70,
            .value = "Mozilla/5.0 (X11; Linux x86_64; rv:138.0) Gecko/20100101 Firefox/138.0",
        };

        TEST_EQ(
                tc,
                "header_line_parser_1_eq",
                header_t,
                test_header,
                dest,
                header_t_cmp
               );

        header_t dest2 = { 0 };
        line = create_test_string(
                "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*\\*;q=0.8");
        TEST(
                tc,
                "header_line_parser_2",
                http_server_err_e,
                HTTP_SERVER_OK,
                int_cmp,
                _parse_header,
                &dest2,
                &buf,
                line
            );
        free(line);

        header_t test_header2 = {
            .key_len = 6,
            .key = "Accept",
            .value_len = 63,
            .value = "text/html,application/xhtml+xml,application/xml;q=0.9,*\\*;q=0.8",
        };

        TEST_EQ(
                tc,
                "header_line_parser_2_eq",
                header_t,
                test_header2,
                dest2,
                header_t_cmp
               );
    }

    {
        request_data_t dest = { 0 };
        buffer_t buf2;
        const char headers_raw[] = 
            "GET https://stackoverflow.com/questions/46991861/what-is-an-easy-way-to-implement-"
            "fprintf-in-python HTTP/1.1\r\n"
            "Host: 192.168.0.167:8080\r\n"
            "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:138.0) Gecko/20100101 Firefox/138.0\r\n"
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,;q=0.8\r\n"
            "Accept-Language: en-US,en;q=0.5\r\n"
            "Accept-Encoding: gzip, deflate\r\n"
            "Connection: keep-alive\r\n"
            "Upgrade-Insecure-Requests: 1\r\n"
            "Priority: u=0, i\r\n"
            "\r\n";
        
        buffer_init(&buf2);
        buf2.allow_realloc = true;
        buffer_append_sentence(&buf2, headers_raw, sizeof(headers_raw) - 1, NULL);

        TEST(
                tc,
                "parse_http_request_1",
                http_server_err_e,
                HTTP_SERVER_OK,
                int_cmp,
                parse_http_request,
                &dest, 
                &buf2
            );

        header_t header_list[8] = {
            {
                .key = "Host",
                .key_len = 4,
                .value = "192.168.0.167:8080",
                .value_len = 18,
            },
            {
                .key = "User-Agent",
                .key_len = 10,
                .value = "Mozilla/5.0  Gecko/20100101 Firefox/138.0",
                .value_len = 70,
            },
            {
                .key = "Accept",
                .key_len = 6,
                .value = "text/html,application/xhtml+xml,application/xml;q=0.9,;q=0.8",
                .value_len = 60,
            },
            {
                .key = "Accept-Language",
                .key_len = 15,
                .value = "en-US,en;q=0.5",
                .value_len = 14,
            },
            {
                .key = "Accept-Encoding",
                .key_len = 15,
                .value = "gzip, deflate",
                .value_len = 13,
            },
            {
                .key = "Connection",
                .key_len = 10,
                .value = "keep-alive",
                .value_len = 10,
            },
            {
                .key = "Upgrade-Insecure-Requests",
                .key_len = 25,
                .value = "1",
                .value_len = 1,
            },
            {
                .key = "Priority",
                .key_len = 8,
                .value = "u=0, i",
                .value_len = 6,
            },
        };

        request_data_t test_req_data = {
            .method = HTTP_METHOD_GET,
            .route = "https://stackoverflow.com/questions/46991861/what-is-an-easy-way-to-"
                "implement-fprintf-in-python",
            .version = HTTP_VERSION_1_1,
            .n_headers = 8,
            .headers = header_list,
        };

        TEST_EQ(
                tc,
                "parse_http_request_1_eq",
                request_data_t,
                dest,
                test_req_data,
                request_data_cmp
               );

        buffer_free(&buf2);
    }

    {
        request_data_t dest = { 0 };
        buffer_t buf2;
        const char headers_raw[] = 
            "POST /home HTTP/1.1\r\n"
            "\r\n";
        
        buffer_init(&buf2);
        buf2.allow_realloc = true;
        buffer_append_sentence(&buf2, headers_raw, sizeof(headers_raw) - 1, NULL);

        TEST(
                tc,
                "parse_http_request_only_request_line",
                http_server_err_e,
                HTTP_SERVER_OK,
                int_cmp,
                parse_http_request,
                &dest, 
                &buf2
            );

        request_data_t test_req_data = {
            .method = HTTP_METHOD_POST,
            .route = "/home",
            .version = HTTP_VERSION_1_1,
            .n_headers = 0,
        };

        TEST_EQ(
                tc,
                "parse_http_request_only_request_line_eq",
                request_data_t,
                dest,
                test_req_data,
                request_data_cmp
               );

        buffer_free(&buf2);
    }

    buffer_free(&buf);

    tc_print_result(__FILE__, &tc);
    printf("\n");

    return EXIT_SUCCESS;
}
