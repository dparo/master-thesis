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

#pragma once

#if __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "misc.h"
#include "utils.h"

typedef struct Mat2d {
    int32_t w, h;
    double *data;
} Mat2d;

static inline bool feq(double a, double b, double epsilon) {
    return fabs(a - b) <= epsilon;
}

static inline bool flt(double a, double b, double epsilon) {
    if (feq(a, b, epsilon)) {
        return false;
    }
    return a < b;
}

static inline bool fgt(double a, double b, double epsilon) {
    if (feq(a, b, epsilon)) {
        return false;
    }
    return a > b;
}

static inline bool flte(double a, double b, double epsilon) {
    if (feq(a, b, epsilon)) {
        return true;
    }
    return a < b;
}

static inline bool fgte(double a, double b, double epsilon) {
    if (feq(a, b, epsilon)) {
        return true;
    }
    return a > b;
}

static inline double fratio(double lb, double ub, double shift) {
    return 1.0 + ((ub - lb) / (shift + fabs(lb)));
}

static inline double fgap(double a, double b) {
    double ub = MIN(a, b);
    double lb = MAX(a, b);

    double gap = (ub - lb) / (1e-10 + fabs(ub));
    return gap;
}

static inline bool fgapcmp(double a, double b, double epsilon) {
    return fabs(fgap(a, b)) <= epsilon;
}

typedef struct Vec2d {
    double x, y;
} Vec2d;

int32_t *veci32_create(int32_t len);
int32_t *mati32_create(int32_t w, int32_t h);

int32_t *veci32_copy(int32_t *other, int32_t len);
int32_t *mati32_copy(int32_t *other, int32_t w, int32_t h);

static inline int32_t *veci32_access(int32_t *vec, int32_t idx,
                                     ATTRIB_MAYBE_UNUSED int32_t len) {
    assert(idx >= 0 && idx < len);
    return &vec[idx];
}

static inline int32_t *mati32_access(int32_t *mat, int32_t row, int32_t col,
                                     int32_t width,
                                     ATTRIB_MAYBE_UNUSED int32_t height) {
    assert(row >= 0 && row < height);
    assert(col >= 0 && col < width);
    return &mat[row * width + col];
}

static inline void veci32_set(int32_t *vec, int32_t len, int32_t val) {
    for (int32_t i = 0; i < len; i++)
        vec[i] = val;
}

static inline void mati32_set(int32_t *mat, int32_t w, int32_t h, int32_t val) {
    for (int32_t row = 0; row < h; row++)
        for (int32_t col = 0; col < w; col++)
            *mati32_access(mat, row, col, w, h) = val;
}

static inline double vec2d_dist(Vec2d const *a, Vec2d const *b) {
    double dx = b->x - a->x;
    double dy = b->y - a->y;
    return sqrt(dx * dx + dy * dy);
}

typedef struct EnumToStrMapping {
    int32_t value;
    const char *name;
} EnumToStrMapping;

#define ENUM_TO_STR_TABLE_FIELD_CUSTOM(x, STR)                                 \
    { (x), STR }

#define ENUM_TO_STR_TABLE_FIELD(x) ENUM_TO_STR_TABLE_FIELD_CUSTOM(x, #x)

#define ENUM_TO_STR_TABLE_DECL(ENUM_TYPE)                                      \
    const EnumToStrMapping ENUM_TO_STR_MAPPING_TABLE_##ENUM_TYPE[]

#define ENUM_TO_STR(ENUM_TYPE, x)                                              \
    (__enum_to_str(ENUM_TO_STR_MAPPING_TABLE_##ENUM_TYPE,                      \
                   (int32_t)ARRAY_LEN(ENUM_TO_STR_MAPPING_TABLE_##ENUM_TYPE),  \
                   (x)))

#define STR_TO_ENUM(ENUM_TYPE, x)                                              \
    (__str_to_enum(ENUM_TO_STR_MAPPING_TABLE_##ENUM_TYPE,                      \
                   (int32_t)ARRAY_LEN(ENUM_TO_STR_MAPPING_TABLE_##ENUM_TYPE),  \
                   (x)))

#define STR_TO_ENUM_DEFAULT(ENUM_TYPE, x, default_val)                         \
    ((ENUM_TYPE)(__str_to_enum_default(                                        \
        ENUM_TO_STR_MAPPING_TABLE_##ENUM_TYPE,                                 \
        (int32_t)ARRAY_LEN(ENUM_TO_STR_MAPPING_TABLE_##ENUM_TYPE), (x),        \
        (default_val))))

const char *__enum_to_str(const EnumToStrMapping *table, int32_t table_len,
                          int32_t value);

const int32_t *__str_to_enum(const EnumToStrMapping *table, int32_t table_len,
                             const char *name);

ATTRIB_MAYBE_UNUSED static inline int32_t
__str_to_enum_default(const EnumToStrMapping *table, int32_t table_len,
                      const char *name, int32_t default_val) {
    const int32_t *lookup = __str_to_enum(table, table_len, name);
    if (!lookup) {
        return default_val;
    }
    return *lookup;
}

typedef enum {
    TYPED_PARAM_DOUBLE,
    TYPED_PARAM_FLOAT,
    TYPED_PARAM_BOOL,
    TYPED_PARAM_INT32,
    TYPED_PARAM_USIZE,
    TYPED_PARAM_STR,
} ParamType;

typedef struct {
    int32_t count;
    ParamType type;
    union {
        double dval;
        float fval;
        int32_t ival;
        size_t sizeval;
        bool bval;
        const char *sval;
    };
} TypedParam;

#if __cplusplus
}
#endif
