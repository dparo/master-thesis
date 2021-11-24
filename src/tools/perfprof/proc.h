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

pid_t proc_spawn(char *const args[]);
int proc_spawn_sync(char *const args[]);
bool proc_terminated(pid_t pid, int *exit_status);

#if __cplusplus
}
#endif
