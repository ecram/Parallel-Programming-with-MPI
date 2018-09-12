/*
 *
 * Copyright (c) 2001-2006 Hewlett-Packard Development Company, L.P.
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
#include <time.h>

#ifdef __GNUC__

static inline void
clear_psr_ac(void)
{
	__asm__ __volatile__("rum psr.ac;;" ::: "memory" );
}

static inline unsigned long 
get_umask(void)
{
        unsigned long tmp;

        asm volatile ("mov %0=psr.um" : "=r"(tmp));

        return tmp;
}

#elif defined(__ECC) && defined(__INTEL_COMPILER)
#include <ia64intrin.h>
#define clear_psr_ac()	__rum(1<<3)
#define get_umask()	__getReg(IA64_PSR_L);
#else
#error "You need to define clear_psr_ac() for your compiler"
#endif


#define PFM_TEST_INVALID	-1
#define PFM_TEST_VALID		0


static union {
	unsigned long   l_tab[2];
	unsigned int    i_tab[4];
	unsigned short  s_tab[8];
	unsigned char   c_tab[16];
} __attribute__((__aligned__(32))) messy;


/*
 * 1 load, 1 store both unaligned
 */
int
do_two_una(unsigned long pace_count)
{
	unsigned int *l, v;
	unsigned long c = pace_count;

	static unsigned int called;

	called++;
	l = (unsigned int *)(messy.c_tab+1);

	if (((unsigned long)l & 0x1) == 0) {
		printf("Data is not unaligned, can't run test\n");
		return  -1;
	}

	v = *l;
	while(c) c--; /* space the accesses */
	v++;
	*l = v;

	if (v != called) return -1;

	return c == 0 ? 0: -1;
}

int
do_una_test1(unsigned long pace)
{
	return do_two_una(pace);
}

int
do_una_test2(unsigned long pace)
{
	return do_two_una(pace);
}

int
do_una_test(unsigned long count, unsigned long pace)
{
	int ret;

	ret = 0;
	while (count-- && ret == 0) {
		ret = count & 0x1 ? do_una_test1(pace) : do_una_test2(pace);
	}
	return ret;
}



int
main(int argc, char **argv)
{
	unsigned long count, pace;
	unsigned long umask;

	umask = get_umask();
	printf("default umask=0x%lx psr.ac=%ld\n", umask, (umask >> 3) & 0x1);

	/* let the hardware do the unaligned access */
	clear_psr_ac();

	umask = get_umask();
	printf("changed umask=0x%lx psr.ac=%ld\n", umask, (umask >> 3) & 0x1);

	count = argc > 1 ? strtoul(argv[1], NULL, 10) : 1;
	pace  = argc > 2 ? strtoul(argv[2], NULL, 10) : 0;

	return do_una_test(count, pace);
}
