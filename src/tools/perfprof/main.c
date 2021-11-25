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

void prova(void) {
    int result = nftw("./data", file_walk_cb, 8, 0);
    if (result != 0) {
        perror("nftw walk");
    }
}

int main(int argc, char **argv) {

    prova();
    exit(0);

    ProcPool pool = {0};
    pool.max_num_procs = 4;

    for (int32_t i = 0; i < 20; i++) {
        char amt[2] = "5";
        amt[0] = rand() % 10 + '0';
        amt[1] = '\0';
        char *args[] = {"sleep", amt, NULL};

        queue_process(&pool, args);
    }

    pool_join(&pool);
    return 0;
}
