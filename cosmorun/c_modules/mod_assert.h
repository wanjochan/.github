/*
 * mod_assert.h - Assertion utilities for testing
 *
 * Provides comprehensive assertion macros with:
 * - Clear error messages with file:line information
 * - Test statistics tracking
 * - Multiple assertion types (bool, equality, null checks)
 * - Color output support
 *
 * Usage:
 *   #include "mod_assert.h"
 *
 *   void my_test() {
 *       ASSERT_TRUE(1 + 1 == 2, "basic math");
 *       ASSERT_EQ_INT(strlen("hi"), 2);
 *       ASSERT_NOT_NULL(malloc(10));
 *   }
 *
 *   int main() {
 *       my_test();
 *       assert_print_summary();
 *       return assert_get_stats()->failed_tests;
 *   }
 */

#ifndef MOD_ASSERT_H
#define MOD_ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Test statistics structure */
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
} assert_stats_t;

/* Core assertion functions */
void assert_true(int condition, const char* message, const char* file, int line);
void assert_false(int condition, const char* message, const char* file, int line);

/* Equality assertions for integers */
void assert_eq_int(int actual, int expected, const char* file, int line);
void assert_ne_int(int actual, int expected, const char* file, int line);
void assert_gt_int(int actual, int expected, const char* file, int line);
void assert_lt_int(int actual, int expected, const char* file, int line);

/* Equality assertions for strings */
void assert_eq_str(const char* actual, const char* expected, const char* file, int line);
void assert_ne_str(const char* actual, const char* expected, const char* file, int line);

/* NULL pointer checks */
void assert_null(const void* ptr, const char* file, int line);
void assert_not_null(const void* ptr, const char* file, int line);

/* Statistics management */
assert_stats_t* assert_get_stats(void);
void assert_reset_stats(void);
void assert_print_summary(void);

/* Convenience macros that auto-inject file/line */
#define ASSERT_TRUE(cond, msg) assert_true(cond, msg, __FILE__, __LINE__)
#define ASSERT_FALSE(cond, msg) assert_false(cond, msg, __FILE__, __LINE__)

#define ASSERT_EQ_INT(a, e) assert_eq_int(a, e, __FILE__, __LINE__)
#define ASSERT_NE_INT(a, e) assert_ne_int(a, e, __FILE__, __LINE__)
#define ASSERT_GT_INT(a, e) assert_gt_int(a, e, __FILE__, __LINE__)
#define ASSERT_LT_INT(a, e) assert_lt_int(a, e, __FILE__, __LINE__)

#define ASSERT_EQ_STR(a, e) assert_eq_str(a, e, __FILE__, __LINE__)
#define ASSERT_NE_STR(a, e) assert_ne_str(a, e, __FILE__, __LINE__)

#define ASSERT_NULL(p) assert_null(p, __FILE__, __LINE__)
#define ASSERT_NOT_NULL(p) assert_not_null(p, __FILE__, __LINE__)

#ifdef __cplusplus
}
#endif

#endif /* MOD_ASSERT_H */
