#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <wchar.h>
#include <float.h>
#include <math.h>

typedef enum {
    OK, NOT_EXTRA_FORMAT, INVALID_FORMAT, INVALID_VALUE, MEMORY_ERROR, SIZE_ERROR
} Status;

typedef enum {
    NORMAL, HH, H, L, LL, J, Z, T, BIG_L
} Length;

typedef struct {
    char *text;
    size_t size;
    size_t capacity;
} Text;

typedef struct {
    Length length;
    char conversion;
    char *text;
    int width;
    int precision;
} Format;

Status grow_text(Text *buffer, const size_t extra)
{
    if (buffer == NULL) return INVALID_VALUE;
    if (buffer->size > INT_MAX || extra > (size_t)INT_MAX - buffer->size)
        return SIZE_ERROR;
    size_t needed = buffer->size + extra + 1;
    if (needed <= buffer->capacity) return OK;
    size_t capacity = needed;
    if (capacity <= SIZE_MAX / 2) capacity *= 2;
    char *temp = realloc(buffer->text, capacity);
    if (temp == NULL) return MEMORY_ERROR;
    buffer->text = temp;
    buffer->capacity = capacity;
    buffer->text[buffer->size] = '\0';
    return OK;
}

Status add_text(Text *buffer, const char *text, const size_t size)
{
    if (text == NULL) return INVALID_VALUE;
    Status status = grow_text(buffer, size);
    if (status != OK) return status;
    memcpy(buffer->text + buffer->size, text, size);
    buffer->size += size;
    buffer->text[buffer->size] = '\0';
    return OK;
}

Status roman_number(Text *buffer, const int number)
{
    if (buffer == NULL) return INVALID_VALUE;
    if (number == 0) return add_text(buffer, "N", 1);
    long long value = number;
    if (value < 0) value = -value;
    const unsigned int values[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    const char *const symbols[] = {"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};
    Status status = number < 0 ? add_text(buffer, "-", 1) : OK;
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]) && status == OK; ++i) {
        while (value >= values[i] && status == OK) {
            status = add_text(buffer, symbols[i], strlen(symbols[i]));
            value -= values[i];
        }
    }
    return status;
}

Status fibonacci_number(Text *buffer, const unsigned int number)
{
    if (buffer == NULL) return INVALID_VALUE;
    if (number == 0) return add_text(buffer, "01", 2);
    unsigned long long fibonacci[2 * sizeof(unsigned int) * CHAR_BIT + 1] = {1, 2};
    char digits[2 * sizeof(unsigned int) * CHAR_BIT + 2];
    size_t count = 2;
    while (fibonacci[count - 1] <= number) {
        fibonacci[count] = fibonacci[count - 1] + fibonacci[count - 2];
        ++count;
    }
    --count;
    memset(digits, '0', count);
    unsigned int remaining = number;
    for (size_t i = count; i > 0; --i) {
        if (fibonacci[i - 1] <= remaining) {
            digits[i - 1] = '1';
            remaining -= fibonacci[i - 1];
        }
    }
    digits[count] = '1';
    return add_text(buffer, digits, count + 1);
}

Status number_to_base(Text *buffer, const int number, int base, const int upper)
{
    if (buffer == NULL || (upper != 0 && upper != 1)) return INVALID_VALUE;
    if (base < 2 || base > 36) base = 10;
    const char *alphabet = upper ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" : "0123456789abcdefghijklmnopqrstuvwxyz";
    char digits[sizeof(int) * CHAR_BIT + 1];
    size_t count = 0;
    long long value = number;
    if (value < 0) value = -value;
    do {
        digits[count++] = alphabet[value % base];
        value /= base;
    } while (value != 0);
    if (number < 0) digits[count++] = '-';
    for (size_t i = 0; i < count / 2; ++i) {
        char temp = digits[i];
        digits[i] = digits[count - 1 - i];
        digits[count - 1 - i] = temp;
    }
    return add_text(buffer, digits, count);
}

Status number_from_base(const char *text, int base, const int upper, int *result)
{
    if (text == NULL || result == NULL || (upper != 0 && upper != 1))
        return INVALID_VALUE;
    if (base < 2 || base > 36) base = 10;
    int negative = *text == '-';
    if (*text == '-' || *text == '+') ++text;
    if (*text == '\0') return INVALID_VALUE;
    long long limit = negative ? -(long long)INT_MIN : INT_MAX;
    long long value = 0;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        int digit;
        if (text[i] >= '0' && text[i] <= '9') digit = text[i] - '0';
        else if (!upper && text[i] >= 'a' && text[i] <= 'z') digit = text[i] - 'a' + 10;
        else if (upper && text[i] >= 'A' && text[i] <= 'Z') digit = text[i] - 'A' + 10;
        else return INVALID_VALUE;
        if (digit >= base) return INVALID_VALUE;
        if (value > (limit - digit) / base)
            return SIZE_ERROR;
        value = value * base + digit;
    }
    if (negative) value = -value;
    *result = (int)value;
    return OK;
}

Status memory_dump(Text *buffer, const void *value, const size_t size)
{
    if (buffer == NULL || value == NULL || size == 0) return INVALID_VALUE;
    const unsigned char *bytes = value;
    char bits[CHAR_BIT];
    Status status = OK;
    for (size_t i = 0; i < size && status == OK; ++i) {
        if (i != 0) status = add_text(buffer, " ", 1);
        for (int bit = 0; bit < CHAR_BIT; ++bit)
            bits[bit] = (char)('0' + ((bytes[i] >> (CHAR_BIT - 1 - bit)) & 1));
        if (status == OK) status = add_text(buffer, bits, sizeof(bits));
    }
    return status;
}

Status extra_format(Text *buffer, const char *format, va_list *args)
{
    if (buffer == NULL || format == NULL || args == NULL) return INVALID_VALUE;
    if (strncmp(format, "Ro", 2) == 0)
        return roman_number(buffer, va_arg(*args, int));
    if (strncmp(format, "Zr", 2) == 0)
        return fibonacci_number(buffer, va_arg(*args, unsigned int));
    if (strncmp(format, "Cv", 2) == 0 || strncmp(format, "CV", 2) == 0) {
        int number = va_arg(*args, int);
        int base = va_arg(*args, int);
        return number_to_base(buffer, number, base, format[1] == 'V');
    }
    if (strncmp(format, "to", 2) == 0 || strncmp(format, "TO", 2) == 0) {
        const char *text = va_arg(*args, const char *);
        int base = va_arg(*args, int);
        int number;
        Status status = number_from_base(text, base, format[0] == 'T', &number);
        if (status != OK) return status;
        return number_to_base(buffer, number, 10, 0);
    }
    if (strncmp(format, "mi", 2) == 0) {
        int32_t number = va_arg(*args, int32_t);
        return memory_dump(buffer, &number, sizeof(number));
    }
    if (strncmp(format, "mu", 2) == 0) {
        uint32_t number = va_arg(*args, uint32_t);
        return memory_dump(buffer, &number, sizeof(number));
    }
    if (strncmp(format, "md", 2) == 0) {
        double number = va_arg(*args, double);
        return memory_dump(buffer, &number, sizeof(number));
    }
    if (strncmp(format, "mf", 2) == 0) {
        double number = va_arg(*args, double);
        if (isfinite(number) && (number > FLT_MAX || number < -FLT_MAX))
            return INVALID_VALUE;
        float value = (float)number;
        return memory_dump(buffer, &value, sizeof(value));
    }
    return NOT_EXTRA_FORMAT;
}
Status read_decimal(const char **position, int *number)
{
    if (position == NULL || *position == NULL || number == NULL) return INVALID_VALUE;
    *number = 0;
    while (**position >= '0' && **position <= '9') {
        int digit = **position - '0';
        if (*number > (INT_MAX - digit) / 10) return SIZE_ERROR;
        *number = *number * 10 + digit;
        ++*position;
    }
    return OK;
}

Status read_format(const char **position, va_list *args, Format *format)
{
    if (position == NULL || *position == NULL || args == NULL || format == NULL || **position != '%')
        return INVALID_FORMAT;
    *format = (Format){NORMAL, '\0', NULL, 0, -1};
    size_t size = strlen(*position);
    if (size > SIZE_MAX - 5) return SIZE_ERROR;
    format->text = malloc(size + 5);
    if (format->text == NULL) return MEMORY_ERROR;
    const char *p = *position + 1;
    const char *start = p;
    size_t used = 0;
    format->text[used++] = '%';
    while (*p != '\0' && strchr("-+ #0", *p) != NULL)
        format->text[used++] = *p++;

    Status status = OK;
    if (*p == '*') {
        format->width = va_arg(*args, int);
        ++p;
    } else {
        status = read_decimal(&p, &format->width);
    }
    if (status != OK) return status;
    if (*p == '.') {
        ++p;
        if (*p == '*') {
            format->precision = va_arg(*args, int);
            ++p;
        } else {
            status = read_decimal(&p, &format->precision);
        }
    }
    if (status != OK) return status;
    int options = p != start;
    format->text[used++] = '*';
    format->text[used++] = '.';
    format->text[used++] = '*';

    const char *length_start = p;
    if (*p == 'h') {
        ++p;
        format->length = H;
        if (*p == 'h') { ++p; format->length = HH; }
    } else if (*p == 'l') {
        ++p;
        format->length = L;
        if (*p == 'l') { ++p; format->length = LL; }
    } else if (*p == 'j') { ++p; format->length = J; }
    else if (*p == 'z') { ++p; format->length = Z; }
    else if (*p == 't') { ++p; format->length = T; }
    else if (*p == 'L') { ++p; format->length = BIG_L; }
    while (length_start != p) format->text[used++] = *length_start++;
    if (*p == '\0') return INVALID_FORMAT;
    format->conversion = *p;
    if (strchr("diouxXn", *p) != NULL) {
        if (format->length == BIG_L || (*p == 'n' && options)) return INVALID_FORMAT;
    } else if (strchr("aAeEfFgG", *p) != NULL) {
        if (format->length != NORMAL && format->length != L && format->length != BIG_L)
            return INVALID_FORMAT;
    } else if (*p == 'c' || *p == 's') {
        if (format->length != NORMAL && format->length != L) return INVALID_FORMAT;
    } else if (*p != 'p' || format->length != NORMAL) {
        return INVALID_FORMAT;
    }
    format->text[used++] = *p;
    format->text[used] = '\0';
    *position = p + 1;
    return OK;
}
Status store_count(va_list *args, const Length length, const size_t count)
{
    if (args == NULL || count > INT_MAX) return INVALID_VALUE;
    if ((length == HH && count > SCHAR_MAX) || (length == H && count > SHRT_MAX))
        return SIZE_ERROR;
    void *address;
    switch (length) {
        case HH: address = va_arg(*args, signed char *); break;
        case H: address = va_arg(*args, short *); break;
        case NORMAL: address = va_arg(*args, int *); break;
        case L: address = va_arg(*args, long *); break;
        case LL: address = va_arg(*args, long long *); break;
        case J: address = va_arg(*args, intmax_t *); break;
        case Z: address = va_arg(*args, ptrdiff_t *); break;
        case T: address = va_arg(*args, ptrdiff_t *); break;
        default: return INVALID_FORMAT;
    }
    if (address == NULL) return INVALID_VALUE;
    switch (length) {
        case HH: *(signed char *)address = (signed char)count; break;
        case H: *(short *)address = (short)count; break;
        case NORMAL: *(int *)address = (int)count; break;
        case L: *(long *)address = (long)count; break;
        case LL: *(long long *)address = (long long)count; break;
        case J: *(intmax_t *)address = (intmax_t)count; break;
        case Z: *(ptrdiff_t *)address = (ptrdiff_t)count; break;
        case T: *(ptrdiff_t *)address = (ptrdiff_t)count; break;
        default: return INVALID_FORMAT;
    }
    return OK;
}
Status add_standard(Text *buffer, const char *format, ...)
{
    if (buffer == NULL || format == NULL) return INVALID_VALUE;
    va_list args, copy;
    va_start(args, format);
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    Status status = needed < 0 ? INVALID_VALUE : grow_text(buffer, (size_t)needed);
    if (status == OK) {
        int written = vsnprintf(buffer->text + buffer->size, (size_t)needed + 1, format, args);
        if (written != needed) status = INVALID_VALUE;
        else buffer->size += (size_t)written;
    }
    va_end(args);
    return status;
}

Status read_value(Text *buffer, const Format *format, va_list *args)
{
    if (buffer == NULL || format == NULL || format->text == NULL || args == NULL)
        return INVALID_VALUE;
    const char *text = format->text;
    int width = format->width, precision = format->precision;
    char letter = format->conversion;
    if (letter == 'n') return store_count(args, format->length, buffer->size);
    if (letter == 'd' || letter == 'i') {
        switch (format->length) {
            case L: return add_standard(buffer, text, width, precision, va_arg(*args, long));
            case LL: return add_standard(buffer, text, width, precision, va_arg(*args, long long));
            case J: return add_standard(buffer, text, width, precision, va_arg(*args, intmax_t));
            case Z:
            case T: return add_standard(buffer, text, width, precision, va_arg(*args, ptrdiff_t));
            default: return add_standard(buffer, text, width, precision, va_arg(*args, int));
        }
    }
    if (strchr("ouxX", letter) != NULL) {
        switch (format->length) {
            case L: return add_standard(buffer, text, width, precision, va_arg(*args, unsigned long));
            case LL: return add_standard(buffer, text, width, precision, va_arg(*args, unsigned long long));
            case J: return add_standard(buffer, text, width, precision, va_arg(*args, uintmax_t));
            case Z:
            case T: return add_standard(buffer, text, width, precision, va_arg(*args, size_t));
            default: return add_standard(buffer, text, width, precision, va_arg(*args, unsigned int));
        }
    }
    if (strchr("aAeEfFgG", letter) != NULL) {
        if (format->length == BIG_L)
            return add_standard(buffer, text, width, precision, va_arg(*args, long double));
        return add_standard(buffer, text, width, precision, va_arg(*args, double));
    }
    if (letter == 's' && format->length == L) {
        const wchar_t *value = va_arg(*args, const wchar_t *);
        if (value == NULL) return INVALID_VALUE;
        return add_standard(buffer, text, width, precision, value);
    }
    if (letter == 's') {
        const char *value = va_arg(*args, const char *);
        if (value == NULL) return INVALID_VALUE;
        return add_standard(buffer, text, width, precision, value);
    }
    if (letter == 'p') return add_standard(buffer, text, width, precision, va_arg(*args, void *));
    if (letter == 'c' && format->length == L) {
#if WINT_MAX <= INT_MAX
        wint_t value = (wint_t)va_arg(*args, int);
#elif WINT_MAX <= UINT_MAX
        wint_t value = va_arg(*args, unsigned int);
#else
        wint_t value = va_arg(*args, wint_t);
#endif
        return add_standard(buffer, text, width, precision, value);
    }
    if (letter == 'c') return add_standard(buffer, text, width, precision, va_arg(*args, int));
    return INVALID_FORMAT;
}

Status standard_format(Text *buffer, const char **position, va_list *args)
{
    if (buffer == NULL || position == NULL || args == NULL) return INVALID_VALUE;
    Format format = {0};
    Status status = read_format(position, args, &format);
    if (status == OK) status = read_value(buffer, &format, args);
    free(format.text);
    return status;
}
Status make_text(Text *buffer, const char *format, va_list *args)
{
    if (buffer == NULL || format == NULL || args == NULL) return INVALID_VALUE;
    Status status = OK;
    const char *p = format;
    while (*p != '\0' && status == OK) {
        if (*p != '%') {
            const char *start = p;
            while (*p != '\0' && *p != '%') ++p;
            status = add_text(buffer, start, (size_t)(p - start));
        } else if (p[1] == '%') {
            status = add_text(buffer, "%", 1);
            p += 2;
        } else {
            status = extra_format(buffer, p + 1, args);
            if (status == NOT_EXTRA_FORMAT)
                status = standard_format(buffer, &p, args);
            else
                p += 3;
        }
    }
    return status == OK ? grow_text(buffer, 0) : status;
}

int overfprintf(FILE *stream, const char *format, ...)
{
    if (stream == NULL || format == NULL) return -1;
    Text buffer = {0};
    va_list args;
    va_start(args, format);
    Status status = make_text(&buffer, format, &args);
    va_end(args);
    int result = -1;
    if (status == OK && fwrite(buffer.text, 1, buffer.size, stream) == buffer.size && !ferror(stream))
        result = (int)buffer.size;
    free(buffer.text);
    return result;
}

int oversprintf(char *destination, const char *format, ...)
{
    if (destination == NULL || format == NULL) return -1;
    Text buffer = {0};
    va_list args;
    va_start(args, format);
    Status status = make_text(&buffer, format, &args);
    va_end(args);
    int result = -1;
    if (status == OK) {
        memcpy(destination, buffer.text, buffer.size + 1);
        result = (int)buffer.size;
    }
    free(buffer.text);
    return result;
}

int main(void)
{
    int written = overfprintf(stdout,
        "overfprintf:\n"
        "Standard: %s %d %.2f %%\n"
        "Roman: %Ro\n"
        "Zeckendorf: %Zr\n"
        "Base conversion: %Cv %CV\n"
        "Decimal: %to %TO\n"
        "int32: %mi\n"
        "uint32: %mu\n"
        "double: %md\n"
        "float: %mf\n",
        "test", 42, 3.14, 1994, 10u, -255, 16, 255, 16,
        "ff", 16, "FF", 16, (int32_t)-5, (uint32_t)5, 1.5, 1.5f);
    if (written < 0) {
        fprintf(stderr, "Formatting or output error.\n");
        return EXIT_FAILURE;
    }

    char text[2048];
    written = oversprintf(text,
        "\noversprintf:\n%Ro | %Zr | %Cv | %CV | %to | %TO\n"
        "%mi\n%mu\n%md\n%mf\n",
        2026, 10u, 255, 16, 255, 16, "ff", 16, "FF", 16,
        (int32_t)-5, (uint32_t)5, 1.5, 1.5f);
    if (written < 0 || fputs(text, stdout) == EOF || fflush(stdout) == EOF) {
        fprintf(stderr, "Formatting or output error.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
