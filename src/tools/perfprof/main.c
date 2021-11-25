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

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "utils.h"
#include "misc.h"
#include "proc.h"
#include "os.h"

#include <ftw.h>

#ifndef NDEBUG
#define CPTP_EXE "./build/Debug/src/cptp"
#else
#define CPTP_EXE "./build/Release/src/cptp"
#endif

typedef struct {
    int32_t a, b;
} int32_interval_t;

typedef struct {
    char *family;
    int32_interval_t ncustomers;
    int32_interval_t nvehicles;
} Filter;
static inline Filter make_filter(char *family, int32_interval_t ncustomers,
                                 int32_interval_t nvehicles) {
    Filter result = {0};
    if (ncustomers.a == 0 && ncustomers.b == 0) {
        ncustomers.b = 99999;
    }

    if (nvehicles.a == 0 && nvehicles.b == 0) {
        nvehicles.b = 9999;
    }

    result.family = family;
    result.ncustomers = ncustomers;
    result.nvehicles = nvehicles;
    return result;
}

static const Filter DEFAULT_FILTER = ((Filter){NULL, {0, 99999}, {0, 99999}});

typedef struct {
    char *name;
    char *exe_path;
    char *args[PROC_MAX_ARGS];
} Solver;

#define BAPCOD_SOLVER_NAME ("BaPCod")
static const Solver BAPCOD_SOLVER = ((Solver){BAPCOD_SOLVER_NAME, NULL, {0}});

#define MAX_NUM_SOLVERS_PER_GROUP 16

typedef struct {
    char *name;
    Filter filter;
    double timelimit;
    int32_t nseeds;
    Solver solvers[MAX_NUM_SOLVERS_PER_GROUP];
} BatchGroup;

ProcPool G_pool = {0};
BatchGroup *G_active_bgroup = NULL;

int file_walk_cb(const char *fpath, const struct stat *sb, int typeflag,
                 struct FTW *ftwbuf) {
    if (typeflag == FTW_F || typeflag == FTW_SL) {
        // Is a regular file
        const char *ext = os_get_fext(fpath);
        if (ext && (0 == strcmp(ext, "vrp"))) {
            printf("Found file: %s\n", fpath);
        }
    } else if (typeflag == FTW_D) {
        printf("Found dir: %s\n", fpath);
    }

    return FTW_CONTINUE;
}

void scan_dir_and_solve(char *dirpath) {
    int result = nftw(dirpath, file_walk_cb, 8, 0);
    if (result != 0) {
        perror("nftw walk");
    }
}

static void do_batch(BatchGroup *bgroup) {
    G_active_bgroup = bgroup;
    scan_dir_and_solve("./data");
}

int main(int argc, char *argv[]) {
    G_pool.max_num_procs = 1;

    BatchGroup batches[] = {{"Integer separation vs Fractional separation",
                             DEFAULT_FILTER,
                             600.0,
                             3,
                             {}}};

    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(batches); i++) {
        do_batch(&batches[i]);
    }

#if 0
    for (int32_t i = 0; i < 20; i++) {
        char amt[2] = "5";
        amt[0] = rand() % 10 + '0';
        amt[1] = '\0';
        char *args[] = {"sleep", amt, NULL};

        queue_process(&G_pool, args);
    }

#endif

    pool_join(&G_pool);

    return 0;
}
