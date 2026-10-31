#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>

typedef enum {
    OK, INVALID_ARGUMENT, INVALID_SEED, NO_MEMORY, OVERFLOW, OUTPUT_ERROR
} Status;

Status length(const char *s, size_t *result) {
    if (s == NULL || result == NULL)
        return INVALID_ARGUMENT;

    *result = 0;
    while (s[*result] != '\0') {
        if (*result == SIZE_MAX - 1)
            return OVERFLOW;
        ++*result;
    }
    return OK;
}

Status copy_string(const char *s, char **result, size_t *n) {
    if (result == NULL || n == NULL)
        return INVALID_ARGUMENT;
    *result = NULL;
    Status status = length(s, n);
    if (status != OK)
        return status;

    *result = malloc(*n + 1);
    if (*result == NULL)
        return NO_MEMORY;
    for (size_t i = 0; i <= *n; ++i)
        (*result)[i] = s[i];
    return OK;
}

Status reverse(const char *s, char **result) {
    size_t n;
    Status status = copy_string(s, result, &n);
    if (status != OK)
        return status;

    for (size_t i = 0; i < n; ++i)
        (*result)[i] = s[n - 1 - i];
    return OK;
}

Status upper_odd(const char *s, char **result) {
    size_t n;
    Status status = copy_string(s, result, &n);
    if (status != OK)
        return status;

    for (size_t i = 0; i < n; ++i)
        if (i % 2 == 1)
            (*result)[i] = (char)toupper((unsigned char)s[i]);
    return OK;
}

Status group(const char *s, char **result) {
    size_t n, position = 0;
    Status status = copy_string(s, result, &n);
    if (status != OK)
        return status;
    for (int part = 0; part < 3; ++part) {
        for (size_t i = 0; i < n; ++i) {
            unsigned char ch = (unsigned char)s[i];
            int type = isdigit(ch) ? 0 : (isalpha(ch) ? 1 : 2);
            if (type == part)
                (*result)[position++] = s[i];
        }
    }
    return OK;
}

Status parse_seed(const char *s, unsigned int *seed) {
    if (s == NULL || seed == NULL)
        return INVALID_ARGUMENT;
    if (*s == '\0')
        return INVALID_SEED;

    unsigned int value = 0;
    for (size_t i = 0; s[i] != '\0'; ++i) {
        if (s[i] < '0' || s[i] > '9')
            return INVALID_SEED;
        unsigned int digit = (unsigned int)(s[i] - '0');
        if (value > (UINT_MAX - digit) / 10)
            return INVALID_SEED;
        value = value * 10 + digit;
    }
    *seed = value;
    return OK;
}

Status concatenate(int count, char *const extra[], unsigned int seed, char **result) {
    if (count < 0 || extra == NULL || result == NULL)
        return INVALID_ARGUMENT;
    *result = NULL;

    size_t total = 0;
    size_t selected = (size_t)count / 2 + (size_t)count % 2;
    for (int i = 0; i < count; ++i) {
        size_t n;
        Status status = length(extra[i], &n);
        if (status != OK)
            return status;
        if (i % 2 == 0) {
            if (n > SIZE_MAX - 1 - total)
                return OVERFLOW;
            total += n;
        }
    }
    if (selected > SIZE_MAX / sizeof(const char *))
        return OVERFLOW;
    const char **order = NULL;
    if (selected != 0) {
        order = malloc(selected * sizeof(*order));
        if (order == NULL)
            return NO_MEMORY;
    }
    *result = malloc(total + 1);
    if (*result == NULL) {
        free(order);
        return NO_MEMORY;
    }

    for (size_t i = 0; i < selected; ++i)
        order[i] = extra[2 * i];

    srand(seed);
    for (size_t i = selected; i > 1; --i) {
        size_t j = (size_t)rand() % i;
        const char *temp = order[i - 1];
        order[i - 1] = order[j];
        order[j] = temp;
    }

    size_t position = 0;
    for (size_t i = 0; i < selected; ++i)
        for (size_t j = 0; order[i][j] != '\0'; ++j)
            (*result)[position++] = order[i][j];
    (*result)[position] = '\0';
    free(order);
    return OK;
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argv[1][0] != '-' || argv[1][1] == '\0' || argv[1][2] != '\0') {
        fprintf(stderr, "Usage: %s -l|-r|-u|-n string\n"
                        "       %s -c string seed [strings...]\n", argv[0], argv[0]);
        return EXIT_FAILURE;
    }
    char flag = argv[1][1];
    if ((flag == 'c' && argc < 4) || (flag != 'c' && argc != 3)) {
        fprintf(stderr, "Invalid number of arguments.\n");
        return EXIT_FAILURE;
    }

    Status status;
    char *result = NULL;
    size_t n = 0;
    unsigned int seed;
    switch (flag) {
        case 'l': status = length(argv[2], &n); break;
        case 'r': status = reverse(argv[2], &result); break;
        case 'u': status = upper_odd(argv[2], &result); break;
        case 'n': status = group(argv[2], &result); break;
        case 'c':
            status = parse_seed(argv[3], &seed);
            if (status == OK)
                status = concatenate(argc - 4, argv + 4, seed, &result);
            break;
        default: status = INVALID_ARGUMENT; break;
    }
    if (status == OK) {
        int written = flag == 'l' ? printf("%zu\n", n) : printf("%s\n", result);
        if (written < 0 || fflush(stdout) == EOF)
            status = OUTPUT_ERROR;
    }
    free(result);

    switch (status) {
        case OK: return EXIT_SUCCESS;
        case INVALID_ARGUMENT: fprintf(stderr, "Invalid argument or flag.\n"); break;
        case INVALID_SEED: fprintf(stderr, "Seed must be decimal digits from 0 to %u.\n", UINT_MAX); break;
        case NO_MEMORY: fprintf(stderr, "Not enough memory.\n"); break;
        case OVERFLOW: fprintf(stderr, "String is too large.\n"); break;
        case OUTPUT_ERROR: fprintf(stderr, "Output error.\n"); break;
    }
    return EXIT_FAILURE;
}
