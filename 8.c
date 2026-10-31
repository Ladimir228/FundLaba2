#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

typedef enum {
    OK, INVALID_ARGUMENT, MEMORY_ERROR, SIZE_ERROR, OUTPUT_ERROR
} Status;

int digit_value(const char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 10;
    return -1;
}

Status check_number(const char *number, const int base, size_t *length)
{
    if (number == NULL || length == NULL || base < 2 || base > 36 || *number == '\0')
        return INVALID_ARGUMENT;
    *length = strlen(number);
    for (size_t i = 0; i < *length; ++i) {
        int digit = digit_value(number[i]);
        if (digit < 0 || digit >= base) return INVALID_ARGUMENT;
    }
    return OK;
}

Status add_numbers(const char *first, const char *second, const int base, char **result)
{
    if (result == NULL) return INVALID_ARGUMENT;
    *result = NULL;
    size_t first_length, second_length;
    Status status = check_number(first, base, &first_length);
    if (status == OK) status = check_number(second, base, &second_length);
    if (status != OK) return status;

    size_t length = first_length > second_length ? first_length : second_length;
    if (length > SIZE_MAX - 2) return SIZE_ERROR;
    char *sum = malloc(length + 2);
    if (sum == NULL) return MEMORY_ERROR;
    const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    size_t position = length + 1;
    sum[position] = '\0';
    int carry = 0;
    while (first_length > 0 || second_length > 0 || carry != 0) {
        int value = carry;
        if (first_length > 0) value += digit_value(first[--first_length]);
        if (second_length > 0) value += digit_value(second[--second_length]);
        sum[--position] = digits[value % base];
        carry = value / base;
    }
    while (sum[position] == '0' && sum[position + 1] != '\0') ++position;
    memmove(sum, sum + position, length + 2 - position);
    *result = sum;
    return OK;
}

Status sum_numbers(const int base, const int count, char **result, ...)
{
    if (result == NULL) return INVALID_ARGUMENT;
    *result = NULL;
    if (base < 2 || base > 36 || count < 0) return INVALID_ARGUMENT;
    *result = malloc(2);
    if (*result == NULL) return MEMORY_ERROR;
    strcpy(*result, "0");

    Status status = OK;
    va_list args;
    va_start(args, result);
    for (int i = 0; i < count; ++i) {
        const char *number = va_arg(args, const char *);
        char *next = NULL;
        status = add_numbers(*result, number, base, &next);
        if (status != OK) break;
        free(*result);
        *result = next;
    }
    va_end(args);
    if (status != OK) {
        free(*result);
        *result = NULL;
    }
    return status;
}

int main(void)
{
    const char *first = "000FFFFFFFFFFFFFFFFFFFFFFFF";
    const char *second = "1";
    const char *third = "a";
    char *result = NULL;
    Status status = sum_numbers(16, 3, &result, first, second, third);
    if (status == OK) {
        if (printf("%s + %s + %s (base 16) = %s\n", first, second, third, result) < 0 ||
            fflush(stdout) == EOF)
            status = OUTPUT_ERROR;
    }
    free(result);

    switch (status) {
        case OK: return EXIT_SUCCESS;
        case INVALID_ARGUMENT: fprintf(stderr, "Invalid base, count or number.\n"); break;
        case MEMORY_ERROR: fprintf(stderr, "Not enough memory.\n"); break;
        case SIZE_ERROR: fprintf(stderr, "Number is too long.\n"); break;
        case OUTPUT_ERROR: fprintf(stderr, "Cannot write output.\n"); break;
    }
    return EXIT_FAILURE;
}
