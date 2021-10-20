/*
 * Copyright (c) 2021 Davide Paro
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#if __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

typedef uint64_t usecs_t;

#define NUM_USECS_IN_A_MSEC ((usecs_t)1000ll)
#define NUM_USECS_IN_A_SEC ((usecs_t)1000000ll)
#define NUM_USECS_IN_A_MINUTE ((usecs_t)60000000ll)
#define NUM_USECS_IN_AN_HOUR ((usecs_t)3600000000ll)
#define NUM_USECS_IN_A_DAY ((usecs_t)86400000000ll)

typedef struct TimeRepr {
    int32_t days;
    int32_t hours;
    int32_t minutes;
    int32_t seconds;
    int32_t milliseconds;
    int32_t microseconds;
} TimeRepr;

void os_sleep(usecs_t usecs);
usecs_t os_get_usecs(void);
TimeRepr timerepr_from_usecs(usecs_t usecs);

#if __cplusplus
}
#endif
