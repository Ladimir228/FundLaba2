#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

typedef enum {
    OK, INVALID_ARGUMENT, MEMORY_ERROR, SIZE_ERROR, FILE_ERROR, OUTPUT_ERROR
} Status;

typedef struct {
    int file;
    size_t line;
    size_t column;
} Match;

Status read_file(const char *path, char **text, size_t *length)
{
    if (path == NULL || *path == '\0' || text == NULL || length == NULL)
        return INVALID_ARGUMENT;
    *text = NULL;
    *length = 0;
    FILE *file = fopen(path, "r");
    if (file == NULL) return FILE_ERROR;

    size_t capacity = 0;
    Status status = OK;
    int ch;
    while ((ch = fgetc(file)) != EOF) {
        if (*length == capacity) {
            if (capacity > SIZE_MAX / 2) {
                status = SIZE_ERROR;
                break;
            }
            size_t new_capacity = capacity == 0 ? 128 : capacity * 2;
            char *temp = realloc(*text, new_capacity);
            if (temp == NULL) {
                status = MEMORY_ERROR;
                break;
            }
            *text = temp;
            capacity = new_capacity;
        }
        (*text)[(*length)++] = (char)ch;
    }
    if (ferror(file)) status = FILE_ERROR;
    if (fclose(file) == EOF) status = FILE_ERROR;
    return status;
}

Status find_in_files(const char *substring, const int file_count,
                     Match **matches, size_t *count, ...)
{
    if (matches == NULL || count == NULL) return INVALID_ARGUMENT;
    *matches = NULL;
    *count = 0;
    if (substring == NULL || *substring == '\0' || file_count <= 0)
        return INVALID_ARGUMENT;

    const size_t pattern_length = strlen(substring);
    Status status = OK;
    va_list args;
    va_start(args, count);
    for (int file = 0; file < file_count && status == OK; ++file) {
        const char *path = va_arg(args, const char *);
        char *text = NULL;
        size_t length = 0;
        status = read_file(path, &text, &length);
        size_t line = 1, column = 1;
        for (size_t i = 0; status == OK && i < length; ++i) {
            size_t j = 0;
            while (j < pattern_length && j < length - i &&
                   text[i + j] == substring[j])
                ++j;
            if (j == pattern_length) {
                if (*count >= SIZE_MAX / sizeof(Match)) {
                    status = SIZE_ERROR;
                    break;
                }
                Match *temp = realloc(*matches, (*count + 1) * sizeof(Match));
                if (temp == NULL) {
                    status = MEMORY_ERROR;
                    break;
                }
                *matches = temp;
                (*matches)[*count] = (Match){file, line, column};
                ++*count;
            }
            if (text[i] == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
        free(text);
    }
    va_end(args);
    return status;
}

Status print_results(const Match *matches, const size_t count,
                     const char *const paths[], const int file_count)
{
    if ((matches == NULL && count != 0) || paths == NULL || file_count <= 0)
        return INVALID_ARGUMENT;
    for (int file = 0; file < file_count; ++file) {
        if (paths[file] == NULL || *paths[file] == '\0') return INVALID_ARGUMENT;
        if (printf("File: %s\n", paths[file]) < 0) return OUTPUT_ERROR;
        int found = 0;
        for (size_t i = 0; i < count; ++i) {
            if (matches[i].file == file) {
                if (printf("  Line %zu, column %zu\n", matches[i].line,
                           matches[i].column) < 0)
                    return OUTPUT_ERROR;
                found = 1;
            }
        }
        if (!found && printf("  No matches\n") < 0) return OUTPUT_ERROR;
    }
    return fflush(stdout) == EOF ? OUTPUT_ERROR : OK;
}

int main(int argc, char *argv[])
{
    if (argc != 1 && argc != 4) {
        fprintf(stderr, "Usage: %s [substring file1 file2]\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *substring = "aba";
    const char *paths[2] = {"tests/input3_1.txt", "tests/input3_2.txt"};
    if (argc == 4) {
        substring = argv[1];
        paths[0] = argv[2];
        paths[1] = argv[3];
    }

    Match *matches = NULL;
    size_t count = 0;
    Status status = find_in_files(substring, 2, &matches, &count, paths[0], paths[1]);
    if (status == OK) status = print_results(matches, count, paths, 2);
    free(matches);

    switch (status) {
        case OK: return EXIT_SUCCESS;
        case INVALID_ARGUMENT: fprintf(stderr, "Invalid argument or empty substring.\n"); break;
        case MEMORY_ERROR: fprintf(stderr, "Not enough memory.\n"); break;
        case SIZE_ERROR: fprintf(stderr, "Data is too large.\n"); break;
        case FILE_ERROR: fprintf(stderr, "Cannot open, read or close an input file.\n"); break;
        case OUTPUT_ERROR: fprintf(stderr, "Cannot write output.\n"); break;
    }
    return EXIT_FAILURE;
}
