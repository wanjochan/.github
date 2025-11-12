// Calculator internal implementation
#ifndef CALCULATOR_INTERNAL_H
#define CALCULATOR_INTERNAL_H

// Internal functions (not exposed to users)
int calc_internal_add(int a, int b);
int calc_internal_sub(int a, int b);
int calc_internal_mul(int a, int b);
int calc_internal_div(int a, int b);
double calc_internal_power(double base, int exp);
double calc_internal_sqrt(double x);

#endif
