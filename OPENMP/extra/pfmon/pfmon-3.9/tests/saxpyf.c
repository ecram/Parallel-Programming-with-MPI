/*
 * Copyright (c) 2002-2006 Hewlett-Packard Development Company, L.P.
 * Contributed by Stephane Eranian <eranian@hpl.hp.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA
 */
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>

#define VECTOR_SIZE	1000000

static void fatal_error(char *fmt,...) __attribute__((noreturn));

static void
fatal_error(char *fmt, ...) 
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	exit(1);
}


int
saxpyf(double *a, double *b, double *c, unsigned long size)
{

	int i;
	for(i=0; i < size; i++) {
		c[i] = 2*a[i] + b[i];
	}
	return c[0] > 0.0 ? 1 : 0;
}

int
main(int argc, char **argv)
{
	unsigned long size, sum = 0;
	uint64_t nloop;
	int ret = -1;
	double *a, *b, *c;

	size = argc > 1 ? strtoul(argv[1], NULL, 0) : VECTOR_SIZE;
	nloop = argc > 2 ? strtoul(argv[2], NULL, 0) : 1;

	printf("%lu entries = 3 vectors of %lu bytes = %lu Mbytes total\n", 
		size, 
		size*sizeof(unsigned long),
		(3*size*sizeof(unsigned long))>>20
	);
	printf("%"PRIu64" iterations\n", nloop);

	a = malloc(size*sizeof(double));
	b = malloc(size*sizeof(double));
	c = malloc(size*sizeof(double));

	if (a == NULL || b == NULL || c == NULL)
		fatal_error("Cannot allocate vectors\n");

	memset(a, 0, size*sizeof(double));
	memset(b, 0, size*sizeof(double));
	memset(c, 0, size*sizeof(double));


	while(nloop--) {
		ret = saxpyf(a, b, c, size);
		if (ret == -1)
			break;
		sum += ret;
	}
	printf("done saxpyf c=%lu\n", sum);


	return ret == 0 ? 0 : -1;
}
