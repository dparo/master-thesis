#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <ctype.h>

static bool expect_newline(FILE *filehandle, const char *filepath,
                           int32_t line_cnt) {
    char c = ' ';
    do {
        c = fgetc(filehandle);
    } while (c == ' ' || c == '\t');

    if (c != '\r' && c != '\n') {
        fprintf(stderr,
                "%s: Parsing error at line %d. Expected newline termination "
                "(`\\n`). "
                "Found `%c` instead.\n",
                filepath, line_cnt, c);
        return false;
    }

    do {
        c = fgetc(filehandle);
    } while (c == '\n');

    ungetc(c, filehandle);
    return true;
}

static bool parse_hdr(Instance *instance, FILE *filehandle,
                      const char *filepath, int32_t *line_cnt) {

    int32_t num_read = fscanf(filehandle, "%d %d %lf", &instance->num_customers,
                              &instance->num_vehicles, &instance->vehicle_cap);
    *line_cnt = *line_cnt + 1;
    if (num_read != 3) {
        fprintf(stderr, "%s: Parsing error at line %d\n", filepath, *line_cnt);
        return false;
    }
    if (!expect_newline(filehandle, filepath, *line_cnt)) {
        return false;
    }
    return true;
}

static bool parse_line(Instance *instance, FILE *filehandle,
                       const char *filepath, int32_t *line_cnt) {

    int32_t idx;
    double x, y;
    double demand;
    double dual;

    int32_t num_read =
        fscanf(filehandle, "%d %lf %lf %lf %lf", &idx, &x, &y, &demand, &dual);

    *line_cnt = *line_cnt + 1;
    int32_t expected_idx = *line_cnt - 2;

    if (num_read == EOF) {
        return false;
    }

    if (num_read != 5) {
        fprintf(stderr, "%s: Parsing error at line %d\n", filepath, *line_cnt);
        return false;
    }

    if (idx != expected_idx) {
        fprintf(stderr,
                "%s: Parsing error at line %d. Expected idx %d but found %d\n",
                filepath, *line_cnt, expected_idx, idx);
        return false;
    }

    if (!expect_newline(filehandle, filepath, *line_cnt)) {
        return false;
    }

    instance->positions[idx] = (Vec2d){x, y};
    instance->demands[idx] = demand;
    instance->duals[idx] = dual;

    return true;
}

bool prep_memory(Instance *instance) {

    instance->positions =
        calloc(instance->num_customers + 1, sizeof(*instance->positions));

    instance->demands =
        calloc(instance->num_customers + 1, sizeof(*instance->demands));

    instance->duals =
        calloc(instance->num_customers + 1, sizeof(*instance->duals));

    return instance->positions && instance->demands && instance->duals;
}

bool parse_file(Instance *instance, FILE *filehandle, const char *filepath) {
    int32_t line_cnt = 0;
    int32_t found_num_customers = 0;

    if (filehandle == NULL) {
        fprintf(stderr, "%s: Failed to open file\n", filepath);
        return false;
    }

    if (!parse_hdr(instance, filehandle, filepath, &line_cnt)) {
        return false;
    }

    if (!prep_memory(instance)) {
        fprintf(stderr, "%s: Failed to allocate memory for %d customers\n",
                filepath, instance->num_customers);
        return false;
    }

    while (parse_line(instance, filehandle, filepath, &line_cnt))
        ;

    found_num_customers = line_cnt - 3;
    if (found_num_customers != instance->num_customers) {
        fprintf(stderr, "%s: Expected %d customers but found %d\n", filepath,
                instance->num_customers, found_num_customers);
        return false;
    }

    return true;
}

Instance parse(const char *filepath) {
    FILE *filehandle = fopen(filepath, "r");
    Instance result = {0};
    bool success = false;

    if (filehandle) {
        success = parse_file(&result, filehandle, filepath);
        fclose(filehandle);
    }

    if (success) {
        return result;
    } else {
        instance_destroy(&result);
        return result;
    }
}
