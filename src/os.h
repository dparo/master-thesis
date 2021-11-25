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

#define OS_MAX_PATH 4096

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct TimeRepr {
    int32_t days;
    int32_t hours;
    int32_t minutes;
    int32_t seconds;
    int32_t milliseconds;
    int32_t microseconds;
} TimeRepr;

typedef struct {
    char cstr[OS_MAX_PATH];
} Path;

void os_sleep(int64_t usecs);

int64_t os_get_nanosecs(void);
static inline int64_t os_get_usecs(void) { return os_get_nanosecs() / 1000; }

TimeRepr timerepr_from_usecs(int64_t usecs);

static inline TimeRepr timerepr_from_nanosecs(int64_t nsecs) {
    return timerepr_from_usecs(nsecs / 1000);
}

double os_get_elapsed_secs(int64_t usecs_begin);

void print_timerepr(FILE *f, const TimeRepr *repr);

const char *os_get_fext(const char *filepath);

bool os_fexists(char *filepath);
bool os_direxists(char *filepath);

bool os_mkdir(char *path, bool exist_ok);

char *os_basename(const char *path, Path *p);
char *os_dirname(const char *path, Path *p);

#if __cplusplus
}
#endif
