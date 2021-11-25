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

#include "os.h"
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "misc.h"

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
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>
#elif defined _WIN64
#include <windows.h>
#include <profileapi.h>
#else
#error "Unsupported platform"
#endif

#define NUM_USECS_IN_A_MSEC ((int64_t)1000ll)
#define NUM_USECS_IN_A_SEC ((int64_t)1000000ll)
#define NUM_USECS_IN_A_MINUTE ((int64_t)60000000ll)
#define NUM_USECS_IN_AN_HOUR ((int64_t)3600000000ll)
#define NUM_USECS_IN_A_DAY ((int64_t)86400000000ll)

void os_sleep(int64_t usecs) {
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

#ifndef CAST
#define CAST(type, x) ((type)x)
#endif

int64_t os_get_nanosecs(void) {
#if defined(_MSC_VER) || defined(__MINGW64__) || defined(__MINGW32__)
    LARGE_INTEGER counter;
    static LARGE_INTEGER frequency = {0};
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    QueryPerformanceCounter(&counter);
    return CAST(int64_t, (counter.QuadPart * 1000000000) / frequency.QuadPart);
#elif defined(__linux__) && defined(__STRICT_ANSI__)
    return CAST(int64_t, clock()) * 1000000000 / CLOCKS_PER_SEC;
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) ||    \
    defined(__NetBSD__) || defined(__DragonFly__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (int64_t)ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
#elif __APPLE__
    return (int64_t)clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);
#elif __EMSCRIPTEN__
    return emscripten_performance_now() * 1000000.0;
#else
#error Unsupported platform!
#endif
}

double os_get_elapsed_secs(int64_t usecs_begin) {
    int64_t now = os_get_usecs();
    int64_t diff = now - usecs_begin;
    return (double)diff / (double)NUM_USECS_IN_A_SEC;
}

TimeRepr timerepr_from_usecs(int64_t usecs) {
    TimeRepr result = {0};

    int64_t x = usecs;

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

const char *os_get_fext(const char *filepath) {

    for (int32_t i = strlen(filepath) - 1; i >= 0; i--) {
        if (filepath[i] == '/' || filepath[i] == '\\') {
            return NULL;
        } else if (filepath[i] == '.') {
            const char *extstr = &filepath[i + 1];
            return extstr;
        }
    }
    return NULL;
}

bool os_fexists(char *filepath) {
    bool result = false;
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) ||      \
    defined(__NetBSD__) || defined(__DragonFly__)
    struct stat st;
    int stres = stat(filepath, &st);
    if (stres == -1) {
        if (errno == ENOENT || errno == EACCES || errno == ENOTDIR) {
            return false;
        }
        perror("fexists failure");
        exit(EXIT_FAILURE);
    } else {
        // S_ISLNK(st.st_mode) // Check if is symbolic link
        if (S_ISREG(st.st_mode)) {
            return true;
        } else {
            return false;
        }
    }
#elif __APPLE__
#else
#error "TODO os_fexists"
#endif

    return result;
}

bool os_direxists(char *filepath) {
    bool result = false;
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) ||      \
    defined(__NetBSD__) || defined(__DragonFly__)
    struct stat st;
    int stres = stat(filepath, &st);

    if (stres == -1) {
        if (errno == ENOENT || errno == EACCES || errno == ENOTDIR) {
            return false;
        }
        perror("fexists failure");
        exit(EXIT_FAILURE);
    } else {
        if (S_ISDIR(st.st_mode)) {
            return true;
        } else {
            return false;
        }
    }
#elif __APPLE__
#error "TODO os_fexists for APPLE platform"
#else
#error "TODO os_fexists for WINDOWS platform"
#endif

    return result;
}

bool os_mkdir(char *path, bool exist_ok) {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) ||      \
    defined(__NetBSD__) || defined(__DragonFly__)
    if (exist_ok && os_direxists(path)) {
        return true;
    } else {
        mode_t mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
        int mkres = mkdir(path, mode);
        if (mkres == -1) {
            return false;
        } else {
            return true;
        }
    }

#elif __APPLE__
#error "TODO os_mkdir for APPLE platform"
#else
#error "TODO os_mkdir for WINDOWS platform"
#endif
}

char *os_basename(char *path, Path *p) {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) ||      \
    defined(__NetBSD__) || defined(__DragonFly__)
    p->cstr[0] = 0;
    strncpy(p->cstr, path, ARRAY_LEN(p->cstr));
    char *c = basename(p->cstr);
    assert(c >= p->cstr && c < p->cstr + strlen(p->cstr));
    return c;
#elif __APPLE__
#error "TODO os_basename for APPLE platform"
#else
#error "TODO os_basename for WINDOWS platform"
#endif
}

char *os_dirname(char *path, Path *p) {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) ||      \
    defined(__NetBSD__) || defined(__DragonFly__)
    p->cstr[0] = 0;
    strncpy(p->cstr, path, ARRAY_LEN(p->cstr));
    char *c = dirname(p->cstr);
    assert(c >= p->cstr && c < p->cstr + strlen(p->cstr));
    return p->cstr;
#elif __APPLE__
#error "TODO os_dirname for APPLE platform"
#else
#error "TODO os_dirname for WINDOWS platform"
#endif
}
