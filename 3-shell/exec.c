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

#include "backup-errno.h"
#include "exec.h"

#ifdef TRACE_TCSETPGRP
static int echo_tcsetpgrp(int fd, pid_t pgrp) {
	int ret = tcsetpgrp(fd, pgrp);
	BACKUP_ERRNO();
	fprintf(stderr, "tcsetpgrp(%d, %ld) = %d\n", fd, (long)pgrp, ret);
	return ret;
}
#define tcsetpgrp echo_tcsetpgrp
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
 *  Used as `__attribute__((cleanup(closep_no_std_no_errno)))`.
 */
static inline void closep_no_std_no_errno(int *fd) {
	BACKUP_ERRNO();
	(void)!closep_no_std(fd);
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
	ERROR_SETPGID,
	ERROR_PIPE,
	ERROR_TCSETPGRP,
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
			_exit(127);  // bash uses 126 or 127 after failed execve
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
		_exit(127);  // bash uses 126 or 127 after failed execve
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
			} else if(errno == EINTR) {
				// redo `read` if interrupted by a signal
			} else {
				BACKUP_ERRNO();
				(void)!close(error_fds[0]);
				kill_child_no_errno(child);
				return -1;
			}
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

/**
 *   1. create a new process group
 *   2. open initial stdin/final stdout
 *   3. create required pioes
 *   4. start commands with their pipes
 **/
static int run_piped_commands(const struct pipeline *p, int error_fd, int verbose) {
	// TODO signal handling

	// The following hierarchy exists:
	// session > controlling termjnal > process group
	//
	// All processes sharing the same controlling terminal are also part of
	// the same session. Since we want to keep the controlling terminal
	// (if any) we do not change the session ID (with setsid(2)).
	//
	// A controlling terminal has exactly one foreground process group and
	// any number of background process groups. If a background process group
	// tries to read from the controlling terminal it receices SIGTTIN. The
	// foreground process group is set with `tcsetgrp(3)`. `SIGINT` caused
	// by Ctrl+C on the terminal is only sent to the foreground process group.
	//
	// Initially the shell's process group is the controlling terminal's
	// foreground process group. Once a pipeline is started, its process group
	// is set to be in the foreground, unless the pipeline is to be run in
	// the background. Once the pipeline is finished the shell will become
	// foreground again.
	//
	// see credentials(7), setsid(2)
	if(setpgid(getpid(), 0) < 0) {
		write_error_pipe_no_errno(error_fd, errno, ERROR_SETPGID);
		return EXIT_FAILURE;
	}
	if(!p->background) {
		// set current pipeline's process group to foreground
		// TODO STDIN_FILENO vs STDERR_FILENO, bash seems to use STDERR_FILENO
		if(tcsetpgrp(STDIN_FILENO, getpid()) < 0 && errno != ENOTTY) {
			write_error_pipe_no_errno(error_fd, errno, ERROR_TCSETPGRP);
			return EXIT_FAILURE;
		}
	}

	__attribute__((cleanup(closep_no_std_no_errno)))
	int current_stdin = STDIN_FILENO;
	if(p->stdin) {
		current_stdin = open(p->stdin, O_RDONLY);
		if(current_stdin < 0) {
			write_error_pipe_no_errno(error_fd, errno, ERROR_OPEN);
			return EXIT_FAILURE;
		}
	}

	__attribute__((cleanup(closep_no_std_no_errno)))
	int final_stdout = STDOUT_FILENO;
	if(p->stdout) {
		final_stdout = open(p->stdout, O_CREAT | O_WRONLY | O_TRUNC, 0666);
		if(final_stdout < 0) {
			write_error_pipe_no_errno(error_fd, errno, ERROR_OPEN);
			return EXIT_FAILURE;
		}
	}

	for(argument_list *cmd = p->commands; *cmd; ++cmd) {
		int fds[2] = {-1, final_stdout};
		if(cmd[1]) {
			// There is a command following after this one, so we create pipe
			// from this command to the next.
			if(pipe(fds) < 0) {
				write_error_pipe_no_errno(error_fd, errno, ERROR_PIPE);
				return EXIT_FAILURE;
			}
		}

		// After creating the pipe the following file descriptors exist:
		// `current_stdin` is the current command's stdin
		// `final_stdout` is the final command's stdout
		// `fds[0]` is the next command's stdin
		// `fds[1]` is the current command's stdout

		pid_t child = start_command(cmd[0], (struct file_descriptors){
			.stdin = current_stdin,
			.stdout = fds[1],
			// Close read end of pipe (it is meant for the next command) and
			// the final command's stdout.
			// `fds[0]` must come last, because it is -1 for the last
			// command (`cmd[1]` == NULL).
			.close = (int[]){final_stdout, fds[0], -1},
		});
		if(child < 0) {
			write_error_pipe_no_errno(error_fd, errno, ERROR_FORK);
			BACKUP_ERRNO();
			(void)!closep_no_std(&fds[0]);
			(void)!closep_no_std(&fds[1]);
			return EXIT_FAILURE;
		}

		if(verbose) {
			print_command(cmd[0], (struct file_descriptors){
				.stdin = current_stdin,
				.stdout = fds[1],
			}, fds[0]);
		}

		// Close the write end of the pipe, so it is exclusively used as the
		// child's stdout.
		if(closep_no_std(&fds[1])) {
			write_error_pipe_no_errno(error_fd, errno, ERROR_CLOSE);
			BACKUP_ERRNO();
			(void)!closep_no_std(&fds[0]);
			(void)!closep_no_std(&fds[1]);
			kill_child_no_errno(child);
			return EXIT_FAILURE;
		}

		// Another file-descriptor to the read end won't mess with EOF, so we
		// can ignore the error.
		(void)!closep_no_std(&current_stdin);
		current_stdin = fds[0];
	}

	// close error_fd, starting processes was successful
	if(close(error_fd) < 0) {
		write_error_pipe_no_errno(error_fd, errno, ERROR_CLOSE);
		return EXIT_FAILURE;
	}

#ifdef TRACE_FILE_DESCRIPTORS
	flock(STDERR_FILENO, LOCK_EX);
	dprintf(STDERR_FILENO, "complete pipeline started (%d).\n", getpid());
	print_open_file_descriptors_no_errno();
	flock(STDERR_FILENO, LOCK_UN);
#endif

	// If the commands are to be run in the background we can just exit our
	// pipeline's subprocess after all its commands where started.
	if(p->background) {
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;

	// `waitpid(0, ...)` waits for any child process in our process group.
	// We could use `waitpid(-1, ...)` to also wait for child processes that
	// changed their process group, but they might have created their own
	// session and so on, so we do not wait for them anymore.
	// see https://man7.org/linux/man-pages/man2/wait.2.html#DESCRIPTION
	while(1) {
		int status;
		if(waitpid(0, &status, 0) < 0) {
			if(errno == ECHILD) {
				// no more child processes running
				break;
			} else if(errno == EINTR) {
				// interrupted by a signal handler, just retry
				continue;
			} else {
				// should not be returned by waitpid, but better safe than sorry
				write_error_pipe_no_errno(error_fd, errno, ERROR_WAIT);
				return EXIT_FAILURE;
			}
		}
		if(exit_status == 0 && (WIFEXITED(status) || WIFSIGNALED(status))) {
			// collect the first non-zero exit status
			exit_status = WEXITSTATUS(status);
		}
	}

	return exit_status;
}

/**
 *  Execute the pipeline. (`run_piped_commands` actually does everything.)
 */
int run_pipeline(const struct pipeline *p, int verbose) {
	int error_fds[2];
	pid_t pgrp = fork_with_pipe(error_fds);
	if(pgrp < 0) {
		return -1;
	} else if(pgrp == 0) {
		// child process
		_exit(run_piped_commands(p, error_fds[1], verbose));
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
			} else if(errno == EINTR) {
				// redo `read` if interrupted by a signal
			} else {
				BACKUP_ERRNO();
				(void)!close(error_fds[0]);
				kill_child_no_errno(pgrp);
				return -1;
			}
		}

		// We cannot use BACKUP_ERRNO() because we might want to return our
		// errno.
		int errno_backup = errno;

		int status;
		do {
			if(waitpid(pgrp, &status, 0) < 0) {
				kill(pgrp, SIGKILL);
				errno = errno_backup;
				return -1;
			}
		} while(!WIFEXITED(status) && !WIFSIGNALED(status));

		// set the shells process group to foreground
		// TODO STDIN_FILENO vs STDERR_FILENO, bash seems to use STDERR_FILENO
		if(tcsetpgrp(STDIN_FILENO, getpid()) < 0 && errno != ENOTTY) {
			return -1;
		}

		return WEXITSTATUS(status);
	}
}
