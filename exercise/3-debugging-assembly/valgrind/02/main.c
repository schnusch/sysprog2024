#include <stdlib.h>
int main() {
	int *p, i, a;
	p = malloc(10*sizeof(int));
	p[10] = 1;
	a = p[10];
	free(p);
	return 0;
}
