#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

typedef enum {
    OK, INVALID_DATA, MEMORY_ERROR, SIZE_ERROR, INPUT_ERROR, OUTPUT_ERROR
} Status;

typedef struct {
    char *name;
    size_t size;
    size_t importance;
} File;

typedef struct {
    File *files;
    size_t count;
    size_t total_size;
    size_t total_importance;
} Backup;

void free_files(File *files, const size_t count)
{
    if (files == NULL) return;
    for (size_t i = 0; i < count; ++i)
        free(files[i].name);
    free(files);
}

Status parse_number(const char *text, size_t *number)
{
    if (text == NULL || *text == '\0' || number == NULL) return INVALID_DATA;
    size_t value = 0;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (text[i] < '0' || text[i] > '9') return INVALID_DATA;
        size_t digit = (size_t)(text[i] - '0');
        if (value > (SIZE_MAX - digit) / 10) return SIZE_ERROR;
        value = value * 10 + digit;
    }
    *number = value;
    return OK;
}

Status read_record(FILE *input, char **line, char *fields[], const int count)
{
    if (input == NULL || line == NULL || fields == NULL || count <= 0)
        return INVALID_DATA;
    *line = NULL;
    size_t capacity = 128, length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) return MEMORY_ERROR;
    Status status = OK;
    int ch;
    while ((ch = fgetc(input)) != EOF && ch != '\n') {
        if (ch == '\0') {
            status = INVALID_DATA;
            break;
        }
        if (length == capacity - 1) {
            if (capacity > SIZE_MAX / 2) {
                status = SIZE_ERROR;
                break;
            }
            char *temp = realloc(buffer, capacity * 2);
            if (temp == NULL) {
                status = MEMORY_ERROR;
                break;
            }
            buffer = temp;
            capacity *= 2;
        }
        buffer[length++] = (char)ch;
    }
    if (ferror(input)) status = INPUT_ERROR;
    buffer[length] = '\0';
    char *p = buffer;
    for (int i = 0; i < count && status == OK; ++i) {
        while (isspace((unsigned char)*p)) ++p;
        if (*p == '\0') {
            status = INVALID_DATA;
            break;
        }
        fields[i] = p;
        while (*p != '\0' && !isspace((unsigned char)*p)) ++p;
        if (*p != '\0') *p++ = '\0';
    }
    while (isspace((unsigned char)*p)) ++p;
    if (*p != '\0' && status == OK) status = INVALID_DATA;
    if (status != OK) {
        free(buffer);
        return status;
    }
    *line = buffer;
    return OK;
}

Status read_files(FILE *input, File **files, size_t *count, size_t *capacity)
{
    if (input == NULL || files == NULL || count == NULL || capacity == NULL)
        return INVALID_DATA;
    *files = NULL;
    *count = 0;
    *capacity = 0;
    char *line = NULL;
    char *fields[3];
    size_t n = 0;
    Status status = read_record(input, &line, fields, 2);
    if (status == OK) status = parse_number(fields[0], &n);
    if (status == OK) status = parse_number(fields[1], capacity);
    free(line);
    if (status != OK) return status;
    if (n == 0) return INVALID_DATA;
    if (n > SIZE_MAX / sizeof(File)) return SIZE_ERROR;
    *files = calloc(n, sizeof(File));
    if (*files == NULL) return MEMORY_ERROR;
    *count = n;

    for (size_t i = 0; i < n; ++i) {
        status = read_record(input, &line, fields, 3);
        if (status != OK) return status;
        (*files)[i].name = line;
        status = parse_number(fields[1], &(*files)[i].size);
        if (status == OK) status = parse_number(fields[2], &(*files)[i].importance);
        if (status != OK) return status;
        memmove(line, fields[0], strlen(fields[0]) + 1);
    }
    int ch;
    while ((ch = fgetc(input)) != EOF)
        if (!isspace((unsigned char)ch)) return INVALID_DATA;
    return ferror(input) ? INPUT_ERROR : OK;
}

Status select_files(const File *files, const size_t count,
                    const size_t capacity, Backup *result)
{
    if (result == NULL) return INVALID_DATA;
    *result = (Backup){0};
    if (files == NULL || count == 0) return INVALID_DATA;
    if (count > SIZE_MAX / sizeof(File)) return SIZE_ERROR;
    int fits = 0;
    for (size_t i = 0; i < count; ++i) {
        if (files[i].name == NULL || *files[i].name == '\0' || files[i].size == 0)
            return INVALID_DATA;
        for (size_t j = 0; files[i].name[j] != '\0'; ++j)
            if (isspace((unsigned char)files[i].name[j])) return INVALID_DATA;
        for (size_t j = 0; j < i; ++j)
            if (strcmp(files[i].name, files[j].name) == 0) return INVALID_DATA;
        if (files[i].size <= capacity) fits = 1;
    }
    if (!fits) return OK;
    if (capacity == SIZE_MAX) return SIZE_ERROR;
    const size_t width = capacity + 1;
    if (count + 1 > SIZE_MAX / width / sizeof(size_t)) return SIZE_ERROR;
    size_t *dp = calloc((count + 1) * width, sizeof(size_t));
    if (dp == NULL) return MEMORY_ERROR;

    Status status = OK;
    for (size_t i = 1; i <= count && status == OK; ++i) {
        const File *file = &files[i - 1];
        for (size_t c = 0; c <= capacity; ++c) {
            dp[i * width + c] = dp[(i - 1) * width + c];
            if (file->size <= c) {
                size_t previous = dp[(i - 1) * width + c - file->size];
                if (file->importance > SIZE_MAX - previous) {
                    status = SIZE_ERROR;
                    break;
                }
                size_t with_file = previous + file->importance;
                if (with_file > dp[i * width + c])
                    dp[i * width + c] = with_file;
            }
        }
    }
    if (status == OK && dp[count * width + capacity] != 0) {
        result->files = malloc(count * sizeof(File));
        if (result->files == NULL) status = MEMORY_ERROR;
        if (status == OK) {
            result->total_importance = dp[count * width + capacity];
            size_t c = capacity;
            for (size_t i = count; i > 0; --i) {
                if (dp[i * width + c] != dp[(i - 1) * width + c]) {
                    result->files[result->count++] = files[i - 1];
                    result->total_size += files[i - 1].size;
                    c -= files[i - 1].size;
                }
            }
            for (size_t i = 0; i < result->count / 2; ++i) {
                File temp = result->files[i];
                result->files[i] = result->files[result->count - 1 - i];
                result->files[result->count - 1 - i] = temp;
            }
        }
    }
    free(dp);
    return status;
}

Status print_backup(const Backup *backup)
{
    if (backup == NULL || (backup->count != 0 && backup->files == NULL))
        return INVALID_DATA;
    if (printf("TOTAL_SIZE %zu\nTOTAL_IMPORTANCE %zu\nFILES %zu\n",
               backup->total_size, backup->total_importance, backup->count) < 0)
        return OUTPUT_ERROR;
    for (size_t i = 0; i < backup->count; ++i) {
        if (backup->files[i].name == NULL) return INVALID_DATA;
        if (printf("%s\n", backup->files[i].name) < 0) return OUTPUT_ERROR;
    }
    return fflush(stdout) == EOF ? OUTPUT_ERROR : OK;
}

int main(int argc, char *argv[])
{
    if (argc > 2 || (argc == 2 && argv[1][0] == '\0')) {
        fprintf(stderr, "Usage: %s [input_file]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc == 2 && freopen(argv[1], "r", stdin) == NULL) {
        fprintf(stderr, "Cannot open input file.\n");
        return EXIT_FAILURE;
    }
    File *files = NULL;
    size_t count = 0, capacity = 0;
    Backup backup = {0};
    Status status = read_files(stdin, &files, &count, &capacity);
    if (argc == 2 && fclose(stdin) == EOF && status == OK) status = INPUT_ERROR;
    if (status == OK) status = select_files(files, count, capacity, &backup);
    if (status == OK) status = print_backup(&backup);
    free(backup.files);
    free_files(files, count);

    switch (status) {
        case OK: return EXIT_SUCCESS;
        case INVALID_DATA: fprintf(stderr, "Invalid input data or duplicate file name.\n"); break;
        case MEMORY_ERROR: fprintf(stderr, "Not enough memory.\n"); break;
        case SIZE_ERROR: fprintf(stderr, "Number or table is too large.\n"); break;
        case INPUT_ERROR: fprintf(stderr, "Cannot read input.\n"); break;
        case OUTPUT_ERROR: fprintf(stderr, "Cannot write output.\n"); break;
    }
    return EXIT_FAILURE;
}
