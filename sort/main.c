#include "exit.h"
#include "mmap.h"
#include "read-write.h"

void *memcpy(void *dest, const void *src, unsigned long n) {
	char *d = dest;
	const char *s = src;
	while(n-- > 0) {
		*d++ = *s++;
	}
	return dest;
}

char *read_lines(unsigned long *len, int fd) {
	unsigned long size = 0;
	char *buf = NULL;
	unsigned long off = 0;

	while(1) {
		if(off >= size) {
			// realloc
			size += PAGE_SIZE;
			char *new = malloc(size);
			if(!new) {
				return NULL;
			}
			memcpy(new, buf, off);
			buf = new;
		}
		long nread = read(fd, buf + off, size - off);
		if(nread < 0) {
			return NULL;
		} else if(nread == 0) {
			break;
		}
		off += nread;
	}

	if(off > 0 && buf[off - 1] != '\n') {
		// append \n
		if(off >= size) {
			// realloc
			size += PAGE_SIZE;
			char *new = malloc(size);
				if(!new) {
				return NULL;
			}
			memcpy(new, buf, off);
			buf = new;
		}

		buf[off++] = '\n';
	}

	*len = off;
	return buf;
}

struct line {
	const char *start;
	unsigned long length;
};

struct line *create_line_index(const char *data, unsigned long size) {
	unsigned long line_count = 0;
	for(const char *p = data; p < data + size; ++p) {
		if(*p == '\n') {
			++line_count;
		}
	}
	struct line *lines = malloc((line_count + 1) * sizeof(*lines));
	if(!lines) {
		return NULL;
	}
	const char *p = data;
	unsigned long i = 0;
	while(p < data + size) {
		lines[i].start = p;
		while(p < data + size && *p != '\n') {
			++p;
		}
		lines[i].length = p - lines[i].start;
		++i;
		++p;
	}
	if(i != line_count) {
		return NULL;
	}
	lines[i].start = NULL;
	return lines;
}

int linecmp(const struct line *a, const struct line *b) {
	const char *c = a->start;
	const char *d = b->start;
	unsigned long m = a->length;
	unsigned long n = b->length;
	while(m > 0 && n > 0 && *c == *d) {
		++c, ++d;
		--m, --n;
	}
	if(m > 0) {
		if(n > 0) {
			return *c - *d;
		} else {
			return 1;
		}
	} else {
		if(n > 0) {
			return -1;
		} else {
			return 0;
		}
	}
}

int main(void) {
	unsigned long size;
	char *input = read_lines(&size, STDIN_FILENO);
	if(!input) {
		return 1;
	}

	struct line *lines = create_line_index(input, size);
	if(!lines) {
		return 1;
	}

	int changed;
	do {
		changed = 0;

		struct line *a = lines;
		struct line *b = lines + 1;
		if(a->start) {
			while(b->start) {
				if(linecmp(a, b) > 0) {
					struct line tmp = *a;
					*a = *b;
					*b = tmp;
					changed = 1;
				}
				a = b;
				++b;
			}
		}
	} while(changed);

	for(struct line *a = lines; a->start; ++a) {
		// write including \n
		if(write(STDOUT_FILENO, a->start, a->length + 1) <= (long)a->length) {
			return 1;
		}
	}

	return 0;
}
