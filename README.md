# os-materials
A repo for assignments related to COMP 310 (Operating Systems)

Currently, this repository contains a shell. Here are a few things to know about the shell:

The shell reads Unix commands from the user and executes them by forking and using the execvp() function in the child process. Jobs can be run in the background (i.e. the shell will not wait on the processes to finish before accepting more commands) if the command ends with `&`. Simple redirection of command output to files is also implemented, by ending the command with `> filename`. It implements a few built-in commands:

`exit` to exit the shell.

`cd [/path/to/directory]` to change directory according to argument supplied.

`pwd` to output the current working directory to the screen (implemented using the getcwd() system call).

`history` to display the past ten commands entered, with an index. If the user enters a number as command, the shell will attempt to execute the command with that index in history. If the number entered does not refer to an item in history, an error message is printed instead. If the command referred to by the number entered was erroneous (i.e. an invalid bash command), the shell will not execute the command again or record it agian in the history.

`jobs` to display all jobs previously run in the background.

`fg [pid]` to bring the job associated with the entered PID to the foreground. If the PID does not refer to a currently running process, the shell will instead print an error message (see the list of bugs for a problem with this). If the user does not enter a PID, an error message is printed.

Please compile this program using the Makefile supplied.

A note on the freecmd() function in my shell:

I do NOT implement freecmd(), instead, in each iteration of the main loop, I copy what is stored in *line after the getline() call in getcmd() to a variable on the heap that can be used to store full command strings in the history array or in the list of jobs. At the end of getcmd(), I free line, and I free the copy at the end of the main loop. I believe that my strategy does cover all memory management issues regarding getcmd().

Quick acknowledgment of some things that don't work with my shell:

1. I used a linked list to keep track of background jobs. Unfortunately, something seems to be wrong with the way that I was removing them, so as the program is currently background jobs are stored even after they have stopped running.

2. There is a bug in the 'fg' command where if the number given with the command does NOT correspond to a currently active process, the command must be entered again before the program responds with the correct error message.

3. Running a command from history that redirects its output will not work. Redirect works fine, just not run from history.

