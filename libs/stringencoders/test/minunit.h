
#ifndef MINUNIT_H
#define MINUNIT_H

/* http://www.jera.com/techinfo/jtns/jtn002.html */

#define mu_assert(test) do { if (!(test)) { snprintf(mu_buf, sizeof(mu_buf), "ASSERTION FAILED: %s:%d", __FILE__, __LINE__); return mu_buf; } } while (0)

#define mu_assert_int_equals(lhs, rhs)  do { if (lhs != rhs) { snprintf(mu_buf, sizeof(mu_buf), "ASSERTION FAILED: %s:%d", __FILE__, __LINE__); return mu_buf; } } while (0)

#define mu_assert_str_equals(lhs, rhs)  do { if (strcmp(lhs,rhs) != 0) { snprintf(mu_buf, sizeof(mu_buf), "ASSERTION FAILED: %s:%d %s != %s", __FILE__, __LINE__, lhs, rhs); return mu_buf; } } while (0)

#define mu_assert_str_equals_msg(msg, lhs, rhs)  do { if (strcmp(lhs,rhs) != 0) { snprintf(mu_buf, sizeof(mu_buf), "ASSERTION FAILED: %s  %s:%d %s != %s", msg, __FILE__, __LINE__, lhs, rhs); return mu_buf; } } while (0)

#define mu_assert_int_equals_msg(msg, lhs, rhs)  do { if (lhs != rhs) { snprintf(mu_buf, sizeof(mu_buf), "ASSERTION FAILED: %s:%d", __FILE__, __LINE__); return mu_buf; } } while (0)

#define mu_run_test(test) do { printf("."); fflush(stdout); char *message = test(); tests_run++; \
                                if (message) return message; } while (0)

#define UNITTESTS \
int main() { \
    printf("%s ", __FILE__); fflush(stdout); \
    char *result = all_tests(); \
    if (result != 0) { \
        printf("%s\n", result); \
    } else { \
        printf("OK (%d tests)\n", tests_run); \
    } \
    return result != 0; \
}

int tests_run = 0;
char mu_buf[1024];

#endif
