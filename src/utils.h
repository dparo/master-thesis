
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

#include "misc.h"

#define ABS(x) ((x) > 0 ? (x) : -(x))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

#define SWAP(TYPE, a, b)                                                       \
    do {                                                                       \
        (TYPE) tmp;                                                            \
        tmp = (a);                                                             \
        (a) = (b);                                                             \
        (b) = tmp;                                                             \
    } while (0)

#define CLAMP_MAX(x, max) MIN(x, max)
#define CLAMP_MIN(x, min) MAX(x, min)

/* Returns true if `x` is a power of 2 */
#define IS_POW2(x) (!((x) & ((x)-1)))

/* Align any generic value to any alignment */
#define ALIGN(TYPE, N, S)                                                      \
    ((TYPE)((((TYPE)(N) + (TYPE)((TYPE)(S) - (TYPE)1)) / (TYPE)(S)) *          \
            (TYPE)(S)))
/* This macro is the same as ALIGN but only works when `S` is a POWER of 2.
   `N` can still be any number.
   @EXAMPLE
       POW2_ALIGN(1, 16)  -> 16
       POW2_ALIGN(2, 16)  -> 16
       POW2_ALIGN(16, 16) -> 16
       POW2_ALIGN(17, 16) -> 32
       POW2_ALIGN(1, 9)   -> Garbage (9 is not a power of 2)
   It can possibly lead to better assembly generation. */
#define POW2_ALIGN(TYPE, N, S)                                                 \
    ((TYPE)(((TYPE)(N) + ((TYPE)(S) - (TYPE)1)) &                              \
            ((TYPE) ~((TYPE)(S) - (TYPE)1))))
#define ALIGN_PTR(N, S) ((void*) (ALIGN(usize, N, S))

#define KILOBYTES(x) (((size_t)(x)) << 10)
#define MEGABYTES(x) ((KILOBYTES(x)) << 10)
#define GIGABYTES(x) ((MEGABYTES(x)) << 10)

#define KILO(x) (((size_t)(x)) << 10)
#define MEGA(x) ((KILOBYTES(x)) << 10)
#define GIGA(x) ((MEGABYTES(x)) << 10)

#define BIT(TYPE, POSITION) (((TYPE)1 << (TYPE)POSITION))

#define memclr(SRC, SIZE) (memset((SRC), 0, (SIZE)))

#define ZERO_STRUCT(S) memclr(S, sizeof(*(S)))

#include "log.h"

#define FATAL(...)                                                             \
    do {                                                                       \
        log_fatal(__VA_ARGS__);                                                \
        fflush(stdout);                                                        \
        fflush(stderr);                                                        \
        abort();                                                               \
    } while (0)

static inline size_t get_file_size(FILE *f) {
    fseek(f, 0L, SEEK_END);
    size_t result = ftell(f);
    fseek(f, 0L, SEEK_SET);
    return result;
}

#if __cplusplus
}
#endif
