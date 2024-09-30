#define _POSIX_C_SOURCE 200809L  // fdopendir, fstatat
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>  // AT_SYMLINK_NOFOLLOW
#include <stdlib.h>  // EXIT_*
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct cmd_args {
	const char *name;
	mode_t mode;
	int stat_flags;
	int xdev;
};

_Static_assert(
	S_IFMT != (mode_t)-1,
	"We use (mode_t)-1 as a special value, so it must not be a valid file type."
);

/**
 *  Parse arguments and reorder argv so that positional arguments start at the
 *  returned index.
 */
static int parse_args(struct cmd_args *args, int argc, char **argv) {
	*args = (struct cmd_args){
		.name = NULL,
		.mode = -1,
		.stat_flags = 0,
		.xdev = 0,
	};

	int old_argc = argc;
	int i = 1;
	while(i < argc) {
		if(strcmp(argv[i], "-name") == 0) {
			++i;
			if(i >= argc) {
				fprintf(stderr, "%s: missing argument after -name\n", argv[0]);
			usage:
				fprintf(
					stderr,
					"Usage: %s [-name NAME] [-type <d|f>] [-follow|-L] [-xdev] [DIR...]\n",
					argv[0]
				);
				return -1;
			}
			args->name = argv[i];
		} else if(strcmp(argv[i], "-type") == 0) {
			++i;
			if(i >= argc) {
				fprintf(stderr, "%s: missing argument after -type\n", argv[0]);
				goto usage;
			}
			if(argv[i][0] != '\0' && argv[i][1] == '\0') {
				switch(argv[i][0]) {
				case 'd':
					args->mode = S_IFDIR;
					break;
				case 'f':
					args->mode = S_IFREG;
					break;
				default:
					goto unsupported_type;
				}
			} else {
			unsupported_type:
				fprintf(stderr, "%s: unsupported -type: %s\n", argv[0], argv[i]);
				goto usage;
			}
		} else if(strcmp(argv[i], "-follow") == 0 || strcmp(argv[i], "-L") == 0) {
			args->stat_flags |= AT_SYMLINK_NOFOLLOW;
		} else if(strcmp(argv[i], "-xdev") == 0) {
			args->xdev = 0;
		} else {
			// We move positional arguments to the end of argv. argc is
			// decremented because we do not want to parse the moved argument
			// again.
			char *positional = argv[i];
			for(int j = i + 1; j < argc; ++j) {
				argv[j - 1] = argv[j];
			}
			--argc;
			argv[argc] = positional;
			// do not increment i
			continue;
		}
		++i;
	}

	// argv[argc..old_argc] is now sorted reversely
	for(int j = argc, k = old_argc; j < --k; ++j) {
		char *tmp = argv[j];
		argv[j] = argv[k];
		argv[k] = tmp;
	}

	return argc;
}

/**
 *  We store a linked list of `struct dir_chain` on the stack. Every callee
 *  receives a pointer to the caller's `struct dir_chain` it can traverse (or
 *  use `print_dir_chain`) to get the current path.
 *
 *  The callee will receive a `dir_fd` of the parent directory and the `name`
 *  of the current file. It can open it's on directory file descriptors with
 *  `openat(dir_fd, name, ...)`.
 */
struct dir_chain {
	const char *name;
	int dir_fd;
	const struct dir_chain *parent;
};

static int print_dir_chain(FILE *out, const struct dir_chain *chain) {
	if(chain) {
		int ret = print_dir_chain(out, chain->parent);
		if(ret < 0) {
			return ret;
		} else if(ret != 0) {
			if(fputc('/', out) == EOF) {
				return -1;
			}
		}
		if(fputs(chain->name, out) == EOF) {
			return -1;
		}
		return 1;
	} else {
		return 0;
	}
}

/**
 *  `out`         the file stream to write to
 *  `err`         if `err != NULL` write errors to `err`
 *  `err_prefix`  if `err_prefix` prefix all error messages with `err + ": "`
 *  `search_name` search for name
 *  `mode`        if `mode != -1` only look for `st_mode & S_IFMT == mode`
 *  `xdev`        if `xdev != -1` stop recursing if `st_dev != xdev`
 *  `stat_flags`  passed to fstatat(2) (mainly `AT_SYMLINK_NOFOLLOW`)
 */
struct find_args {
	FILE *out;
	FILE *err;
	const char *err_prefix;
	const char *search_name;
	mode_t mode;
	dev_t xdev;
	int stat_flags;
};

static void find_error(
	FILE *err,
	const char *prefix,
	const char *verb,
	const struct dir_chain *this
) {
	if(err) {
		int errbak = errno;
		if(prefix) {
			fprintf(err, "%s: ", prefix);
		}
		fprintf(err, "cannot %s ", verb);
		print_dir_chain(err, this);
		fprintf(err, ": %s\n", strerror(errbak));
		errno = errbak;
	}
}

/**
 *  Implements find(1).
 *  `args` base arguments, passed on recursively, see `struct find_args`
 *  `this` see `struct dir_chain`
 *
 *  TODO shared `struct stat` to save memory
 */
static int find(const struct find_args *args, const struct dir_chain *this) {
	struct stat st;
	if(fstatat(this->dir_fd, this->name, &st, args->stat_flags) < 0) {
		find_error(args->err, args->err_prefix, "stat", this);
		return -1;
	}

	// st_dev changed, so we crossed onto another file system, stop recursion
	if(args->xdev != (mode_t)-1 && st.st_dev == args->xdev) {
		return 0;
	}

	// TODO glob
	if(
		// name does not match
		(args->search_name && strcmp(this->name, args->search_name) != 0)
		// type does not match
		|| (args->mode != (mode_t)-1 && (st.st_mode & S_IFMT) != args->mode)
	) {
		// nop
	} else {
		if(print_dir_chain(args->out, this) < 0 || fputc('\n', args->out) == EOF) {
			// TODO error message
			abort();
			return -1;
		}
	}

	// we cannot recurse into non-directories
	if(!S_ISDIR(st.st_mode)) {
		return 0;
	}

	// prepare directory chain for callees
	struct dir_chain child = {
		.parent = this,
	};

	// access children by file descriptor
	child.dir_fd = openat(this->dir_fd, this->name, O_DIRECTORY);
	if(child.dir_fd < 0) {
		find_error(args->err, args->err_prefix, "open", this);
		return -1;
	}

	// the fd should not be used after fdopendir(3)
	int dup_fd = dup(child.dir_fd);
	if(dup_fd < 0) {
		find_error(args->err, args->err_prefix, "duplicate file descriptor to", this);
		int errbak = errno;
		close(child.dir_fd);
		errno = errbak;
		return -1;
	}
	DIR *d = fdopendir(dup_fd);
	if(!d) {
		find_error(args->err, args->err_prefix, "open directory", this);
		int errbak = errno;
		close(dup_fd);
		close(child.dir_fd);
		errno = errbak;
		return -1;
	}

	// iterate over children
	int ret = 0;
	for(struct dirent *e; (e = readdir(d));) {
		if(
			e->d_name[0] == '.'
			&& (
				e->d_name[1] == '\0'
				|| (
					e->d_name[1] == '.'
					&& e->d_name[2] == '\0'
				)
			)
		) {
			continue;
		}
		child.name = e->d_name;
		if(find(args, &child) < 0) {
			ret = -1;
		}
	}

	int errbak = errno;
	if(closedir(d) < 0 || close(child.dir_fd) < 0) {
		find_error(args->err, args->err_prefix, "close", this);
		// restore errno if there was a previous error
		if(ret) {
			errno = errbak;
		}
		ret = -1;
	}
	return ret;
}

static int find_prepare(const char *argv0, const char *name, const struct cmd_args *cmd) {
	struct dir_chain root = {
		.name = name,
		.dir_fd = AT_FDCWD,
		.parent = NULL,
	};
	struct find_args args = {
		.out = stdout,
		.err = stderr,
		.err_prefix = argv0,
		.search_name = cmd->name,
		.mode = cmd->mode,
		.xdev = -1,
		.stat_flags = cmd->stat_flags,
	};
	if(cmd->xdev) {
		struct stat st;
		if(fstatat(root.dir_fd, root.name, &st, args.stat_flags) < 0) {
			return 1;
		}
		assert(st.st_dev != args.xdev);  // we use (mode_t)-1 as a special value
		args.xdev = st.st_dev;
	}
	return find(&args, &root);
}

_Static_assert(EXIT_FAILURE != 2, "we use exit(2) for wrong command line usage");

int main(int argc, char **argv) {
	struct cmd_args args;
	int optind = parse_args(&args, argc, argv);
	if(optind < 0) {
		return 2;
	}

	int ret = EXIT_SUCCESS;
	if(optind < argc) {
		for(int i = optind; i < argc; ++i) {
			if(find_prepare(argv[0], argv[i], &args) < 0) {
				ret = EXIT_FAILURE;
			}
		}
	} else {
		if(find_prepare(argv[0], ".", &args) < 0) {
			ret = EXIT_FAILURE;
		}
	}

	return ret;
}
