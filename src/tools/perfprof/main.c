#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#define SHARE_PROCESS_GROUP (true)
#define SHARE_STDIN (true)
#define SHARE_STDOUT (true)
#define SHARE_STDERR (true)

pid_t spawn(char *const argv[]) {
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
        execvp(argv[0], argv);
        perror("Failed");
        exit(EXIT_FAILURE);
    }

    printf("Spawning process [PID=%d]:", pid);
    for (int32_t i = 0; argv[i] != NULL; i++) {
        printf(" %s", argv[i]);
    }
    printf("\n");
    return pid;
}

int wstatus_to_exit_status(pid_t pid, int wstatus) {
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

bool terminated(pid_t pid, int *exit_status) {
    int wstatus;
    int w = waitpid(pid, &wstatus, WNOHANG);
    if (w < 0) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }

    if (WIFEXITED(wstatus) || WIFSIGNALED(wstatus)) {
        assert(w == 1);
        *exit_status = wstatus_to_exit_status(pid, wstatus);
        return true;
    } else {
        assert(w == 0);
    }

    return false;
}

int spawn_sync(char *const argv[]) {
    pid_t pid = spawn(argv);
    int wstatus;
    int w = waitpid(0, &wstatus, 0);
    if (w < 0) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }

    assert(WIFEXITED(wstatus) || WIFSIGNALED(wstatus));
    return wstatus_to_exit_status(pid, wstatus);
}

int main(int argc, char **argv) {
    char *args[] = {"nvim", NULL};
    int exit_status = spawn_sync(args);
    printf("exit_status: %d\n", exit_status);
    return 0;
}
