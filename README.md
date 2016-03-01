# os-materials
A repo for assignments related to COMP 310 (Operating Systems)

Currently, this repository contains a small shell program. Here are a few things to know about the shell:

The shell reads Unix commands from the user and executes them by forking and using the execvp() function in the child process. Jobs can be run in the background (i.e. the shell will not wait on the processes to finish before accepting more commands) if the command ends with `&`. Simple redirection of command output to files is also implemented, by ending the command with `> filename`. The shell will execute standard Unix commands using execvp(), but it includes a few built-in commands that are run without forking or calling execvp():

`exit` to exit the shell.

`cd [/path/to/directory]` to change directory according to argument supplied.

`history` to display the past ten commands entered, with an index. If the user enters a number as command, the shell will attempt to execute the command with that index in history. If the number entered does not refer to an item in history, an error message is printed instead. If the command referred to by the number entered was erroneous (i.e. an invalid bash command), the shell will not execute the command again or record it agian in the history.

`jobs` to display all jobs previously run in the background.

`fg [pid]` to bring the job associated with the entered PID to the foreground. If the PID does not refer to a currently running process, the shell will instead print an error message (see the list of bugs for a problem with this). If the user does not enter a PID, an error message is printed.

Known bugs or other issues:

1. Trying to redirect a command run from history will not work. Commands run from history are run exactly as the first time they were executed.

2. Sometimes when running the `jobs` command to list currently running jobs, the operation fails giving Abort Trap 6.

