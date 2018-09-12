/*
 * exectest.c: a stupid test with exec
 *
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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>

int
noploop(uint64_t loop)
{
	unsigned int ret = 0;

	while ( loop-- ) ret +=getpid();

	return ret;
}

int 
main(int argc, char **argv)
{
	char **p, **q;
	uint64_t loop;
	
	/* we are done */
	if (argc < 2)
		return 0;

	loop  = strtoull(argv[1], NULL, 0);

	printf("[%d] %s %s\n", getpid(), argv[0], argv[1]);
	printf("%"PRIu64" iterations\n", loop);

	noploop(loop);

	/*
	 * preserve argv[0], shift the rest down
	 */
	q = argv+1;
	p = argv+2;

	while (*p) {
		*q++ = *p++;
	}
	*q = NULL;

	/* we are done */
	if (argc == 2) return 0;

	return execvp(argv[0], argv);
}
