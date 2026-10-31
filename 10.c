#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>

typedef enum {
    OK, ERR_INVALID_ARG, ERR_ALLOC, ERR_OVERFLOW, ERR_OUTPUT
} Status;

Status shift_polynomial(const double epsilon, const double a,
                              double **result, const int degree, ...)
{
    if (result == NULL) return ERR_INVALID_ARG;
    *result = NULL;
    if (!isfinite(epsilon) || epsilon <= 0.0 || !isfinite(a) || degree < 0)
        return ERR_INVALID_ARG;
    if ((size_t)degree >= SIZE_MAX / sizeof(double)) return ERR_OVERFLOW;
    const size_t count = (size_t)degree + 1;
    double *g = malloc(count * sizeof(double));
    if (g == NULL) return ERR_ALLOC;

    Status status = OK;
    va_list args;
    va_start(args, degree);
    for (size_t i = 0; i < count; ++i) {
        g[i] = va_arg(args, double);
        if (!isfinite(g[i])) {
            status = ERR_INVALID_ARG;
            break;
        }
    }
    va_end(args);
    for (int i = 0; i < degree && status == OK; ++i) {
        for (int j = degree - 1; j >= i; --j) {
            g[j] += a * g[j + 1];
            if (!isfinite(g[j])) {
                status = ERR_OVERFLOW;
                break;
            }
        }
    }
    if (status != OK) {
        free(g);
        return status;
    }
    for (size_t i = 0; i < count; ++i)
        if (fabs(g[i]) < epsilon) g[i] = 0.0;
    *result = g;
    return OK;
}

static const char *status_text(const Status status)
{
    switch (status) {
        case OK: return "OK";
        case ERR_INVALID_ARG: return "Invalid argument or non-finite coefficient";
        case ERR_ALLOC: return "Not enough memory";
        case ERR_OVERFLOW: return "Size or arithmetic overflow";
        case ERR_OUTPUT: return "Cannot write output";
    }
    return "Unknown error";
}

static Status print_result(const double a, const double *g, const int degree)
{
    if (!isfinite(a) || g == NULL || degree < 0) return ERR_INVALID_ARG;
    if (printf("a = %g\n", a) < 0) return ERR_OUTPUT;
    for (size_t i = 0; i <= (size_t)degree; ++i) {
        if (!isfinite(g[i])) return ERR_INVALID_ARG;
        if (printf("g%zu = %.10g\n", i, g[i]) < 0) return ERR_OUTPUT;
    }
    return OK;
}

int main(void)
{
    const double epsilon = 1e-9;
    const double a = 2.0;
    double *g = NULL;
    Status status = shift_polynomial(epsilon, a, &g, 3, 1.0, -2.0, 0.0, 1.0);
    if (status == OK) {
        if (printf("f(x) = 1 - 2x + x^3\n") < 0) status = ERR_OUTPUT;
        else status = print_result(a, g, 3);
    }
    if (fflush(stdout) == EOF) status = ERR_OUTPUT;
    free(g);
    if (status != OK) {
        fprintf(stderr, "%s\n", status_text(status));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
