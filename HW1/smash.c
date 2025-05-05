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
	//new
	 bool bg = false;
        char* amp = strrchr(_cmd, '&');
        if (amp && (amp == _cmd || *(amp - 1) == ' ')) {
            bg = true;
        }//check if bg command
        cmd = MALLOC_VALIDATED(ParsedCommand, sizeof(ParsedCommand));
        parse_result = parseCommand(_cmd, cmd);

        if (parse_result == INVALID_COMMAND) {
            free(cmd); // only structure allocated
            continue;
        }

        updateJobTable();
        execute_result = executeCommand(cmd);

        if (execute_result == 0) {
        	int ext_result = executeExternalCommand(cmd, bg);
		if (ext_result != 0) {
        		//free cmd if exec failed and not job
       		 freeCMD(cmd);
   		 }
        } else if (execute_result == 2) {
	    freeCMD(cmd); 
	    break;        // exit loop 
	} else {
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
		execv(cmd->cmd, cmd->args);
		// if exe fail do:
		write(pipefds[1], &errno, sizeof(int));
		close(pipefds[1]);//close
		free(job);
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
			free(job);
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
                free(job);
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
}
