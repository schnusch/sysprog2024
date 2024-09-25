#ifndef EXEC_H
#define EXEC_H

#include "parse.h"

enum pipeline_error {
	ERROR_CLOSE = -1,
	ERROR_DUP = -2,
	ERROR_EXEC = -3,
	ERROR_FORK = -4,
	ERROR_OPEN = -5,
	ERROR_SETPGID = -6,
	ERROR_PIPE = -7,
	ERROR_WAIT = -8,
	ERROR_TCSETPGRP = -9,
	FATAL_TCSETPGRP = -10,
	// used internally
	EXIT_NO_ERROR = 0,
};

int run_pipeline(const struct pipeline *p, int verbose);

#endif
