#include <fcntl.h>  // AT_SYMLINK_NOFOLLOW
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

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
					"Usage: %s [-name NAME] [-type <d|f>] [-follow|-L] [-xdev] DIR...\n",
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

int main(int argc, char **argv) {
	struct cmd_args args;
	int optind = parse_args(&args, argc, argv);
	if(optind < 0) {
		return 2;
	}

	printf("optind = %d\n", optind);
	printf("(struct cmd_args){\n");
	printf("\t.name = \"%s\",\n", args.name);
	printf("\t.mode = %zx,\n", (size_t)args.mode);
	printf("\t.stat_flags = %x,\n", args.stat_flags);
	printf("\t.xdev = %d,\n", args.xdev);
	printf("}\n");
	for(int i = optind; i < argc; ++i) {
		printf("argc[%d] = \"%s\"\n", i, argv[i]);
	}

	return 1;
}
