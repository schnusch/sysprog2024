#include <stdlib.h>
int main() {
	int *p, i;
	p = malloc(10*sizeof(int));
	for(i = 0;i < 10;i++)
		p[i] = i;
	free(p);
	free(p);
	return 0;
}
