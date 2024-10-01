#define _POSIX_C_SOURCE 200809L  // fdopendir, fstatat
#define _GNU_SOURCE  // strndupa
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>  // AT_SYMLINK_NOFOLLOW
#include <fnmatch.h>
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
		.stat_flags = AT_SYMLINK_NOFOLLOW,
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
			args->stat_flags &= ~AT_SYMLINK_NOFOLLOW;
		} else if(strcmp(argv[i], "-xdev") == 0) {
			args->xdev = 1;
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

int fnmatch_slash(const char *pattern, const char *string, int flags) {
	int ret = fnmatch(pattern, string, flags);
	if(ret != 0) {
		const char *end = string + strlen(string);
		// Remove all trailing slashes, except string[0].
		while(end > string + 1 && end[-1] == '/') {
			--end;
		}
		// Skip until last non-trailing slash.
		const char *start = memrchr(string, '/', end - string);
		if(start) {
			if(start < end - 1) {
				++start;
			}
		} else {
			start = string;
		}
		if(start > string || *end == '/') {
			// Copy substring on the stack and match again.
			string = strndupa(start, end - start);
			ret = fnmatch(pattern, string, flags);
		}
	}
	return ret;
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
	dev_t dev;
	ino_t ino;
	struct dir_chain *parent;
};

/**
 *  Print chain->parent->...->name, chaing->parent->name, and then chain->name
 *  separated by '/'.
 */
static int print_dir_chain(FILE *out, const struct dir_chain *chain) {
	if(chain) {
		int ret = print_dir_chain(out, chain->parent);
		if(ret < 0) {
			return ret;
		} else if(ret != 0) {
			// Return 0 tells us no preceding path element or the preceding
			// element ended with `/`. Only print `/` if neither is the case.
			if(fputc('/', out) == EOF) {
				return -1;
			}
		}
		if(fputs(chain->name, out) == EOF) {
			return -1;
		}
		size_t n = strlen(chain->name);
		if(n > 0 && chain->name[n - 1] == '/') {
			// Trailing slash, do not print another slash after it.
			return 0;
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
	struct stat st;
};

/**
 *  Print error to `err`.
 *  Format: "${prefix}${prefix+: }cannot ${verb} ${path}: ${strerror}"
 */
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
 */
static int find(struct find_args *args, struct dir_chain *this) {
	////////////////////////////////////////////////////////////////////
	// WE MUST NOT USE `args->st` AFTER PASSING IT TO ANOTHER `find`, //
	// BECAUSE IT WILL BE REUSED.                                     //
	////////////////////////////////////////////////////////////////////

	// Call stat(2) on the file by directory file descriptor and its name
	// relative to the file descriptor. ("/proc/self/fd/${dir_fd}/${name}")
	// Avoids some file system race conditions, but mainly lets us avoid string
	// manipulation.
	if(fstatat(this->dir_fd, this->name, &args->st, args->stat_flags) < 0) {
		find_error(args->err, args->err_prefix, "stat", this);
		return -1;
	}

	// detect file system loop
	this->dev = args->st.st_dev;
	this->ino = args->st.st_ino;
	for(const struct dir_chain *c = this->parent; c; c = c->parent) {
		if(c->ino == this->ino && c->dev == this->dev) {
			if(args->err) {
				int errbak = errno;
				if(args->err_prefix) {
					fprintf(args->err, "%s: ", args->err_prefix);
				}
				fputs("file system loop detected: ", args->err);
				print_dir_chain(args->err, this);
				fputs(" = ", args->err);
				print_dir_chain(args->err, c);
				fputc('\n', args->err);
				errno = errbak;
			}
			return -1;
		}
	}

	if(
		// name does not match
		(args->search_name && fnmatch_slash(args->search_name, this->name, 0) != 0)
		// type does not match
		|| (args->mode != (mode_t)-1 && (args->st.st_mode & S_IFMT) != args->mode)
	) {
		// nop
	} else {
		// Print path.
		if(print_dir_chain(args->out, this) < 0 || fputc('\n', args->out) == EOF) {
			if(args->err) {
				int errbak = errno;
				if(args->err_prefix) {
					fprintf(args->err, "%s: ", args->err_prefix);
				}
				fprintf(args->err, "cannot write: %s\n", strerror(errbak));
				errno = errbak;
			}
			return -1;
		}
	}

	// we cannot recurse into non-directories
	if(!S_ISDIR(args->st.st_mode)) {
		return 0;
	}

	// st_dev changed, so we crossed onto another file system, stop recursion
	if(args->xdev != (dev_t)-1 && args->st.st_dev != args->xdev) {
		return 0;
	}

	//////////////////////////////////////////////////////////////
	// BE VERY CAREFUL ABOUT USING `args->st` AFTER THIS POINT. //
	//////////////////////////////////////////////////////////////

	// Prepare next element in directory chain for callees.
	struct dir_chain child = {
		.parent = this,
	};

	// Open a new directory file descriptor to access children of the current
	// directory. We cannot use O_PATH because we later use fdopendir(3).
	child.dir_fd = openat(this->dir_fd, this->name, O_RDONLY | O_DIRECTORY);
	if(child.dir_fd < 0) {
		find_error(args->err, args->err_prefix, "open", this);
		return -1;
	}

	// We must copy the directory file descriptor before passing it to to
	// fdopendir(3) because it will take posession of it.
	//
	// > The fdopendir() function is like opendir(), but returns a
	// > directory stream for the directory referred to by the open file
	// > descriptor fd.  After a successful call to fdopendir(), fd is
	// > used internally by the implementation, and should not otherwise
	// > be used by the application.
	// -- https://man7.org/linux/man-pages/man3/opendir.3.html#DESCRIPTION
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
		// ignore "." and ".."
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
		// Now `child` will look like this:
		// (struct dir_chain){
		//     .name = e->d_name,
		//     .dir_fd = /* file descriptor to the directory we are iterating */,
		//     .parent = this,
		// };
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

/**
 *  Prepare `struct find_args` and `struct dir_chain` for `find`.
 *  If `cmd->xdev` is set, we also stat `name` to retrieve the expected `dev_t`.
 */
static int find_prepare(const char *argv0, const char *name, const struct cmd_args *cmd) {
	// We cannot remove trailing / from `name` here, so that `print_dir_chain`
	// does not print //, because `a` and `a/` might be different paths when
	// symlinks are involved and the path `/`.
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
		if(fstatat(root.dir_fd, root.name, &args.st, args.stat_flags) < 0) {
			return 1;
		}
		assert(args.st.st_dev != args.xdev);  // we use (mode_t)-1 as a special value
		args.xdev = args.st.st_dev;
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
