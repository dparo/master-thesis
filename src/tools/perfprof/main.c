#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>

#define SHARE_PROCESS_GROUP (true)

pid_t spawn(char *const argv[]) {
    pid_t pid = 0;
    if ((pid = fork()) == 0) {

        printf("Spawning process [PID=%d]:", pid);
        for (int32_t i = 0; argv[i] != NULL; i++) {
            printf(" %s", argv[i]);
        }
        printf("\n");

        for (int i = STDERR_FILENO + 1; i < 256; i++)
            close(i);
        if (!SHARE_PROCESS_GROUP)
            setsid();
        execvp(argv[0], argv);
        perror("Failed");
        exit(EXIT_FAILURE);
    }
    return pid;
}

int spawn_sync(char *const argv[]) {
    pid_t pid = spawn(argv);
    int wstatus;
    waitpid(0, &wstatus, 0);
    return wstatus;
}

int main(int argc, char **argv) {
    char *args[] = {"kitty", "-e", "nvim", NULL};
    int exit_status = spawn_sync(args);
    printf("exit_status: %d\n", exit_status);
    return 0;
}
