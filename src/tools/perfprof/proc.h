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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#define PROC_MAX_ARGS 256
#define PROC_POOL_SIZE 16
#define PROC_POOL_UPDATE_PERIOD_MSEC (10)

pid_t proc_spawn(char *const args[]);
int proc_spawn_sync(char *const args[]);
bool proc_terminated(pid_t pid, int *exit_status);

typedef struct {
    bool valid;
    pid_t pid;
    char *args[PROC_MAX_ARGS];
} Process;

typedef struct ProcPool {
    int32_t max_num_procs;
    Process procs[PROC_POOL_SIZE];
} ProcPool;

void proc_pool_queue(ProcPool *pool, char *const args[]);
void proc_pool_sync(ProcPool *pool);
void proc_pool_join(ProcPool *pool);

#if __cplusplus
}
#endif
