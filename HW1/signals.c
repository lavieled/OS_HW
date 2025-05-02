// signals.c
#include "signals.h"
#include "commands.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

extern pid_t fg_pid;
extern ParsedCommand* fg_cmd;

void ctrl_c_handler(int sig) {
    printf("smash: caught CTRL+C\n");

    if (fg_pid > 0 && fg_pid != getpid()) {
        if (kill(fg_pid, SIGKILL) == 0) {
            printf("smash: process %d was killed\n", fg_pid);
        } else {
            perror("smash error: kill failed");
        }
    }

    fg_pid = -1;
    fg_cmd = NULL;
}

void ctrl_z_handler(int sig) {
    printf("smash: caught CTRL+Z\n");

    if (fg_pid > 0 && fg_pid != getpid()) {
        if (kill(fg_pid, SIGSTOP) == 0) {
            printf("smash: process %d was stopped\n", fg_pid);

            Job* job = MALLOC_VALIDATED(Job, sizeof(Job));
            job->pid = fg_pid;
            job->cmd = cloneParsedCommand(fg_cmd); // copy for mem leak?
            job->start = time(NULL);
            job->state = STOPPED;
            addJob(job);
        } else {
            perror("smash error: kill failed");
        }
    }

    fg_pid = -1;
    fg_cmd = NULL;
}
