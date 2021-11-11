/*
 * Copyright (c) 2021 Davide Paro
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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

static bool prep_memory(Instance *instance) {

    instance->positions =
        calloc(instance->num_customers + 1, sizeof(*instance->positions));

    instance->demands =
        calloc(instance->num_customers + 1, sizeof(*instance->demands));

    instance->duals =
        calloc(instance->num_customers + 1, sizeof(*instance->duals));

    return instance->positions && instance->demands && instance->duals;
}

static bool parse_file(Instance *instance, FILE *filehandle,
                       const char *filepath) {
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

typedef struct VrplibParser {
    const char *filename;
    char *base;
    char *at;
    int32_t curline;
    int32_t size;
} VrplibParser;

static inline int32_t parser_remainder_size(const VrplibParser *p) {
    return p->base - p->at + p->size;
}

static inline void parser_adv(VrplibParser *p, int32_t amt) {
    assert(amt != 0);
    amt = MIN(amt, parser_remainder_size(p));
    p->at += amt;
}

static inline bool parser_is_eof(const VrplibParser *p) {
    return parser_remainder_size(p) == 0;
}

static inline void parser_eat_whitespaces(VrplibParser *p) {
    while (!parser_is_eof(p) && (*p->at == ' ' || *p->at == '\t')) {
        parser_adv(p, 1);
    }
}

static inline void parser_eat_newline(VrplibParser *p) {
    parser_eat_whitespaces(p);

    while (!parser_is_eof(p) && (*p->at == '\r' || *p->at == '\n')) {
        if (*p->at == '\r') {
            parser_adv(p, 1);
            if (!parser_is_eof(p) && (*p->at == '\n')) {
                parser_adv(p, 1);
            }
            p->curline += 1;
        }
    }
}

static inline bool parser_match_newline(VrplibParser *p) {
    parser_eat_whitespaces(p);
    int32_t cache_curline = p->curline;
    parser_eat_newline(p);
    return p->curline > cache_curline;
}

static inline bool parser_match_string(VrplibParser *p, char *string) {
    parser_eat_whitespaces(p);
    int32_t len = MIN(parser_remainder_size(p), (int32_t)strlen(string));
    bool result = (0 == strncmp(string, p->at, len));

    if (result) {
        parser_adv(p, len);
    }
    parser_eat_whitespaces(p);
    return result;
}

ATTRIB_PRINTF(2, 3)
static void parse_error(VrplibParser *p, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s:%d: error: ", p->filename, p->curline);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static char *parse_hdr_field(VrplibParser *p, char *fieldname) {
    if (parser_match_string(p, fieldname)) {
        parser_eat_whitespaces(p);

        if (parser_match_string(p, ":")) {
            char *at = p->at;

            while (!parser_is_eof(p)) {
                bool is_newline = (*p->at == '\r' || *p->at == '\n');
                if (is_newline) {
                    break;
                }
                parser_adv(p, 1);
            };
            size_t namesize = (p->at - 1) - at;
            char *name = malloc(namesize + 1);
            name[namesize] = 0;
            strncpy(name, at, namesize);
            return name;
        } else {
            return NULL;
        }
    }

    return NULL;
}

static bool parse_vrplib_hdr(VrplibParser *p) {
    bool result = true;
    while (!parser_is_eof(p)) {
        bool header_terminated = parser_match_string(p, "NODE_COORD_SECTION") &&
                                 parser_match_newline(p);
        if (header_terminated) {
            break;
        }

        char *value = NULL;
        if ((value = parse_hdr_field(p, "NAME"))) {
            // TODO
        } else if ((value = parse_hdr_field(p, "COMMENT"))) {
            // TODO
        } else if ((value = parse_hdr_field(p, "TYPE"))) {
            if (0 != strcmp(value, "CVRP")) {
                parse_error(p,
                            "only CVRP type is supported. Found `%s` "
                            "instead",
                            value);
                result = false;
            }
        } else if ((value = parse_hdr_field(p, "DIMENSION"))) {
            // TODO
        } else if ((value = parse_hdr_field(p, "EDGE_WEIGHT_TYPE"))) {
            if (0 != strcmp(value, "EUC_2D")) {
                parse_error(
                    p,
                    "only EUC_2D edge weight type is supported. Found `%s` "
                    "instead",
                    value);
                result = false;
            }
        } else if ((value = parse_hdr_field(p, "CAPACITY"))) {
            // TODO
        } else {
            parse_error(p, "while parsing header encountered invalid input");
            result = false;
        }

        if (value) {
            free(value);
        }

        if (result == false) {
            goto terminate;
        }
    }

terminate:
    return result;
}

static bool parse_vrplib_nodecoord_section(VrplibParser *p) {}

static bool parse_vrplib_demand_section(VrplibParser *p) {}

static bool parse_vrplib_reducedcost_section(VrplibParser *p) {}

static bool parse_vrplib_profit_section(VrplibParser *p) {}

static bool parse_vrplib_depot_section(VrplibParser *p) {}

bool parse_vrp_file(Instance *instance, FILE *filehandle,
                    const char *filepath) {

    bool result = true;
    size_t filesize = get_file_size(filehandle);

    // + 1 for null termination
    char *buffer = malloc(filesize + 1);
    if (!buffer) {
        result = false;
        goto terminate;
    }

    size_t readamt = fread(buffer, 1, filesize, filehandle);
    if (readamt != filesize) {
        result = false;
        goto terminate;
    }

    // NULL terminate the buffer
    buffer[filesize] = 0;
    VrplibParser parser = {0};
    parser.filename = filepath;
    parser.base = buffer;
    parser.at = buffer;
    parser.size = filesize;

    // First extract the header informations
    bool hdr_parse_result = parse_vrplib_hdr(&parser);

terminate:
    if (buffer) {
        free(buffer);
    }
    return result;
}

Instance parse(const char *filepath) {
    FILE *filehandle = fopen(filepath, "r");
    Instance result = {0};
    result.rounding_strat = CPTP_DIST_ROUND;
    bool success = false;

    if (filehandle) {
        success = parse_file(&result, filehandle, filepath);
        fclose(filehandle);
    }

    if (success) {
        instance_set_name(&result, filepath);
        return result;
    } else {
        instance_destroy(&result);
        return result;
    }
}
