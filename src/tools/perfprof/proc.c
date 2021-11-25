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

#include "proc.h"
#include <string.h>
#include <stdint.h>
#include "misc.h"
#include "utils.h"

#define SHARE_PROCESS_GROUP (true)
#define SHARE_STDIN (false)
#define SHARE_STDOUT (true)
#define SHARE_STDERR (true)

static int wstatus_to_exit_status(pid_t pid, int wstatus) {
    int result = EXIT_FAILURE;
    assert(WIFEXITED(wstatus) || WIFSIGNALED(wstatus));

    if (WIFEXITED(wstatus)) {
        result = WEXITSTATUS(wstatus);
    } else if (WIFSIGNALED(wstatus)) {
        if (WCOREDUMP(wstatus)) {
            fprintf(stderr, "Process %d core dumped (signal: %d)\n", pid,
                    WTERMSIG(wstatus));
            result = EXIT_FAILURE;
        } else {
            fprintf(stderr, "Process %d was stopped by signal %d\n", pid,
                    WTERMSIG(wstatus));
            result = EXIT_FAILURE;
        }
    }

    return result;
}

pid_t proc_spawn(char *const args[]) {
    pid_t pid = 0; 
    if ((pid = fork()) == 0) {

        if (!SHARE_STDIN)
            close(STDIN_FILENO);
        if (!SHARE_STDOUT)
            close(STDOUT_FILENO);
        if (!SHARE_STDERR)
            close(STDERR_FILENO);

        for (int i = STDERR_FILENO + 1; i < 256; i++)
            close(i);
        if (!SHARE_PROCESS_GROUP)
            setsid();
        execvp(args[0], args);
        perror("Failed");
        exit(EXIT_FAILURE);
    }

    printf("Spawning process [PID=%d]:", pid);
    for (int32_t i = 0; args[i] != NULL; i++) {
        printf(" %s", args[i]);
    }
    printf("\n");
    return pid;
}

int proc_spawn_sync(char *const args[]) {
    pid_t pid = proc_spawn(args);
    int wstatus;
    int w = waitpid(0, &wstatus, 0);
    if (w < 0) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }

    assert(WIFEXITED(wstatus) || WIFSIGNALED(wstatus));
    return wstatus_to_exit_status(pid, wstatus);
}

bool proc_terminated(pid_t pid, int *exit_status) {
    int wstatus;
    int w = waitpid(pid, &wstatus, WNOHANG);
    if (w < 0) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }

    if (w != pid) {
        return false;
    }

    if (WIFEXITED(wstatus) || WIFSIGNALED(wstatus)) {
        *exit_status = wstatus_to_exit_status(pid, wstatus);
        return true;
    } else {
        assert(w == 0);
    }

    return false;
}

static void proc_destroy(Process *proc) {
    proc->pid = INT32_MIN;
    for (int32_t i = 0; i < PROC_MAX_ARGS; i++) {
        free(proc->args[i]);
    }
    memset(proc, 0, sizeof(*proc));
}

static void insert_proc_in_pool(ProcPool *pool, int32_t idx, void *user_handle,
                                char *const args[]) {
    if (pool->procs[idx].valid) {
        proc_destroy(&pool->procs[idx]);
    }

    pid_t pid = proc_spawn(args);

    pool->procs[idx].valid = true;
    pool->procs[idx].user_handle = user_handle;
    pool->procs[idx].pid = pid;
    int32_t i = 0;
    for (i = 0; i < PROC_MAX_ARGS - 1 && args[i] != NULL; i++) {
        pool->procs[idx].args[i] = strdup(args[i]);
    }
    pool->procs[idx].args[i] = NULL;
}

static int32_t pool_sync2(ProcPool *pool) {
    bool any_valid = false;
    do {
        for (int32_t idx = 0; idx < MIN(pool->max_num_procs, PROC_POOL_SIZE);
             idx++) {
            Process *p = &pool->procs[idx];
            if (!p->valid) {
                continue;
            } else {
                any_valid = true;
            }

            int exit_status;
            if (proc_terminated(p->pid, &exit_status)) {
                printf("Process %d %s [status=%d]: ", p->pid,
                       exit_status == 0 ? "exited correctly" : "failed",
                       exit_status);
                printf("(CMD");

                for (int32_t i = 0; i < PROC_MAX_ARGS - 1 && p->args[i] != NULL;
                     i++) {
                    printf(" %s", p->args[i]);
                }
                printf(")\n");
                if (pool->on_async_proc_exit) {
                    pool->on_async_proc_exit(p, exit_status, p->user_handle);
                }
                proc_destroy(p);
                return idx;
            }
        }
        usleep(PROC_POOL_UPDATE_PERIOD_MSEC * 1000);
    } while (any_valid);

    return INT32_MIN;
}

void proc_pool_queue(ProcPool *pool, void *user_handle, char *const args[]) {
    for (int32_t idx = 0; idx < MIN(pool->max_num_procs, PROC_POOL_SIZE);
         idx++) {
        Process *p = &pool->procs[idx];
        if (!p->valid) {
            insert_proc_in_pool(pool, idx, user_handle, args);
            return;
        }
    }

    int32_t idx = pool_sync2(pool);
    assert(idx >= 0);
    if (!pool->aborted) {
        insert_proc_in_pool(pool, idx, user_handle, args);
    }
}

void proc_pool_sync(ProcPool *pool) { (void)pool_sync2(pool); }

void proc_pool_join(ProcPool *pool) {
    while ((pool_sync2(pool) >= 0)) {
    }

    // NOTE: Technically this loop is not necessary since `pool_sync` will take
    //       care of destroying terminated process. But we live it here for
    //       the sake of clarity
    for (int32_t idx = 0; idx < MIN(pool->max_num_procs, PROC_POOL_SIZE);
         idx++) {
        if (pool->procs[idx].valid) {
            proc_destroy(&pool->procs[idx]);
        }
    }
}
