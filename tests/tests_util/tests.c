#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned passed;
    unsigned failed;
    unsigned total;
} tests_counter_t;

void tc_add_passed(tests_counter_t *tc) {
    tc->passed++;
    tc->total++;
}

void tc_add_failed(tests_counter_t *tc) {
    tc->total++;
    tc->failed++;
}

void tc_print_result(const char *name, const tests_counter_t *tc) {
    printf("%-40s : %u/%u\n", name, tc->passed, tc->total);
}

char *create_test_string(const char static_string[]) {
    int len = strlen(static_string);
    char *res = malloc(len + 1);
    memcpy(res, static_string, len);
    res[len] = '\0';
    return res;
}

int int_cmp(long long v1, long long v2) {
    if (v1 == v2) return 0;
    if (v1 > v2)  return 1;
    else          return -1;
}

int ptr_cmp(void *p1, void *p2) {
    if (p1 == p2) return 0;
    if (p1 > p2)  return 1;
    else          return -1;
}

#define TEST_EQ(tc, name, type, v1, v2, cmp_func)                               \
    do {                                                                        \
        type item1 = (v1);                                                      \
        type item2 = (v2);                                                      \
        int cmp_res = cmp_func(v1, v2);                                         \
        printf("%-40s : %s\n", (name), (cmp_res == 0) ? "PASSED" : "FAILED");   \
        fflush(stdout);                                                         \
        (cmp_res == 0) ? tc_add_passed(&tc) : tc_add_failed(&tc);               \
    } while (0)

#define TEST(tc, name, ret_type, expected_ret, cmp_func, func, ...)             \
    do {                                                                        \
        ret_type v1 = func(__VA_ARGS__);                                        \
        ret_type v2 = expected_ret;                                             \
        TEST_EQ(tc, name, ret_type, v1, v2, cmp_func);                          \
    } while (0)

