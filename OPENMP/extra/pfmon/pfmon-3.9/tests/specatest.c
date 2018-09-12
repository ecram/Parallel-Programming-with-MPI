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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#define RESET_VAL  (~0UL)

#ifdef __GNUC__
#define test_chka(res, adr) \
{ \
	__asm__ __volatile__(  \
		"{.mii ld8.a r30=[%1]}\n" \
		";;\n" \
		"{.mii st8 [%1]=r0}\n" \
		";;\n" \
		"chk.a.clr r30, 1f\n" \
		";;\n" \
		"mov %0=1\n" \
		";;\n" \
		"1:\n" \
		"mov %0=2;;\n" \
		: "=r"(res): "r"(adr): "r30", "memory"); \
}
#else
/*
 * don't quite know how to do this without the GNU inline assembly support!
 * So we force a test failure
 */
#define test_chka(res)	res = 0
#endif

int
specloop(unsigned long loop)
{
	unsigned long res;
	unsigned long tab[1];

	while ( loop-- ) {
		res = 7;
		test_chka(res,tab);

		if (res != 2) return -1;
	}
	return 0;
}

int
main(int argc, char **argv)
{
	unsigned long loop;
#ifndef __GNUC__
#error "This test program does not work if not compiled with GNU C.\n"
#endif

	loop = argc > 1 ? strtoul(argv[1], NULL, 10) : 1;
	return specloop(loop);
}
