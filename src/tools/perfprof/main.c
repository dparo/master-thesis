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
#include <signal.h>

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
    int32_t max_num_procs;
    char *name;
    Filter filter;
    double timelimit;
    int32_t nseeds;
    Solver solvers[MAX_NUM_SOLVERS_PER_GROUP];
} BatchGroup;

bool G_should_terminate;
ProcPool G_pool = {0};
BatchGroup *G_active_bgroup = NULL;

static void my_sighandler(int signum) {
    switch (signum) {
    case SIGTERM:
        log_warn("Received SIGINT");
        break;
    case SIGINT:
        log_warn("Received SIGTERM");
        break;
    default:
        break;
    }
    if (signum == SIGTERM || signum == SIGINT) {
        G_should_terminate = true;
        G_pool.aborted = true;
    }
}

void handle_vrp_instance(const char *fpath) {
    char *args[PROC_MAX_ARGS];
    int32_t argidx = 0;

    char timelimit[128];
    snprintf_safe(timelimit, ARRAY_LEN(timelimit), "%g",
                  G_active_bgroup->timelimit);

    char timelimit_extended[128];
    snprintf_safe(timelimit_extended, ARRAY_LEN(timelimit_extended), "%g",
                  G_active_bgroup->timelimit * 1.05 + 2);

    char killafter[128];
    snprintf_safe(killafter, ARRAY_LEN(killafter), "%g",
                  G_active_bgroup->timelimit * 1.1 + 2 -
                      G_active_bgroup->timelimit);

    Path p;
    char outdir[] = "perfprof-dump";
    os_mkdir(outdir, true);

    char json_report_path[OS_MAX_PATH + 32];
    snprintf_safe(json_report_path, ARRAY_LEN(json_report_path), "%s/%s.json",
                  outdir, os_basename(fpath, &p));

    args[argidx++] = "timeout";
    args[argidx++] = "-k";
    args[argidx++] = killafter;
    args[argidx++] = timelimit_extended;
    args[argidx++] = CPTP_EXE;
    args[argidx++] = "-t";
    args[argidx++] = timelimit;
    args[argidx++] = "-i";
    args[argidx++] = (char *)fpath;
    args[argidx++] = "-w";
    args[argidx++] = (char *)json_report_path;
    args[argidx++] = NULL;

    proc_pool_queue(&G_pool, args);
    if (G_pool.max_num_procs == 1) {
        proc_pool_join(&G_pool);
    }
}

int file_walk_cb(const char *fpath, const struct stat *sb, int typeflag,
                 struct FTW *ftwbuf) {
    if (typeflag == FTW_F || typeflag == FTW_SL) {
        // Is a regular file
        const char *ext = os_get_fext(fpath);
        if (ext && (0 == strcmp(ext, "vrp"))) {
            printf("Found file: %s\n", fpath);
            handle_vrp_instance(fpath);
        }
    } else if (typeflag == FTW_D) {
        printf("Found dir: %s\n", fpath);
    }

    return G_should_terminate ? FTW_STOP : FTW_CONTINUE;
}

void scan_dir_and_solve(char *dirpath) {
    int result = nftw(dirpath, file_walk_cb, 8, 0);
    if (result == FTW_STOP) {
        proc_pool_join(&G_pool);
        printf("Requested to stop scanning dirpath %s\n", dirpath);
    } else if (result != 0) {
        perror("nftw walk");
        exit(EXIT_FAILURE);
    }
}

static void do_batch(BatchGroup *bgroup) {
    proc_pool_join(&G_pool);
    G_active_bgroup = bgroup;
    G_pool.max_num_procs = bgroup->max_num_procs;
    if (!G_should_terminate) {
        scan_dir_and_solve("./data/BaPCod generated - Test instances/A-n37-k5");
    }
}

int main(int argc, char *argv[]) {
    sighandler_t prev_sigterm_handler = signal(SIGTERM, my_sighandler);
    sighandler_t prev_sigint_handler = signal(SIGINT, my_sighandler);

    BatchGroup batches[] = {{1,
                             "Integer separation vs Fractional separation",
                             DEFAULT_FILTER,
                             600.0,
                             3,
                             {}}};

    for (int32_t i = 0; i < (int32_t)ARRAY_LEN(batches); i++) {
        do_batch(&batches[i]);
    }

    signal(SIGTERM, prev_sigterm_handler);
    signal(SIGINT, prev_sigint_handler);

#if 0
    for (int32_t i = 0; i < 20; i++) {
        char amt[2] = "5";
        amt[0] = rand() % 10 + '0';
        amt[1] = '\0';
        char *args[] = {"sleep", amt, NULL};

        proc_pool_queue(&G_pool, args);
    }

#endif

    proc_pool_join(&G_pool);

    return 0;
}
