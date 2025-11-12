/*
 * mod_assert.c - Assertion utilities implementation
 *
 * Features:
 * - Test statistics tracking
 * - Clear error messages with file:line
 * - Color output (when supported)
 * - Multiple assertion types
 *
 * Migrated to System Layer: libc_shim API adaptation for v2 compatibility
 */

#include "../cosmorun_system/libc_shim/sys_libc_shim.h"
#include "mod_assert.h"

/* ANSI color codes */
#define COLOR_RED     "\033[0;31m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_YELLOW  "\033[0;33m"
#define COLOR_RESET   "\033[0m"

/* Check if we should use colors (basic check for terminal) */
static int use_colors = 1;

/* Global test statistics */
static assert_stats_t global_stats = {0, 0, 0};

/* Internal helper: print error location */
static void print_location(const char* file, int line) {
    if (use_colors) {
        shim_printf("%s%s:%d%s: ", COLOR_YELLOW, file, line, COLOR_RESET);
    } else {
        shim_printf("%s:%d: ", file, line);
    }
}

/* Internal helper: print failure message */
static void print_failure(const char* msg) {
    if (use_colors) {
        shim_printf("%s%s%s\n", COLOR_RED, msg, COLOR_RESET);
    } else {
        shim_printf("%s\n", msg);
    }
}

/* Internal helper: increment stats */
static void record_test(int passed) {
    global_stats.total_tests++;
    if (passed) {
        global_stats.passed_tests++;
    } else {
        global_stats.failed_tests++;
    }
}

/* ==================== Core Assertions ==================== */

void assert_true(int condition, const char* message, const char* file, int line) {
    if (!condition) {
        print_location(file, line);
        print_failure("Assertion failed: expected condition to be true");
        if (message) {
            shim_printf("  Message: %s\n", message);
        }
        record_test(0);
    } else {
        record_test(1);
    }
}

void assert_false(int condition, const char* message, const char* file, int line) {
    if (condition) {
        print_location(file, line);
        print_failure("Assertion failed: expected condition to be false");
        if (message) {
            shim_printf("  Message: %s\n", message);
        }
        record_test(0);
    } else {
        record_test(1);
    }
}

/* ==================== Integer Assertions ==================== */

void assert_eq_int(int actual, int expected, const char* file, int line) {
    if (actual != expected) {
        print_location(file, line);
        print_failure("Assertion failed: integers not equal");
        shim_printf("  Expected: %d\n", expected);
        shim_printf("  Actual:   %d\n", actual);
        record_test(0);
    } else {
        record_test(1);
    }
}

void assert_ne_int(int actual, int expected, const char* file, int line) {
    if (actual == expected) {
        print_location(file, line);
        print_failure("Assertion failed: integers should not be equal");
        shim_printf("  Both values: %d\n", actual);
        record_test(0);
    } else {
        record_test(1);
    }
}

void assert_gt_int(int actual, int expected, const char* file, int line) {
    if (actual <= expected) {
        print_location(file, line);
        print_failure("Assertion failed: expected greater than");
        shim_printf("  Expected: > %d\n", expected);
        shim_printf("  Actual:   %d\n", actual);
        record_test(0);
    } else {
        record_test(1);
    }
}

void assert_lt_int(int actual, int expected, const char* file, int line) {
    if (actual >= expected) {
        print_location(file, line);
        print_failure("Assertion failed: expected less than");
        shim_printf("  Expected: < %d\n", expected);
        shim_printf("  Actual:   %d\n", actual);
        record_test(0);
    } else {
        record_test(1);
    }
}

/* ==================== String Assertions ==================== */

void assert_eq_str(const char* actual, const char* expected, const char* file, int line) {
    /* Handle NULL cases */
    if (actual == NULL && expected == NULL) {
        record_test(1);
        return;
    }

    if (actual == NULL || expected == NULL) {
        print_location(file, line);
        print_failure("Assertion failed: one string is NULL");
        shim_printf("  Expected: %s\n", expected ? expected : "(NULL)");
        shim_printf("  Actual:   %s\n", actual ? actual : "(NULL)");
        record_test(0);
        return;
    }

    if (shim_strcmp(actual, expected) != 0) {
        print_location(file, line);
        print_failure("Assertion failed: strings not equal");
        shim_printf("  Expected: \"%s\"\n", expected);
        shim_printf("  Actual:   \"%s\"\n", actual);
        record_test(0);
    } else {
        record_test(1);
    }
}

void assert_ne_str(const char* actual, const char* expected, const char* file, int line) {
    /* Handle NULL cases - two NULLs are equal */
    if (actual == NULL && expected == NULL) {
        print_location(file, line);
        print_failure("Assertion failed: both strings are NULL (equal)");
        record_test(0);
        return;
    }

    /* If one is NULL, they're not equal - pass */
    if (actual == NULL || expected == NULL) {
        record_test(1);
        return;
    }

    if (shim_strcmp(actual, expected) == 0) {
        print_location(file, line);
        print_failure("Assertion failed: strings should not be equal");
        shim_printf("  Both values: \"%s\"\n", actual);
        record_test(0);
    } else {
        record_test(1);
    }
}

/* ==================== NULL Assertions ==================== */

void assert_null(const void* ptr, const char* file, int line) {
    if (ptr != NULL) {
        print_location(file, line);
        print_failure("Assertion failed: expected NULL pointer");
        shim_printf("  Got: %p\n", ptr);
        record_test(0);
    } else {
        record_test(1);
    }
}

void assert_not_null(const void* ptr, const char* file, int line) {
    if (ptr == NULL) {
        print_location(file, line);
        print_failure("Assertion failed: expected non-NULL pointer");
        record_test(0);
    } else {
        record_test(1);
    }
}

/* ==================== Statistics Management ==================== */

assert_stats_t* assert_get_stats(void) {
    return &global_stats;
}

void assert_reset_stats(void) {
    global_stats.total_tests = 0;
    global_stats.passed_tests = 0;
    global_stats.failed_tests = 0;
}

void assert_print_summary(void) {
    shim_printf("\n");
    shim_printf("========================================\n");
    shim_printf("Test Summary\n");
    shim_printf("========================================\n");
    shim_printf("Total tests:  %d\n", global_stats.total_tests);

    if (use_colors) {
        shim_printf("Passed:       %s%d%s\n", COLOR_GREEN, global_stats.passed_tests, COLOR_RESET);
        if (global_stats.failed_tests > 0) {
            shim_printf("Failed:       %s%d%s\n", COLOR_RED, global_stats.failed_tests, COLOR_RESET);
        } else {
            shim_printf("Failed:       0\n");
        }
    } else {
        shim_printf("Passed:       %d\n", global_stats.passed_tests);
        shim_printf("Failed:       %d\n", global_stats.failed_tests);
    }

    if (global_stats.total_tests > 0) {
        int pass_rate = (global_stats.passed_tests * 100) / global_stats.total_tests;
        shim_printf("Pass rate:    %d%%\n", pass_rate);
    }

    shim_printf("========================================\n");

    if (global_stats.failed_tests == 0 && global_stats.total_tests > 0) {
        if (use_colors) {
            shim_printf("%s✓ All tests passed!%s\n", COLOR_GREEN, COLOR_RESET);
        } else {
            shim_printf("✓ All tests passed!\n");
        }
    }
}
