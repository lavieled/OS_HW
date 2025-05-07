//commands.c
#include "commands.h"

//example function for printing errors from internal commands
void perrorSmash(const char* cmd, const char* msg)
{
    fprintf(stderr, "smash error: %s%s%s\n",
        cmd ? cmd : "",
        cmd ? ": " : "",
        msg);
}


int parseCommand(char* line, ParsedCommand* cmd)
{
    memset(cmd, 0, sizeof(*cmd)); // initialize to prevent garbage
    cmd->line = strdup(line);     // copy original line
    cmd->line[strlen(line) - 1] = '\0'; // strip newline

    char* delimiters = " \t\n";
    char* token = strtok(line, delimiters);
    if (!token)
        return INVALID_COMMAND;

    cmd->cmd = strdup(token);
    cmd->args[0] = strdup(token);
    cmd->nargs = 0;

	while ((token = strtok(NULL, delimiters))) {
	    if (cmd->nargs + 1 >= ARGS_NUM_MAX) {
		// skip parsing to avoid overflow and leak
		break;
	    }
	    ++cmd->nargs;
	    cmd->args[cmd->nargs] = strdup(token);
		}
    return VALID_COMMMAND;
}

// Dispatcher for each command to the apropriate handler:
int executeCommand (ParsedCommand* cmd, bool bg){
	if (!strcmp(cmd->cmd , SHOWPID))		handleShowpid(cmd, bg);
	else if(!strcmp(cmd->cmd , PWD)) 		handlePwd(cmd, bg);
	else if(!strcmp(cmd->cmd , CD)) 		handleCd(cmd, bg);
	else if(!strcmp(cmd->cmd , JOBS)) 		printJobs(cmd, bg);
	else if(!strcmp(cmd->cmd , KILL)) 		handleKill(cmd, bg);
	else if(!strcmp(cmd->cmd , FOREGROUND)) handleForeground(cmd, bg);
	else if(!strcmp(cmd->cmd , BACKGROUND)) handleBackground(cmd, bg);
	else if(!strcmp(cmd->cmd , QUIT))		handleQuit(cmd, bg);
	else if(!strcmp(cmd->cmd , DIFF)) 		handleDiff(cmd, bg);
	else return 0; // in case its not built in command
	return 1; // command found and ran

}

void handleShowpid(ParsedCommand* cmd, bool bg){ // ready
	
	// Input Validation:
	if ((bg && cmd->nargs > 1) || 
		(!bg && cmd->nargs > 0)){
		perrorSmash(cmd->cmd, "expected 0 arguments"); // didnt happened
		return;
	}

	// common resources:
	int smash_pid = getpid();

	// Built-in without & :
	if (!bg){
		printf("smash pid is %d\n", smash_pid);
		return;
	}
	
	// Background ruuning:
	Job* job = MALLOC_VALIDATED(Job, sizeof(Job));
	job->cmd = cmd;
	pid_t pid = fork();
	if (pid > 0){
		job->pid = pid;
		addJob(job);
		return;
	}// father signing his son up to JobTable:
	else if (pid == 0){
		setpgrp();
		printf("smash pid is %d\n", smash_pid);
		freeJob(job);
		exit(SMASH_SUCCESS); // prevent duplicate of the remaining code in main
	}
	else{ 
		// fork() failed
		perror("smash error: showpid failed");
		freeJob(job);
		exit(SMASH_QUIT);

	}
}

void handlePwd(ParsedCommand* cmd, bool bg){ 
	// Input Validation:	
	if ((bg && cmd->nargs > 1) || 
		(!bg && cmd->nargs > 0)){
		perrorSmash(cmd->cmd, "expected 0 arguments");
		return;
	}
	// common resources:
	char path[PATH_MAX];

	// Built in without & :
	if(!bg){
		if (!getcwd(path, sizeof(path))) {
			perror("smash error: pwd failed");		
		}
		else{
			printf("%s\n", path);
		}
		return;
	}

	// background:
	Job* job = MALLOC_VALIDATED(Job, sizeof(Job));
	job->cmd = cmd;
	pid_t pid = fork();
	if (pid > 0){
		// father signing his son up to JobTable:
		job->pid = pid;
		addJob(job);
		return;
	}
	else if (pid == 0){
		setpgrp();
		if (!getcwd(path, sizeof(path))) {
			perror("smash error: getcwd failed");
			freeJob(job);
			exit(SMASH_FAIL);
		}
		printf("%s\n", path);
		freeJob(job);
		exit(SMASH_SUCCESS);
	}
	else {
		// fork() failed
		perror("smash error: fork failed");
		freeJob(job);
		exit(SMASH_QUIT);
	}
}
void handleCd(ParsedCommand* cmd, bool bg){
	// Input Validation:	
	if ((bg && cmd->nargs != 2) || 
		(!bg && cmd->nargs != 1)){
		perrorSmash(cmd->cmd, "expected 1 arguments");
		return;
	}
	// common resources:
	char thisDir[PATH_MAX] = {0};
	if (!getcwd(thisDir, sizeof(thisDir))) {
		perror("smash error: getcwd failed");
		return;
	}

	//run in front:
	if(!bg){
		if (!strcmp(cmd->args[1], "-")){
			if (lastdir[0] == '\0'){ // validation of history
				perrorSmash(cmd->cmd, "old pwd not set");
			}
			else if(chdir(lastdir) != 0){
				//failed
				perror("smash error: chdir failed");
				strcpy(lastdir, thisDir);
				return;
			}
			strcpy(lastdir, thisDir);
			return;
		}
		else if(!(strcmp(cmd->args[1], ".."))){
			//check if currently in /
			if (strcmp(thisDir, "/") != 0){
				strcpy(lastdir, thisDir);
				int i = strlen(thisDir);
				while (lastdir[i] != '/')
				{
					lastdir[i] = '\0';
					i--;
				}
				if(chdir(lastdir) != 0){
					// chdr fail handle
					perror("smash error: chdir failed");
				}
				
			}
			strcpy(lastdir, thisDir);
			return;
		}
		else{
			int error = chdir(cmd->args[1]); // changing the directory and catch the return value
			if (error == 0) strcpy(lastdir, thisDir); // on success update history
			// catch the 2 supported errors event:
			else if (error == ENOENT) perrorSmash(cmd->cmd, " target directory does not exist");
			else if (error == ENOTDIR) perrorSmash(cmd->cmd,strcat(cmd->args[1] ,": not a directory"));
			// catch any other error:
			else perror("smash error: cd failed");

		}
		return;
	}

	//run in background:
	Job* job = MALLOC_VALIDATED(Job, sizeof(Job));
	job->cmd = cmd;
	pid_t pid = fork();
	if (pid > 0){
		// father sgining his son up to JobTable:
		job->pid = pid;
		addJob(job);
		return;
	}
	else if (pid == 0){
		setpgrp();
		if (!strcmp(cmd->args[1], "-")){
			if (lastdir[0] == '\0'){ // validation of history
				perrorSmash(cmd->cmd, "old pwd not set");
			}
			else if(chdir(lastdir) != 0){
				//failed
				perror("smash error: chdir failed");
				freeJob(job);
				exit(SMASH_FAIL);
			}
			strcpy(lastdir, thisDir);
			freeJob(job);
			exit(SMASH_SUCCESS);
		}
		else if(!(strcmp(cmd->args[1], ".."))){
			//check if currently in /
			if (strcmp(thisDir, "/") != 0){
				strcpy(lastdir, thisDir);
				int i = strlen(thisDir);
				while (lastdir[i] != '/')
				{
					lastdir[i] = '\0';
					i--;
				}
				if(chdir(lastdir) != 0){
					// chdr fail handle
					perror("smash error: chdir failed");
					freeJob(job);
					exit(SMASH_FAIL);
				}
				
			}
			strcpy(lastdir, thisDir);
			freeJob(job);
			exit(SMASH_SUCCESS);
		}
		else{
			int error = chdir(cmd->args[1]); // changing the directory and catch the return value
			if (error == 0) strcpy(lastdir, thisDir); // on success update history
			// catch the 2 supported errors event:
			else if (error == ENONET) perrorSmash(cmd->cmd, " target directory does not exist");
			else if (error == ENOTDIR) perrorSmash(cmd->cmd,strcat(cmd->args[1] ,": not a directory"));
			// catch any other error:
			else{
				 perror("smash error: chdir failed");
				 freeJob(job);
				 exit(SMASH_FAIL);
			}
		}
		freeJob(job);
		exit(SMASH_SUCCESS);
		
	}
		
	else{
		// fork failed
		perror("smash error: cd failed");
		freeJob(job);
		exit(SMASH_QUIT);
	} 
	
}

/*============================================================================
 JOB HANDLING EVENTS
 - addJob: on background execution adding a Job to the JobTable, called by the parent
 - printJob: printing all background Jobs
 - updateJobTable: searching for finnished children, and call remove on them, called by smash main befor every command
 - removeJob: get pid and remove the matching job->pid from JobTable, called by updateJobTable
=============================================================================*/
void addJob(Job* job){
	time_t now = time(NULL);
	job->jid = min_jid_free;
	job->start = now;
	job->state = (job->state == STOPPED) ? STOPPED : RUNNING; 
	jobsTable[min_jid_free] = job;
	for (int i = 0; i < JOBS_NUM_MAX; i++){ // searching next value for min job id
		if(jobsTable[i] == NULL){
			min_jid_free = i;
			break;
		}
	}
	return;
}
void printJobs(ParsedCommand* cmd, bool bg){
	// purpose: printing the jobTable:
	// checking input:
	if ((!bg && cmd->nargs!=0) || (bg && cmd->nargs !=1)){
		perrorSmash(cmd->cmd, "expected 0 arguments");
		return;
	}
	// setting up recources:
	time_t now = time(NULL);
	if(!bg){
	// loop printing each job:
	for(int i =0; i< JOBS_NUM_MAX; i++){
		if(jobsTable[i] == NULL) continue; //free place, dont print
		char* status = jobsTable[i]->state == RUNNING ? "" : "(stopped)";
		printf("[%d] %s: %d %.0f secs %s\n", 
		(*jobsTable[i]).jid,
		(*jobsTable[i]).cmd->line,
		(*jobsTable[i]).pid,
		difftime(now, jobsTable[i]->start),
		status);
	}
	return;
	}

	Job* job = MALLOC_VALIDATED(Job, sizeof(Job));
	job->cmd = cmd;
	pid_t pid = fork();
	if(pid > 0){
		job->pid = pid;
		addJob(job);
	}
	else if(pid == 0){
		setpgrp();
		now = time(NULL);
		for(int i =0; i< JOBS_NUM_MAX; i++){
			if(jobsTable[i] == NULL) continue; //free place, dont print
			char* status = jobsTable[i]->state == RUNNING ? "" : "(stopped)";
			printf("[%d] %s: %d %.0f %s\n", 
			jobsTable[i]->jid,
			jobsTable[i]->cmd->line,
			jobsTable[i]->pid,
			difftime(now, jobsTable[i]->start),
			status);
		}
		freeJob(job);
		exit(SMASH_SUCCESS);
	}
	else{
		// fork failed
		perror("smash error: cd failed");
		freeJob(job);
		exit(SMASH_QUIT);
	}

}

void updateJobTable(){
	pid_t cur_pid;
	int status;
	// read all children that commit exit() without waiting
	for (int i = 0; i < JOBS_NUM_MAX; i++)
	{
		if (jobsTable[i] == NULL) continue;
		cur_pid = waitpid(jobsTable[i]->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
		//printf("cur_pid = %d | status = %d\n", cur_pid, status);
		if(cur_pid > 0){
			jobsTable[i]->state = 	WIFSTOPPED(status)? STOPPED:
									WIFCONTINUED(status)? RUNNING:DONE;
			if(jobsTable[i]->state == DONE)
				removeJob(cur_pid);

		}

	}
	
}
/*==========================================================
======================JOB'S STUFF END=======================
============================================================*/

void handleKill(ParsedCommand* cmd, bool bg) {
    if ((cmd->nargs != 2 && !bg) || (cmd->nargs != 3 && bg)){
        perrorSmash(cmd->cmd, "invalid arguments");
        return;
    }

    char* endptr_signal;
    char* endptr_jid;
    int signal = strtol(cmd->args[1], &endptr_signal, 10);
    int jid = strtol(cmd->args[2], &endptr_jid, 10);

    if (*endptr_signal != '\0' || cmd->args[1] == endptr_signal ||
        *endptr_jid != '\0' || cmd->args[2] == endptr_jid) {
        perrorSmash(cmd->cmd, "invalid arguments");
        return;
    }

    if (jid < 0 || jid > JOBS_NUM_MAX || jobsTable[jid] == NULL) {
        int len_msg = strlen("job id ") + strlen(cmd->args[2]) + strlen(" does not exist");
        char* msg = malloc(len_msg + 1);
        if (!msg) {
            perror("smash error: malloc failed");
            return;
        }
        snprintf(msg, len_msg + 1, "job id %s does not exist", cmd->args[2]);
        perrorSmash(cmd->cmd, msg);
        free(msg);
        return;
    }

    if(!bg){
		if (kill(jobsTable[jid]->pid, signal) == -1) {
			perror("smash error: kill failed");
		} else {
			printf("signal %d was sent to pid %d\n", signal, jobsTable[jid]->pid);
		}
		return;
	}

	//background:
	Job* job = MALLOC_VALIDATED(Job, sizeof(Job));
	job->cmd = cmd;
	pid_t pid = fork();
	if(pid > 0){
		// father sgining his son up to JobTable:
		job->pid = pid;
		addJob(job);
		return;
	}
	else if(pid == 0){
		setpgrp();
		if (kill(jobsTable[jid]->pid, signal) == -1) {
			perror("smash error: kill failed");
		} else {
			printf("signal %d was sent to pid %d\n", signal, jobsTable[jid]->pid);
		}
		freeJob(job);
		exit (SMASH_SUCCESS);
	}
	else{
		// fork failed
		perror("smash error: cd failed");
		freeJob(job);
		exit(SMASH_QUIT);
	}

   }

void handleForeground( ParsedCommand* cmd, bool bg){
	// input validation:
	if (bg) {
		return;
	}
	int jid = -1;
	if(cmd->nargs == 0){
		for (int i = JOBS_NUM_MAX-1 ; i >= 0; i--){
			if (jobsTable[i] != NULL){
				jid = i;
				break;
			}
		}
		if (jid == -1){
			perrorSmash(cmd->cmd, "jobs list is empty");
			return;
		}
	}
	else if(cmd->nargs == 1){
		char* endptr;
		jid = strtol(cmd->args[1], &endptr, 10);
		if(endptr == cmd->args[1]){
			perrorSmash(cmd->cmd, "invalid arguments");
			freeCMD(cmd);
			return;
		}
		else if(*endptr != '\0'){
			perrorSmash(cmd->cmd, "invalid arguments");
			return;
		}
		if (jobsTable[jid] == NULL){
			int len_msg = strlen("job id ") + strlen(cmd->args[1]) + strlen(" does not exist");
			char* msg = malloc(sizeof(char)*(len_msg+1));
			snprintf(msg, len_msg+1, "job id %s does not exist", cmd->args[1]);
			perrorSmash(cmd->cmd, msg);
			free(msg);
			return;
		}

	}
	else {
		perrorSmash(cmd->cmd, "invalid arguments");
		return;
	}
	printf("[%d] %s\n", jid, jobsTable[jid]->cmd->cmd);
	if (jobsTable[jid]->state == STOPPED){
		kill(jobsTable[jid]->pid, SIGCONT);
		printf("%s\n", jobsTable[jid]->cmd->line);
	}

	front_pid = jobsTable[jid]->pid; // to catch signals 
	int status;
	int r = waitpid(jobsTable[jid]->pid, &status, WUNTRACED);
	if (r == -1){
		perror("smash error: waitpid failed");
		return;
	}
	if (WIFSTOPPED(status)) {
		// CTRL+Z- move job to the background list
		Job* job = MALLOC_VALIDATED(Job, sizeof(Job));
		job->cmd = jobsTable[jid]->cmd;
		jobsTable[jid]->cmd = NULL;
		job->state = STOPPED;
		job->pid = front_pid;
		removeJob(front_pid);
		addJob(job);
		front_pid = -1;
	}
	else{
		// CTRL+C
		removeJob(front_pid);
		front_pid =-1;
	}
}

void handleBackground( ParsedCommand* cmd, bool bg){
	// input validation:
	if (bg) {
		return;
	}
	int jid = -1;
	if(cmd->nargs == 0){
		for (int i = JOBS_NUM_MAX - 1; i >= 0; i--) {
			if (jobsTable[i] != NULL) {
			jid = i;
			break;
			}
		}
		if (jid == -1){
			perrorSmash(cmd->cmd, "jobs list is empty");
			return;
		}
		else if (jobsTable[jid]->state != STOPPED){
			int job_num_len = (jid < 10) ? 1 : (jid == 100) ? 3 : 2;
			int len_msg = strlen("job id ") + job_num_len + strlen(" is already in background");
			char* msg = malloc(sizeof(char)*(len_msg+1));
			snprintf(msg, len_msg+1, "job id %d is already in background", jid);
			perrorSmash(cmd->cmd, msg);
			free(msg);
			return;
		}
	}
	else if(cmd->nargs == 1){
		char* endptr;
		jid = strtol(cmd->args[1], &endptr, 10);
		if(endptr == cmd->args[1]){
			perrorSmash(cmd->cmd, "invalid arguments");
			return;
		}
		else if(*endptr != '\0'){
			perrorSmash(cmd->cmd, "invalid arguments");
			return;
		}
		if (jobsTable[jid] == NULL){
			int len_msg = strlen("job id ") + strlen(cmd->args[1]) + strlen(" does not exist");
			char* msg = malloc(sizeof(char)*(len_msg+1));
			snprintf(msg, len_msg+1, "job id %s does not exist", cmd->args[1]);
			perrorSmash(cmd->cmd, msg);
			free(msg);
			return;
		}
		else if(jobsTable[jid]->state != STOPPED){
			int len_msg = strlen("job id ") + strlen(cmd->args[1]) + strlen(" is already in background");
			char* msg = malloc(sizeof(char)*(len_msg+1));
			snprintf(msg, len_msg+1, "job id %s is already in background", cmd->args[1]);
			perrorSmash(cmd->cmd, msg);
			free(msg);
			return;
		}

		}
		else {
			perrorSmash(cmd->cmd, "invalid arguments");
			return;
		}
		
		printf("[%d] %s: %d\n", jid, jobsTable[jid]->cmd->line, jobsTable[jid]->pid);
		kill(jobsTable[jid]->pid, SIGCONT);
	
}//handle background func

void handleQuit(ParsedCommand* cmd, bool bg){
	// input validation:
	if (bg){
		perrorSmash(cmd->cmd, "invalid arguments");
		return;
	}
	else if (cmd->nargs > 1){
		perrorSmash(cmd->cmd, "invalid arguments");
		return;
	}
	else if (cmd->nargs == 1 && strcmp(cmd->args[1], "kill") == 0) {
        for (int i = 0; i < JOBS_NUM_MAX; i++) {
            if (jobsTable[i] != NULL) {
                kill(jobsTable[i]->pid, SIGKILL);
                freeJob(jobsTable[i]);
                jobsTable[i] = NULL;
          	  }
        	}
        	freeCMD(cmd);
        	exit(0);		
	}
	else if (cmd->nargs == 1 ){
		perrorSmash(cmd->cmd, "invalid arguments");
		return;
	}

	for (int i = 0; i < JOBS_NUM_MAX; i++)
	{
		if(jobsTable[i] == NULL) continue;
		printf("[%d] %s - ", jobsTable[i]->jid, jobsTable[i]->cmd->cmd);
		kill(jobsTable[i]->pid, SIGTERM);
		printf("sending SIGTERM... ");
		fflush(stdout); // print without waiting
		sleep(3);
		if (waitpid(jobsTable[i]->pid, NULL, WNOHANG) == 0){
			kill(jobsTable[i]->pid, SIGKILL);
			printf("sending SIGKILL... done");
		}
		else
			printf("done");
		
		printf("\n");
		freeJob(jobsTable[i]);
        	jobsTable[i] = NULL;
		}
	freeCMD(cmd);
	exit(0);
}//handle quit function

void handleDiff(ParsedCommand* cmd, bool bg){
	//input validation:
	if ((!bg && cmd->nargs != 2) || (bg && cmd->nargs != 3)){
		perrorSmash(cmd->cmd, "expected 2 arguments");
		return;
	}
	
	// check for valid path to files:
	struct stat st;
	if (stat(cmd->args[1], &st) < 0){
		if (errno == ENOENT){
			perrorSmash(cmd->cmd, "expected valid paths for files");
			return;
		}
		else if(errno != 0){
			perror("smash error: diff failed");
			return;
		}//no path
		
	}
	else if(S_ISDIR(st.st_mode)){
		perrorSmash(cmd->cmd, "paths are not files"); // doesnt work
		return ;
	}
	else if(S_ISREG(st.st_mode)){}
	else{
		perror("smash error: paths not a file but not supported error either"); // doesnt work
		return ;
	}
	if (stat(cmd->args[2], &st) < 0){
		if (errno == ENOENT){
			perrorSmash(cmd->cmd, "expected valid paths for files"); // works 
			return;
		}
		else if(errno != 0){
			perror("smash error: diff failed");
			return;
		}//no path
	}
	else if(S_ISDIR(st.st_mode)){
		perrorSmash(cmd->cmd, "paths are not files"); // doesnt work
		return ;
	}
	else if(S_ISREG(st.st_mode)){}
	else{
		perror("smash error: paths not a file but not supported error either"); // doesnt work
		return ;
	}

	// done checking, fetching the files:
	FILE *fp1 = fopen(cmd->args[1], "r");
    FILE *fp2 = fopen(cmd->args[2], "r");
	if (fp1 == NULL || fp2 == NULL) {
		perror("smash error: fopen failed"); // doesnt work
        return ;
    }

	// checking if equals:
	char c1;
	char c2;
	if(!bg){
	do
	{
		c1 = fgetc(fp1);
		c2 = fgetc(fp2);
		if (c1 != c2)
			{
				printf("1\n");
				fclose(fp1);
				fclose(fp2);
				return;
			}
	} while (c1 != EOF && c2 != EOF);
	
	if (c1 == EOF && c2 == EOF){
		printf("0\n");
	}
	else{
		printf("1\n");
	}
	fclose(fp1);
	fclose(fp2);
    return;
	}

	// background
	Job* job = MALLOC_VALIDATED(Job, sizeof(Job));
	job->cmd = cmd;
	pid_t pid = fork();
	if(pid > 0){
		job->pid = pid;
		addJob(job);
		fclose(fp1);
		fclose(fp2);
		return;
	}
	else if(pid == 0){
		do
	{
		c1 = fgetc(fp1);
		c2 = fgetc(fp2);
		if (c1 != c2)
			{
				printf("1\n");
				fclose(fp1);
				fclose(fp2);
				freeJob(job);
				exit(SMASH_SUCCESS);
			}
	} while (c1 != EOF && c2 != EOF);
	
	if (c1 == EOF && c2 == EOF){
		printf("0\n");
	}
	else{
		printf("1\n");
	}
	fclose(fp1);
	fclose(fp2);
	freeJob(job);
    exit(SMASH_SUCCESS);

	}

	else{
		perror("smash error: fork failed");
		freeJob(job);
		fclose(fp1);
		fclose(fp2);
		exit(SMASH_QUIT);
	}

}


/*==================
	FREE FUNC
==================*/

void freeCMD(ParsedCommand* cmd){
    if (cmd == NULL) return;
	for (int i = 0; i < ARGS_NUM_MAX; i++) {
    	if (cmd->args[i] != NULL){
			free(cmd->args[i]); // free strdup
		} 
	}//added for saftey
    if(cmd->cmd) free(cmd->cmd); // free strdup

    if(cmd->line) free(cmd->line); // free strdup

    free(cmd); // free the struct
}

void freeJob(Job* job) {
    if (job->cmd) {
        freeCMD(job->cmd);
        job->cmd = NULL;
    }
    free(job);
}//free job mem
void removeJob(pid_t pid){
	int i = 0;
	for ( ; i < JOBS_NUM_MAX; i++)
	{
		if(jobsTable[i] != NULL && jobsTable[i]->pid == pid){
			freeJob(jobsTable[i]);
			jobsTable[i] = NULL;
			min_jid_free = (min_jid_free < i) ? min_jid_free : i ;
			break;
		}
	}
	
}//remove job from jobTable
