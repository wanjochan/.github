/*
 * example_assert.c - Practical examples using mod_assert
 *
 * Demonstrates how to write clean, readable tests using the assertion library.
 * This shows real-world usage patterns for testing functions.
 */

#include "mod_assert.h"

/* ==================== Example Functions to Test ==================== */

/* Simple math function */
int add(int a, int b) {
    return a + b;
}

/* String manipulation */
char* concat_strings(const char* a, const char* b) {
    if (!a || !b) return NULL;

    size_t len = strlen(a) + strlen(b) + 1;
    char* result = malloc(len);
    if (!result) return NULL;

    strcpy(result, a);
    strcat(result, b);
    return result;
}

/* Find maximum in array */
int find_max(int* arr, int size) {
    if (!arr || size <= 0) return -1;

    int max = arr[0];
    for (int i = 1; i < size; i++) {
        if (arr[i] > max) {
            max = arr[i];
        }
    }
    return max;
}

/* Check if string is palindrome */
int is_palindrome(const char* str) {
    if (!str) return 0;

    int len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        if (str[i] != str[len - 1 - i]) {
            return 0;
        }
    }
    return 1;
}

/* ==================== Test Cases ==================== */

void test_add_function(void) {
    printf("\n=== Testing add() function ===\n");

    ASSERT_EQ_INT(add(2, 3), 5);
    ASSERT_EQ_INT(add(0, 0), 0);
    ASSERT_EQ_INT(add(-5, 5), 0);
    ASSERT_EQ_INT(add(100, 200), 300);

    printf("✓ All add() tests passed\n");
}

void test_concat_strings_function(void) {
    printf("\n=== Testing concat_strings() function ===\n");

    /* Test normal concatenation */
    char* result1 = concat_strings("hello", "world");
    ASSERT_NOT_NULL(result1);
    ASSERT_EQ_STR(result1, "helloworld");
    free(result1);

    /* Test empty strings */
    char* result2 = concat_strings("", "test");
    ASSERT_NOT_NULL(result2);
    ASSERT_EQ_STR(result2, "test");
    free(result2);

    /* Test NULL handling */
    char* result3 = concat_strings(NULL, "test");
    ASSERT_NULL(result3);

    printf("✓ All concat_strings() tests passed\n");
}

void test_find_max_function(void) {
    printf("\n=== Testing find_max() function ===\n");

    int arr1[] = {1, 5, 3, 9, 2};
    ASSERT_EQ_INT(find_max(arr1, 5), 9);

    int arr2[] = {-5, -1, -10, -3};
    ASSERT_EQ_INT(find_max(arr2, 4), -1);

    int arr3[] = {42};
    ASSERT_EQ_INT(find_max(arr3, 1), 42);

    /* Test NULL array */
    ASSERT_EQ_INT(find_max(NULL, 5), -1);

    /* Test empty array */
    ASSERT_EQ_INT(find_max(arr1, 0), -1);

    printf("✓ All find_max() tests passed\n");
}

void test_is_palindrome_function(void) {
    printf("\n=== Testing is_palindrome() function ===\n");

    /* True cases */
    ASSERT_TRUE(is_palindrome("racecar"), "racecar is palindrome");
    ASSERT_TRUE(is_palindrome("madam"), "madam is palindrome");
    ASSERT_TRUE(is_palindrome("a"), "single char is palindrome");
    ASSERT_TRUE(is_palindrome(""), "empty string is palindrome");

    /* False cases */
    ASSERT_FALSE(is_palindrome("hello"), "hello is not palindrome");
    ASSERT_FALSE(is_palindrome("world"), "world is not palindrome");
    ASSERT_FALSE(is_palindrome("ab"), "ab is not palindrome");

    /* NULL case */
    ASSERT_FALSE(is_palindrome(NULL), "NULL is not palindrome");

    printf("✓ All is_palindrome() tests passed\n");
}

void test_comparison_assertions(void) {
    printf("\n=== Testing comparison assertions ===\n");

    int x = 10;
    int y = 20;

    /* Greater than */
    ASSERT_GT_INT(y, x);
    ASSERT_GT_INT(100, 50);

    /* Less than */
    ASSERT_LT_INT(x, y);
    ASSERT_LT_INT(5, 10);

    /* Not equal */
    ASSERT_NE_INT(x, y);
    ASSERT_NE_STR("foo", "bar");

    printf("✓ All comparison tests passed\n");
}

void test_pointer_assertions(void) {
    printf("\n=== Testing pointer assertions ===\n");

    int* valid_ptr = malloc(sizeof(int));
    int* null_ptr = NULL;

    ASSERT_NOT_NULL(valid_ptr);
    ASSERT_NULL(null_ptr);

    free(valid_ptr);

    printf("✓ All pointer tests passed\n");
}

/* ==================== Main Test Runner ==================== */

int main(void) {
    printf("========================================\n");
    printf("Example: Using mod_assert for Testing\n");
    printf("========================================\n");

    /* Reset statistics */
    assert_reset_stats();

    /* Run all test suites */
    test_add_function();
    test_concat_strings_function();
    test_find_max_function();
    test_is_palindrome_function();
    test_comparison_assertions();
    test_pointer_assertions();

    /* Print summary */
    assert_print_summary();

    /* Return exit code based on results */
    assert_stats_t* stats = assert_get_stats();
    return stats->failed_tests;
}
