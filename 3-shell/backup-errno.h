#ifndef BACKUP_ERRNO_H
#define BACKUP_ERRNO_H

// dprintf
#ifndef _POSIX_C_SOURCE
 #define _POSIX_C_SOURCE 200809L
#else
 #if _POSIX_C_SOURCE < 200809L
  #undef _POSIX_C_SOURCE
  #define _POSIX_C_SOURCE 200809L
 #endif
#endif

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

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
	const char *filename;
	size_t line;
};
static inline struct errno_backup backup_errno(const char *filename, size_t line) {
	int errnum = errno;
	dprintf(STDERR_FILENO, "errno: backed up at  %s:%zu: %d\n", filename, line, errno);
	errno = errnum;
	return (struct errno_backup) {
		.errnum = errno,
		.filename = filename,
		.line = line,
	};
}
static inline void restore_errno(struct errno_backup *backup) {
	dprintf(STDERR_FILENO, "errno: restored from %s:%zu: %d\n", backup->filename, backup->line, backup->errnum);
	errno = backup->errnum;
}
 #define BACKUP_ERRNO() struct errno_backup errno_backup_##__LINE__ __attribute__((cleanup(restore_errno))) = backup_errno(__FILE__, __LINE__)
#else
// silent version
static inline void restore_errno(int *errnum) {
	errno = *errnum;
}
 #define BACKUP_ERRNO() int errno_backup_##__LINE__ __attribute__((cleanup(restore_errno))) = errno
#endif

#endif
