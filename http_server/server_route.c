#include "http_server.h"
#include "http_server_internal.h"

#include <regex.h>
#include <string.h>
#include <stdlib.h>

#define HS_FORMAT_TYPE_LAST (HS_FORMAT_TYPE_INT)
typedef enum {
    HS_FORMAT_TYPE_STR = 0,
    HS_FORMAT_TYPE_INT,
} hs_format_types_e;

static const char 
*HS_FORMAT_USER_STR[] = {
    [HS_FORMAT_TYPE_STR] = "{str}",
    [HS_FORMAT_TYPE_INT] = "{int}",
};

static const char 
*HS_FORMAT_REGEX[] = {
    [HS_FORMAT_TYPE_STR] = "[[:alnum:]]+",
    [HS_FORMAT_TYPE_INT] = "[[:digit:]]+",
};

HS_STATIC const char *
_hs_find_user_str(const char *str, const int len) {
    for (int i = 0; i <= HS_FORMAT_TYPE_LAST; i++) {
        if (strncmp(str, HS_FORMAT_USER_STR[i], len) == 0) {
            return HS_FORMAT_REGEX[i];
        }
    }
    return NULL;
}

HS_STATIC bool 
_hs_should_escape(const char ch) {
    static const char 
    TO_ESCAPE[] = { '.', '^', '$', '*', '+', '?', '{', '}', '[', ']', '(', ')', '`', '\\' };

    for (int i = 0; i < sizeof(TO_ESCAPE); i++) {
        if (ch == TO_ESCAPE[i]) return true;
    }
    return false;
}

HS_STATIC hs_err_t 
_hs_escape_char_and_add(const char *str, const int len, hs_buffer_t *buf) {
    hs_err_t err;
    for (int i = 0; i < len; i++) {
        if (_hs_should_escape(str[i])) {
            if (HS_ERROR_CHECK(err, hs_buffer_append_mem(buf, 1, 1, "\\", NULL)))
                return err;
        }
        if (HS_ERROR_CHECK(err, hs_buffer_append_mem(buf, 1, 1, &str[i], NULL)))
            return err;
    }
    return HS_CREATE_ERR(HS_OK);
}

HS_STATIC hs_err_t 
_hs_create_regex_from_user_str(const char *str, char **re, int *n_matches) {
    hs_err_t err;
    hs_buffer_t buf;
    int last_c = 0;;
    *n_matches = 0;
    char *start, *end;

    hs_buffer_init(&buf);

    while ((start = strchr(str + last_c, '{')) != NULL) {
        if (HS_ERROR_CHECK(err, _hs_escape_char_and_add(str + last_c, start - str, &buf)))
            goto failed;

        last_c = start - str;
        if ((end = strchr(str, '}')) == NULL) {
            err = HS_CREATE_ERR(HS_ROUTE_ERR);
            goto failed;
        }

        (*n_matches)++;

        const int fmt_len = end - start + 1;
        const char *re_fmt = _hs_find_user_str(start, fmt_len);
        if (re_fmt == NULL) {
            LOG_ERROR("Incorrect format: '%.*s'.", fmt_len, start);
            err = HS_CREATE_ERR(HS_ROUTE_ERR);
            goto failed;
        }

        if (    HS_ERROR_CHECK(err, hs_buffer_append_mem(&buf, 1, 1, "(", NULL)) ||
                HS_ERROR_CHECK(err, hs_buffer_append_mem(&buf, 1, strlen(re_fmt), re_fmt, NULL)) ||
                HS_ERROR_CHECK(err, hs_buffer_append_mem(&buf, 1, 1, ")", NULL)))
            goto failed;
        last_c = end - str + 1;
    }

    if (    HS_ERROR_CHECK(err, _hs_escape_char_and_add(str + last_c, strlen(str + last_c), &buf)) ||
            HS_ERROR_CHECK(err, hs_buffer_append_mem(&buf, 1, 1, "$", NULL)) ||
            HS_ERROR_CHECK(err, hs_buffer_append_mem(&buf, 1, 1, "\0", NULL)))
        goto failed;

    *re = buf.data;

    return HS_CREATE_ERR(HS_OK);

failed:
    *re = NULL;
    *n_matches = 0;
    hs_buffer_free(&buf);
    return err;
}

HS_LIB hs_err_t
hs_cpy_init_routes_to_server(hs_server_t *self, const hs_server_route_t *routes, const int n_routes) {
    hs_err_t err;
    hs_buffer_t buf = { 0 };
    char *route_re = NULL;
    int re_init_n = 0;

    /* count mem */
    const int routes_list_mem = n_routes * sizeof(hs_server_route_t);
    int total_mem = routes_list_mem;
    for (int i = 0; i < n_routes; i++) {
        int tmp = 0;
        if (HS_ERROR_CHECK(err, _hs_create_regex_from_user_str(routes[i].route_tmp, &route_re, &tmp)))
            goto failed;
        total_mem += strlen(route_re) + 1;
        free(route_re);
        route_re = NULL;
    }

    if (    HS_ERROR_CHECK(err, hs_buffer_init_with_size(&buf, total_mem)) ||
            HS_ERROR_CHECK(err, hs_buffer_append_mem(&buf, sizeof(hs_server_route_t), 
                    n_routes, NULL, NULL)))
        goto failed;

    self->routes = (hs_server_route_t*)buf.mem;

    if (memcpy(self->routes, routes, routes_list_mem) == NULL) {
        err = HS_CREATE_ERR(HS_MEMCPY_ERR);
        goto failed;
    }

    for (int i = 0; i < n_routes; i++) {
        if (HS_ERROR_CHECK(err, _hs_create_regex_from_user_str(routes[i].route_tmp, &route_re, 
                        &self->routes[i]._n_matches)))
            goto failed;
        if (HS_ERROR_CHECK(err, hs_buffer_append_sentence(&buf, route_re, strlen(route_re), 
                        &self->routes[i].route_tmp)))
            goto failed;
        if (regcomp(&self->routes[i]._re, self->routes[i].route_tmp, REG_EXTENDED) != 0)
            goto failed;
        re_init_n++;
        free(route_re);
        route_re = NULL;

        LOG_TRACE("New route created for tmp '%s': '%s' (%d).", routes[i].route_tmp, 
                self->routes[i].route_tmp, self->routes[i]._n_matches);
    }

    self->mem = buf.mem;
    self->n_routes = n_routes;

    return HS_CREATE_ERR(HS_OK);
failed:
    if (route_re != NULL) free(route_re);
    hs_buffer_free(&buf);
    for (int i = 0; i < re_init_n; i++) {
        regfree(&self->routes[i]._re);
    }
    return err;
}
