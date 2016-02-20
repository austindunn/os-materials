#can add variables, kind of like precompiler commands, turned into text upon $(VARIABLENAME)

all:
	gcc tinyshell.c -o tinyshell -g -std=c99
clean:
	rm -rf ./tinyshell ./tinyshell.dSYM
