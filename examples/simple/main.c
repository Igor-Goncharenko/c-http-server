#include "../../http_server.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#define n_routes 2

int home_callback(const request_data_t *request, const va_list args, hs_response_t *dest) {
    static const char home_resp[] = "<h1>Home page!</h1>";

    dest->code = 200;
    dest->content_len = sizeof(home_resp);
    dest->content = malloc(dest->content_len);
    strcpy(dest->content_type, "text/html");
    strcpy(dest->content, home_resp);

    hs_response_add_header(dest, "Test-header", "Test header value");

    return 0;
}

int favicon_callback(const request_data_t *request, const va_list args, hs_response_t *dest) {
    const char file[] = "favicon.ico";
    FILE *fd = fopen(file, "rb");
    fseek(fd, 0, SEEK_END);
    const int size = ftell(fd);
    rewind(fd);
    char *bytes = malloc(size);
    fread(bytes, 1, size, fd);
    fclose(fd);

    dest->code = 200;
    dest->content_len = size;
    dest->content = bytes;
    strcpy(dest->content_type, "image/x-icon");

    return 0;
}

server_t server;

void cleanup(void) {
    server_destroy(&server);
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

    const server_route_t routes[n_routes] = {
        {
            .method = HTTP_METHOD_GET,
            .route_tmp = "/",
            .cb = home_callback,
            .n_args = 0,
        },
        {
            .method = HTTP_METHOD_GET,
            .route_tmp = "/favicon.ico",
            .cb = favicon_callback,
            .n_args = 0,
        },
    };

    if (init_server(&server, 8080, 10, routes, n_routes) != 0) {
        fprintf(stderr, "Failed to init server\n");
        exit(EXIT_FAILURE);
    }

    if (start_server(&server) != 0) {
        fprintf(stderr, "Failed to start server\n");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

