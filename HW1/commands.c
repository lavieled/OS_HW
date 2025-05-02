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


//example function for parsing commands
int parseCommand(char* line, ParsedCommand* cmd)
{
	memset(cmd, 0, sizeof(*cmd)); // initializing here prevent random crashes, dont change
	cmd->line = strdup(line);
	cmd->line[strlen(line)-1] = '\0';
	char* delimiters = " \t\n"; //parsing should be done by spaces, tabs or newlines
	char* token  = strtok(line, delimiters); //read strtok documentation - parses string by delimiters
	if(!token)
		return INVALID_COMMAND; //this means no tokens were found, most like since command is invalid
	
	cmd->cmd = strdup(token);
	cmd->args[0] = strdup(token); //first token before spaces/tabs/newlines should be command name
	cmd->nargs =0;
	for(int i = 1; i < ARGS_NUM_MAX; i++)
	{
		token = strtok(NULL, delimiters);
        if (!token) break;
        cmd->args[i] = strdup(token);
		cmd->nargs++;
	}

	return VALID_COMMMAND;
}



// Dispatcher for each command to the apropriate handler:
int executeCommand (ParsedCommand* cmd){
	//printf("starting execute\n");
	bool bg = !strcmp(cmd->args[cmd->nargs],"&");
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
		//printf("show pid fail, more than 0 args\n");
		perrorSmash(cmd->cmd, "expected 0 arguments"); // didnt happened
		return;
	}

	// common resources:
	int smash_pid = getpid();

	// Built-in without & :
	if (!bg){
		printf("smash pid is %d\n", smash_pid);
		freeCMD(cmd); // ran and wont be used
		return;
	}
	
	// Background ruuning:
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
		printf("smash pid is %d\n", smash_pid);
		exit(SMASH_SUCCESS); // prevent duplicate of the remaining code in main
	}
	else{ 
		// fork() failed
		perror("smash error: showpid failed");
		freeJob(job);
		exit(SMASH_QUIT);

	}
}

void handlePwd(ParsedCommand* cmd, bool bg){ //ready
	// Input Validation:	
	if ((bg && cmd->nargs > 1) || 
		(!bg && cmd->nargs > 0)){
		//printf("show pid fail, more than 0 args\n");
		perrorSmash(cmd->cmd, "expected 0 arguments");
		return;
	}
	// common resources:
	char path[PATH_MAX];

	// Biult in without & :
	if(!bg){
		if (!getcwd(path, sizeof(path))) {
			perror("smash error: pwd failed");		
		}
		else{
			printf("%s\n", path);
		}
		freeCMD(cmd); // ran and wont be used
		return;
	}

	// background:
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
		if (!getcwd(path, sizeof(path))) {
			perror("smash error: getcwd failed");
			exit(SMASH_FAIL);
		}
		printf("%s\n", path);
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
				freeCMD(cmd);
				strcpy(lastdir, thisDir);
				return;
			}
			strcpy(lastdir, thisDir);
			freeCMD(cmd); // ran and wont be used
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
			freeCMD(cmd);
			return;
		}
		else{
			int error = chdir(cmd->args[1]); // changing the directory and catch the return value
			if (error == 0) strcpy(lastdir, thisDir); // on success update history
			// catch the 2 supported errors event:
			else if (error == ENONET) perrorSmash(cmd->cmd, " target directory does not exist");
			else if (error == ENOTDIR) perrorSmash(cmd->cmd,strcat(cmd->args[1] ,": not a directory"));
			// catch any other error:
			else perror("smash error: cd failed");

		}
		freeCMD(cmd);
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
				freeCMD(cmd);
				exit(SMASH_FAIL);
			}
			strcpy(lastdir, thisDir);
			freeCMD(cmd); // ran and wont be used
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
					exit(SMASH_FAIL);
				}
				
			}
			strcpy(lastdir, thisDir);
			freeCMD(cmd);
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
				 exit(SMASH_FAIL);
			}
		}
		freeCMD(cmd);
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
	job->jid = min_jid_free +1;
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
		freeCMD(cmd);
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
	freeCMD(cmd);
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
	pid_t pid;
	int status;
	job_state new_state;
	// read all children that commit exit() without waiting
	while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
		if (WIFSTOPPED(status)) {
			new_state = STOPPED;
		}
		else if (WIFCONTINUED(status)) { // update status to running
			new_state = RUNNING;
		}
		else if (WIFEXITED(status) || WIFSIGNALED(status)) { // remove after exit
			removeJob(pid);
			continue;
		}
		for (int i = 0; i < JOBS_NUM_MAX; i++)
		{
			if(jobsTable[i] != NULL && jobsTable[i]->pid == pid){
				jobsTable[i]->state = new_state;
				break; 
			}
		}
		
	}

	
}
/*==========================================================
======================JOB'S STUFF END=======================
============================================================*/

void handleKill( ParsedCommand* cmd, bool bg){
	int signal;
	int jid;
	char* endptr;
	// input validation:
	if((!bg && cmd->nargs != 2) || (bg && cmd->nargs != 3)){
		perrorSmash(cmd->cmd, "invalid arguments");

	}
	else{
		signal = strtol(cmd->args[1], &endptr, 10);
		if(endptr == cmd->args[1]){
			perrorSmash(cmd->cmd, "invalid arguments");
			return;
		}
		else if(*endptr != '\0'){
			perrorSmash(cmd->cmd, "invalid arguments");
			return;
		}
		jid = strtol(cmd->args[2], &endptr, 10);
		if(endptr == cmd->args[2]){
			perrorSmash(cmd->cmd, "invalid arguments");
			return;
		}
		else if(*endptr != '\0'){
			perrorSmash(cmd->cmd, "invalid arguments");
			return;
		}
	}
	if (jobsTable[jid-1] == NULL){
		int len_msg = strlen("job id ") + strlen(cmd->args[2]) + strlen(" does not exist");
		char* msg = malloc(sizeof(char)*(len_msg+1));
		snprintf(msg, len_msg+1, "job id %s does not exist", cmd->args[2]);
		perrorSmash(cmd->cmd, msg);
		free(msg);
		return;
	}

	if(!bg){
		int error = kill(jobsTable[jid-1]->pid, signal);
		if (error == 0){
			printf ("signal %d was sent to pid %d\n", signal, jobsTable[jid-1]->pid);
		}
		else{
			perror("smash error: kill failed");
		}
		freeCMD(cmd);
		return;
	}

	// background
	Job* job = MALLOC_VALIDATED(Job, sizeof(Job));
	job->cmd = cmd;
	pid_t pid = fork();
	if(pid > 0){
		job->pid = pid;
		addJob(job);
	}
	else if(pid == 0){
		setpgrp();
		
		int error = kill(jobsTable[jid-1]->pid, signal);
		if (error == 0){
			printf ("signal %d was sent to pid %d\n", signal, jobsTable[jid-1]->pid);
			freeCMD(cmd);
			exit(SMASH_SUCCESS);
		}
		else{
			perror("smash error: kill failed");
			freeCMD(cmd);
			exit(SMASH_FAIL);
		}
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
		freeCMD(cmd);
		return;
	}
	int jid = -1;
	if(cmd->nargs == 0){
		for (int i = JOBS_NUM_MAX; i >= 0; i--){
			if (jobsTable[i] != NULL){
				jid = i+1;
				break;
			}
		}
		if (jid == -1){
			perrorSmash(cmd->cmd, "jobs list is empty");
			freeCMD(cmd);
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
			freeCMD(cmd);
			return;
		}
		if (jobsTable[jid-1] == NULL){
			int len_msg = strlen("job id ") + strlen(cmd->args[1]) + strlen(" does not exist");
			char* msg = malloc(sizeof(char)*(len_msg+1));
			snprintf(msg, len_msg+1, "job id %s does not exist", cmd->args[1]);
			perrorSmash(cmd->cmd, msg);
			free(msg);
			freeCMD(cmd);
			return;
		}

	}
	else {
		perrorSmash(cmd->cmd, "invalid arguments");
		freeCMD(cmd);
		return;
	}
	printf("[%d] %s\n", jid, jobsTable[jid-1]->cmd->cmd);
	if (jobsTable[jid-1]->state == STOPPED){
		kill(jobsTable[jid-1]->pid, SIGCONT);
	}
	printf("%s\n", jobsTable[jid-1]->cmd->line);
	waitpid(jobsTable[jid-1]->pid,NULL , 0);
	removeJob(jobsTable[jid-1]->pid);
	freeCMD(cmd);
	

}
void handleBackground( ParsedCommand* cmd, bool bg){
	// input validation:
	if (bg) {
		freeCMD(cmd);
		return;
	}
	int jid = -1;
	if(cmd->nargs == 0){
		for (int i = JOBS_NUM_MAX; i >= 0; i--){
			if (jobsTable[i] != NULL){
				jid = i+1;		
				if(jobsTable[i]->state == STOPPED)		
					break;
			}
		}
		if (jid == -1){
			perrorSmash(cmd->cmd, "jobs list is empty");
			freeCMD(cmd);
			return;
		}
		else if (jobsTable[jid]->state != STOPPED){
			perrorSmash(cmd->cmd, "there are no stopped jobs to resume");
			freeCMD(cmd);
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
			freeCMD(cmd);
			return;
		}
		if (jobsTable[jid-1] == NULL){
			int len_msg = strlen("job id ") + strlen(cmd->args[1]) + strlen(" does not exist");
			char* msg = malloc(sizeof(char)*(len_msg+1));
			snprintf(msg, len_msg+1, "job id %s does not exist", cmd->args[1]);
			perrorSmash(cmd->cmd, msg);
			free(msg);
			freeCMD(cmd);
			return;
		}
		else if(jobsTable[jid-1]->state != STOPPED){
			int len_msg = strlen("job id ") + strlen(cmd->args[1]) + strlen(" is already in background");
			char* msg = malloc(sizeof(char)*(len_msg+1));
			snprintf(msg, len_msg+1, "job id %s is already in background", cmd->args[1]);
			perrorSmash(cmd->cmd, msg);
			free(msg);
			freeCMD(cmd);
			return;
		}

	}
	else {
		perrorSmash(cmd->cmd, "invalid arguments");
		freeCMD(cmd);
		return;
	}
	printf("[%d] %s\n", jid, jobsTable[jid-1]->cmd->cmd);
	kill(jobsTable[jid-1]->pid, SIGCONT);
	freeCMD(cmd);
	
}

void handleQuit(ParsedCommand* cmd, bool bg){
	if (bg){
		perrorSmash(cmd->cmd, "invalid arguments");
		freeCMD(cmd);
		return;
	}
	else if (cmd->nargs > 1){
		perrorSmash(cmd->cmd, "invalid arguments");
		freeCMD(cmd);
		return;
	}
	else if (cmd->nargs == 1 && strcmp(cmd->args[1], "kill") == 0){ // quit
		for (int i = 0; i < JOBS_NUM_MAX; i++)
		{
			if(jobsTable[i] != NULL){
				kill(jobsTable[i]->pid, SIGKILL);
				removeJob(jobsTable[i]->pid);
			}
		}
		freeCMD(cmd);
		exit(0);		
	}
	else if (cmd->nargs == 1 ){
		perrorSmash(cmd->cmd, "invalid arguments");
		freeCMD(cmd);
		return;
	}

	for (int i = 0; i < JOBS_NUM_MAX; i++)
	{
		if(jobsTable[i] == NULL) continue;
		printf("[%d] %s - ",jobsTable[i]->jid, jobsTable[i]->cmd->cmd);
		kill(jobsTable[i]->pid, SIGTERM);
		printf("sending SIGTERM... ");
		sleep(3);
		if (waitpid(jobsTable[i]->pid, NULL, WNOHANG) == 0){
			kill(jobsTable[i]->pid, SIGKILL);
			printf("sending SIGKILL... done");
		}
		else
			printf("done");
		
		printf("\n");
		removeJob(jobsTable[i]->pid);
	}
	freeCMD(cmd);
	exit(0);
}

void handleDiff(ParsedCommand* cmd, bool bg){
	if ((!bg && cmd->nargs != 2) || (bg && cmd->nargs != 3)){
		perrorSmash(cmd->cmd, "expected 2 arguments");
		freeCMD(cmd);
		return;
	}
	
	struct stat st;
	if (stat(cmd->args[1], &st) < 0){
		if (errno == ENOENT){
			perrorSmash(cmd->cmd, "expected valid paths for files");
			freeCMD(cmd);
			return;
		}
		else if(errno != 0){
			perror("smash error: diff failed");
			freeCMD(cmd);
			return;
		}
			//no path
	}
	if (stat(cmd->args[2], &st) < 0){
		if (errno == ENOENT){
			perrorSmash(cmd->cmd, "expected valid paths for files"); // works 
			freeCMD(cmd);
			return;
		}
		else if(errno != 0){
			perror("smash error: diff failed");
			freeCMD(cmd);
			return;
		}
			//no path
	}
	FILE *fp1 = fopen(cmd->args[1], "r");
    FILE *fp2 = fopen(cmd->args[2], "r");
	if (fp1 == NULL || fp2 == NULL) {
		perrorSmash(cmd->cmd, "paths are not files"); // doesnt work
		freeCMD(cmd);
        return ;
    }

	// TODO: compare the files
	char c1;
	char c2;
	if(!bg){
	do
	{
		c1 = fgetc(fp1);
		c2 = fgetc(fp2);
		if (c1 != c2)
			{
				printf("1");
				freeCMD(cmd);
				return;
			}
	} while (c1 != EOF && c2 != EOF);
	
	if (c1 == EOF && c2 == EOF){
		printf("0\n");
	}
	else{
		printf("1\n");
	}
	freeCMD(cmd);
    return;
	}

	// background
	Job* job = MALLOC_VALIDATED(Job, sizeof(Job));
	job->cmd = cmd;
	pid_t pid = fork();
	if(pid > 0){
		job->pid = pid;
		addJob(job);
		return;
	}
	else if(pid == 0){
		do
	{
		c1 = fgetc(fp1);
		c2 = fgetc(fp2);
		if (c1 != c2)
			{
				printf("1");
				freeCMD(cmd);
				exit(SMASH_SUCCESS);
			}
	} while (c1 != EOF && c2 != EOF);
	
	if (c1 == EOF && c2 == EOF){
		printf("0\n");
	}
	else{
		printf("1\n");
	}
	freeCMD(cmd);
    exit(SMASH_SUCCESS);

	}

	else{
		perror("smash error: fork failed");
		freeJob(job);
		exit(SMASH_QUIT);
	}

}

/*==================
	FREE FUNC
==================*/
void freeCMD(ParsedCommand* cmd){
	if (cmd == NULL) return; //gaurd
	free(cmd->cmd);
	free(cmd->line);
	for (int i = 0; i < ARGS_NUM_MAX; i++){
		if (cmd->args[i])
			free(cmd->args[i]);
	}
	free(cmd);
	cmd = NULL;
}
void freeJob(Job* job){
	if(job == NULL) return;
	freeCMD(job->cmd);
	free(job);
	job = NULL;
}
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
	for ( ; i < JOBS_NUM_MAX; i++)
	{
		if(jobsTable[i] != NULL)
			jobsTable[i]->jid--; // update all other Job ID after the remmoval
	}
	
	
}
