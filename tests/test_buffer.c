#include <stdlib.h>
#include <stdio.h>

#include "tests.c"

#include "buffer.c"

int buffer_cmp(buffer_t *buf1, buffer_t *buf2) {
    if (buf1->len != buf2->len) return 1;
    if (buf1->cap != buf2->cap) return 1;
    if (buf1->allow_realloc != buf2->allow_realloc) return 1;
    for (int i = 0; i < buf1->len; i++)
        if (buf1->data[i] != buf2->data[i])
            return 1;
    return 0;
}

int main(int argc, char *argv[]) {
    tests_counter_t tc = { 0 };
    buffer_t test_buf;

    {
        TEST(
                tc,
                "buffer_init_1",
                http_server_err_e,
                HTTP_SERVER_OK,
                int_cmp,
                buffer_init,
                &test_buf
            );

        buffer_t test_buf_2 = {
            .allow_realloc = false,
            .cap = BUFFER_START_CAP,
            .len = 0,
            .mem = NULL,
        };

        TEST_EQ(
                tc,
                "buffer_init_1_eq",
                buffer_t*,
                &test_buf,
                &test_buf_2,
                buffer_cmp
               );

        buffer_free(&test_buf);

        test_buf_2.cap = 0;
        test_buf_2.len = 0;

        TEST_EQ(
                tc,
                "buffer_init_1_eq_after_free",
                buffer_t*,
                &test_buf,
                &test_buf_2,
                buffer_cmp
               );
    }

    {
        TEST(
                tc,
                "buffer_init_with_size_1",
                http_server_err_e,
                HTTP_SERVER_OK,
                int_cmp,
                buffer_init_with_size,
                &test_buf,
                1000
            );

        buffer_t test_buf_2 = {
            .allow_realloc = false,
            .cap = 1000,
            .len = 0,
            .mem = NULL,
        };

        TEST_EQ(
                tc,
                "buffer_init_with_size_1_eq",
                buffer_t*,
                &test_buf,
                &test_buf_2,
                buffer_cmp
               );

        buffer_free(&test_buf);

        test_buf_2.cap = 0;
        test_buf_2.len = 0;

        TEST_EQ(
                tc,
                "buffer_init_with_size_1_eq_after_free",
                buffer_t*,
                &test_buf,
                &test_buf_2,
                buffer_cmp
               );
    }

    {
        char test_data[1000] = "1234567890123456789012345678901234567";
        buffer_init_with_size(&test_buf, 1000);
        TEST(
                tc,
                "buffer_append_mem_2",
                http_server_err_e,
                HTTP_SERVER_OK,
                int_cmp,
                buffer_append_mem,
                &test_buf,
                1,
                strlen(test_data),
                test_data,
                NULL
            );

        buffer_t test_buf_2 = {
            .len = strlen(test_data),
            .cap = 1000,
            .data = test_data,
            .allow_realloc = false,
        };

        TEST_EQ(
                tc,
                "buffer_append_mem_1_eq",
                buffer_t*,
                &test_buf,
                &test_buf_2,
                buffer_cmp
               );

        void *test_ptr;
        TEST(
                tc,
                "buffer_append_mem_2",
                http_server_err_e,
                HTTP_SERVER_OK,
                int_cmp,
                buffer_append_mem,
                &test_buf,
                1,
                strlen(test_data),
                test_data,
                &test_ptr
            );

        char test_data_2[1000] = "1234567890123456789012345678901234567"
            "1234567890123456789012345678901234567";
        test_buf_2.len = strlen(test_data_2);
        test_buf_2.data = test_data_2;

        TEST_EQ(
                tc,
                "buffer_append_mem_1_eq",
                buffer_t*,
                &test_buf,
                &test_buf_2,
                buffer_cmp
               );

        TEST_EQ(
                tc,
                "buffer_append_mem_beginning_ptr",
                void*,
                test_ptr,
                test_buf.mem + test_buf.len - strlen(test_data),
                ptr_cmp
               );

        buffer_free(&test_buf);
    }

    {
        buffer_init(&test_buf);
        test_buf.allow_realloc = true;
        char *test_ptr;

        TEST(
                tc,
                "buffer_append_sentence",
                http_server_err_e,
                HTTP_SERVER_OK,
                int_cmp,
                buffer_append_sentence,
                &test_buf,
                "1234567890123456789012345678901234567",
                37,
                &test_ptr
            );

        buffer_t test_buf_2 = {
            .allow_realloc = true,
            .len = 38,
            .cap = 64,
            .data = "1234567890123456789012345678901234567",
        };

        TEST_EQ(
                tc,
                "buffer_append_sentence_eq",
                buffer_t*,
                &test_buf,
                &test_buf_2,
                buffer_cmp
               );

        TEST_EQ(
                tc,
                "buffer_append_sentence_beginning_ptr",
                char*,
                test_ptr,
                test_buf.data,
                ptr_cmp
               );

        buffer_free(&test_buf);
    }

    {
        buffer_init(&test_buf);
        test_buf.allow_realloc = true;
        buffer_t test_buf_2;
        buffer_init(&test_buf_2);
        buffer_append_sentence(&test_buf, "1234567890123456789012345678901234567", 37, NULL);
        buffer_append_sentence(&test_buf_2, "1234567890123456789012345678901234567", 37, NULL);

        void *test_ptr;

        TEST(
                tc,
                "buffer_join_buffer",
                http_server_err_e,
                HTTP_SERVER_OK,
                int_cmp,
                buffer_join_buffer,
                &test_buf,
                &test_buf_2,
                &test_ptr
            );

        buffer_t test_buf_3 = {
            .allow_realloc = true,
            .len = 38 * 2,
            .cap = 128,
            .data = "1234567890123456789012345678901234567\0"
                "1234567890123456789012345678901234567\0",
        };

        TEST_EQ(
                tc,
                "buffer_join_buffer_eq",
                buffer_t*,
                &test_buf,
                &test_buf_3,
                buffer_cmp
               );

        TEST_EQ(
                tc,
                "buffer_join_buffer_beginning_ptr",
                void*,
                test_ptr,
                test_buf.mem + 38,
                ptr_cmp
               );
    }

    tc_print_result(__FILE__, &tc);
    printf("\n");
    return EXIT_SUCCESS;
}

