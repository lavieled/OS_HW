// signals.c
#include "signals.h"
#include "commands.h"
//handle CTRL+C/Z signals

extern pid_t front_pid;

void sigTermhandler(int signum) {
    printf("smash: caught CTRL+C\n");

     if (front_pid > 0 && front_pid != getpid()) {
        if (kill(front_pid, SIGKILL) == 0) {
            printf("smash: process %d was killed\n", front_pid);
        } else {
            perror("smash error: kill failed");
        }
	}
	
    
 }//CTRL+C function

 void sigStophandler(int signum) {
    printf("smash: caught CTRL+Z\n");
   if (front_pid > 0 && front_pid != getpid()) {
        if (kill(front_pid, SIGSTOP) == 0) {
            printf("smash: process %d was stopped\n", front_pid);
        } else {
            perror("smash error: stop failed");
        }
    }
}//CTRL+z function

