//smash.c

/*=============================================================================
* includes, defines, usings
=============================================================================*/
#include <stdlib.h>

#include "commands.h"
#include "signals.h"

/*=============================================================================
* classes/structs declarations
=============================================================================*/

/*=============================================================================
* global variables & data structures
=============================================================================*/
char _line[CMD_LENGTH_MAX];

/*=============================================================================
* main function
=============================================================================*/
int main(int argc, char* argv[])
{
	char _cmd[CMD_LENGTH_MAX];
	ParsedCommand* cmd;
	int parse_result;
	int execute_result;
	while(1) {
		printf("smash > ");
		fgets(_line, CMD_LENGTH_MAX, stdin);
		strcpy(_cmd, _line);
		//execute command
		cmd = MALLOC_VALIDATED(ParsedCommand, sizeof(ParsedCommand));
		parse_result = parseCommand(_cmd, cmd);
		if (parse_result == INVALID_COMMAND){
			//initialize buffers for next command
			_line[0] = '\0';
			_cmd[0] = '\0';
			continue;
		}
		execute_result = executeCommand(cmd);
		if (execute_result == 0){
			//Not a built-in command, try run external
			pid_t child_pid = fork();
			if (child_pid == -1) {
			    //fork failed
			    //perror("smash error: fork failed");
			    freeCMD(cmd);
			    continue;
			}
			else if (child_pid == 0) {
			    // Child process
			    setpgrp(); // Detach the child from smash's group
			    execvp(cmd->cmd, cmd->args);
			    // If we get here execvp failed:
			    perrorSmash("external", "cannot find program");
			    freeCMD(cmd);
			    exit(1); // Exit with failure
			}
			else {
			    // Parent process
			    bool bg = (cmd->nargs >= 0) && (strcmp(cmd->args[cmd->nargs], "&") == 0);
			    if (bg) {
			        // Background job: Add to jobs list
			        Job* job = MALLOC_VALIDATED(Job, sizeof(Job));
			        job->pid = child_pid;
			        job->cmd = cmd;
			        addJob(job);
			    }
			    else {
			        // Foreground job: wait for it
			        fg_pid = child_pid;
			        fg_cmd = cmd;
			        waitpid(child_pid, NULL, 0);
			        fg_pid = -1;
			        fg_cmd = NULL;
			        freeCMD(cmd);
			    }
			}

		}
		//initialize buffers for next command
		_line[0] = '\0';
		_cmd[0] = '\0';
		updateJobTable();
	}

	return 0;
}
