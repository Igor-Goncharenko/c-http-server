#include "../../http_server.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#define n_routes 1

int home_callback(const request_data_t *request, const va_list args, 
        char **resp_dest, int *resp_len) {
    static const char home_resp[] = "<h1>Home page!</h1>";

    *resp_len = sizeof(home_resp);
    *resp_dest = malloc(*resp_len);
    strcpy(*resp_dest, home_resp);

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

