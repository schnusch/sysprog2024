#ifndef _GNU_SOURCE
 #define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "exec.h"

enum error_cause {
	ERROR_CLOSE,
	ERROR_DUP,
	ERROR_EXEC,
	ERROR_FORK,
	ERROR_OPEN,
	ERROR_PIPE,
	ERROR_WAIT,
};
struct error_packet {
	int errnum;
	enum error_cause cause; // TODO actually use
};
_Static_assert(
	sizeof(struct error_packet) <= PIPE_BUF,
	"struct error_packet is too large to be written/read atomically"
);

static inline void write_error_pipe_no_errno(int error_fd, int errnum, enum error_cause cause) {
	int errbak = errno;
	struct error_packet e;
	e.cause = cause;
	e.errnum = errnum;
	(void)!write(error_fd, &e, sizeof(e));
	errno = errbak;
}

static inline void kill_child_no_errno(pid_t child) {
	int errbak = errno;
	// kill and reap child process
	if(kill(child, SIGKILL) == 0) {
		int status;
		while(
			waitpid(child, &status, 0) >= 0
			&& !WIFEXITED(status)
			&& !WIFSIGNALED(status)
		) { }
	}
	errno = errbak;
}

static pid_t fork_with_pipe(int error_fds[2]) {
	// O_DIRECT guarantees atomic read/write for n <= PIPE_BUF
	if(pipe2(error_fds, O_CLOEXEC | O_DIRECT) < 0) {
		return -1;
	}
	pid_t child = fork();
	if(child < 0) {
		return -1;
	} else if(child == 0) {
		// we actually do not need to close the read end, but we do anyway
		(void)!close(error_fds[0]);
		error_fds[0] = -1;
		return child;
	} else {
		// we must close the write end, so the only remaining write end belongs to the child process
		if(close(error_fds[1]) < 0) {
			kill_child_no_errno(child);
			return -1;
		}
		error_fds[1] = -1;
		return child;
	}
}

static int run_process_group(const struct pipeline *p, int error_fd, int verbose) {
	return -1;
}

int run_pipeline(const struct pipeline *p, int verbose) {
	int error_fds[2];
	pid_t pgrp = fork_with_pipe(error_fds);
	if(pgrp < 0) {
		return -1;
	} else if(pgrp == 0) {
		// child process
		_exit(
			run_process_group(p, error_fds[1], verbose) < 0
			? EXIT_FAILURE
			: EXIT_SUCCESS
		);
	} else {
		// parent process
		struct error_packet e;
		while(1) {
			ssize_t n = read(error_fds[0], &e, sizeof(e));
			if(n > 0) {
				errno = e.errnum;
			} else if(n == 0) {
				// write end was close
				(void)!close(error_fds[0]);
				break;
			}
			int errbak = errno;
			(void)!close(error_fds[0]);
			errno = errbak;
			kill_child_no_errno(pgrp);
			return -1;
		}

		// TODO waitpid
		return 0;
	}
}
