#include <stddef.h>

typedef char** argument_list;
typedef argument_list* command_list;

struct pipeline {
	char *stdin;
	char *stdout;
	int background;
	command_list commands;
};

struct pipeline *parse_pipeline(const char *line, const char **error);

void free_pipeline(struct pipeline *);

void print_pipeline(const struct pipeline *);
