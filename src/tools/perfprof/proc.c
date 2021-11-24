#include "proc.h"

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
