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

#define PROMPT		"$ "

struct builtin {
	char		name[64];
	int		(*exec)(char *[]);
};

struct redirect {
	int		fd, flags, oldd;
	char		*path;
	struct redirect	*next;
};

struct command {
	char		**argv;
	struct redirect	*redirs;
	int		detached;
};

struct command	*read_command(FILE *);
int		 read_token(char **, struct redirect **, FILE *f);
void		 free_command(struct command *);
int		 exec_command(struct command *);
int		 redirect(struct redirect *);
void		 restore(struct redirect *);
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
	struct command *command;
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
	signal(SIGQUIT, SIG_IGN);
	while (1) {
		if (interactive)
			fputs(PROMPT, stdout);
		if ((command = read_command(f)) == NULL) {
			if (feof(f)) {
				if (interactive)
					putchar('\n');
				return EXIT_SUCCESS;
			} else if (interactive) {
				fpurge(f);
				continue;
			} else
				return EXIT_FAILURE;
		}
		result = exec_command(command);
		free_command(command);
	}
}

/* Token types. */
#define EOL		0
#define DETACHED	1
#define SEPERATOR	2
#define REDIRECTION	3
#define TEXT		4

struct command *
read_command(FILE *f)
{
	struct command *command;
	struct redirect *r, *r1;
	int type, i = 0, k = 0;
	char *text;

	command = malloc(sizeof *command);
	command->argv = NULL;
	command->redirs = NULL;
	command->detached = 0;
	while ((type = read_token(&text, &r, f)) != EOL)
		switch (type) {
		case EOF:
			return NULL;
		case DETACHED:
			command->detached = 1;
		case SEPERATOR:
			goto done;
		case TEXT:
			if (i == k)
				command->argv =
					realloc(command->argv,
						(k += 8)*sizeof (char *));
			command->argv[i++] = text;
			break;
		case REDIRECTION:
			if ((type = read_token(&text, &r1, f)) != TEXT) {
				fprintf(stderr, "%s: Redirection operator not "
					"followed by file path\n", progname);
				command->argv[i] = NULL;
				if (type == REDIRECTION)
					free(r1);
				free(r);
				free_command(command);
				return NULL;
			}
			r->oldd = -1;
			r->path = text;
			r->next = command->redirs;
			command->redirs = r;
			break;
		}
done:
	if (i == k)
		command->argv = realloc(command->argv,
					(k + 1)*sizeof (char *));
	command->argv[i] = NULL;
	return command;
}

int
read_token(char **text, struct redirect **r, FILE *f)
{
	size_t length = 0, size = 0;
	int fd = -1, c, c1;

	if ((c = getc(f)) == EOF)
		return EOF;
	while (isspace(c)) {
		if (c == '\n')
			return EOL;
		c = getc(f);
	}
	if (isdigit(c)) {
		if ((c1 = getc(f)) == '<' || c1 == '>') {
			fd = c - '0';
			c = c1;
		} else
			ungetc(c1, f);
	}
	switch (c) {
	case '&':	return DETACHED;
	case ';':	return SEPERATOR;
	case '<':
	case '>':
		*r = malloc(sizeof **r);
		if (fd != -1)
			(*r)->fd = fd;
		else if (c == '<')
			(*r)->fd = STDIN_FILENO;
		else
			(*r)->fd = STDOUT_FILENO;
		if (c == '<')
			(*r)->flags = O_RDONLY;
		else if ((c = getc(f)) == '>')
			(*r)->flags = O_WRONLY | O_CREAT | O_APPEND;
		else {
			(*r)->flags = O_WRONLY | O_CREAT | O_TRUNC;
			ungetc(c, f);
		}
		return REDIRECTION;
	}
	*text = NULL;
	do {
		if (length == size)
			*text = realloc(*text, size += 8);
		if (c == '\\') {
			if ((c1 = getc(f)) == '\n')
				c = getc(f);
			else {
				(*text)[length++] = c1;
				c = getc(f);
			}
			continue;
		}
		(*text)[length++] = c;
		if ((c = getc(f)) == '&' || c == ';' || c == '<' || c == '>')
			break;
	} while (!isspace(c));
	ungetc(c, f);
	if (length == size)
		*text = realloc(*text, size + 1);
	(*text)[length] = 0;
	return TEXT;
}

void
free_command(struct command *command)
{
	struct redirect *r, *r1;

	for (int i = 0; command->argv[i] != NULL; i++)
		free(command->argv[i]);
	free(command->argv);
	for (r = command->redirs; r != NULL; r = r1) {
		r1 = r->next;
		free(r->path);
		free(r);
	}
	free(command);
}

int
exec_command(struct command *command)
{
	int result;
	pid_t pid;

	result = redirect(command->redirs);
	if (result != EXIT_SUCCESS)
		return result;

	if (command->argv[0] == NULL) {
		/* Empty command. */
		restore(command->redirs);
		return EXIT_SUCCESS;
	}

	for (size_t i = 0; i < NUM_BUILTINS; i++)
		if (!strcmp(builtins[i].name, command->argv[0])) {
			result = builtins[i].exec(command->argv);
			restore(command->redirs);
			return result;
		}

	if ((pid = fork()) == 0) {
		if (interactive && !command->detached) {
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
		} else if (command->detached) {
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
		} else
			signal(SIGQUIT, SIG_DFL);
		execvp(command->argv[0], command->argv);
		fprintf(stderr, "%s: %s: %s\n", progname, command->argv[0],
			strerror(errno));
		exit(EXIT_FAILURE);
	} else if (pid == -1) {
		fprintf(stderr, "%s: %s\n", progname, strerror(errno));
		return EXIT_FAILURE;
	}
	if (!command->detached)
		while (wait(&result) != pid)
			;
	restore(command->redirs);
	return result;
}

int
redirect(struct redirect *r)
{
	int newd;

	for (struct redirect *r1 = r; r1 != NULL; r1 = r1->next) {
		r1->oldd = fcntl(r1->fd, F_DUPFD_CLOEXEC, 0);
		if ((newd = open(r1->path, r1->flags, 0666)) == -1) {
			restore(r);
			fprintf(stderr, "%s: %s: %s\n", progname, r1->path,
				strerror(errno));
			return EXIT_FAILURE;
		}
		if (newd != r1->fd) {
			dup2(newd, r1->fd);
			close(newd);
		}
	}
	return EXIT_SUCCESS;
}

void
restore(struct redirect *r)
{
	while (r != NULL) {
		if (r->oldd == -1)
			close(r->fd);
		else {
			dup2(r->oldd, r->fd);
			close(r->oldd);
		}
		r = r->next;
	}
}

/*
 * Builtin commands.
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
	signal(SIGQUIT, SIG_DFL);
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
