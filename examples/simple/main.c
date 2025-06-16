#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../http_server.h"

#define N_ROUTES 4

int home_callback(hs_response_t *dest, const hs_request_data_t *request, char **data,
                  const int n_data);
int css_callback(hs_response_t *dest, const hs_request_data_t *request, char **data,
                 const int n_data);
int favicon_callback(hs_response_t *dest, const hs_request_data_t *request, char **data,
                     const int n_data);
int user_callback(hs_response_t *dest, const hs_request_data_t *request, char **data,
                  const int n_data);

int main(int argc, char *argv[]) {
    hs_server_t server;
    const hs_server_route_t routes[N_ROUTES] = {
        {
            .method = HS_METHOD_GET,
            .route_tmp = "/",
            .cb = home_callback,
        },
        {
            .method = HS_METHOD_GET,
            .route_tmp = "/{str}.css",
            .cb = css_callback,
        },
        {
            .method = HS_METHOD_GET,
            .route_tmp = "/favicon.ico",
            .cb = favicon_callback,
        },
        {
            .method = HS_METHOD_GET,
            .route_tmp = "/users/{int}",
            .cb = user_callback,
        },
    };

    if (hs_init_server(&server, 8080, 10, routes, N_ROUTES) != 0) {
        fprintf(stderr, "Failed to init server\n");
        return EXIT_FAILURE;
    }

    if (hs_start_server(&server) != 0) {
        fprintf(stderr, "Failed to start server\n");
        hs_server_destroy(&server);
        return EXIT_FAILURE;
    }

    hs_server_destroy(&server);

    return EXIT_SUCCESS;
}

/* callbacks */

int home_callback(hs_response_t *dest, const hs_request_data_t *request, char **data,
                  const int n_data) {
    if ((dest->content_len = hs_load_file("home.html", &dest->content)) <= 0) return -1;

    dest->code = 200;
    strcpy(dest->content_type, "text/html");

    hs_response_add_header(dest, "Test-header", "Test header value");

    return 0;
}

int css_callback(hs_response_t *dest, const hs_request_data_t *request, char **data,
                 const int n_data) {
    int len = strlen(data[0]);
    char *filename = malloc(len + 5);

    strcpy(filename, data[0]);
    strcat(filename, ".css");

    if ((dest->content_len = hs_load_file(filename, &dest->content)) <= 0) return -1;
    dest->code = 200;
    strcpy(dest->content_type, "text/css");

    free(filename);

    return 0;
}

int favicon_callback(hs_response_t *dest, const hs_request_data_t *request, char **data,
                     const int n_data) {
    if ((dest->content_len = hs_load_file("favicon.ico", &dest->content)) <= 0) return -1;

    dest->code = 200;
    strcpy(dest->content_type, "image/x-icon");

    return 0;
}

int user_callback(hs_response_t *dest, const hs_request_data_t *request, char **data,
                  const int n_data) {
    static const char fmt[] = "<h1>User #%s</h1>";
    const int data_len = strlen(data[0]);
    dest->content = malloc(strlen(fmt) + data_len);
    dest->content_len = sprintf(dest->content, fmt, data[0]);
    dest->code = 200;
    strcpy(dest->content_type, "text/html");
    return 0;
}
