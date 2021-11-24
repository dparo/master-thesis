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

void queue_process(ProcPool *pool, char *args[]);
void pool_sync(ProcPool *pool);
void pool_join(ProcPool *pool);

#if __cplusplus
}
#endif
