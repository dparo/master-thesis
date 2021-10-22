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

#include "parsing-utils.h"
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>

bool str_to_int32(const char *string, int32_t *out) {
    size_t len = strlen(string);
    int base = 10;

    bool is_negated = false;
    if (len >= 1 && (string[0] == '-' || string[0] == '+')) {
        is_negated = string[0] == '-';

        len -= 1;
        string += 1;
    }

    if (len >= 2 && string[0] == '0' && string[1] == 'x') {
        base = 16;
        string += 2;
        len -= 2;
    } else if (len >= 2 && string[0] == '0' && string[1] == 'b') {
        base = 2;
        string += 2;
        len -= 2;
    }

    STATIC_ASSERT(sizeof(long) >= sizeof(int32_t),
                  "These sizes should match so that we can use the correct "
                  "strtoXXX C function");

    char *endptr = NULL;
    errno = 0;
    long conv_ret_val = strtol(string, &endptr, base);

    bool out_of_range = (errno == ERANGE) || (conv_ret_val < INT32_MIN) ||
                        (conv_ret_val > INT32_MAX);

    bool failed =
        len == 0 || out_of_range || endptr == string || endptr == NULL;
    bool result = !failed && *endptr == '\0';

    if (result) {
        if (is_negated) {
            *out = -conv_ret_val;
        } else {
            *out = conv_ret_val;
        }
    } else {
        *out = INT32_MIN;
    }

    return result;
}

bool str_to_usize(const char *string, size_t *out) {
    size_t len = strlen(string);
    int base = 10;

    bool is_negated = false;
    if (len >= 1 && (string[0] == '-' || string[0] == '+')) {
        is_negated = string[0] == '-';

        len -= 1;
        string += 1;
    }

    if (len >= 2 && string[0] == '0' && string[1] == 'x') {
        base = 16;
        string += 2;
        len -= 2;
    } else if (len >= 2 && string[0] == '0' && string[1] == 'b') {
        base = 2;
        string += 2;
        len -= 2;
    }

    STATIC_ASSERT(sizeof(uintmax_t) >= sizeof(size_t),
                  "These sizes should match so that we can use the correct "
                  "strtoXXX C function");

    char *endptr = NULL;
    errno = 0;
    uintmax_t conv_ret_val = strtoumax(string, &endptr, base);

    bool out_of_range =
        (errno == ERANGE) || (conv_ret_val > UINTMAX_MAX) || is_negated;

    bool failed =
        len == 0 || out_of_range || endptr == string || endptr == NULL;
    bool result = !failed && *endptr == '\0';

    if (result) {
        if (is_negated) {
            *out = -conv_ret_val;
        } else {
            *out = conv_ret_val;
        }
    } else {
        *out = 0;
    }

    return result;
}

bool str_to_float(const char *string, float *out) {
    size_t len = strlen(string);

    bool terminating_f =
        len >= 1 && (string[len - 1] == 'f' || string[len - 1] == 'F');

    char *endptr = NULL;

    errno = 0;
    float conv_ret_val = strtof(string, &endptr);

    bool out_of_range = (errno == ERANGE);
    bool failed =
        len == 0 || out_of_range || endptr == string || endptr == NULL;

    bool valid_endptr =
        *endptr == '\0' || (terminating_f && endptr == string + len - 1);
    bool result = !failed && valid_endptr;

    if (result) {
        *out = conv_ret_val;
    } else {
        *out = NAN;
    }

    return result;
}

bool str_to_double(const char *string, double *out) {
    size_t len = strlen(string);

    bool terminating_f =
        len >= 1 && (string[len - 1] == 'f' || string[len - 1] == 'F');

    char *endptr = NULL;

    errno = 0;
    float conv_ret_val = strtod(string, &endptr);

    bool out_of_range = (errno == ERANGE);
    bool failed =
        len == 0 || out_of_range || endptr == string || endptr == NULL;

    bool valid_endptr =
        *endptr == '\0' || (terminating_f && endptr == string + len - 1);
    bool result = !failed && valid_endptr;

    if (result) {
        *out = conv_ret_val;
    } else {
        *out = NAN;
    }

    return result;
}

bool str_to_bool(const char *string, bool *out) {
    if ((0 == strcasecmp(string, "true")) || (0 == strcmp(string, "1"))) {
        *out = true;
        return true;
    } else if (0 == strcasecmp(string, "false") || (0 == strcmp(string, "0"))) {
        *out = false;
        return true;
    }

    return false;
}
