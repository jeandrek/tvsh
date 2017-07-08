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

#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_CMD_SIZE	4096
#define MAX_ARG_COUNT	64
#define MAX_ARG_SIZE	64

char	*progname;
int	 command(const char *cmd);

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
command(const char *cmd)
{
	char *argv[MAX_ARG_COUNT];
	int i, j, result;

	for (i = 0; *cmd != '\0'; i++) {
		if (i == MAX_ARG_COUNT) {
			fprintf(stderr, "%s: Too many arguments\n", progname);
			return EXIT_FAILURE;
		}

		while (isspace(*cmd))
			cmd++;

		if (*cmd == '\0')
			break;

		argv[i] = malloc(MAX_ARG_SIZE);

		for (j = 0; *cmd != '\0' && !isspace(*cmd); j++) {
			if (j == MAX_ARG_SIZE) {
				fprintf(stderr, "%s: Argument too big\n", progname);
				return EXIT_FAILURE;
			}

			argv[i][j] = *cmd++;
		}

		argv[i][j] = '\0';
	}

	argv[i] = NULL;

	if (argv[0] == NULL)
		return EXIT_SUCCESS;

	if (fork() == 0) {
		execvp(argv[0], argv);
		perror(progname);
		exit(EXIT_FAILURE);
	}

	wait(&result);

	for (i = 0; argv[i] != NULL; i++)
		free(argv[i]);

	return result;
}
