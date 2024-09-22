#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"

// subset of history_word_delimiters
// https://tiswww.case.edu/php/chet/readline/history.html#index-history_005fword_005fdelimiters
static const char *word_delimiters = " \t\n";

static int iter_words(const char **start, size_t *len, const char **ctx) {
	size_t len_blank = strspn(*ctx, word_delimiters);
	*start = *ctx + len_blank;
	if(**start == '\0') {
		// end of string
		*ctx = *start;
		return 0;
	} else if(**start == '<' || **start == '>' || **start == '|') {
		*len = 1;
	} else if(**start == '&') {
		*len = 1;
		const char *end = *start + *len;
		size_t n = strspn(end, word_delimiters);
		if(end[n] != '\0') {
			return -1;
		}
	} else {
		*len = strcspn(*start, word_delimiters);
	}
	*ctx = *start + *len;
	return 1;
}

static int add_list_generic(void ***list, void *elem) {
	size_t n = 0;
	if(*list) {
		// count non-null pointers
		for(void **p = *list; *p; ++p) {
			++n;
		}
	}
	void **tmp = realloc(*list, (n + 2) * sizeof(*tmp));
	if(!tmp) {
		return -1;
	}
	*list = tmp;
	(*list)[n] = elem;
	(*list)[n + 1] = NULL;
	return 0;
}

static inline int add_argument(argument_list *argv, char *arg) {
	return add_list_generic((void ***)argv, arg);
}

static inline int add_command(command_list *cmdlist, argument_list cmd) {
	return add_list_generic((void ***)cmdlist, cmd);
}

static int is_reserved_word(const char *word, size_t len) {
	return len == 1 && (*word == '<' || *word == '>' || *word == '|' || *word == '&');
}

struct pipeline *parse_pipeline(const char *line, const char **error) {
	struct pipeline *p = malloc(sizeof(*p));
	if(!p) {
		return NULL;
	}
	*p = (struct pipeline){
		.stdin = NULL,
		.stdout = NULL,
		.background = 0,
		.commands = NULL,
	};

	argument_list command = NULL;

	const char *word;
	size_t len;
	int e;
	for(const char *ctx = line; (e = iter_words(&word, &len, &ctx)) > 0;) {
		if(len == 1 && (*word == '<' || *word == '>')) {
			char **dest = *word == '<' ? &p->stdin : &p->stdout;
			if(*dest) {
				free(command);
				free_pipeline(p);
				if(error) {
					*error = dest == &p->stdin ? "duplicate stdin redirection" : "duplicate stdout redirection";
				}
				return NULL;
			}
			// use next word as filename
			if(iter_words(&word, &len, &ctx) <= 0 || is_reserved_word(word, len)) {
				free(command);
				free_pipeline(p);
				if(error) {
					*error = dest == &p->stdin ? "missing word after <" : "missing word after >";
				}
				return NULL;
			}
			*dest = strndup(word, len);
		} else if(len == 1 && *word == '&') {
			p->background = 1;
		} else if(len == 1 && *word == '|') {
			// add finished command
			if(add_command(&p->commands, command) < 0) {
				free(command);
				free_pipeline(p);
				return NULL;
			}
			command = NULL;
		} else {
			// duplicate arg string and add to current command
			char *arg = strndup(word, len);
			if(!arg) {
				free(command);
				free_pipeline(p);
				return NULL;
			}
			if(add_argument(&command, arg) < 0) {
				free(arg);
				free(command);
				free_pipeline(p);
				return NULL;
			}
		}
	}
	if(e < 0) {
		if(error) {
			*error = "unexpected word after &";
		}
		free(command);
		free_pipeline(p);
		return NULL;
	}

	// TODO assert command
	// append current command
	if(add_command(&p->commands, command) < 0) {
		free(command);
		free_pipeline(p);
		return NULL;
	}
	command = NULL;

	return p;
}

void free_pipeline(struct pipeline *p) {
	free(p->stdin);
	free(p->stdout);
	if(p->commands) {
		for(argument_list *cmd = p->commands; *cmd; ++cmd) {
			for(char **arg = *cmd; *arg; ++arg) {
				free(*arg);
			}
			free(*cmd);
		}
	}
	free(p);
}

#define FMT_QUOTED_STRING "%s%s%s"
#define ARG_QUOTED_STRING(s) (s ? "\"" : ""), s, (s ? "\"" : "")

void print_pipeline(const struct pipeline *p) {
	printf("{\n");
	printf("\t.stdin = "FMT_QUOTED_STRING",\n", ARG_QUOTED_STRING(p->stdin));
	printf("\t.stdin = "FMT_QUOTED_STRING",\n", ARG_QUOTED_STRING(p->stdout));
	printf("\t.background = %d,\n", p->background);
	printf("\t.commands = ");
	if(p->commands) {
		printf("{\n");
		argument_list *cmd = p->commands;
		do {
			if(*cmd) {
				printf("\t\t{");
				for(char **arg = *cmd; *arg; ++arg) {
					if(arg != *cmd) {
						printf(", ");
					}
					printf(FMT_QUOTED_STRING, ARG_QUOTED_STRING(*arg));
				}
				printf("},\n");
			} else {
				printf("\t\tNULL,\n");
			}
		} while(*cmd++);
		printf("\t},\n");
	} else {
		printf("%p,\n", NULL);
	}
	printf("}\n");
}
