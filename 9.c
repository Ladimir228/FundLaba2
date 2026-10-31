#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>

#define MAX_DIGITS 18

typedef enum {
    OK, ERR_INVALID_ARG, ERR_RANGE, ERR_PRECISION
} Status;

static unsigned long long gcd(unsigned long long a, unsigned long long b)
{
    while (b != 0) {
        unsigned long long remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

static Status to_fraction(const double x, const double epsilon,
                               unsigned long long *numerator, unsigned long long *denominator,
                               unsigned int *decimal_shift)
{
    if (numerator == NULL || denominator == NULL || decimal_shift == NULL ||
        !isfinite(epsilon) || epsilon <= 0.0 || epsilon >= 1.0)
        return ERR_INVALID_ARG;
    if (!isfinite(x) || x <= 0.0 || x >= 1.0) return ERR_RANGE;
    double value = x;
    *decimal_shift = 0;
    while (value < 0.1) {
        value *= 10.0;
        ++*decimal_shift;
    }
    unsigned long long scale = 10;
    for (int digits = 1; digits <= MAX_DIGITS; ++digits) {
        double scaled = value * (double)scale;
        double rounded = round(scaled);
        if (rounded >= 1.0 && rounded < (double)scale &&
            fabs(scaled - rounded) <= epsilon) {
            *numerator = (unsigned long long)rounded;
            *denominator = scale;
            return OK;
        }
        if (scale > ULLONG_MAX / 10) return ERR_PRECISION;
        scale *= 10;
    }
    return ERR_PRECISION;
}

static int is_finite_in_base(unsigned long long numerator, unsigned long long denominator,
                             const unsigned int base, const unsigned int decimal_shift)
{
    if (numerator == 0 || denominator == 0 || base < 2) return 0;
    unsigned long long divisor = gcd(numerator, denominator);
    numerator /= divisor;
    denominator /= divisor;
    unsigned int twos = decimal_shift, fives = decimal_shift;
    while (twos > 0 && numerator % 2 == 0) { numerator /= 2; --twos; }
    while (fives > 0 && numerator % 5 == 0) { numerator /= 5; --fives; }
    if ((twos > 0 && base % 2 != 0) || (fives > 0 && base % 5 != 0)) return 0;
    while (denominator > 1) {
        divisor = gcd(denominator, base);
        if (divisor == 1) return 0;
        denominator /= divisor;
    }
    return 1;
}

Status has_finite_representation(const double epsilon, const unsigned int base,
                                      const size_t count, int *results, ...)
{
    if (!isfinite(epsilon) || epsilon <= 0.0 || epsilon >= 1.0 || base < 2 ||
        count == 0 || count > SIZE_MAX / sizeof(int) || results == NULL)
        return ERR_INVALID_ARG;
    Status status = OK;
    va_list args;
    va_start(args, results);
    for (size_t i = 0; i < count && status == OK; ++i) {
        double number = va_arg(args, double);
        unsigned long long numerator, denominator;
        unsigned int decimal_shift;
        status = to_fraction(number, epsilon, &numerator, &denominator, &decimal_shift);
        if (status == OK)
            results[i] = is_finite_in_base(numerator, denominator, base, decimal_shift);
    }
    va_end(args);
    return status;
}

static const char *status_text(const Status status)
{
    switch (status) {
        case OK: return "OK";
        case ERR_INVALID_ARG: return "Invalid argument";
        case ERR_RANGE: return "Number must be finite and belong to (0, 1)";
        case ERR_PRECISION: return "Cannot recover fraction with the given precision";
    }
    return "Unknown error";
}

int main(void)
{
    const double epsilon = 1e-12;
    const double numbers[] = {0.5, 0.25, 0.1, 0.125, 0.75, 0.2};
    const unsigned int bases[] = {2, 3, 5, 6, 10};
    int results[6];
    for (size_t i = 0; i < 5; ++i) {
        Status status = has_finite_representation(epsilon, bases[i], 6, results,
            numbers[0], numbers[1], numbers[2], numbers[3], numbers[4], numbers[5]);
        if (status != OK) {
            fprintf(stderr, "%s\n", status_text(status));
            return 1;
        }
        if (printf("Base %u:\n", bases[i]) < 0) return 1;
        for (size_t j = 0; j < 6; ++j)
            if (printf("  %g -> %s\n", numbers[j], results[j] ? "finite" : "infinite") < 0)
                return 1;
    }
    return fflush(stdout) == EOF ? 1 : 0;
}
