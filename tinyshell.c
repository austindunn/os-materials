#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <ctype.h>

//a struct to create a linked list of jobs, along with PIDs and strings to represent
//the commands that the jobs are associated with
struct jobNode {
	pid_t thisPid;
	char *jobString;
	struct jobNode *prev;
	struct jobNode *next;
};
struct jobNode *head;		//global pointer to head jobNode

//a struct to keep track of information pertaining to each of the ten items stored in history
struct hist {
	int index;
	int errFlag;
	char *args;
};

//getcmd() reads a string and parses it into an array readable by execvp
int getcmd(char *prompt, char *args[], int *background, char **cmdcopy)
{
	int length, i = 0;
	char *token, *loc;
	char *line = NULL;
	size_t linecap = 0;

	printf("%s", prompt);
	length = getline(&line, &linecap, stdin);

	//here the string pointed to by line is copied into cmdcopy, which is kept on the heap
	*cmdcopy = (char *) malloc(sizeof(char) * length);
	strcpy(*cmdcopy,line);

	if (length <= 1) {
		return 0;
	}

    	// Check if job should run in background, and if so, remove '&' from command string
    	if ((loc = index(line, '&')) != NULL) {
    		*background = 1;
    		*loc = ' ';
    	} else
        	*background = 0;
	
	//break line into array of args for execvp()
    	while ((token = strsep(&line, " \t\n")) != NULL) {
    	    	for (int j = 0; j < strlen(token); j++)
            		if (token[j] <= 32)
				token[j] = '\0';
        	if (strlen(token) > 0)
        		args[i++] = token;
    	}
    	args[i++] = NULL;
	free(line);
	return i;
}

//adds items to the 10-item history array
int addToHist(struct hist *histList, int *crtindex, char *cmdargs) {
	//if crtindex is less than 10, simply put the struct at position *crtindex-1
	if (*crtindex < 10) {
		histList[*crtindex].index = *crtindex + 1;
		histList[*crtindex].args = malloc(sizeof(cmdargs));
		strcpy(histList[*crtindex].args, cmdargs);
		histList[*crtindex].errFlag = 0;
	//otherwise (i.e. if array is full), remove first item, push all other structs back
	//in the array, and set last item to have new command info
	} else { 
		for (int i = 0; i < 9; i++) {
			histList[i].index = histList[i+1].index;
			free(histList[i].args);
			histList[i].args = malloc(sizeof(histList[i+1].args));
			strcpy(histList[i].args, histList[i+1].args);
			histList[i].errFlag = histList[i+1].errFlag;
		}
		histList[9].index = *crtindex + 1;
		free(histList[9].args);
		histList[9].args = malloc(sizeof(cmdargs));
		strcpy(histList[9].args, cmdargs);
		histList[9].errFlag = 0;
	}
	*crtindex = *crtindex + 1;
	return 1;
}

//prints the contents of the history array neatly
int printHistory(struct hist *histList, int count) {
	if (count < 10) {
		for (int i = 0; i < count; i++)
			printf("[%d] %s", histList[i].index, histList[i].args);
	} else {
		for (int i = 0; i < 10; i++)
			printf("[%d] %s", histList[i].index, histList[i].args);
	}
	return 1;
}

//this function uses the same code used in getcmd() to break up a command string into its arguments.
//this is used to separate out arguments when a command is called from history (the hist struct keeps
//only the whole command string)
int extractArgs(char *args[], char *line, int *background) {

	int i = 0;
	char *token, *loc;
	
	//copy the input parameter to keep the string it points to safe
	char *linecopy = (char *) malloc(sizeof(char) * strlen(line));
	strcpy(linecopy, line);

	//set the background variable to 1 if last character in command is '&'
    	if ((loc = index(linecopy, '&')) != NULL) {
		*background = 1;
		*loc = ' ';
    	} else
        	*background = 0;

	while ((token = strsep(&linecopy, " \t\n")) != NULL) {
		for (int j = 0; j < strlen(token); j++)
			if (token[j] <= 32)
				token[j] = '\0';
		if (strlen(token) > 0)
			args[i++] = token;
	}
	args[i++] = NULL;
	free(linecopy);
	return 1;
}

struct jobNode *createNewJob(pid_t newPid, char *newCommand) {
	struct jobNode *newJob = (struct jobNode*) malloc(sizeof(struct jobNode));
	newJob->thisPid = newPid;
	newJob->jobString = malloc(strlen(newCommand+1));
	strcpy(newJob->jobString, newCommand);
	newJob->prev = NULL;
	newJob->next = NULL;
	return newJob;
}

//adds a background job to the end of the list of background jobs
void addToJobs(pid_t newPid, char *newCommand) {
	struct jobNode *newJob = createNewJob(newPid, newCommand);
	//if the list is empty, set the head's information to that of the new job
	if (head == NULL) {
		head = newJob;
		return;
	}
	//otherwise, move a pointer to the end of the list
	struct jobNode *crt = head;
	while (crt->next != NULL)
		crt = crt->next;
	crt->next = newJob;
	newJob->prev = crt;
}

//the following function removes a job from the current list of jobs
void removeJob(struct jobNode *del) {
	//if need to remove head
	if (head == del) {
		head = del->next;
	//if need to remove tail
	} else if (del->next == NULL) {
		del->prev->next = NULL;
	//removing from somewhere in the middle
	} else  {
		del->prev->next = del->next;
		del->next->prev = del->prev;
	}
	free(del->jobString);
	free(del);
	return;
}

//This function checks which processes are currently running, and then removes that job's jobNode 
//from the list of running jobs
void updateJobs() {
	//if list is empty, return 0
	if (head == 0)
		return;
	//otherwise, traverse through list to see which jobs are running using waitpid()
	struct jobNode *crt = head;
	int status, waitcheck;
	int i = 0;
	while (crt != NULL) {
		i++;
		waitcheck = waitpid(crt->thisPid, &status, WNOHANG);
		if (waitcheck != 0 && (WIFEXITED(status) || WIFSIGNALED(status))){
			removeJob(crt);
		}
		crt = crt->next;
	}
	return;
}

//Prints the currently running jobs
void printJobs() {
	if (head == NULL) {
		printf("No jobs currently running in the background.\n");
		return;
	}
	struct jobNode *crt = head;
	int i;
	while (crt != NULL) {
		//print the PID and command associated with each background job
		i = (int) crt->thisPid;
		printf("[%d] %s", i, crt->jobString);
		crt = crt->next;
	}
	return;
}

//if fg is called, this function brings the job specified by PID to the foreground
int bringToForeground(char *args[]) {
	struct jobNode *crt = head;
	int status;
	while (crt != NULL) {
		if (crt->thisPid == atoi(args[1])) {
			removeJob(crt);
			waitpid((pid_t) atoi(args[1]), &status, 0);
			return 1;
		}
		crt = crt->next;
	}
	return 0;
}

//if command is to be run from history, this function changes the input string to the command
//corresponding with the history item specified
int getCmdFromHistory(char *argList[], char **command, struct hist *histList, int count) {

	int ref = atoi(argList[0]);
	int i;
	
	//make sure that the number entered matches the index of a history item
	if (ref < 0 || ref < count-9 || ref > count) {
		printf("Number entered does not refer to an item held in History.\n");
		return 0;
	}

	//move to item in history array with the given index
	for (i = 0; i < 10; i++) {
		if (histList[i].index == ref)
			break;
	}

	//don't execute the command again if it didn't work the first time
	if (histList[i].errFlag == 1) {
		printf("That command was erroneous. I won't execute or record that one in History again.\n");
		return 0;
	}

	//give input and argList values according to the history item
	free(*command);
	*command = malloc(sizeof(histList[i].args)+1);
	strcpy(*command, histList[i].args);
	return 1;
}

//this function does what's needed for each built-in command. Note: these commands cannot be
//run in the background
int builtInCmd(char *argList[], struct hist *histList, int count) {

	int cdCheck;
	int isBuiltIn = 1;

	//builtin exit command
	if (strcmp(argList[0], "exit") == 0) {
		printf("Exiting shell...\n\n");
		exit(0);
	//builtin cd (change directory) command, implemented with chdir() system call
	} else if (strcmp(argList[0], "cd") == 0) {
		if ((cdCheck = chdir(argList[1]) == -1)) {
			printf("chdir() failure... now exiting shell");
			_exit(EXIT_FAILURE);
		}
	//builtin command to display history
	} else if (strcmp(argList[0], "history") == 0) {
		printHistory(&histList[0], count);
	//builtin command to display currently running jobs
	} else if (strcmp(argList[0], "jobs") == 0) {
		updateJobs();
		printJobs();
	//builtin command to bring a background job to the foreground
	} else if (strcmp(argList[0], "fg") == 0) {
		if (argList[1] == NULL)
			printf("Invalid command: must give a Process ID number with the fg command.\n");
		if (bringToForeground(argList) == 0)
			printf("The process with that PID is not currently running.\n");
	} else {
		isBuiltIn = 0;
	}
	return isBuiltIn;
}

//runs the entered command in the background
void backgroundExec(char *args[], int argsLen, char *command) {
	pid_t pid, endId;
	printf("Background enabled...\n");
	//fork process...
	if ((pid = fork()) == -1) {
		printf("fork() failure... now exiting shell.");
		_exit(EXIT_FAILURE);
	} else if (pid == 0) {		//child process
		//check if we need to redirect output, and do what's appropriate if so
		if (argsLen > 3 && strcmp(args[argsLen-3],">") == 0) {
			close(1);
			open(args[argsLen-2], O_WRONLY | O_APPEND);
			//free last two positions in args[] and move NULL value two positions back in 
			//the array so that command is readable by execvp()
			args[argsLen-3] = NULL;
			if (execvp(args[0],args) == -1) {
				perror(args[0]);
				_exit(EXIT_FAILURE);
			}
		} else if (execvp(args[0],args) == -1) {
			perror(args[0]);
			_exit(EXIT_FAILURE);
		}
	} else {			//parent process
		addToJobs(pid, command);
	}
}

//runs the entered command in the foreground. If there is an error in running the entered command,
//a pipe between the forked child process and the parent alerts the parent so that the command
//is recorded as erroneous, and can't be run or re-recorded in history again
void foregroundExec(char *args[], int argsLen, struct hist *histList, int count) {

	int status;
	pid_t pid, endId;
	int pipeints[2];	//two-int array used to create pipe 
	int err, errexec;	//err is a buffer for read, errexec will be used to check for execvp failure

	if (pipe(pipeints)) {
		_exit(EXIT_FAILURE);
	}

	//CLOEXEC will close the pipe on successful execvp() call
	if (fcntl(pipeints[1], F_SETFD, fcntl(pipeints[1], F_GETFD) | FD_CLOEXEC)) {
		_exit(EXIT_FAILURE);
	}

	//fork process...
	if ((pid = fork()) == -1) {
		printf("fork() failure... now exiting shell.");
		_exit(EXIT_FAILURE);
	} else if (pid == 0) {			//child process
		close(pipeints[0]);
		//in case output of command should be redirected
		if (argsLen > 3 && strcmp(args[argsLen-3],">") == 0) {
			close(1);
			open(args[argsLen-2], O_WRONLY | O_APPEND);
			//free last two positions in args[] and move NULL value two positions back in the array
			//so that command is readable by execvp()
			args[argsLen-3] = NULL;
			if (execvp(args[0],args) == -1) {
				write(pipeints[1], &errno, sizeof(int));
				_exit(EXIT_FAILURE);
			}
		} else if (execvp(args[0],args) == -1) {
			write(pipeints[1], &errno, sizeof(int));
			_exit(EXIT_FAILURE);
		}
	} else {				//parent process
		close(pipeints[1]);
		while ((errexec = read(pipeints[0], &err, sizeof(errno))) == -1)
			if (errno != EAGAIN && errno != EINTR)
				break;
		//set history item error flag to 1 if execvp() failed
		if (errexec != 0) {
			if (count < 10) { 
				histList[count-1].errFlag = 1;
			} else {
				histList[9].errFlag = 1;
			}
		}
		close(pipeints[0]);
		//wait on the child process
		if ((endId = waitpid(pid, &status, 0)) == -1) {
			printf("waitpid() failure... now exiting shell.");
			_exit(EXIT_FAILURE);
		} else if (endId == pid) {
			if (WIFSIGNALED(status)) {
				printf("Child process terminated because of an uncaught signal\n");
			} else if (WIFSTOPPED(status)) {
				printf("Child process has stopped\n");
			}
		}
	}
}

int main() {

	char *args[20];
	char *input = NULL;			//to be filled by getcmd
	int cnt, bg, status;
	int histCount = 0; 			//keeps track of which history item we're on
	struct hist histArray[10];		//stores history items
    
	while(1) {

		if ((cnt = getcmd("\n>>  ", args, &bg, &input)) == 0) {
			printf("\nPlease enter a command.\n");
			continue;
		}
		printf("\n");

		//if a number is entered as first argument, execute the command with that index in the history array
		if (isdigit(*args[0])) {
			if (getCmdFromHistory(args, &input, &histArray[0], histCount) == 0)
				continue;
			extractArgs(args, input, &bg);
		}

		//add the command to history
		addToHist(&histArray[0], &histCount, input);

		if (builtInCmd(args, &histArray[0], histCount) == 1)
			continue;

		//run command if issued with '&' at the end
		else if (bg) {
			backgroundExec(args, cnt, input);
		} else {
			foregroundExec(args, cnt, &histArray[0], histCount);
		}
		free(input);
	}
}

