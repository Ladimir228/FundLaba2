#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <wchar.h>

typedef enum {
    OK, NOT_EXTRA, END, NO_MATCH, INVALID_ARGUMENT, MEMORY_ERROR, SIZE_ERROR, IO_ERROR
} Status;

typedef enum {
    NORMAL, HH, H, L, LL, J, Z, T, BIG_L
} Length;

typedef struct {
    FILE *file;
    const char *text;
    size_t count;
} Input;

typedef struct {
    char *text;
    Length length;
    char conversion;
    int skip;
} Format;

int scan_source(Input *input, int *consumed, const char *format, ...)
{
    if (input == NULL || consumed == NULL || format == NULL) return EOF;
    *consumed = -1;
    if (input->file == NULL && input->text == NULL) return EOF;
    va_list args;
    va_start(args, format);
    int result;
    if (input->file != NULL) result = vfscanf(input->file, format, args);
    else result = vsscanf(input->text + input->count, format, args);
    va_end(args);
    if ((input->file != NULL && ferror(input->file)) ||
        (*consumed >= 0 && (size_t)*consumed > SIZE_MAX - input->count)) {
        *consumed = -1;
        return EOF;
    }
    if (*consumed >= 0) input->count += (size_t)*consumed;
    return result;
}

Status read_token(Input *input, const char *alphabet, const int fibonacci, char **result)
{
    if (input == NULL || alphabet == NULL || *alphabet == '\0' || result == NULL)
        return INVALID_ARGUMENT;
    *result = NULL;
    char pattern[64];
    if (strlen(alphabet) > sizeof(pattern) - 8) return INVALID_ARGUMENT;
    strcpy(pattern, "%1[");
    strcat(pattern, alphabet);
    strcat(pattern, "]%n");
    int consumed;
    if (scan_source(input, &consumed, " %n", &consumed) == EOF) return IO_ERROR;
    size_t size = 0, capacity = 32;
    char *text = malloc(capacity);
    if (text == NULL) return MEMORY_ERROR;
    Status status = OK;
    for (;;) {
        char symbol[2];
        int count = scan_source(input, &consumed, pattern, symbol, &consumed);
        if (count != 1) {
            if (input->file != NULL && ferror(input->file)) status = IO_ERROR;
            else if (size == 0) status = count == EOF ? END : NO_MATCH;
            break;
        }
        if (size == capacity - 1) {
            if (capacity > SIZE_MAX / 2) { status = SIZE_ERROR; break; }
            char *temp = realloc(text, capacity * 2);
            if (temp == NULL) { status = MEMORY_ERROR; break; }
            text = temp;
            capacity *= 2;
        }
        text[size++] = symbol[0];
        if (fibonacci && size >= 2 && text[size - 2] == '1' && symbol[0] == '1') break;
    }
    if (status != OK) { free(text); return status; }
    text[size] = '\0';
    *result = text;
    return OK;
}
int roman_digit(const char ch)
{
    switch (ch) {
        case 'I': return 1;
        case 'V': return 5;
        case 'X': return 10;
        case 'L': return 50;
        case 'C': return 100;
        case 'D': return 500;
        case 'M': return 1000;
        default: return 0;
    }
}

Status roman_number(const char *text, int *result)
{
    if (text == NULL || result == NULL) return INVALID_ARGUMENT;
    int negative = *text == '-';
    if (*text == '-' || *text == '+') ++text;
    long long value = 0;
    long long limit = negative ? -(long long)INT_MIN : INT_MAX;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        int current = roman_digit(text[i]);
        int next = roman_digit(text[i + 1]);
        value += current < next ? -current : current;
        if (value > limit) return SIZE_ERROR;
    }
    *result = (int)(negative ? -value : value);
    return OK;
}

Status fibonacci_number(const char *text, unsigned int *result)
{
    if (text == NULL || result == NULL || *text == '\0') return INVALID_ARGUMENT;
    unsigned long long first = 1, second = 2, value = 0;
    for (size_t i = 0; text[i + 1] != '\0'; ++i) {
        if (text[i] == '1') {
            if (first > UINT_MAX - value) return SIZE_ERROR;
            value += first;
        }
        if (first > ULLONG_MAX - second) return SIZE_ERROR;
        unsigned long long next = first + second;
        first = second;
        second = next;
    }
    *result = (unsigned int)value;
    return OK;
}

Status base_number(const char *text, const int base, int *result)
{
    if (text == NULL || result == NULL || base < 2 || base > 36) return INVALID_ARGUMENT;
    int negative = *text == '-';
    if (*text == '-' || *text == '+') ++text;
    long long value = 0, limit = negative ? -(long long)INT_MIN : INT_MAX;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        int digit;
        if (text[i] >= '0' && text[i] <= '9') digit = text[i] - '0';
        else if (text[i] >= 'a' && text[i] <= 'z') digit = text[i] - 'a' + 10;
        else digit = text[i] - 'A' + 10;
        if (value > (limit - digit) / base) return SIZE_ERROR;
        value = value * base + digit;
    }
    *result = (int)(negative ? -value : value);
    return OK;
}

Status read_extra(Input *input, const char *format, va_list *args)
{
    if (input == NULL || format == NULL || args == NULL) return INVALID_ARGUMENT;
    int roman = strncmp(format, "Ro", 2) == 0;
    int fibonacci = strncmp(format, "Zr", 2) == 0;
    int base_format = strncmp(format, "Cv", 2) == 0 || strncmp(format, "CV", 2) == 0;
    if (!roman && !fibonacci && !base_format) return NOT_EXTRA;
    char *text = NULL;
    if (fibonacci) {
        unsigned int *result = va_arg(*args, unsigned int *);
        if (result == NULL) return INVALID_ARGUMENT;
        Status status = read_token(input, "01", 1, &text);
        if (status == OK) status = fibonacci_number(text, result);
        free(text);
        return status;
    }
    int *result = va_arg(*args, int *);
    if (result == NULL) return INVALID_ARGUMENT;
    int base = 10;
    char alphabet[39] = "+";
    if (roman) {
        strcpy(alphabet, "+MDCLXVIN-");
    } else {
        base = va_arg(*args, int);
        if (base < 2 || base > 36) base = 10;
        const char *digits = format[1] == 'V' ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" : "0123456789abcdefghijklmnopqrstuvwxyz";
        memcpy(alphabet + 1, digits, (size_t)base);
        alphabet[base + 1] = '-';
        alphabet[base + 2] = '\0';
    }
    Status status = read_token(input, alphabet, 0, &text);
    if (status == OK)
        status = roman ? roman_number(text, result) : base_number(text, base, result);
    free(text);
    return status;
}

Status read_format(const char **position, Format *format)
{
    if (position == NULL || *position == NULL || format == NULL || **position != '%')
        return INVALID_ARGUMENT;
    const char *start = *position, *p = start + 1;
    *format = (Format){NULL, NORMAL, '\0', 0};
    if (*p == '*') { format->skip = 1; ++p; }
    int width = 0;
    while (*p >= '0' && *p <= '9') {
        int digit = *p++ - '0';
        if (width > (INT_MAX - digit) / 10) return SIZE_ERROR;
        width = width * 10 + digit;
    }
    if (*p == 'h') {
        ++p; format->length = H;
        if (*p == 'h') { ++p; format->length = HH; }
    } else if (*p == 'l') {
        ++p; format->length = L;
        if (*p == 'l') { ++p; format->length = LL; }
    } else if (*p == 'j') { ++p; format->length = J; }
    else if (*p == 'z') { ++p; format->length = Z; }
    else if (*p == 't') { ++p; format->length = T; }
    else if (*p == 'L') { ++p; format->length = BIG_L; }
    if (*p == '\0') return INVALID_ARGUMENT;
    format->conversion = *p++;
    if (format->conversion == '[') {
        if (*p == '^') ++p;
        if (*p == ']') ++p;
        while (*p != '\0' && *p != ']') ++p;
        if (*p == '\0') return INVALID_ARGUMENT;
        ++p;
    }
    char c = format->conversion;
    if (strchr("diouxXn", c) != NULL) {
        if (format->length == BIG_L) return INVALID_ARGUMENT;
    } else if (strchr("aAeEfFgG", c) != NULL) {
        if (format->length != NORMAL && format->length != L && format->length != BIG_L)
            return INVALID_ARGUMENT;
    } else if (c == 'c' || c == 's' || c == '[') {
        if (format->length != NORMAL && format->length != L) return INVALID_ARGUMENT;
    } else if (c != 'p' || format->length != NORMAL) return INVALID_ARGUMENT;
    size_t size = (size_t)(p - start);
    if (size > SIZE_MAX - 3) return SIZE_ERROR;
    format->text = malloc(size + 3);
    if (format->text == NULL) return MEMORY_ERROR;
    memcpy(format->text, start, size);
    memcpy(format->text + size, "%n", 3);
    *position = p;
    return OK;
}

#define SCAN_VALUE(type) do { \
    type *value = va_arg(*args, type *); \
    if (value == NULL) return EOF; \
    if (c == 'n') { *value = (type)input->count; *consumed = 0; return 0; } \
    return scan_source(input, consumed, format->text, value, consumed); \
} while (0)

int read_standard(Input *input, const Format *format, va_list *args, int *consumed)
{
    if (input == NULL || format == NULL || format->text == NULL || args == NULL || consumed == NULL)
        return EOF;
    if (format->skip) return scan_source(input, consumed, format->text, consumed);
    char c = format->conversion;
    if (c == 'd' || c == 'i' || c == 'n') {
        switch (format->length) {
            case HH: SCAN_VALUE(signed char);
            case H: SCAN_VALUE(short);
            case L: SCAN_VALUE(long);
            case LL: SCAN_VALUE(long long);
            case J: SCAN_VALUE(intmax_t);
            case Z:
            case T: SCAN_VALUE(ptrdiff_t);
            default: SCAN_VALUE(int);
        }
    }
    if (strchr("ouxX", c) != NULL) {
        switch (format->length) {
            case HH: SCAN_VALUE(unsigned char);
            case H: SCAN_VALUE(unsigned short);
            case L: SCAN_VALUE(unsigned long);
            case LL: SCAN_VALUE(unsigned long long);
            case J: SCAN_VALUE(uintmax_t);
            case Z:
            case T: SCAN_VALUE(size_t);
            default: SCAN_VALUE(unsigned int);
        }
    }
    if (strchr("aAeEfFgG", c) != NULL) {
        if (format->length == BIG_L) SCAN_VALUE(long double);
        if (format->length == L) SCAN_VALUE(double);
        SCAN_VALUE(float);
    }
    if (c == 'p') {
        void **value = va_arg(*args, void **);
        if (value == NULL) return EOF;
        return scan_source(input, consumed, format->text, value, consumed);
    }
    if (format->length == L) SCAN_VALUE(wchar_t);
    SCAN_VALUE(char);
}

#undef SCAN_VALUE
int scan_all(Input *input, const char *format, va_list *args)
{
    if (input == NULL || format == NULL || args == NULL) return EOF;
    int assigned = 0;
    while (*format != '\0') {
        if (isspace((unsigned char)*format)) {
            while (isspace((unsigned char)*format)) ++format;
            int consumed;
            if (scan_source(input, &consumed, " %n", &consumed) == EOF) return EOF;
        } else if (*format != '%' || format[1] == '%') {
            char pattern[] = {*format, '%', 'n', '\0'};
            int consumed;
            int count = scan_source(input, &consumed,
                                    *format == '%' ? "%%%n" : pattern, &consumed);
            if (consumed < 0) return count == EOF && assigned == 0 ? EOF : assigned;
            format += *format == '%' ? 2 : 1;
        } else {
            Status status = read_extra(input, format + 1, args);
            if (status == OK) {
                if (assigned == INT_MAX) return EOF;
                ++assigned;
                format += 3;
            } else if (status == NOT_EXTRA) {
                Format specification = {0};
                status = read_format(&format, &specification);
                if (status != OK) return EOF;
                int consumed = -1;
                int count = read_standard(input, &specification, args, &consumed);
                free(specification.text);
                if (consumed < 0) return count == EOF && assigned == 0 ? EOF : assigned;
                if (count > 0) {
                    if (assigned == INT_MAX) return EOF;
                    ++assigned;
                }
            } else {
                if (status == NO_MATCH) return assigned;
                if (status == END) return assigned == 0 ? EOF : assigned;
                return EOF;
            }
        }
    }
    return assigned;
}
int overfscanf(FILE *file, const char *format, ...)
{
    if (file == NULL || format == NULL) return EOF;
    Input input = {file, NULL, 0};
    va_list args;
    va_start(args, format);
    int result = scan_all(&input, format, &args);
    va_end(args);
    return result;
}

int oversscanf(const char *text, const char *format, ...)
{
    if (text == NULL || format == NULL) return EOF;
    Input input = {NULL, text, 0};
    va_list args;
    va_start(args, format);
    int result = scan_all(&input, format, &args);
    va_end(args);
    return result;
}

int main(int argc, char *argv[])
{
    if (argc > 2 || (argc == 2 && argv[1][0] == '\0')) {
        fprintf(stderr, "Usage: %s [input_file]\n", argv[0]);
        return EXIT_FAILURE;
    }
    FILE *file = fopen(argc == 2 ? argv[1] : "tests/input6.txt", "r");
    if (file == NULL) { fprintf(stderr, "Cannot open input file.\n"); return EXIT_FAILURE; }
    int roman, lower, upper, number;
    unsigned int fibonacci;
    char word[32];
    const char *format = "%Ro %Zr %Cv %CV %d %31s";
    int count = overfscanf(file, format, &roman, &fibonacci, &lower, 16, &upper, 16, &number, word);
    int closed = fclose(file);
    if (count != 6 || closed == EOF) {
        fprintf(stderr, "Input or file error.\n");
        return EXIT_FAILURE;
    }
    if (printf("overfscanf: %d %u %d %d %d %s\n", roman, fibonacci, lower, upper, number, word) < 0)
        return EXIT_FAILURE;
    count = oversscanf("XIV 1011 101 7B 3 hello", format,
                       &roman, &fibonacci, &lower, 2, &upper, 16, &number, word);
    if (count != 6) { fprintf(stderr, "String input error.\n"); return EXIT_FAILURE; }
    if (printf("oversscanf: %d %u %d %d %d %s\n", roman, fibonacci, lower, upper, number, word) < 0 ||
        fflush(stdout) == EOF) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
