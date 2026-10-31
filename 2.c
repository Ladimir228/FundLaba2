#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <math.h>

typedef enum { OK, END, DATA_ERROR, MEMORY_ERROR, FILE_ERROR } Status;

typedef struct {
    unsigned int id;
    char *name;
    char *surname;
    double salary;
} Employee;

void free_employees(Employee *employees, const size_t count)
{
    if (employees == NULL) return;
    for (size_t i = 0; i < count; ++i) {
        free(employees[i].name);
        free(employees[i].surname);
    }
    free(employees);
}

Status read_line(FILE *file, char **result)
{
    if (file == NULL || result == NULL) return DATA_ERROR;
    *result = NULL;
    size_t size = 128, length = 0;
    char *line = malloc(size);
    if (line == NULL) return MEMORY_ERROR;

    int ch;
    Status status = OK;
    while ((ch = fgetc(file)) != EOF && ch != '\n') {
        if (ch == '\0') { status = DATA_ERROR; break; }
        if (length == size - 1) {
            if (size > SIZE_MAX / 2) { status = MEMORY_ERROR; break; }
            char *temp = realloc(line, size * 2);
            if (temp == NULL) { status = MEMORY_ERROR; break; }
            line = temp;
            size *= 2;
        }
        line[length++] = (char)ch;
    }
    if (ferror(file)) status = FILE_ERROR;
    if (status == OK && ch == EOF && length == 0) status = END;
    if (status != OK) {
        free(line);
        return status;
    }
    line[length] = '\0';
    *result = line;
    return OK;
}

Status parse_employee(char *line, Employee *result)
{
    if (line == NULL || result == NULL) return DATA_ERROR;

    char *field[4];
    char *p = line;
    for (int i = 0; i < 4; ++i) {
        while (isspace((unsigned char)*p)) ++p;
        if (*p == '\0') return DATA_ERROR;
        field[i] = p;
        while (*p != '\0' && !isspace((unsigned char)*p)) ++p;
        if (*p != '\0') *p++ = '\0';
    }
    while (isspace((unsigned char)*p)) ++p;
    if (*p != '\0') return DATA_ERROR;

    unsigned int id = 0;
    for (size_t i = 0; field[0][i] != '\0'; ++i) {
        char ch = field[0][i];
        if (ch < '0' || ch > '9') return DATA_ERROR;
        unsigned int digit = (unsigned int)(ch - '0');
        if (id > (UINT_MAX - digit) / 10) return DATA_ERROR;
        id = id * 10 + digit;
    }
    for (int i = 1; i <= 2; ++i) {
        for (size_t j = 0; field[i][j] != '\0'; ++j) {
            char ch = field[i][j];
            if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')))
                return DATA_ERROR;
        }
    }

    if (strspn(field[3], "+-0123456789.eE") != strlen(field[3]))
        return DATA_ERROR;
    char *end;
    double salary = strtod(field[3], &end);
    if (end == field[3] || *end != '\0' || !isfinite(salary) || salary < 0.0)
        return DATA_ERROR;
    int nonzero = 0;
    for (p = field[3]; *p != '\0' && *p != 'e' && *p != 'E'; ++p)
        if (*p >= '1' && *p <= '9') nonzero = 1;
    if (nonzero && (salary == 0.0 || field[3][0] == '-'))
        return DATA_ERROR;

    char *name = malloc(strlen(field[1]) + 1);
    char *surname = malloc(strlen(field[2]) + 1);
    if (name == NULL || surname == NULL) {
        free(name);
        free(surname);
        return MEMORY_ERROR;
    }
    strcpy(name, field[1]);
    strcpy(surname, field[2]);
    *result = (Employee){id, name, surname, salary};
    return OK;
}

Status read_employees(FILE *file, Employee **employees, size_t *count)
{
    if (file == NULL || employees == NULL || count == NULL) return DATA_ERROR;
    *employees = NULL;
    *count = 0;
    for (;;) {
        char *line = NULL;
        Status status = read_line(file, &line);
        if (status == END) return OK;
        if (status != OK) return status;

        if (*count >= SIZE_MAX / sizeof(Employee)) {
            free(line);
            return MEMORY_ERROR;
        }
        Employee *temp = realloc(*employees, (*count + 1) * sizeof(Employee));
        if (temp == NULL) {
            free(line);
            return MEMORY_ERROR;
        }
        *employees = temp;
        status = parse_employee(line, &(*employees)[*count]);
        free(line);
        if (status != OK) return status;
        ++*count;
    }
}

Status compare_employees(const Employee *a, const Employee *b,
                         const double eps, int *result)
{
    if (a == NULL || b == NULL || result == NULL || !isfinite(eps) || eps <= 0.0 ||
        a->name == NULL || b->name == NULL || a->surname == NULL || b->surname == NULL ||
        !isfinite(a->salary) || !isfinite(b->salary)) return DATA_ERROR;

    double difference = a->salary - b->salary;
    if (difference < -eps) { *result = -1; return OK; }
    if (difference > eps) { *result = 1; return OK; }
    if (a->salary < b->salary) { *result = -1; return OK; }
    if (a->salary > b->salary) { *result = 1; return OK; }

    int order = strcmp(a->surname, b->surname);
    if (order == 0) order = strcmp(a->name, b->name);
    if (order == 0) order = (a->id > b->id) - (a->id < b->id);
    *result = (order > 0) - (order < 0);
    return OK;
}

int ascending(const void *left, const void *right)
{
    int result = 0;
    Status status = compare_employees(left, right, 1e-9, &result);
    return status == OK ? result : 0;
}

int descending(const void *left, const void *right)
{
    return -ascending(left, right);
}

Status write_employees(FILE *file, const Employee *employees, const size_t count)
{
    if (file == NULL || (count != 0 && employees == NULL)) return DATA_ERROR;
    for (size_t i = 0; i < count; ++i) {
        if (employees[i].name == NULL || employees[i].surname == NULL ||
            !isfinite(employees[i].salary) || employees[i].salary < 0.0)
            return DATA_ERROR;
        if (fprintf(file, "%u %s %s %.17g\n", employees[i].id,
                    employees[i].name, employees[i].surname, employees[i].salary) < 0)
            return FILE_ERROR;
    }
    return OK;
}

int main(int argc, char *argv[])
{
    if (argc != 4 || (argv[2][0] != '-' && argv[2][0] != '/') ||
        (argv[2][1] != 'a' && argv[2][1] != 'd') || argv[2][2] != '\0') {
        fprintf(stderr, "Usage: %s input.txt <-a|-d|/a|/d> output.txt\n", argv[0]);
        return 1;
    }
    FILE *input = fopen(argv[1], "r");
    if (input == NULL) {
        fprintf(stderr, "Cannot open input file.\n");
        return 1;
    }

    Employee *employees = NULL;
    size_t count = 0;
    Status status = read_employees(input, &employees, &count);
    if (fclose(input) == EOF) status = FILE_ERROR;

    if (status == OK && count > 1) {
        if (argv[2][1] == 'a')
            qsort(employees, count, sizeof(Employee), ascending);
        else
            qsort(employees, count, sizeof(Employee), descending);
    }
    if (status == OK) {
        FILE *output = fopen(argv[3], "w");
        if (output == NULL) status = FILE_ERROR;
        else {
            status = write_employees(output, employees, count);
            if (fclose(output) == EOF) status = FILE_ERROR;
        }
    }
    free_employees(employees, count);

    switch (status) {
        case OK: return 0;
        case DATA_ERROR: fprintf(stderr, "Invalid data. Expected: id name surname salary.\n"); break;
        case MEMORY_ERROR: fprintf(stderr, "Not enough memory or data is too large.\n"); break;
        case FILE_ERROR: fprintf(stderr, "File operation failed.\n"); break;
        default: fprintf(stderr, "Unexpected error.\n"); break;
    }
    return 1;
}
