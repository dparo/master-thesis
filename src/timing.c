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

#include "timing.h"
#include <math.h>
#include <assert.h>
#include <stdio.h>
//
// Includes per platform
//
#if defined __APPLE__
#include <time.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#elif defined __unix__
#include <time.h>
#include <unistd.h>
#elif defined _WIN64
#include <windows.h>
#include <profileapi.h>
#else
#error "Unsupported platform"
#endif

void os_sleep(usecs_t usecs) {
#if defined __APPLE__
    usleep(usecs);
#elif defined __unix__
    struct timespec ts = {};
    ts.tv_sec = usecs / NUM_USECS_IN_A_SEC;
    ts.tv_nsec = (usecs - ts.tv_sec * NUM_USECS_IN_A_SEC) * 1000;
    clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
#elif defined _WIN64
    //
    // TODO: Windows version has milliseconds resolution
    //
    DWORD milliseconds = (DWORD)(usecs / 1000);
    Sleep(milliseconds);
#else
#error "Unsupported platform"
#endif
}
usecs_t os_get_usecs(void) {
#if defined __APPLE__
    static double clock_ticks_to_usecs = 0;
    if (clock_ticks_to_usecs == 0.0) {
        mach_timebase_info_data_t timebase;
        mach_timebase_info(&timebase);
        clock_ticks_to_usecs = 1000.0 * (double)timebase.numer / timebase.denom;
    }
    return (usecs_t)(mach_absolute_time() * clock_ticks_to_usecs);
#elif defined __unix__

    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * NUM_USECS_IN_A_SEC + (usecs_t)(0.001 * ts.tv_nsec);

#elif defined _WIN64
    static LARGE_INTEGER frequency = {0};
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    LARGE_INTEGER ticks = {0};
    QueryPerformanceCounter(&ticks);
    return (usecs_t)((double)ticks.QuadPart * 1000.0 /
                     (double)frequency.QuadPart);

#else
#error "Unsupported platform"
#endif
}

double elapsed_seconds_since(usecs_t begin) {
    usecs_t now = os_get_usecs();
    usecs_t diff = now - begin;
    return (double)diff / (double)NUM_USECS_IN_A_SEC;
}

TimeRepr timerepr_from_usecs(usecs_t usecs) {
    TimeRepr result = {0};

    usecs_t x = usecs;

    result.days = x / NUM_USECS_IN_A_DAY;
    x -= result.days * NUM_USECS_IN_A_DAY;

    result.hours = x / NUM_USECS_IN_AN_HOUR;
    x -= result.hours * NUM_USECS_IN_AN_HOUR;

    result.minutes = x / NUM_USECS_IN_A_MINUTE;
    x -= result.minutes * NUM_USECS_IN_A_MINUTE;

    result.seconds = x / NUM_USECS_IN_A_SEC;
    x -= result.seconds * NUM_USECS_IN_A_SEC;

    result.milliseconds = x / NUM_USECS_IN_A_MSEC;
    x -= result.milliseconds * NUM_USECS_IN_A_MSEC;

    result.microseconds = x;

    return result;
}

void print_timerepr(FILE *f, const TimeRepr *repr) {
    if (repr->days > 0) {
        fprintf(f, "%d day(s), ", repr->days);
    }

    if (repr->hours > 0) {
        fprintf(f, "%d hour(s), ", repr->hours);
    }

    if (repr->minutes > 0) {
        fprintf(f, "%d minute(s), ", repr->minutes);
    }

    if (repr->seconds > 0) {
        fprintf(f, "%d second(s), ", repr->seconds);
    }

    if (repr->milliseconds > 0) {
        fprintf(f, "%d msec(s), ", repr->milliseconds);
    }

    if (repr->microseconds > 0) {
        fprintf(f, "%d usec(s)", repr->microseconds);
    }
}
