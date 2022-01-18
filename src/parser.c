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
#include "parsing-utils.h"
#include "core-utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <ctype.h>

static bool expect_newline(FILE *filehandle, const char *filepath,
                           int32_t line_cnt) {
    int c = ' ';
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

static bool parse_simplified_vrp_hdr(Instance *instance, FILE *filehandle,
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

static bool parse_simplified_vrp_line(Instance *instance, FILE *filehandle,
                                      const char *filepath, int32_t *line_cnt) {

    int32_t idx = 0;
    double x = 0.0, y = 0.0;
    double demand = 0.0;
    double dual = 0.0;

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
    instance->profits[idx] = dual;

    return true;
}

static bool prep_memory(Instance *instance) {

    instance->positions =
        calloc(instance->num_customers + 1, sizeof(*instance->positions));

    instance->demands =
        calloc(instance->num_customers + 1, sizeof(*instance->demands));

    instance->profits =
        calloc(instance->num_customers + 1, sizeof(*instance->profits));

    return instance->positions && instance->demands && instance->profits;
}

static bool parse_simplified_vrp_file(Instance *instance, FILE *filehandle,
                                      const char *filepath) {
    int32_t line_cnt = 0;
    int32_t found_num_customers = 0;

    if (filehandle == NULL) {
        fprintf(stderr, "%s: Failed to open file\n", filepath);
        return false;
    }

    if (!parse_simplified_vrp_hdr(instance, filehandle, filepath, &line_cnt)) {
        return false;
    }

    if (!prep_memory(instance)) {
        fprintf(stderr, "%s: Failed to allocate memory for %d customers\n",
                filepath, instance->num_customers);
        return false;
    }

    while (parse_simplified_vrp_line(instance, filehandle, filepath, &line_cnt))
        ;

    found_num_customers = line_cnt - 3;
    if (found_num_customers != instance->num_customers) {
        fprintf(stderr, "%s: Expected %d customers but found %d\n", filepath,
                instance->num_customers, found_num_customers);
        return false;
    }

    return true;
}

typedef enum EdgeWeightFormat {
    EDGE_WEIGHT_FORMAT_FUNCTION = 0,
    EDGE_WEIGHT_FORMAT_UPPER_ROW = 1,
} EdgeWeightFormat;

static struct {
    char *name;
    EdgeWeightFormat fmt;
} const G_supported_edgew_fmt[] = {
    {"EUC_2D", EDGE_WEIGHT_FORMAT_FUNCTION},
    {"UPPER_ROW", EDGE_WEIGHT_FORMAT_UPPER_ROW},
};

typedef struct VrplibParser {
    const char *filename;
    char *base;
    char *at;
    int32_t curline;
    size_t size;

    EdgeWeightFormat edgew_format;
} VrplibParser;

static inline size_t parser_remainder_size(const VrplibParser *p) {
    return p->base - p->at + p->size;
}

static inline void parser_adv(VrplibParser *p, size_t amt) {
    assert(amt != 0);
    amt = MIN(amt, parser_remainder_size(p));
    p->at += amt;
}

static inline bool parser_is_eof(const VrplibParser *p) {
    return parser_remainder_size(p) == 0;
}

static void parser_eat_whitespaces(VrplibParser *p) {
    while (!parser_is_eof(p) && (*p->at == ' ' || *p->at == '\t')) {
        parser_adv(p, 1);
    }
}

static void parser_eat_newline(VrplibParser *p) {
    parser_eat_whitespaces(p);

    while (!parser_is_eof(p) && (*p->at == '\r' || *p->at == '\n')) {
        if (*p->at == '\r') {
            parser_adv(p, 1);
            if (!parser_is_eof(p) && (*p->at == '\n')) {
                parser_adv(p, 1);
            }
            p->curline += 1;
        } else {
            assert(*p->at == '\n');
            p->curline += 1;
            parser_adv(p, 1);
        }
    }
}

static bool parser_match_newline(VrplibParser *p) {
    parser_eat_whitespaces(p);
    int32_t cache_curline = p->curline;
    parser_eat_newline(p);
    bool result = p->curline > cache_curline;
    parser_eat_whitespaces(p);
    return result;
}

static void parser_eat_all_blanks(VrplibParser *p) {
    char *at = NULL;
    do {
        at = p->at;
        parser_eat_whitespaces(p);
        parser_eat_newline(p);
        parser_eat_whitespaces(p);
    } while (p->at != at);
}

static bool parser_match_string(VrplibParser *p, char *string) {
    parser_eat_whitespaces(p);
    size_t len = MIN(parser_remainder_size(p), strlen(string));
    bool result = (0 == strncmp(string, p->at, len));

    if (result) {
        parser_adv(p, len);
    }
    parser_eat_whitespaces(p);
    return result;
}

ATTRIB_PRINTF(2, 3)
static void parse_error(VrplibParser *p, char *fmt, ...) {
    fprintf(stderr, "%s:%d: error: ", p->filename, p->curline);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

static bool parser_needs_edge_section(VrplibParser *p) {
    return p->edgew_format == EDGE_WEIGHT_FORMAT_UPPER_ROW;
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
            }

            ptrdiff_t namesize = p->at - at;

            // Match end of line
            if (!parser_match_newline(p)) {
                return NULL;
            }

            // Walk string backwards and trim any trailing whitespace
            while (namesize > 0 &&
                   (at[namesize - 1] == ' ' || at[namesize - 1] == '\t')) {
                namesize--;
            }

            char *value = malloc(namesize + 1);
            value[namesize] = 0;
            strncpy(value, at, namesize);
            return value;
        } else {
            return NULL;
        }
    }

    return NULL;
}

static bool parse_vrplib_hdr(VrplibParser *p, Instance *instance) {
    bool result = true;
    bool done = false;
    while (!parser_is_eof(p) && !done) {
        parser_eat_all_blanks(p);
        char *value = NULL;
        if (parser_match_string(p, "#")) {
            // Eat comment
            while (!parser_is_eof(p) && !(*p->at == '\n' || *p->at == '\r')) {
                parser_adv(p, 1);
            }
            parser_eat_newline(p);
            parser_eat_whitespaces(p);
        } else if ((value = parse_hdr_field(p, "NAME"))) {
            instance_set_name(instance, value);
        } else if ((value = parse_hdr_field(p, "COMMENT"))) {
            instance->comment = strdup(value);
        } else if ((value = parse_hdr_field(p, "TYPE"))) {
            if (0 != strcmp(value, "CVRP")) {
                parse_error(p,
                            "only CVRP type is supported. Found `%s` "
                            "instead",
                            value);
                result = false;
            }
        } else if ((value = parse_hdr_field(p, "DIMENSION"))) {
            int32_t dim = 0;
            if (!str_to_int32(value, &dim)) {
                parse_error(p,
                            "expected valid integer for DIMENSION field. Got "
                            "`%s` instead",
                            value);
                result = false;
            }
            instance->num_customers = MAX(0, dim - 1);
        } else if ((value = parse_hdr_field(p, "VEHICLES"))) {
            int32_t num_vehicles = 0;
            if (!str_to_int32(value, &num_vehicles)) {
                parse_error(p,
                            "expected valid integer for VEHICLES field. Got "
                            "`%s` instead",
                            value);
                result = false;
            }
            instance->num_vehicles = MAX(0, num_vehicles);
        } else if ((value = parse_hdr_field(p, "EDGE_WEIGHT_TYPE"))) {
            bool is_supported = false;
            for (int32_t i = 0; i < (int32_t)ARRAY_LEN(G_supported_edgew_fmt);
                 i++) {
                if (0 == strcmp(value, G_supported_edgew_fmt[i].name)) {
                    is_supported = true;
                    p->edgew_format = G_supported_edgew_fmt[i].fmt;
                    break;
                }
            }
            if (!is_supported) {
                parse_error(p, "Found unsupported EDGE_WEIGHT_TYPE (`%s`)",
                            value);
                result = false;
            }
        } else if ((value = parse_hdr_field(p, "EDGE_WEIGHT_FORMAT"))) {
            if (0 == strcmp(value, "FUNCTION")) {
                p->edgew_format = EDGE_WEIGHT_FORMAT_FUNCTION;
            } else if (0 == strcmp(value, "UPPER_ROW")) {
                p->edgew_format = EDGE_WEIGHT_FORMAT_UPPER_ROW;
            } else {
                parse_error(p, "unsupported format `%s` for EDGE_WEIGHT_FORMAT",
                            value);
                result = false;
            }
        } else if ((value = parse_hdr_field(p, "CAPACITY"))) {
            double capacity = 0;
            if (!str_to_double(value, &capacity)) {
                parse_error(p,
                            "expected valid number for CAPACITY field. Got "
                            "`%s` instead",
                            value);
                result = false;
            }
            instance->vehicle_cap = MAX(0.0, capacity);
        } else {
            done = true;
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

static char *get_token_lexeme(VrplibParser *p) {
    parser_eat_whitespaces(p);
    char *at = p->at;

    while (!parser_is_eof(p) && (*p->at == '-' || *p->at == '+' ||
                                 *p->at == '.' || isalnum(*p->at))) {
        parser_adv(p, 1);
    }

    intptr_t toksize = p->at - at;

    if (toksize != 0) {
        char *lexeme = malloc(toksize + 1);
        if (!lexeme) {
            log_fatal("Failed memory allocation");
            return NULL;
        }

        strncpy(lexeme, at, toksize);
        lexeme[toksize] = '\0';
        parser_eat_whitespaces(p);
        return lexeme;
    }

    parser_eat_whitespaces(p);
    return NULL;
}

static bool parse_node_id(VrplibParser *p, char *lexeme, int32_t node_id) {
    int32_t value = 0;
    if (!str_to_int32(lexeme, &value)) {
        parse_error(p, "Failed to retrieve integer");
        return false;
    }

    if (value != node_id + 1) {
        parse_error(p, "Expected node id to be `%d`. Got `%d` instead",
                    node_id + 1, value);
        return false;
    }

    return true;
}

static bool parse_vrplib_nodecoord_section(VrplibParser *p,
                                           Instance *instance) {
    bool result = true;

    for (int32_t node_id = 0;
         result && (node_id != (instance->num_customers + 1)); node_id++) {
        for (int32_t i = 0; i < 3; i++) {
            char *lexeme = get_token_lexeme(p);
            if (!lexeme) {
                result = false;
            } else {
                switch (i) {
                case 0:
                    result = parse_node_id(p, lexeme, node_id);
                    break;
                case 1:
                case 2: {
                    // Parsing the x, y coordinate
                    double coord = 0;
                    if (!str_to_double(lexeme, &coord)) {
                        parse_error(p, "Expected valid double for coordinate");
                        result = false;
                    } else {
                        if (i == 1) {
                            instance->positions[node_id].x = coord;
                        } else {
                            instance->positions[node_id].y = coord;
                        }
                    }
                    break;
                }
                }

                if (lexeme) {
                    free(lexeme);
                }
            }
        }

        if (!parser_match_newline(p)) {
            parse_error(p, "Expected newline after parsing node id `%d`",
                        node_id);
            result = false;
        }
    }

    return result;
}

static bool parse_node_double_tuple_section(VrplibParser *p, Instance *instance,
                                            char *valuename, double *outarray) {
    bool result = true;

    for (int32_t node_id = 0;
         result && (node_id != (instance->num_customers + 1)); node_id++) {
        for (int32_t i = 0; i < 2; i++) {
            char *lexeme = get_token_lexeme(p);
            if (!lexeme) {
                result = false;
            } else {
                switch (i) {
                case 0:
                    result = parse_node_id(p, lexeme, node_id);
                    break;
                case 1: {
                    // Parse the value
                    double value = 0;
                    if (!str_to_double(lexeme, &value)) {
                        parse_error(p, "Expected valid double for %s",
                                    valuename);
                        result = false;
                    } else {
                        outarray[node_id] = value;
                    }
                    break;
                }
                }
            }

            if (lexeme) {
                free(lexeme);
            }
        }

        if (!parser_match_newline(p)) {
            parse_error(p, "Expected newline after parsing node id `%d`",
                        node_id);
            result = false;
        }
    }

    return result;
}

static bool parse_vrplib_demand_section(VrplibParser *p, Instance *instance) {
    return parse_node_double_tuple_section(p, instance, "demand",
                                           instance->demands);
}

static bool parse_vrplib_profit_section(VrplibParser *p, Instance *instance) {

    return parse_node_double_tuple_section(p, instance, "profit",
                                           instance->profits);
}

static bool parse_vrplib_depot_section(VrplibParser *p, Instance *instance) {
    UNUSED_PARAM(instance);
    bool result = true;
    for (int32_t i = 0; result && (i <= 1); i++) {
        char *lexeme = get_token_lexeme(p);
        if (!lexeme) {
            result = false;
        } else {
            int32_t nodeid = 0;
            if (!str_to_int32(lexeme, &nodeid)) {
                parse_error(p, "Expected valid integer for DEPOT_SECTION");
                result = false;
            } else {
                if (i == 0) {
                    if (nodeid != 1) {
                        parse_error(p,
                                    "Expected single depot with index `1`. Got "
                                    "`%d` instead",
                                    nodeid);
                        result = false;
                    }
                } else {
                    if (nodeid != -1) {
                        parse_error(p,
                                    "Expected value `-1` marking the end of "
                                    "the depot section, Found `%d` instead",
                                    nodeid);
                        result = false;
                    }
                }
            }
        }

        if (lexeme) {
            free(lexeme);
        }

        if (!parser_match_newline(p)) {
            parse_error(p, "Expected newline");
            result = false;
        }
    }

    return result;
}

static bool parse_vrplib_edge_weight_section(VrplibParser *p,
                                             Instance *instance) {
    bool result = true;
    int32_t n = instance->num_customers + 1;

    bool needs_edge_section = parser_needs_edge_section(p);
    if (!needs_edge_section) {
        parse_error(p, "Found un-expected `EDGE_WEIGHT_SECTION`. "
                       "EDGE_WEIGHT_TYPE should be set accordingly");
        return false;
    }

    assert(instance->edge_weight);

    for (int32_t i = 0; i < n; i++) {
        for (int32_t j = i + 1; j < n; j++) {
            int32_t idx = sxpos(n, i, j);

            for (int32_t lexid = 0; lexid < 3; lexid++) {
                char *lexeme = get_token_lexeme(p);
                if (lexid == 0 || lexid == 1) {
                    result = parse_node_id(p, lexeme, lexid == 0 ? i : j);
                } else {
                    // Parse the reduced cost variable
                    double value = 0;
                    if (!str_to_double(lexeme, &value)) {
                        parse_error(p,
                                    "Expected valid double for reduced cost");
                        result = false;
                    } else {
                        instance->edge_weight[idx] = value;
                    }
                }

                if (lexeme) {
                    free(lexeme);
                }

                if (!result) {
                    goto terminate;
                }
            }

            if (!parser_match_newline(p)) {
                parse_error(p,
                            "Expected newline after reduced cost for arc "
                            "`(%d, %d)``",
                            i + 1, j + 1);
                result = false;
                goto terminate;
            }
        }
    }

terminate:
    return result;
}

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

    // First extract the header information
    if (!parse_vrplib_hdr(&parser, instance)) {
        result = false;
        goto terminate;
    }

    if (instance->num_customers <= 0) {
        parse_error(&parser, "couldn't deduce number of customers after "
                             "parsing the VRPLIB header");
        result = false;
        goto terminate;
    } else if (instance->vehicle_cap <= 0.0) {
        parse_error(&parser, "couldn't deduce vehicle capacity after "
                             "parsing the VRPLIB header");
        result = false;
        goto terminate;
    }

    if (!prep_memory(instance)) {
        log_fatal("Failed to prepare memory for storing instance");
        result = false;
        goto terminate;
    }

    bool needs_edge_section = parser_needs_edge_section(&parser);

    if (needs_edge_section) {
        instance->edge_weight =
            malloc(hm_nentries(1 + instance->num_customers) *
                   sizeof(*instance->edge_weight));
        if (!instance->edge_weight) {
            log_fatal("Failed to prepare memory for storing instance");
            result = false;
            goto terminate;
        }
    }

    struct {
        char *name;
        bool (*parse_fn)(VrplibParser *p, Instance *instance);
        bool required;
        bool found;
    } sections[] = {
        {"NODE_COORD_SECTION", parse_vrplib_nodecoord_section, 1, 0},
        {"DEMAND_SECTION", parse_vrplib_demand_section, 1, 0},
        {"DEPOT_SECTION", parse_vrplib_depot_section, 1, 0},
        {"PROFIT_SECTION", parse_vrplib_profit_section, 0, 0},
        {"EDGE_WEIGHT_SECTION", parse_vrplib_edge_weight_section,
         needs_edge_section, 0},
    };

    while (result) {
        bool done =
            parser_is_eof(&parser) || parser_match_string(&parser, "EOF");
        if (done) {
            break;
        }

        bool found_matching_section = false;
        for (int32_t secid = 0; secid < (int32_t)ARRAY_LEN(sections); secid++) {
            if (parser_match_string(&parser, sections[secid].name) &&
                parser_match_newline(&parser)) {
                if (sections[secid].found) {
                    parse_error(&parser,
                                "Multiple definitions for section `%s`",
                                sections[secid].name);
                    result = false;
                    break;
                }

                sections[secid].found = true;
                found_matching_section = true;
                result = sections[secid].parse_fn(&parser, instance);
                if (!result) {
                    parse_error(&parser, "Failure while parsing section `%s`",
                                sections[secid].name);
                    result = false;
                }
                break;
            }
        }
        if (!found_matching_section) {
            parse_error(&parser, "invalid input");
            result = false;
        }
    }

    if (result) {

        // Keep eating whitespaces and newlines while there are any
        parser_eat_all_blanks(&parser);

        if (parser_remainder_size(&parser) != 0) {
            parse_error(&parser, "Found premature `EOF` while more input "
                                 "is still available");
            result = false;
            goto terminate;
        }

        for (int32_t secid = 0; secid < (int32_t)ARRAY_LEN(sections); secid++) {
            if (sections[secid].required && !sections[secid].found) {
                parse_error(&parser, "Required section `%s` was not found",
                            sections[secid].name);
                result = false;
                goto terminate;
            }
        }

        if (instance->demands[0] != 0) {
            parse_error(
                &parser,
                "demand for the depot node should be `0`. Got `%f` instead",
                instance->demands[0]);
            result = false;
            goto terminate;
        }
    }

terminate:
    if (buffer) {
        free(buffer);
    }
    return result;
}

typedef enum {
    PARSING_FILE_EXT_AUTODETECT = 0,
    PARSING_FILE_EXT_VRPLIB = 1,
    PARSING_FILE_EXT_SIMPLIFIED_VRP = 2,
} ParsingFileExt;

static Instance parse_impl(const char *filepath, ParsingFileExt ext) {
    FILE *filehandle = fopen(filepath, "r");
    Instance result = {0};
    result.rounding_strat = CPTP_DIST_ROUND;
    bool success = false;

    if (ext == PARSING_FILE_EXT_AUTODETECT) {
        // Default to VRPLIB parsing
        ext = PARSING_FILE_EXT_VRPLIB;

        const char *extstring = os_get_fext(filepath);
        if (extstring && 0 == strcmp(extstring, "simplified-vrp")) {
            ext = PARSING_FILE_EXT_SIMPLIFIED_VRP;
        }
    }

    if (filehandle) {
        switch (ext) {
        case PARSING_FILE_EXT_SIMPLIFIED_VRP:
            success = parse_simplified_vrp_file(&result, filehandle, filepath);
            break;
        case PARSING_FILE_EXT_VRPLIB:
            success = parse_vrp_file(&result, filehandle, filepath);
            break;
        default:
            assert(!"Invalid code path");
            break;
        }
        fclose(filehandle);
    }

    if (success) {
        if (!result.name || result.name[0] == '\0') {
            instance_set_name(&result, filepath);
        }
        return result;
    } else {
        instance_destroy(&result);
        return result;
    }
}

Instance parse(const char *filepath) {
    return parse_impl(filepath, PARSING_FILE_EXT_AUTODETECT);
}
