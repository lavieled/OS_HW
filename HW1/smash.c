// smash.c

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>

#include "commands.h"
#include "signals.h"

// =========================================================================
// Global variables
// =========================================================================
char _line[CMD_LENGTH_MAX];
pid_t fg_pid = -1;                // foreground process ID
ParsedCommand* fg_cmd = NULL;     // pointer to foreground command

// =========================================================================
// Main
// =========================================================================
int main(int argc, char* argv[]) {
    char _cmd[CMD_LENGTH_MAX];
    ParsedCommand* cmd;
    int parse_result;

    // ======================
    // Signal handlers setup
    // ======================
    struct sigaction sa;

    // CTRL+C (SIGINT)
    sa.sa_handler = ctrl_c_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("smash error: sigaction failed");
        exit(1);
    }

    // CTRL+Z (SIGTSTP)
    sa.sa_handler = ctrl_z_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("smash error: sigaction failed");
        exit(1);
    }

    // ======================
    // Main loop
    // ======================
    while (1) {
        printf("smash > ");
        fflush(stdout);

        // Read command
        if (!fgets(_line, CMD_LENGTH_MAX, stdin)) {
            continue;
        }

        strcpy(_cmd, _line);
        cmd = MALLOC_VALIDATED(ParsedCommand, sizeof(ParsedCommand));
        parse_result = parseCommand(_cmd, cmd);

        if (parse_result == INVALID_COMMAND) {
            _line[0] = '\0';
            _cmd[0] = '\0';
            continue;
        }

        updateJobTable(); // ✅ always update finished jobs

        // Check if built-in command
        int execute_result = executeCommand(cmd);
        if (execute_result == 0) {
            // ======================
            // External Command
            // ======================

            // Check for background command (ends with "&")
            bool bg = false;
            if (cmd->nargs >= 0 && cmd->args[cmd->nargs] &&
                strcmp(cmd->args[cmd->nargs], "&") == 0) {
                bg = true;
                cmd->args[cmd->nargs] = NULL; // remove "&" for execvp
            }

            // Fork the process
            pid_t child_pid = fork();
            if (child_pid == -1) {
                perror("smash error: fork failed");
                freeCMD(cmd);
                continue;
            }

            if (child_pid == 0) {
                // Child process
                setpgrp(); // separate from smash's group
                execvp(cmd->cmd, cmd->args);
                // If execvp fails:
                perrorSmash("external", "cannot find program");
                freeCMD(cmd);
                exit(1);
            } else {
                // Parent process
                if (bg) {
                    Job* job = MALLOC_VALIDATED(Job, sizeof(Job));
                    job->pid = child_pid;
                    job->cmd = cmd;
                    job->start = time(NULL);
                    job->state = RUNNING;
                    addJob(job);
                } else {
                    // Foreground
                    fg_pid = child_pid;
                    fg_cmd = cmd;
                    waitpid(child_pid, NULL, WUNTRACED);
                    fg_pid = -1;
                    fg_cmd = NULL;
                    freeCMD(cmd); // only free if not cloned/stopped
                }
            }
        }

        // Clear input buffers
        _line[0] = '\0';
        _cmd[0] = '\0';
    }

    return 0;
}
