/*
 * Copyright (c) 2017 Jeandre Kruger
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_CMD_SIZE	768
#define MAX_ARG_COUNT	64
#define NUM_BUILTINS	3

struct builtin {
	char	  name[64];
	int	(*exec)(char *argv[]);
};

/*
 * Procedure forward declarations.
 */

int	command(char *cmd);
int	read_command(char *argv[], char *cmd);
int	exec_command(char *argv[]);
int	builtin_exit(char *argv[]);
int	builtin_exec(char *argv[]);
int	builtin_cd(char *argv[]);

/*
 * Global variable declarations.
 */

char		*progname;
struct builtin	 builtins[NUM_BUILTINS] =
{
	{ "exit",	builtin_exit },
	{ "exec",	builtin_exec },
	{ "cd",		builtin_cd },
};

int
main(int argc, char *argv[])
{
	char cmd[MAX_CMD_SIZE];

	progname = argv[0];

	if (argc != 1) {
		fprintf(stderr, "usage: %s\n", progname);
		exit(EXIT_FAILURE);
	}

	signal(SIGINT, SIG_IGN);

	while (1) {
		fputs("$ ", stdout);

		if (fgets(cmd, MAX_CMD_SIZE, stdin) == NULL) {
			putchar('\n');
			exit(EXIT_SUCCESS);
		}

		command(cmd);
	}
}

int
command(char *cmd)
{
	int result;
	char *argv[MAX_ARG_COUNT];

	result = read_command(argv, cmd);

	if (result != EXIT_SUCCESS)
		return result;

	result = exec_command(argv);

	return result;
}

int
read_command(char *argv[], char *cmd)
{
	int idx	= 0;
	char *token = NULL;
	const char *delims = " \t\v\f\n\r";

	token = strtok(cmd, delims);

	while (token != NULL) {
		if (idx == MAX_ARG_COUNT) {
			fprintf(stderr, "%s: Too many arguments\n", progname);

			return EXIT_FAILURE;
		}

		argv[idx++]	= token;
		token		= strtok(NULL, delims);
	}

	argv[idx] = NULL;

	return EXIT_SUCCESS;
}

int
exec_command(char *argv[])
{
	int result;

	if (argv[0] == NULL)
		/* Empty command. */
		return EXIT_SUCCESS;

	for (int idx = 0; idx < NUM_BUILTINS; idx++) {
		if (!strcmp(builtins[idx].name, argv[0])) {
			return builtins[idx].exec(argv);
		}
	}

	if (fork() == 0) {
		signal(SIGINT, SIG_DFL);
		execvp(argv[0], argv);
		perror(progname);
		exit(EXIT_FAILURE);
	}

	wait(&result);

	return result;
}

/*
 * Builtin commands follow.
 */

int
builtin_exit(char *argv[])
{
	if (argv[1] != NULL && argv[2] != NULL) {
		fprintf(stderr, "usage: exit [code]\n");
		return EXIT_FAILURE;
	}

	if (argv[1] == NULL)
		exit(EXIT_SUCCESS);
	else
		exit(atoi(argv[1]));
}

int
builtin_exec(char *argv[])
{
	if (argv[1] == NULL) {
		fprintf(stderr, "usage: exec command ...\n");
		return EXIT_FAILURE;
	}

	signal(SIGINT, SIG_DFL);
	execvp(argv[1], argv+1);
	perror(progname);

	return EXIT_FAILURE;
}

int
builtin_cd(char *argv[])
{
	if (argv[1] == NULL || argv[2] != NULL) {
		fprintf(stderr, "usage: cd directory\n");
		return EXIT_FAILURE;
	}

	if (chdir(argv[1]) == -1) {
		perror("cd");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
