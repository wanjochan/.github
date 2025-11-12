// Advanced math library - depends on calculator module
#include "src/cosmo_libc.h"

// Note: In a real implementation with dependency resolution,
// calculator module would be auto-loaded here

int mathlib_init() {
    printf("[MathLib] Advanced math library initialized\n");
    return 0;
}

// Factorial using recursion
unsigned long mathlib_factorial(int n) {
    if (n < 0) return 0;
    if (n == 0 || n == 1) return 1;
    return n * mathlib_factorial(n - 1);
}

// Fibonacci sequence
unsigned long mathlib_fibonacci(int n) {
    if (n < 0) return 0;
    if (n == 0) return 0;
    if (n == 1) return 1;

    unsigned long a = 0, b = 1, c;
    for (int i = 2; i <= n; i++) {
        c = a + b;
        a = b;
        b = c;
    }
    return b;
}

// Check if number is prime
int mathlib_is_prime(int n) {
    if (n <= 1) return 0;
    if (n <= 3) return 1;
    if (n % 2 == 0 || n % 3 == 0) return 0;

    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) {
            return 0;
        }
    }
    return 1;
}
