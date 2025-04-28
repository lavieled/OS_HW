#ifndef COMMANDS_H
#define COMMANDS_H
/*=============================================================================
* includes, defines, usings
=============================================================================*/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>  // pid_t
#include <unistd.h>     // fork(), execvp(), getpid()
#include <sys/wait.h>   // waitpid()
#include <string.h>  // strtok, strcmp, strdup
#include <linux/limits.h> // PATH_MAX
#include <errno.h> //
#include <time.h>
#include <sys/stat.h>


#define CMD_LENGTH_MAX 120
#define ARGS_NUM_MAX 20
#define JOBS_NUM_MAX 100
#define CHILD_SUBGROUP 12
// Switch - Case commands dispatcher:
#define SHOWPID "showpid"
#define PWD "pwd"
#define CD "cd"
#define JOBS "jobs"
#define KILL "kill"
#define FOREGROUND "fg"
#define BACKGROUND "bg"
#define QUIT "quit"
#define DIFF "diff"

/*=============================================================================
* error handling - some useful macros and examples of error handling,
* feel free to not use any of this
=============================================================================*/
#define ERROR_EXIT(msg) \
    do { \
        fprintf(stderr, "%s: %d\n%s", __FILE__, __LINE__, msg); \
        exit(1); \
    } while(0);

static inline void* _validatedMalloc(size_t size)
{
    void* ptr = malloc(size);
    if(!ptr) ERROR_EXIT("malloc");
    return ptr;
}

// example usage:
// char* bufffer = MALLOC_VALIDATED(char, MAX_LINE_SIZE);
// which automatically includes error handling
#define MALLOC_VALIDATED(type, size) \
    ((type*)_validatedMalloc((size)))


/*=============================================================================
* error definitions
=============================================================================*/
typedef enum  {
	INVALID_COMMAND = 0,
    VALID_COMMMAND =1,
	//feel free to add more values here or delete this
} ParsingError;

typedef enum {
	SMASH_SUCCESS = 0, // for every exit on child proccess after expected behavior (including supported failors)
	SMASH_QUIT, // exit on smash proccess that doesnt allow the smash to continue (unsuccesful fork/ malloc )
	SMASH_FAIL // when a syscall fail but the smash can continue
	//feel free to add more values here or delete this
} CommandResult;

/*=============================
* Structs for data handling:
==============================*/
// Struct for passing parsed CMD
typedef struct {
    char* line; // original command line as given from user
    char* cmd; // the command itself
    char* args[ARGS_NUM_MAX]; // al args given args[0] = cmd
    int nargs; // number of args given
} ParsedCommand;

// Struct for handling jobs
typedef enum { RUNNING, STOPPED, DONE } job_state;

typedef struct {
    int jid;
    pid_t pid;
    time_t start;
    job_state state;
    ParsedCommand* cmd;
} Job;



/*=============================================================================
* global functions
=============================================================================*/
int parseCommand(char* line, ParsedCommand* cmd);
int executeCommand (ParsedCommand* cmd);
void handleShowpid( ParsedCommand* cmd, bool bg);
void handlePwd( ParsedCommand* cmd, bool bg);
void handleCd( ParsedCommand* cmd, bool bg);
void printJobs( ParsedCommand* cmd, bool bg);
void addJob(Job* job);
void updateJobTable();
void handleKill( ParsedCommand* cmd, bool bg);
void handleForeground( ParsedCommand* cmd, bool bg);
void handleBackground( ParsedCommand* cmd, bool bg);
void freeCMD(ParsedCommand* cmd);
void removeJob(pid_t pid);
void freeJob(Job* job);
void handleQuit(ParsedCommand* cmd, bool bg);
void handleDiff(ParsedCommand* cmd, bool bg);
/*
* global variables
*/
static char lastdir[PATH_MAX]= "\0";
static Job* jobsTable[JOBS_NUM_MAX];
static int min_jid_free = 0;
#endif //COMMANDS_H
