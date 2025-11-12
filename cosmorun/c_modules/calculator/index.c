// Calculator module package - Entry point
#include "src/cosmo_libc.h"

// Internal implementations
static int calc_internal_add(int a, int b) {
    return a + b;
}

static int calc_internal_sub(int a, int b) {
    return a - b;
}

static int calc_internal_mul(int a, int b) {
    return a * b;
}

static int calc_internal_div(int a, int b) {
    return a / b;
}

static double calc_internal_power(double base, int exp) {
    if (exp == 0) return 1.0;
    if (exp < 0) return 1.0 / calc_internal_power(base, -exp);

    double result = 1.0;
    for (int i = 0; i < exp; i++) {
        result *= base;
    }
    return result;
}

static double calc_internal_sqrt(double x) {
    if (x < 0) return 0.0;
    if (x == 0) return 0.0;

    // Newton's method
    double guess = x / 2.0;
    for (int i = 0; i < 10; i++) {
        guess = (guess + x / guess) / 2.0;
    }
    return guess;
}

// Public API

int calculator_init() {
    printf("[Calculator] Module initialized\n");
    return 0;  // Success
}

int calculator_add(int a, int b) {
    return calc_internal_add(a, b);
}

int calculator_subtract(int a, int b) {
    return calc_internal_sub(a, b);
}

int calculator_multiply(int a, int b) {
    return calc_internal_mul(a, b);
}

int calculator_divide(int a, int b) {
    if (b == 0) {
        printf("[Calculator] Error: Division by zero\n");
        return 0;
    }
    return calc_internal_div(a, b);
}

double calculator_power(double base, int exp) {
    return calc_internal_power(base, exp);
}

double calculator_sqrt(double x) {
    return calc_internal_sqrt(x);
}
