//smash.c

/*=============================================================================
* includes, defines, usings
=============================================================================*/
#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <fcntl.h>
#include "commands.h"
#include "signals.h"

/*=============================================================================
* classes/structs declarations
=============================================================================*/
void install_handler(int signo, void (*handler)(int)) {
    struct sigaction sa = {0};
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(signo, &sa, NULL);
}
/*=============================================================================
* global variables & data structures
=============================================================================*/
char _line[CMD_LENGTH_MAX];
char lastdir[PATH_MAX];
Job* jobsTable[JOBS_NUM_MAX];
int min_jid_free;
pid_t front_pid;

/*============================================================================
* global func
============================================================================*/
int executeExternalCommand(ParsedCommand* cmd, bool bg);

/*=============================================================================
* main function
=============================================================================*/
int main(int argc, char* argv[])
{
    // Initializations
    char _cmd[CMD_LENGTH_MAX];
    ParsedCommand* cmd;
    int parse_result;
    int execute_result;
    min_jid_free = 0;
    front_pid = -1;
    lastdir[0] = '\0';

    // Install signal handlers
    install_handler(SIGINT, sigTermhandler);
    install_handler(SIGTSTP, sigStophandler);

     while (1) {
        printf("smash > ");
        if (!fgets(_line, CMD_LENGTH_MAX, stdin)) {
            break; // handle EOF (Ctrl+D)
        }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       

        strcpy(_cmd, _line);  // copy before modifying
	bool bg = false;
        char* amp = strrchr(_cmd, '&');
        if (amp && (amp == _cmd || *(amp - 1) == ' ')) {
            bg = true;
        }//check if bg command
        cmd = MALLOC_VALIDATED(ParsedCommand, sizeof(ParsedCommand));
        parse_result = parseCommand(_cmd, cmd);

        if (parse_result == INVALID_COMMAND) {
            freeCMD(cmd); // only structure allocated
		cmd = NULL;
            continue;
        }

        updateJobTable();
        execute_result = executeCommand(cmd, bg);

	if (execute_result == 2) {
	    freeCMD(cmd);        // Exit command
	    break;
	}

	if (execute_result == 0) {
	    // Not internal -> external
	    int ext_result = executeExternalCommand(cmd, bg);
	    if (ext_result != 0 && !bg) {
		// Failed external cmd, not added to jobs
		cmd = NULL;//safty
	    }
	} else if (!bg) {
	    // Successful internal command
	    freeCMD(cmd);
	}
	}
    return 0;
}


int executeExternalCommand(ParsedCommand* cmd, bool bg){
		
	int status;
	Job* job = MALLOC_VALIDATED(Job, sizeof(Job));
	job->cmd = cmd; 
	// for handling exec failed:
	int pipefds[2];
	if (pipe(pipefds)) {
        perror("smash error: pipe failed");
        return 1;
    }
    if (fcntl(pipefds[1], F_SETFD, FD_CLOEXEC)) {
        perror("smash error: fcntl failed");
        return 1;
    }
	front_pid = fork();
	if (front_pid == 0){
		setpgrp();
		close(pipefds[0]); // we dont read in child
		if (bg)
			cmd->args[cmd->nargs] = '\0';
		execv(cmd->cmd, cmd->args);
		// if exe fail do:
		write(pipefds[1], &errno, sizeof(int));
		close(pipefds[1]);//close
		freeJob(job);
		exit(1);
	}
	else if(front_pid > 0){
		close(pipefds[1]); // we dont write in parent
		int err;
		int r;
		do {
			r = read(pipefds[0], &err, sizeof err);
		} while (r == -1 && errno == EINTR);
		
		if (r == sizeof err) {
			// CHILD fails execv
			fprintf(stderr, "smash error: execv failed: %s\n", strerror(err));
			freeJob(job);
			return 1;
		}
		close(pipefds[0]);
		if (!bg) {
            int r = waitpid(front_pid, &status, WUNTRACED);
            if (r == -1) {
                perror("smash error: waitpid failed");
            } else if (WIFSTOPPED(status)) {
                job->state = STOPPED;
                job->pid = front_pid;
                addJob(job);
            } else {
                freeJob(job);
            }
            front_pid = -1;

		}
		else{ // background
			job->state = RUNNING;
			job->pid = front_pid;
			front_pid = -1;
			addJob(job);
		}
	}
	else{
		perror("smash error: fork failed");
		freeJob(job);
		exit(SMASH_QUIT);
	}
	return 0;
}//function to run external commads
