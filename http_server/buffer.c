#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "http_server.h"
#include "http_server_internal.h"

#define BUFFER_START_CAP    64

HTTP_SERVER_LIB http_server_err_t
buffer_init(buffer_t *self) {
    if ((self->mem = malloc(BUFFER_START_CAP)) == NULL)
        return HS_CREATE_ERR(HTTP_SERVER_MALLOC_ERR);
    self->cap = BUFFER_START_CAP;
    self->len = 0;
    self->allow_realloc = false;
    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

HTTP_SERVER_LIB http_server_err_t 
buffer_init_with_size(buffer_t *self, size_t size) {
    if ((self->mem = malloc(size)) == NULL)
        return HS_CREATE_ERR(HTTP_SERVER_MALLOC_ERR);
    self->cap = size;
    self->len = 0;
    self->allow_realloc = false;
    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

HTTP_SERVER_LIB http_server_err_t 
buffer_append_mem(buffer_t *self, const int item_size, const int nitems, const void *src, 
        void **beginning_ptr) {
    assert(!(SIZE_MAX / item_size < nitems));
    size_t add_size = item_size * nitems;

    if (add_size == 0)
        return HS_CREATE_ERR(HTTP_SERVER_OK);

    assert(SIZE_MAX - self->len >= add_size);
    size_t new_size = self->len + add_size;

    if (new_size > self->cap) {
        if (self->allow_realloc == false) {
            fprintf(stderr, "Buffer overflow: required size = %lu, avaliable size = %lu\n",
                    new_size, self->cap);
            fprintf(stderr, "%s\n", self->data);
            fflush(stderr);
            return HS_CREATE_ERR(HTTP_SERVER_BUFFER_OVERFLOW_ERR);
        }

        if (SIZE_MAX / 2 < new_size) {
            new_size = SIZE_MAX;
        } else {
            while (self->cap < new_size)
                self->cap *= 2;
        }
        if ((self->mem = realloc(self->mem, self->cap)) == NULL)
            return HS_CREATE_ERR(HTTP_SERVER_MALLOC_ERR);
    }

    if (src != NULL && memcpy(self->mem + self->len, src, add_size) == NULL)
        return HS_CREATE_ERR(HTTP_SERVER_MEMCPY_ERR);

    if (beginning_ptr != NULL)
        *beginning_ptr = self->mem + self->len;

    self->len = new_size;

    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

HTTP_SERVER_LIB void 
buffer_free(buffer_t *self) {
    if (self != NULL) {
        if (self->mem != NULL) 
            free(self->mem);
        self->len = 0;
        self->cap = 0;
        self->mem = NULL;
    }
}

HTTP_SERVER_LIB http_server_err_t
buffer_append_sentence(buffer_t *self, const char *sentence, const size_t sentence_len, 
        char **beginning_ptr) {
    http_server_err_t err;
    if (HS_ERROR_CHECK(err, buffer_append_mem(self, 1, sentence_len, sentence, NULL)))
        return err;
    if (HS_ERROR_CHECK(err, buffer_append_mem(self, 1, 1, "\0", NULL)))
        return err;
    if (beginning_ptr != NULL)
        *beginning_ptr = self->data + self->len - sentence_len - 1;
    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

HTTP_SERVER_LIB http_server_err_t
buffer_join_buffer(buffer_t *self, const buffer_t *other, void **beginning_ptr) {
    http_server_err_t err;
    if (HS_ERROR_CHECK(err, buffer_append_mem(self, other->len, 1, other->mem, NULL)))
        return err;
    if (beginning_ptr != NULL)
        *beginning_ptr = self->mem + self->len - other->len;
    return HS_CREATE_ERR(HTTP_SERVER_OK);
}

