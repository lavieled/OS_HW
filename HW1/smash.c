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
			// command isn't biult-in, handle here:
		}
		//initialize buffers for next command
		_line[0] = '\0';
		_cmd[0] = '\0';
		updateJobTable();
	}

	return 0;
}
