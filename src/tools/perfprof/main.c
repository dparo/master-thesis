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

#define PROC_MAX_ARGS 256
#define PROC_POOL_SIZE 16
#define PROC_POOL_UPDATE_PERIOD_MSEC (10)

typedef struct {
    bool valid;
    pid_t pid;
    char *args[PROC_MAX_ARGS];
} Process;

typedef struct ProcPool {
    int32_t max_num_procs;
    Process procs[PROC_POOL_SIZE];
} ProcPool;

void proc_destroy(Process *proc) {
    proc->pid = INT32_MIN;
    for (int32_t i = 0; i < PROC_MAX_ARGS; i++) {
        free(proc->args[i]);
    }
    memset(proc, 0, sizeof(*proc));
}

void insert_proc_in_pool(ProcPool *pool, int32_t idx, char *args[]) {
    pid_t pid = proc_spawn(args);

    pool->procs[idx].valid = true;
    pool->procs[idx].pid = pid;
    int32_t i = 0;
    for (i = 0; i < PROC_MAX_ARGS - 1 && args[i] != NULL; i++) {
        pool->procs[idx].args[i] = strdup(args[i]);
    }
    pool->procs[idx].args[i] = NULL;
}

int32_t pool_sync(ProcPool *pool) {
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
                printf("Process %s [$?=%d]: ",
                       exit_status == 0 ? "exited correctly" : "failed",
                       exit_status);
                printf("(cmd");

                for (int32_t i = 0; i < PROC_MAX_ARGS - 1 && p->args[i] != NULL;
                     i++) {
                    printf(" %s", p->args[i]);
                }
                printf(")\n");
                proc_destroy(p);
                return idx;
            }
        }
        usleep(PROC_POOL_UPDATE_PERIOD_MSEC * 1000);
    } while (any_valid);

    return INT32_MIN;
}

void pool_join(ProcPool *pool) {
    while ((pool_sync(pool) >= 0)) {
    }
}

void queue_process(ProcPool *pool, char *args[]) {
    for (int32_t idx = 0; idx < MIN(pool->max_num_procs, PROC_POOL_SIZE);
         idx++) {
        Process *p = &pool->procs[idx];
        if (!p->valid) {
            insert_proc_in_pool(pool, idx, args);
            return;
        }
    }

    int32_t idx = pool_sync(pool);
    assert(idx >= 0);
    insert_proc_in_pool(pool, idx, args);
}

int main(int argc, char **argv) {
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
