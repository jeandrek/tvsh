/*
 * Copyright (c) 2017-2019 Jeandre Kruger
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
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_ARG_COUNT	64
#define PROMPT		"$ "

struct builtin {
	char	name[64];
	int	(*exec)(char *[]);
};

int		 read_command(char *[], FILE *);
void		 free_command(char *[]);
int		 exec_command(char *[]);
int		 redirect(char *[], int[]);
void		 restore(int[]);
int		 builtin_exit(char *[]);
int		 builtin_exec(char *[]);
int		 builtin_cd(char *[]);

int		 interactive;
const char	*progname;
struct builtin	 builtins[] = {
	{"exit", builtin_exit},
	{"exec", builtin_exec},
	{"cd", builtin_cd}
};

#define NUM_BUILTINS	(sizeof (builtins)/sizeof (struct builtin))

int
main(int argc, char *argv[])
{
	char *command[MAX_ARG_COUNT];
	FILE *f = stdin;
	int result;

	if ((progname = strrchr(argv[0], '/')) != NULL)
		progname += 1;
	else
		progname = argv[0];

	if (argc > 1) {
		f = fopen(argv[1], "r");
		if (f == NULL) {
			fprintf(stderr, "%s: %s: %s\n", progname, argv[1],
				strerror(errno));
			return EXIT_FAILURE;
		}
	} else
		interactive = isatty(STDIN_FILENO) && isatty(STDERR_FILENO);

	if (interactive)
		signal(SIGINT, SIG_IGN);
	while (1) {
		if (interactive)
			fputs(PROMPT, stdout);
		if (read_command(command, f) == EOF) {
			if (interactive)
				putchar('\n');
			return EXIT_SUCCESS;
		}
		result = exec_command(command);
		free_command(command);
	}
}

int
read_command(char *argv[], FILE *f)
{
	size_t length, size;
	int i = 0, c, c1;
	char *token;

	if ((c = getc(f)) == EOF)
		return EOF;
	while (isspace(c) && c != '\n')
		c = getc(f);
	while (c != '\n') {
		if (i == MAX_ARG_COUNT) {
			fprintf(stderr, "%s: Too many arguments\n", progname);
			return EXIT_FAILURE;
		}
		token = NULL;
		length = 0;
		size = 0;
		do {
			if (length == size)
				token = realloc(token, size += 8);
			if (c == '\\') {
				if ((c1 = getc(f)) == '\n') {
					c = getc(f);
				} else {
					token[length++] = c1;
					c = getc(f);
				}
				continue;
			}
			token[length++] = c;
			if ((c1 = getc(f)) == '<')
				break;
			if (c != '2' && c != '>' && c1 == '>')
				break;
			if (c == '<' || c == '>')
				while (isspace(c1) && c1 != '\n')
					c1 = getc(f);
			c = c1;
		} while (!isspace(c));
		if (length == size)
			token = realloc(token, size + 1);
		token[length] = 0;
		argv[i++] = token;
		if (c != '\n')
			while (isspace(c = getc(f)) && c != '\n')
				;
	}
	argv[i] = NULL;
	return 0;
}

void
free_command(char *argv[])
{
	while (*argv != NULL)
		free(*argv++);
}

int
exec_command(char *argv[])
{
	int oldds[3];
	int result;

	if (argv[0] == NULL)
		/* Empty command. */
		return EXIT_SUCCESS;

	result = redirect(argv, oldds);
	if (result != EXIT_SUCCESS)
		return result;

	for (size_t i = 0; i < NUM_BUILTINS; i++)
		if (!strcmp(builtins[i].name, argv[0])) {
			result = builtins[i].exec(argv);
			restore(oldds);
			return result;
		}

	if (fork() == 0) {
		if (interactive)
			signal(SIGINT, SIG_DFL);
		execvp(argv[0], argv);
		fprintf(stderr, "%s: %s: %s\n", progname, argv[0],
			strerror(errno));
		exit(EXIT_FAILURE);
	}
	wait(&result);
	restore(oldds);
	return result;
}

int
redirect(char *argv[], int oldds[])
{
	int fd, flags;
	char *path;

	oldds[STDIN_FILENO] = -1;
	oldds[STDOUT_FILENO] = -1;
	oldds[STDERR_FILENO] = -1;
	while (*argv != NULL) {
		fd = -1;
		switch ((*argv)[0]) {
		case '<':
			fd = STDIN_FILENO;
			flags = O_RDONLY;
			path = *argv + 1;
			break;
		case '>':
			fd = STDOUT_FILENO;
			flags = O_WRONLY | O_CREAT;
			if ((*argv)[1] == '>') {
				flags |= O_APPEND;
				path = *argv + 2;
			} else {
				flags |= O_TRUNC;
				path = *argv + 1;
			}
			break;
		case '2':
			if ((*argv)[1] != '>')
				break;
			fd = STDERR_FILENO;
			flags = O_WRONLY | O_CREAT;
			if ((*argv)[2] == '>') {
				flags |= O_APPEND;
				path = *argv + 3;
			} else {
				flags |= O_TRUNC;
				path = *argv + 2;
			}
		}
		if (fd != -1) {
			for (int i = 0; argv[i] != NULL; i++)
				argv[i] = argv[i + 1];
			oldds[fd] = fcntl(fd, F_DUPFD_CLOEXEC, 0);
			close(fd);
			if (open(path, flags, 0666) == -1) {
				restore(oldds);
				fprintf(stderr, "%s: %s: %s\n", progname, path,
					strerror(errno));
				return EXIT_FAILURE;
			}
		} else
			argv++;
	}
	return EXIT_SUCCESS;
}

void
restore(int oldds[])
{
	for (int fd = 0; fd < 3; fd++)
		if (oldds[fd] != -1) {
			dup2(oldds[fd], fd);
			close(oldds[fd]);
		}
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
	if (interactive)
		signal(SIGINT, SIG_DFL);
	execvp(argv[1], argv + 1);
	fprintf(stderr, "exec: %s: %s\n", argv[1], strerror(errno));
	return EXIT_FAILURE;
}

int
builtin_cd(char *argv[])
{
	const char *path;

	if (argv[1] != NULL && argv[2] != NULL) {
		fprintf(stderr, "usage: cd [directory]\n");
		return EXIT_FAILURE;
	}

	if (argv[1] == NULL)
		path = getenv("HOME");
	else
		path = argv[1];

	if (chdir(path) == -1) {
		fprintf(stderr, "cd: %s: %s\n", path, strerror(errno));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
