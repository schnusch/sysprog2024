#define _GNU_SOURCE  // sincos
#include <errno.h>
#include <math.h>
#include <stdio.h>

#include "sincos.h"

int print_sincos(double step) {
	if(step == 0) {
		errno = EINVAL;
		return -1;
	}
	double a = step < 0 ? 360 : 0;
	do {
		double sin, cos;
		sincos(a / 180 * M_PI, &sin, &cos);
		if(printf("%8.3f\t%8.3f\t%8.3f\n", a, sin, cos) < 0) {
			return -1;
		}
		a += step;
	} while(0 <= a && a <= 360);
	return 0;
}
