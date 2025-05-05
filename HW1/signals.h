#ifndef SIGNALS_H
#define SIGNALS_H

/*=============================================================================
* includes, defines, usings
=============================================================================*/
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>  // pid_t
#include <unistd.h>     // fork(), execvp(), getpid()
#include <sys/wait.h>   // waitpid()

/*=============================================================================
* global functions
=============================================================================*/

void sigTermhandler(int signum);
void sigStophandler(int signum);

/*=============================================================================
* global variables
=============================================================================*/

extern pid_t front_pid;

#endif //__SIGNALS_H__
