#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <float.h>
#include <math.h>

typedef enum {
    OK, INVALID_ARGUMENT, INVALID_MATRIX, MEMORY_ERROR, SIZE_ERROR, NUMBER_ERROR, OUTPUT_ERROR
} Status;

typedef struct {
    const double *coordinates;
} Vector;

typedef struct {
    Vector *vectors;
    size_t count;
    double length;
} Result;

typedef Status (*Norm)(const Vector vector, const size_t n, const void *parameters,
                       const double eps, double *result);

Status check_vector(const Vector vector, const size_t n)
{
    if (vector.coordinates == NULL || n == 0) return INVALID_ARGUMENT;
    if (n > SIZE_MAX / sizeof(double)) return SIZE_ERROR;
    for (size_t i = 0; i < n; ++i)
        if (!isfinite(vector.coordinates[i])) return INVALID_ARGUMENT;
    return OK;
}

Status max_norm(const Vector vector, const size_t n, const void *parameters,
                const double eps, double *result)
{
    (void)parameters;
    if (result == NULL || !isfinite(eps) || eps <= 0.0) return INVALID_ARGUMENT;
    Status status = check_vector(vector, n);
    if (status != OK) return status;
    *result = 0.0;
    for (size_t i = 0; i < n; ++i)
        *result = fmax(*result, fabs(vector.coordinates[i]));
    return OK;
}

Status p_norm(const Vector vector, const size_t n, const void *parameters,
              const double eps, double *result)
{
    if (parameters == NULL || result == NULL) return INVALID_ARGUMENT;
    const double p = *(const double *)parameters;
    if (!isfinite(p) || p < 1.0) return INVALID_ARGUMENT;
    double scale;
    Status status = max_norm(vector, n, NULL, eps, &scale);
    if (status != OK) return status;
    if (fpclassify(scale) == FP_ZERO) {
        *result = 0.0;
        return OK;
    }
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i)
        sum += pow(fabs(vector.coordinates[i]) / scale, p);
    *result = scale * pow(sum, 1.0 / p);
    return isfinite(*result) ? OK : NUMBER_ERROR;
}

Status check_matrix(const double *matrix, const size_t n, const double eps)
{
    if (matrix == NULL || n == 0 || !isfinite(eps) || eps <= 0.0)
        return INVALID_ARGUMENT;
    if (n > SIZE_MAX / n / sizeof(long double)) return SIZE_ERROR;
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (!isfinite(matrix[i * n + j])) return INVALID_MATRIX;
            if (fabs(matrix[i * n + j] - matrix[j * n + i]) > eps)
                return INVALID_MATRIX;
        }
    }
    long double *copy = malloc(n * n * sizeof(long double));
    if (copy == NULL) return MEMORY_ERROR;
    for (size_t i = 0; i < n * n; ++i) copy[i] = matrix[i];
    Status status = OK;
    for (size_t k = 0; k < n && status == OK; ++k) {
        if (signbit(copy[k * n + k]) || fpclassify(copy[k * n + k]) == FP_ZERO) {
            status = INVALID_MATRIX;
            break;
        }
        for (size_t i = k + 1; i < n && status == OK; ++i) {
            long double multiplier = copy[i * n + k] / copy[k * n + k];
            for (size_t j = k + 1; j < n; ++j) {
                copy[i * n + j] -= multiplier * copy[k * n + j];
                if (!isfinite(copy[i * n + j])) { status = NUMBER_ERROR; break; }
            }
        }
    }
    free(copy);
    return status;
}

Status matrix_norm(const Vector vector, const size_t n, const void *parameters,
                   const double eps, double *result)
{
    if (parameters == NULL || result == NULL) return INVALID_ARGUMENT;
    Status status = check_vector(vector, n);
    if (status != OK) return status;
    const double *matrix = parameters;
    status = check_matrix(matrix, n, eps);
    if (status != OK) return status;
    long double sum = 0.0L;
    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j)
            sum += (long double)vector.coordinates[i] * matrix[i * n + j] * vector.coordinates[j];
    if (!isfinite(sum) || sum < -(long double)eps) return NUMBER_ERROR;
    long double length = sqrtl(fmaxl(sum, 0.0L));
    if (!isfinite(length) || length > DBL_MAX) return NUMBER_ERROR;
    *result = (double)length;
    return OK;
}

void free_results(Result *results, const size_t count)
{
    if (results == NULL) return;
    for (size_t i = 0; i < count; ++i)
        free(results[i].vectors);
    free(results);
}

Status longest_vectors(const size_t n, const size_t vector_count,
                       const size_t norm_count, const double eps, Result **result, ...)
{
    if (result == NULL) return INVALID_ARGUMENT;
    *result = NULL;
    if (n == 0 || vector_count == 0 || norm_count == 0 || !isfinite(eps) || eps <= 0.0)
        return INVALID_ARGUMENT;
    if (vector_count > SIZE_MAX / sizeof(Vector) ||
        vector_count > SIZE_MAX / sizeof(double) || norm_count > SIZE_MAX / sizeof(Result))
        return SIZE_ERROR;
    Vector *vectors = malloc(vector_count * sizeof(Vector));
    double *lengths = malloc(vector_count * sizeof(double));
    Result *results = calloc(norm_count, sizeof(Result));
    if (vectors == NULL || lengths == NULL || results == NULL) {
        free(vectors);
        free(lengths);
        free(results);
        return MEMORY_ERROR;
    }

    Status status = OK;
    va_list args;
    va_start(args, result);
    for (size_t i = 0; i < vector_count && status == OK; ++i) {
        vectors[i] = va_arg(args, Vector);
        status = check_vector(vectors[i], n);
    }
    for (size_t i = 0; i < norm_count && status == OK; ++i) {
        Norm norm = va_arg(args, Norm);
        const void *parameters = va_arg(args, const void *);
        if (norm == NULL) { status = INVALID_ARGUMENT; break; }
        double maximum = 0.0;
        for (size_t j = 0; j < vector_count && status == OK; ++j) {
            status = norm(vectors[j], n, parameters, eps, &lengths[j]);
            if (status == OK) {
                if (!isfinite(lengths[j]) || lengths[j] < -eps) status = NUMBER_ERROR;
                else maximum = fmax(maximum, lengths[j]);
            }
        }
        if (status != OK) break;
        results[i].vectors = malloc(vector_count * sizeof(Vector));
        if (results[i].vectors == NULL) { status = MEMORY_ERROR; break; }
        results[i].length = maximum;
        for (size_t j = 0; j < vector_count; ++j)
            if (fabs(lengths[j] - maximum) <= eps)
                results[i].vectors[results[i].count++] = vectors[j];
    }
    va_end(args);
    free(vectors);
    free(lengths);
    if (status != OK) {
        free_results(results, norm_count);
        return status;
    }
    *result = results;
    return OK;
}

Status print_result(const char *name, const Result *result, const size_t n)
{
    if (name == NULL || result == NULL || result->vectors == NULL || n == 0)
        return INVALID_ARGUMENT;
    if (printf("%s, length %.6g:\n", name, result->length) < 0) return OUTPUT_ERROR;
    for (size_t i = 0; i < result->count; ++i) {
        Status status = check_vector(result->vectors[i], n);
        if (status != OK) return status;
        if (printf("  (") < 0) return OUTPUT_ERROR;
        for (size_t j = 0; j < n; ++j)
            if (printf("%s%.6g", j == 0 ? "" : ", ", result->vectors[i].coordinates[j]) < 0)
                return OUTPUT_ERROR;
        if (printf(")\n") < 0) return OUTPUT_ERROR;
    }
    return OK;
}

int main(void)
{
    const double x1[] = {3, 0};
    const double x2[] = {0, 3};
    const double x3[] = {2, 2};
    const double x4[] = {-3, 0};
    const Vector a = {x1}, b = {x2}, c = {x3}, d = {x4};
    const double p = 1.0;
    const double matrix[] = {1, 0, 0, 4};
    Result *results = NULL;
    Status status = longest_vectors(2, 4, 3, 1e-9, &results,
        a, b, c, d,
        max_norm, (const void *)NULL,
        p_norm, (const void *)&p,
        matrix_norm, (const void *)matrix);
    const char *const names[] = {"Maximum norm", "p-norm (p = 1)", "Matrix norm"};
    for (size_t i = 0; i < 3 && status == OK; ++i)
        status = print_result(names[i], &results[i], 2);
    if (fflush(stdout) == EOF) status = OUTPUT_ERROR;
    free_results(results, 3);

    switch (status) {
        case OK: return EXIT_SUCCESS;
        case INVALID_ARGUMENT: fprintf(stderr, "Invalid argument.\n"); break;
        case INVALID_MATRIX: fprintf(stderr, "Matrix must be symmetric and positive definite.\n"); break;
        case MEMORY_ERROR: fprintf(stderr, "Not enough memory.\n"); break;
        case SIZE_ERROR: fprintf(stderr, "Data is too large.\n"); break;
        case NUMBER_ERROR: fprintf(stderr, "Numerical overflow or invalid norm value.\n"); break;
        case OUTPUT_ERROR: fprintf(stderr, "Cannot write output.\n"); break;
    }
    return EXIT_FAILURE;
}
