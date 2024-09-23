#ifndef _XOPEN_SOURCE
 #define _XOPEN_SOURCE
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "parse.h"

char *my_getcwd(void) {
	char *buffer = NULL;
	size_t size = 256 / 2;

	char *ret;
	do {
		size_t new_size = size * 2;
		if(new_size < size) {
			free(buffer);
			errno = ERANGE;
			return NULL;
		}
		char *new_buffer = realloc(buffer, new_size);
		if(!new_buffer) {
			free(buffer);
			return NULL;
		}
		buffer = new_buffer;
        size = new_size;
	} while(!(ret = getcwd(buffer, size)) && errno == ERANGE);

	if(ret != buffer) {
		free(buffer);
		return NULL;
	} else {
		return ret;
	}
}

#define PROMPT_CHAR getuid() == 0 ? '#' : '$'
#define PROMPT_SNPRINTF(p, n, e, cwd) \
	(e == 0 \
		? snprintf(p, n, "%s%s %c%s ", "\x1B[32m", cwd, PROMPT_CHAR, "\x1B[39m") \
		: snprintf(p, n, "%s%hhd%s %s %c%s ", "\x1B[1;31m", e, "\x1B[22;32m", cwd, PROMPT_CHAR, "\x1B[39m") \
	)

int prepare_prompt(char **prompt, unsigned char error) {
	char *cwd = my_getcwd();
	if(!cwd) {
		return -1;
	}

	int len = PROMPT_SNPRINTF(NULL, 0, error, cwd);
	if(len < 0) {
		free(cwd);
		return -1;
	}

	// realloc *prompt if too small
	size_t size = *prompt ? strlen(*prompt) + 1 : 0;
	if((size_t)len + 1 > size) {
		size = (size_t)len + 1;
		char *new_prompt = realloc(*prompt, size);
		if(!new_prompt) {
			free(cwd);
			return -1;
		}
		*prompt = new_prompt;
	}

	len = PROMPT_SNPRINTF(*prompt, size, error, cwd);
	free(cwd);
	return len < 0 ? -1 : 0;
}

int main(int argc, char **argv) {
	int verbose = 0;
	for(int opt; (opt = getopt(argc, argv, "v")) != -1;) {
		switch(opt) {
		case 'v':
			verbose = 1;
			break;
		default:
			goto usage;
		}
	}
	if(optind < argc) {
		_Static_assert(
			EXIT_FAILURE != 2,
			"exit code 2 is used for wrong command line usage, but the general error EXIT_FAILURE is equal to 2"
		);
	usage:
		fprintf(stderr, "Usage: %s [-v]\n", argv[0]);
		return 2;
	}

	char *prompt = NULL;
	int last_error = 1;

	while(1) {
		if(isatty(STDIN_FILENO)) {
			if(prepare_prompt(&prompt, last_error) < 0) {
				perror(argv[0]);
				free(prompt);
				return EXIT_FAILURE;
			}
		}
		char *line = readline(prompt);
		if(!line) {
			break;
		}
		if(*line) {
			// avoid double history entry
			// current_history() does not seem to work
			const HIST_ENTRY *previous = history_get(where_history());
			if(!previous || strcmp(line, previous->line) != 0) {
				add_history(line);
			}
		}

		const char *error = NULL;
		struct pipeline *p = parse_pipeline(line, &error);
		if(!p) {
			if(error) {
				fprintf(stderr, "%s: cannot parse command pipeline: %s\n", argv[0], error);
				free(line);
				continue;
			} else {
				perror(argv[0]);
				free(line);
				free(prompt);
				return EXIT_FAILURE;
			}
		}
		if(verbose) {
			print_pipeline(p);
		}
		free_pipeline(p);
		free(line);
	}

	free(prompt);
	return EXIT_SUCCESS;
}
