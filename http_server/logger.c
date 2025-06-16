#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "http_server_internal.h"

static const char *_level_strings[] = {
    [HS_LOG_LEVEL_TRACE] = "TRACE", [HS_LOG_LEVEL_DEBUG] = "DEBUG", [HS_LOG_LEVEL_INFO] = "INFO",
    [HS_LOG_LEVEL_WARN] = "WARN",   [HS_LOG_LEVEL_ERROR] = "ERROR",
};

static const int _level_colors[] = {
    [HS_LOG_LEVEL_TRACE] = 39, /* default terminal color */
    [HS_LOG_LEVEL_DEBUG] = 37, /* white */
    [HS_LOG_LEVEL_INFO] = 32,  /* green */
    [HS_LOG_LEVEL_WARN] = 33,  /* yellow */
    [HS_LOG_LEVEL_ERROR] = 31, /* red */
};

HS_LIB void
hs_log_log(const log_level_e level, const int line, const char *file, const char *func,
           const char *fmt, ...) {
    char time_buf[16], date_buf[16];
    va_list args;
    time_t t = time(NULL);

    struct tm *l_time = localtime(&t);
    time_buf[strftime(time_buf, 16, "%H:%M:%S", l_time)] = '\0';
    date_buf[strftime(date_buf, 16, "%Y-%m-%d", l_time)] = '\0';

    va_start(args, fmt);

    printf("\033[1;%dm", _level_colors[level]);
    printf("%s/%s %-7s %s:%s:%d ", time_buf, date_buf, _level_strings[level], file, func, line);
    vprintf(fmt, args);
    printf("\033[0m\n");
    fflush(stdout);

    va_end(args);
}
