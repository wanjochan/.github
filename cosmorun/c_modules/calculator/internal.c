// Calculator internal implementation
#include "src/cosmo_libc.h"
#include "internal.h"

int calc_internal_add(int a, int b) {
    return a + b;
}

int calc_internal_sub(int a, int b) {
    return a - b;
}

int calc_internal_mul(int a, int b) {
    return a * b;
}

int calc_internal_div(int a, int b) {
    return a / b;
}

double calc_internal_power(double base, int exp) {
    if (exp == 0) return 1.0;
    if (exp < 0) return 1.0 / calc_internal_power(base, -exp);

    double result = 1.0;
    for (int i = 0; i < exp; i++) {
        result *= base;
    }
    return result;
}

double calc_internal_sqrt(double x) {
    if (x < 0) return 0.0;
    if (x == 0) return 0.0;

    // Newton's method
    double guess = x / 2.0;
    for (int i = 0; i < 10; i++) {
        guess = (guess + x / guess) / 2.0;
    }
    return guess;
}
