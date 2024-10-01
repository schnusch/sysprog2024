#include "exit.h"
#include "read-write.h"

int main(void) {
	char buffer[1024];
	long line_count = 0;

	long nread;
	while((nread = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
		for(const char *p = buffer; p < buffer + nread; ++p) {
			if(*p == '\n') {
				++line_count;
			}
		}
	}
	if(nread < 0) {
		return 1;
	}

	// We write from end of buffer backwards, because that is how we convert
	// int to string.
	// The resulting string will be at `p` until end of `buffer`.
	char *p = buffer + sizeof(buffer);
	*--p = '\n';
	do {
		if(p <= buffer) {
			return 1;
		}
		*--p = '0' + (line_count % 10);
		line_count /= 10;
	} while(line_count > 0);

	if(write(STDOUT_FILENO, p, buffer + sizeof(buffer) - p) < 0) {
		return 1;
	} else {
		return 0;
	}
}
