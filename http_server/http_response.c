#include "http_server.h"
#include "http_server_internal.h"

HTTP_SERVER_STATIC const char
*HTTP_CODE_STR[] = {
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

HTTP_SERVER_LIB const char*
get_http_code_str(const int code) {
    if (code < 0 || code > sizeof(HTTP_CODE_STR) / 8)
        return "UNKNOWN";
    const char *res = HTTP_CODE_STR[code];
    return (res != NULL) ? res : "UNKNOWN";
}


