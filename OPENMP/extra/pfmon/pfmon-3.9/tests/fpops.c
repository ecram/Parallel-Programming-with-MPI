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

int
main(int argc, char **argv)
{
	unsigned long count;
	double f = 0.0, g = 1.0;

	count = argc > 1 ? strtoul(argv[1], NULL, 0) : 1;


	while (count--) {
		f = f + 2.0*g;
	}
	printf("f=%g\n", f);

	return 0;
}

