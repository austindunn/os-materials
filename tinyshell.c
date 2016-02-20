#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <ctype.h>

//a struct to create a flexible linked list of jobs, along with PIDs and strings to represent
//the commands that the jobs are associated with
struct jobNode {
	pid_t thispid;
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
//NOTE: I have added an argument that will store what is read into the *line pointer after
//strsep breaks the string up, for purposes of tracking history and jobs
int getcmd(char *prompt, char *args[], int *background, char **cmdcopy)
{
	int length, i = 0;
	char *token, *loc;
	char *line = NULL;
	size_t linecap = 0;

	printf("%s", prompt);
	length = getline(&line, &linecap, stdin);
	
    	//here the string pointed to by line is copied into cmdcopy, which is kept on the
	//heap to be used beyond the scope of this function. This memory is freed at the
	//end of each loop in main()
	*cmdcopy = (char *) malloc(sizeof(char) * strlen(line));
	strcpy(*cmdcopy,line);

    	if (length <= 0) {
    	    	exit(-1);
    	}

    	// Check if background is specified..
    	if ((loc = index(line, '&')) != NULL) {
    		*background = 1;
    		*loc = ' ';
    	} else
        	*background = 0;

    	while ((token = strsep(&line, " \t\n")) != NULL) {
    	    	for (int j = 0; j < strlen(token); j++)
            		if (token[j] <= 32)
				token[j] = '\0';
        	if (strlen(token) > 0)
        		args[i++] = token;
    	}
    	args[i++] = NULL;

	//free the line variable to avoid memory leak
	free(line);

	return i;
}

//method for adding items to the max-10-item history array
int addToHist(struct hist *histlist, int *crtindex, char *cmdargs) {
	//if indx is less than 11, simply put the struct at position indx-1
	if (*crtindex < 10) {
		histlist[*crtindex].index = *crtindex + 1;
		histlist[*crtindex].args = malloc(sizeof(cmdargs));
		strcpy(histlist[*crtindex].args, cmdargs);
		histlist[*crtindex].errFlag = 0;
	//otherwise (i.e. if array is full), remove first item and push all other structs back in the array
	} else { 
		for (int i = 0; i < 9; i++) {
			histlist[i].index = histlist[i+1].index;
			free(histlist[i].args);
			histlist[i].args = malloc(sizeof(histlist[i+1].args));
			strcpy(histlist[i].args, histlist[i+1].args);
			histlist[i].errFlag = histlist[i+1].errFlag;
		}
		//set last item in array to have information of new command entry
		histlist[9].index = *crtindex + 1;
		free(histlist[9].args);
		histlist[9].args = malloc(sizeof(cmdargs));
		strcpy(histlist[9].args, cmdargs);
		histlist[9].errFlag = 0;
	}
	//increment the index
	*crtindex = *crtindex + 1;
	return 1;
}

//method for printing the contents of the history array neatly
int printHistory(struct hist *histlist, int count) {
	//only print as many items as are currently in the history array
	if (count < 10) {
		for (int i = 0; i < count; i++)
			printf("[%d] %s", histlist[i].index, histlist[i].args);
	} else {
		for (int i = 0; i < 10; i++)
			printf("[%d] %s", histlist[i].index, histlist[i].args);
	}
	return 1;
}

//a method that uses the same code used in getcmd() to break up a command string into its arguments.
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
	newJob->thispid = newPid;
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
	//if need to remove head:
	if (head == del) {
		head = del->next;
	//if need to remove tail or a node somewhere in the middle
	} else if (del->next == NULL) {
		del->prev->next = NULL;
	} else  {
		del->prev->next = del->next;
		del->next->prev = del->prev;
	}
	free(del->jobString);
	free(del);
	return;
}

//This function checks which processes are currently running, and would then remove that job's jobNode 
//from the linked list of jobNodes if my removeJobs function was working correctly.
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
		waitcheck = waitpid(crt->thispid, &status, WNOHANG);
		if (waitcheck != 0 && (WIFEXITED(status) || WIFSIGNALED(status))){
			removeJob(crt);
		}
		crt = crt->next;
	}
	return;
}

//Prints the jobs that have been run. Ideally, this would print only the running background jobs, but since my
//removeJobs() function is not working, it lists all jobs that have been run in the background
void printJobs() { //struct jobNode *head) { -- removing parameter, as head is now defined as a global var
	//first check if jobs list is empty, do nothing if so
	if (head == NULL) {
		printf("No jobs currently running in the background.\n");
		return;
	}
	struct jobNode *crt = head;
	int i;
	while (crt != NULL) {
		//print the PID and command associated with each background job
		i = (int) crt->thispid;
		printf("[%d] %s", i, crt->jobString);
		crt = crt->next;
	}
	return;
}

int foregroundJob(char *args[]) {
	struct jobNode *crt = head;
	int status;
	while (crt != NULL) {
		if (crt->thispid == atoi(args[1])) {
			removeJob(crt);
			waitpid((pid_t) atoi(args[1]), &status, 0);
			return 1;
		}
		crt = crt->next;
	}
	return 0;
}

int main()
{
	char *args[20];
	char *input = NULL;
	int bg, status, cdCheck;
	int histCount = 0; 			//keeps track of which history item we're on
	struct hist histarray[10];		//stores history items
	pid_t pid, endID;
    
	while(1) {

		int cnt = getcmd("\n>>  ", args, &bg, &input);
		printf("\n");

		//if a number is entered as first argument, execute the command with that index in the history array
		if (isdigit(*args[0])) {
			int ref = atoi(args[0]);
			int i;
			
			//make sure that the number entered matches the index of a history item
			if (ref < 0 || ref < histCount-9 || ref > histCount) {
				printf("\nNumber entered does not refer to an item held in History\n");
				continue;
			}
			//move to item in history array with the given index
			for (i = 0; i < 10; i++) {
				if (histarray[i].index == ref)
					break;
			}
			//don't execute the command again if it didn't work the first time
			if (histarray[i].errFlag == 1) {
				printf("\nThat command was erroneous. I won't execute or record that one in History again.\n");
				continue;
			}
			//give input and args values according to the history item, then execute the rest of main()
			//in accodance with those values
			free(input);
			input = malloc(sizeof(histarray[i].args)+1);
			strcpy(input, histarray[i].args);
			extractArgs(args, input, &bg);
			//printf("\n%s\n", input);
		}

		//add the command to history
		addToHist(&histarray[0], &histCount, input);

		//builtin exit command
		if (strcmp(args[0], "exit") == 0) {
			printf("Exiting shell...\n\n");
			exit(0);
		//builtin cd (change directory)
		} else if (strcmp(args[0], "cd") == 0) {
			if ((cdCheck = chdir(args[1]) == -1)) {
				printf("chdir() failure... now exiting shell");
				exit(1);
			}
		//builtin pwd (present working directory)
		/*} else if (strcmp(args[0], "pwd") == 0) {
			//in case output should be redirected
			if (cnt > 3) {
				if (strcmp(args[cnt-3], ">") == 0) {
					//fork() to simplify file descriptor table modification
					if ((pid = fork()) == -1) {
						printf("fork() failure... now exiting shell.");
						exit(1);
					} else if (pid == 0) {
						close(1);
						open(args[cnt-2], O_WRONLY | O_APPEND);
						char *buffer = NULL;
						size_t bufferSize = 0;
						printf("\n%s\n", getcwd(buffer, bufferSize));
						exit(0);
					} else {
						continue;
					}
				}
			}
			//if output not redirected, simply call getcwd() and print to screen
			char *buffer = NULL;
			size_t bufferSize = 0;
			printf("\n%s\n", getcwd(buffer, bufferSize));
			*/
		//builtin command to display history
		} else if (strcmp(args[0], "history") == 0) {
			printHistory(&histarray[0], histCount);
		//builtin command to display previously run jobs
		} else if (strcmp(args[0], "jobs") == 0) {
			updateJobs();
			printJobs();
		//builtin command to bring a background job to the foreground
		} else if (strcmp(args[0], "fg") == 0) {
			//make sure a PID is identified with the fg command
			if (args[1] == NULL) {
				printf("\nInvalid command: must give a Process ID number with the fg command.\n");
				continue;
			}
			//
			if (foregroundJob(args) == 0)
				printf("\nThe process with that PID is not currently running.\n");
		}
		//run command if issued with '&' at the end
		else if (bg) {
			printf("Background enabled...\n");
			//fork process...
			if ((pid = fork()) == -1) {
				printf("fork() failure... now exiting shell.");
				exit(1);
			} else if (pid == 0) {		//child process
				//check if we need to redirect output, and do what's appropriate if so
				if (cnt > 3 && strcmp(args[cnt-3],">") == 0) {
					printf("in child\n");
					close(1);
					open(args[cnt-2], O_WRONLY | O_APPEND);
					//free last two positions in args[] and move NULL value two positions back in 
					//the array so that command is readable by execvp()
					args[cnt-3] = NULL;
					if (execvp(args[0],args) == -1) {
						perror(args[0]);
						_exit(EXIT_FAILURE);
					}
				//handle weird error
				} else if (execvp(args[0],args) == -1) {
					perror(args[0]);
					_exit(EXIT_FAILURE);
				}
			} else {			//parent process
				addToJobs(pid, input);
			}
		} else {

			//two ints to create a pipe to communicate execvp failure (to make sure command can't be
			//run again from history
			int pipeints[2];
			//err is a buffer for read, errexec will be used to check for execvp failure
			int err, errexec;
			//create the pipe
			if (pipe(pipeints)) {
				perror("pipe");
				_exit(EXIT_FAILURE);
			}
			//CLOEXEC will close the pipe on successful execvp() call
			if (fcntl(pipeints[1], F_SETFD, fcntl(pipeints[1], F_GETFD) | FD_CLOEXEC)) {
				perror("fcntl");
				_exit(EXIT_FAILURE);
			}

			//fork process...
			if ((pid = fork()) == -1) {
				printf("fork() failure... now exiting shell.");
				exit(1);
			} else if (pid == 0) {			//child process
				//close read end of pipe
				close(pipeints[0]);
				//in case output of command should be redirected
				if (cnt > 3 && strcmp(args[cnt-3],">") == 0) {
					close(1);
					open(args[cnt-2], O_WRONLY | O_APPEND);
					//free last two positions in args[] and move NULL value two positions back in the array
					//so that command is readable by execvp()
					args[cnt-3] = NULL;
					if (execvp(args[0],args) == -1) {
						write(pipeints[1], &errno, sizeof(int));
						//perror(args[0]);
						_exit(EXIT_FAILURE);
					}
				} else if (execvp(args[0],args) == -1) {
					write(pipeints[1], &errno, sizeof(int));
					//perror(args[0]);
					_exit(EXIT_FAILURE);
				}
			} else {				//parent process
				//close write end of pipe
				close(pipeints[1]);
				while ((errexec = read(pipeints[0], &err, sizeof(errno))) == -1)
					if (errno != EAGAIN && errno != EINTR) break;
				//check if there was an error executing the command, and record in approapriate struct in history array 
				if (errexec != 0) {
					if (histCount < 9) { 
						histarray[histCount-1].errFlag = 1;
					} else {
						histarray[9].errFlag = 1;
					}
				}
				close(pipeints[0]);
				//wait on the child process (run in foreground)...
				if ((endID = waitpid(pid, &status, 0)) == -1) {
					printf("waitpid() failure... now exiting shell.");
					exit(1);
				} else if (endID == pid) {
					if (WIFEXITED(status)) {
						//yay! complete
					} else if (WIFSIGNALED(status)) {
						printf("Child process terminated because of an uncaught signal\n");
					} else if (WIFSTOPPED(status)) {
						printf("Child process has stopped\n");
					}
				}
			}
		}
		//free input, was malloc'd in getcmd() call
		free(input);
	}
	printf("\n\n");
}

