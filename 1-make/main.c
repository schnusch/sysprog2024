#define _XOPEN_SOURCE  // getopt
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sincos.h"

_Static_assert(EXIT_FAILURE != 2, "we use exit code 2 for wrong command line usage");

int main(int argc, char **argv) {
	setlocale(LC_NUMERIC, "");

	for(int opt; (opt = getopt(argc, argv, "")) != -1;) {
		(void)opt;
	usage:
		fprintf(stderr, "Usage: %s STEP\n", argv[0]);
		return 2;
	}
	if(optind >= argc) {
		fprintf(stderr, "missing argument: STEP\n");
		goto usage;
	}
	if(optind + 1 < argc) {
		fprintf(stderr, "unexpected argument: %s\n", argv[optind + 1]);
		goto usage;
	}

	errno = 0;
	char *end;
	double step = strtod(argv[optind], &end);
	if(*end) {
		fprintf(stderr, "cannot parse %s: invalid trailing data\n", argv[optind]);
		goto usage;
	} else if(errno) {
		fprintf(stderr, "cannot parse %s: %s\n", argv[optind], strerror(errno));
		goto usage;
	}

	if(print_sincos(step) < 0) {
		perror("print_sincos");
		return EXIT_FAILURE;
	} else {
		return EXIT_SUCCESS;
	}
}
