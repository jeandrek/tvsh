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
#include <sys/wait.h>

#define MAX_CMD_SIZE	2048
#define MAX_ARG_COUNT	64

int	 command(char *cmd);
int	 read_command(char *argv[], char *cmd);
int	 exec_command(char *argv[]);
char	*progname;

int
main(int argc, char *argv[])
{
	char cmd[MAX_CMD_SIZE];

	progname = argv[0];

	if (argc != 1) {
		fprintf(stderr, "usage: %s\n", progname);
		exit(EXIT_FAILURE);
	}

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
	char *argv[MAX_ARG_COUNT];
	int result;

	result = read_command(argv, cmd);

	if (result != EXIT_SUCCESS)
		return result;

	result = exec_command(argv);

	return result;
}

int
read_command(char *argv[], char *cmd)
{
	const char	*delims	= " \t\v\f\n\r";
	char		*token	= NULL;
	int		 idx	= 0;

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

	if (fork() == 0) {
		execvp(argv[0], argv);
		perror(progname);
		exit(EXIT_FAILURE);
	}

	wait(&result);

	return result;
}
