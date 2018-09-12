/*
 * Copyright (c) 2003-2006 Hewlett-Packard Development Company, L.P.
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
#include <sys/wait.h>

#ifdef __GNUC__
static inline void clear_psr_ac(void)
{
	__asm__ __volatile__("rum psr.ac;;" ::: "memory" );
}
#elif defined(__ECC) && defined(__INTEL_COMPILER)
#include <ia64intrin.h>
#define clear_psr_ac()	__rum(1<<3)
#else
#error "You need to define clear_psr_ac() for your compiler"
#endif


static int start, stop;

static union {
	unsigned long   l_tab[2];
	unsigned int    i_tab[4];
	unsigned short  s_tab[8];
	unsigned char   c_tab[16];
} __attribute__((__aligned__(32))) messy;

int
do_una(void)
{
	unsigned int *l, v;
	static unsigned int called;

	called++;
	l = (unsigned int *)(messy.c_tab+1);

	if (((unsigned long)l & 0x1) == 0) {
		printf("Data is not unaligned, can't run test\n");
		return  -1;
	}

	v = *l;
	v++;
	*l = v;

	if (v != called) return -1;


	return 0;
}

int
foo1(unsigned long count)
{
	int ret = 0;

	while ( count-- && ret == 0) {
		ret = do_una();
	}
	return ret;
}

int
foo2(unsigned long count)
{
	int ret = 0;

	while ( count-- && ret == 0) {
		ret = do_una();
	}
	return ret;
}

static void
do_test(unsigned long count)
{
	start = 1;

	foo1(count);

	start = 0;

	stop = 1;

	foo2(count);
}

int
main(int argc, char **argv)
{
	unsigned long count;
	int nfork;

	/* let the hardware do the unaligned access */
	clear_psr_ac();

	count = argc > 1 ? strtoul(argv[1], NULL, 10) : 1;
	nfork = argc > 2 ? atoi(argv[2]) : 0;
	printf("creating %d process(es)\n", nfork);

	while (nfork--) {
		switch(fork()) {
			case -1: perror("fork"); goto cleanup;
			case  0: do_test(count); exit(0);
		}
	}
	do_test(count);
cleanup:
	while(wait4(-1, NULL, 0 , NULL) > 0);

	return 0;
}
