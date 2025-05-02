// Fixed smash.c with proper fg_cmd handling for WET1 compliance
#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include "commands.h"
#include "signals.h"

char _line[CMD_LENGTH_MAX];
char lastdir[PATH_MAX];
Job* jobsTable[JOBS_NUM_MAX];
int min_jid_free;
pid_t front_pid;
ParsedCommand* fg_cmd;  // ADDED GLOBAL fg_cmd pointer

int executeExternalCommand(ParsedCommand* cmd);

int main(int argc, char* argv[]) {
    char _cmd[CMD_LENGTH_MAX];
    ParsedCommand* cmd;
    int parse_result;
    int execute_result;

    min_jid_free = 0;
    front_pid = -1;
    fg_cmd = NULL;

    signal(SIGINT, sigTermhandler);
    signal(SIGTSTP, sigStophandler);

    while (1) {
        printf("smash > ");
        fgets(_line, CMD_LENGTH_MAX, stdin);
        strcpy(_cmd, _line);

        cmd = MALLOC_VALIDATED(ParsedCommand, sizeof(ParsedCommand));
        parse_result = parseCommand(_cmd, cmd);
        if (parse_result == INVALID_COMMAND) {
            _line[0] = '\0';
            _cmd[0] = '\0';
            continue;
        }

        updateJobTable();

        execute_result = executeCommand(cmd);
        if (execute_result == 0) {
            executeExternalCommand(cmd);
        }

        _line[0] = '\0';
        _cmd[0] = '\0';
    }
    return 0;
}

int executeExternalCommand(ParsedCommand* cmd) {
    bool bg = !strcmp(cmd->args[cmd->nargs], "&");
    if (bg)
        cmd->args[cmd->nargs] = NULL; 

    int status;
    Job* job = MALLOC_VALIDATED(Job, sizeof(Job));
    job->cmd = cmd;
    fg_cmd = cmd;  

    front_pid = fork();
    if (front_pid == 0) {
        setpgrp();
        execv(cmd->cmd, cmd->args);
        perror("smash error: execl failed");
        freeCMD(cmd);
        exit(SMASH_FAIL);
    } else if (front_pid > 0) {
        if (!bg) {
            waitpid(front_pid, &status, WUNTRACED);
            if (WIFSTOPPED(status)) {
                job->state = STOPPED;
                job->pid = front_pid;
                addJob(job);
            } else {
                freeJob(job);
            }
            fg_cmd = NULL;   
            front_pid = -1;
        } else {
            job->pid = front_pid;
            addJob(job);
            fg_cmd = NULL; 
            front_pid = -1;
        }
    } else {
        perror("smash error: fork failed");
        freeCMD(cmd);
        exit(SMASH_QUIT);
    }
    return 0;
}
