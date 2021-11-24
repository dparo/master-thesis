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
