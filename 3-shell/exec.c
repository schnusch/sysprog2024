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

/**
 *  Define helpers to backup and restore `errno`. `errno` is backed up, by
 *  assigning `errno` to a variable. `errno` is restored once that variable
 *  goes out of scope by using GCC's `__attribute__((cleanup(...)))` (If
 *  projects like systemd get away with using it, we should in this lab as
 *  well.).
 */
#ifdef TRACE_ERRNO_BACKUP
// tracing version that also prints when/where errno is backed up/restored
struct errno_backup {
	int errnum;
	size_t line;
};
static inline int backup_errno(size_t lineno) {
	int errnum = errno;
	dprintf(STDERR_FILENO, "backing up errno at line %zu: %d\n", lineno, errno);
	errno = errnum;
	return errno;
}
static inline void restore_errno(struct errno_backup *backup) {
	dprintf(STDERR_FILENO, "restoring errno from line %zu: %d\n", backup->line, backup->errnum);
	errno = backup->errnum;
}
 #define BACKUP_ERRNO() struct errno_backup errno_backup_##__LINE__ __attribute__((cleanup(restore_errno))) = { .errnum = backup_errno(__LINE__), .line = __LINE__ }
#else
// silent version
static inline void restore_errno(int *errnum) {
	errno = *errnum;
}
 #define BACKUP_ERRNO() int errno_backup_##__LINE__ __attribute__((cleanup(restore_errno))) = errno
#endif

#ifdef TRACE_FILE_DESCRIPTORS
 #include <dirent.h>
 #include <sys/file.h>

/**
 *  `readlinkat(2)` with allocation and terminating null-byte.
 */
static char *alloc_readlinkat(int dirfd, const char *pathname) {
	char *buf = NULL;
	size_t size = 256 / 2;
	ssize_t n = 0;
	do {
		size *= 2;
		char *tmp = realloc(buf, size);
		if(!tmp) {
			free(buf);
			return NULL;
		}
		buf = tmp;

		n = readlinkat(dirfd, pathname, buf, size);
		if(n < 0) {
			free(buf);
			return NULL;
		}
	} while((size_t)n == size);
	buf[n] = '\0';
	return buf;
}

/**
 *  Iterate and print the contents of `/proc/self/fd` and the file descriptor's
 *  `O_CLOEXEC` flag.
 */
static void print_open_file_descriptors_no_errno(void) {
	BACKUP_ERRNO();

	int dir_fd = open("/proc/self/fd", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if(dir_fd < 0) {
		return;
	}
	int duped_dir_fd = dup(dir_fd);
	if(duped_dir_fd < 0) {
		goto cleanup;
	}
	DIR *d = fdopendir(duped_dir_fd);
	if(!d) {
		close(duped_dir_fd);
		goto cleanup;
	}
	while(1) {
		errno = 0;
		struct dirent *e = readdir(d);
		if(!e) {
			// TODO errno
			break;
		}
		if(
			e->d_name[0] == '.'
			&& (
				e->d_name[1] == '\0'
				|| (e->d_name[1] == '.' && e->d_name[2] == '\0')
			)
		) {
			continue;
		}

		const char *str_flags = "";

		char *end;
		errno = 0;
		long fd = strtol(e->d_name, &end, 10);
		if(fd >= 0 && (fd != LONG_MAX || errno != ERANGE)) {
			int flags = fcntl((int)fd, F_GETFD);
			if(flags >= 0 && flags & FD_CLOEXEC) {
				str_flags = " (O_CLOEXEC)";
			}
		}

		dprintf(STDERR_FILENO, "/proc/self/fd/%s", e->d_name);
		char *target = alloc_readlinkat(dir_fd, e->d_name);
		if(target) {
			dprintf(STDERR_FILENO, " => %s", target);
			free(target);
		}
		dprintf(STDERR_FILENO, "%s\n", str_flags);
	}
	closedir(d);
cleanup:
	close(dir_fd);
}
#endif

/**
 *  Close the file descriptor and set it to -1.
 */
__attribute__((warn_unused_result))
static inline int closep(int *fd) {
	if(*fd < 0) {
		return 0;
	} else {
		int ret = close(*fd);
		if(ret == 0) {
			*fd = -1;
		}
		return ret;
	}
}

/**
 *  Helper that avoids accidentally closing the standard file descriptors.
 */
__attribute__((warn_unused_result))
static inline int closep_no_std(int *fd) {
	if(*fd == STDIN_FILENO || *fd == STDOUT_FILENO || *fd == STDERR_FILENO) {
		return 0;
	} else {
		return closep(fd);
	}
}

/**
 *  Packet written to the pipe of `write_error_pipe_no_errno` to communicate
 *  errors to the parent process.
 *  If it's size is at most `PIPE_BUF` bytes is read/write is guaranteed to be
 *  atomic, see `pipe(7)`.
 */
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
	BACKUP_ERRNO();
	struct error_packet e;
	e.cause = cause;
	e.errnum = errnum;
	// writes of up to `PIPE_BUF` bytes are atomic
	(void)!write(error_fd, &e, sizeof(e));
}

static inline void kill_child_no_errno(pid_t child) {
	BACKUP_ERRNO();
	// kill and reap child process
	if(kill(child, SIGKILL) == 0) {
		int status;
		while(
			waitpid(child, &status, 0) >= 0
			&& !WIFEXITED(status)
			&& !WIFSIGNALED(status)
		) { }
	}
}

/**
 *  Create a pipe (with `O_CLOEXEC`) to atomically communicate from child to
 *  parent, fork, and close all but the required file descritors.
 */
static pid_t fork_with_pipe(int error_fds[2]) {
	if(pipe2(error_fds, O_CLOEXEC) < 0) {
		return -1;
	}
	pid_t child = fork();
	if(child < 0) {
		return -1;
	} else if(child == 0) {
		// We can ignore errors when closing the read end in the child process
		// because it will not interfere with the communication.
		(void)!closep(&error_fds[0]);
		error_fds[0] = -1;
		return child;
	} else {
		// The write end must be closed in the parent process, so the only
		// remainig write end is in the child process and its close in the child
		// process (e.g. implicitly by exec) will result in EOF on the read end.
		if(closep(&error_fds[1]) < 0) {
			BACKUP_ERRNO();
			(void)!close(error_fds[0]);
			kill_child_no_errno(child);
			return -1;
		}
		error_fds[1] = -1;
		return child;
	}
}

/**
 * allow keyword arguments in `start_command`
 */
struct file_descriptors {
	int stdin;
	int stdout;
	int *close;
};

/**
 *   1. fork a child process
 *   2. in the child process: `dup` `fds.stdin` to `STDIN_FILENO` and
 *      `fds.stdout` to `STDOUT_FILENO`
 *   3. in the child process: close `fds.stdin`, `fds.stdout`, and all file
 *      descriptors in `fds.close` (`fds.close` must be terminated with `-1`).
 *      Skip `STDIN_FILENO`, `STDOUT_FILENO`, and `STDERR_FILENO`.
 *   4. in the child process: exec `cmd`
 *   5. in the parent process: wait for successful `execvp` in the child (with
 *      a `O_CLOEXEC` pipe) and return
 */
pid_t start_command(argument_list cmd, struct file_descriptors fds) {
	int error_fds[2];
	pid_t child = fork_with_pipe(error_fds);
	if(child < 0) {
		return -1;
	} else if(child == 0) {
#ifdef TRACE_FILE_DESCRIPTORS
		flock(STDERR_FILENO, LOCK_EX);
		dprintf(STDERR_FILENO, "start_command({");
		for(char **arg = cmd; *arg; ++arg) {
			dprintf(STDERR_FILENO, "\"%s\", ", *arg);
		}
		dprintf(STDERR_FILENO, "NULL}, { /* %d */\n", getpid());
		dprintf(STDERR_FILENO, "\t.stdin = %d,\n", fds.stdin);
		dprintf(STDERR_FILENO, "\t.stdout = %d,\n", fds.stdout);
		dprintf(STDERR_FILENO, "\t.close = {");
		for(int *fd = fds.close; *fd >= 0; ++fd) {
			dprintf(STDERR_FILENO, "%d, ", *fd);
		}
		dprintf(STDERR_FILENO, "-1},\n");
		dprintf(STDERR_FILENO, "})\n");
		flock(STDERR_FILENO, LOCK_UN);
#endif

		// dup2 is a no-op if oldfd and newfd are equal
		if(dup2(fds.stdin, STDIN_FILENO) < 0 || dup2(fds.stdout, STDOUT_FILENO) < 0) {
			write_error_pipe_no_errno(error_fds[1], errno, ERROR_DUP);
			_exit(EXIT_FAILURE);  // FIXME exit with 127/-1?
		}

		// close pipe fds, they were duped
		(void)!closep_no_std(&fds.stdin);
		(void)!closep_no_std(&fds.stdout);
		// close extra file descriptors
		for(int *fd = fds.close; *fd >= 0; ++fd) {
			(void)!closep_no_std(fd);
		}

#ifdef TRACE_FILE_DESCRIPTORS
		flock(STDERR_FILENO, LOCK_EX);
		dprintf(
			STDERR_FILENO,
			"about to exec %s in %d:\n",
			cmd[0],
			getpid()
		);
		print_open_file_descriptors_no_errno();
		flock(STDERR_FILENO, LOCK_UN);
#endif

		execvp(cmd[0], cmd);
		write_error_pipe_no_errno(error_fds[1], errno, ERROR_EXEC);
		_exit(EXIT_FAILURE);  // FIXME exit with 127/-1?
	} else {
		struct error_packet e;
		while(1) {
			ssize_t n = read(error_fds[0], &e, sizeof(e));
			if(n > 0) {
				errno = e.errnum;
			} else if(n == 0) {
				// write end was closed, so exec was successful
				(void)!close(error_fds[0]);
				return child;
			}
			BACKUP_ERRNO();
			(void)!close(error_fds[0]);
			kill_child_no_errno(child);
			return -1;
		}
	}
}

static void print_command(argument_list cmd, struct file_descriptors fds, int pipe_r) {
	BACKUP_ERRNO();

	dprintf(STDERR_FILENO, "started $ ");
	for(char **arg = cmd; *arg; ++arg) {
		dprintf(STDERR_FILENO, "%s ", *arg);
	}
	dprintf(
		STDERR_FILENO,
		"<&%d >&%d (%d => %d)\n",
		fds.stdin,
		fds.stdout,
		fds.stdout,
		pipe_r
	);
}

static int run_process_group(const struct pipeline *p, int error_fd, int verbose) {
	return -1;
}

/**
 *  Execute the pipeline. (`run_process_grouo` actually does everything.)
 */
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
				// Reads of at most `PIPE_BUF` bytes are atomic, so if `n > 0`
				// then `n >= sizeof(e)`.
				errno = e.errnum;
			} else if(n == 0) {
				// write end was close
				(void)!close(error_fds[0]);
				break;
			}
			BACKUP_ERRNO();
			(void)!close(error_fds[0]);
			kill_child_no_errno(pgrp);
			return -1;
		}

		// TODO waitpid
		return 0;
	}
}
