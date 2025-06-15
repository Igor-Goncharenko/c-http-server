#include "../../http_server.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#define n_routes 3

int home_callback(const hs_request_data_t *request, hs_response_t *dest) {
    if ((dest->content_len = hs_load_file("home.html", &dest->content)) <= 0) return -1;

    dest->code = 200;
    strcpy(dest->content_type, "text/html");

    hs_response_add_header(dest, "Test-header", "Test header value");

    return 0;
}

int css_callback(const hs_request_data_t *request, hs_response_t *dest) {
    if ((dest->content_len = hs_load_file("home.css", &dest->content)) <= 0) return -1;
    dest->code = 200;
    strcpy(dest->content_type, "text/css");

    return 0;
}

int favicon_callback(const hs_request_data_t *request, hs_response_t *dest) {
    if ((dest->content_len = hs_load_file("favicon.ico", &dest->content)) <= 0) return -1;

    dest->code = 200;
    strcpy(dest->content_type, "image/x-icon");

    return 0;
}

hs_server_t server;

void cleanup(void) {
    hs_server_destroy(&server);
}

void handle_sigint(int sig) {
    printf("Handled SIGINT\n");
    server.running = false;
}

void setup_signal_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "Failed to setup signal handler: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    atexit(cleanup);
    setup_signal_handler();

    const hs_server_route_t routes[n_routes] = {
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
    };

    if (hs_init_server(&server, 8080, 10, routes, n_routes) != 0) {
        fprintf(stderr, "Failed to init server\n");
        exit(EXIT_FAILURE);
    }

    if (hs_start_server(&server) != 0) {
        fprintf(stderr, "Failed to start server\n");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
