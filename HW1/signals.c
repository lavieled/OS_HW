#include "commands.h"
#include <signals.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

extern pid_t fg_pid; // The PID of the process running in the foreground
extern ParsedCommand* fg_cmd; // The command currently running in the foreground

void ctrl_c_handler(int sig) {
    printf("smash: caught CTRL+C\n");

    if (fg_pid > 0 && fg_pid != getpid()) {
        // If there is a foreground process and it is not smash itself
        if (kill(fg_pid, SIGKILL) == 0) {
            printf("smash: process %d was killed\n", fg_pid);
        } else {
            perror("smash error: failed to kill foreground process");
        }
    }
    fg_pid = -1; // Reset: no foreground process anymore
}

void ctrl_z_handler(int sig) {
    printf("smash: caught CTRL+Z\n");

    if (fg_pid > 0 && fg_pid != getpid()) {
        // If there is a foreground process and it is not smash itself
        if (kill(fg_pid, SIGSTOP) == 0) {
            printf("smash: process %d was stopped\n", fg_pid);
            // Add the stopped process to the jobs table
            Job* job = MALLOC_VALIDATED(Job, sizeof(Job));
            job->pid = fg_pid;
            job->cmd = fg_cmd;
            job->start = time(NULL);
            job->state = STOPPED;
            addJob(job);
        } else {
            perror("smash error: failed to stop foreground process");
        }
    }
    fg_pid = -1; // Reset: no foreground process anymore
}
