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

#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int32_t *veci32_create(int32_t len) {
    int32_t *result;
    result = malloc(len * sizeof(*result));
    return result;
}

int32_t *mati32_create(int32_t w, int32_t h) {
    int32_t *result;
    result = malloc(w * h * sizeof(*result));
    return result;
}

int32_t *veci32_copy(int32_t *other, int32_t len) {
    int32_t *result = malloc(len * sizeof(*result));
    memcpy(result, other, len * sizeof(*result));
    return result;
}

int32_t *mati32_copy(int32_t *other, int32_t w, int32_t h) {
    int32_t *result = malloc(w * h * sizeof(*result));
    memcpy(result, other, w * h * sizeof(*result));
    return result;
}

const char *__enum_to_str(const EnumToStrMapping *table, int32_t table_len,
                          int32_t value) {
    for (int32_t i = 0; i < table_len; i++)
        if (table[i].value == value)
            return table[i].name;

#ifndef NDEBUG
    fprintf(stderr, "__enum_to_str error: Failed to lookup for value %d\n",
            value);
    fflush(stderr);
    assert(!"<INVALID_ENUM_VALUE>");
#endif

    return "<INVALID_ENUM_VALUE>";
}

const int32_t *__str_to_enum(const EnumToStrMapping *table, int32_t table_len,
                             const char *name) {
    for (int32_t i = 0; i < table_len; i++)
        if (0 == strcmp(table[i].name, name))
            return &table[i].value;

    return NULL;
}
