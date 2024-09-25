#include <stdlib.h>

int main()
{
	char *m = malloc(1024);
	(void)m; // avoid warning about unused variable

	// Ohh no! We're leaking memory...
	// free(m);
	return 0;
}
