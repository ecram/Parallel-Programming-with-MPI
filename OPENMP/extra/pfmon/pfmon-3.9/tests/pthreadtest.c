/*
 * pthreadtest.c: a stupid nop loop with pthreads
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
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/wait.h>
#include <err.h>

void
noploop(void *data)
{
	uint64_t loop = *(uint64_t *)data;
	unsigned int ret = 0;
	int retval = 0;

	while ( loop-- ) ret +=getpid();

	pthread_exit((void *)&retval);
}


int
main(int argc, char **argv)
{
	pthread_t	*thread_list;
	uint64_t 	loop;
	int 		nt, nthreads = 0, ret;


	loop = argc > 1 ? strtoull(argv[1], NULL, 0) : 1000;
	nt   = argc > 2 ? atoi(argv[2]) : 0;

	printf("creating %d thread(s)\n", nt);
	printf("%"PRIu64" iterations\n", loop);

	thread_list = malloc(nt*sizeof(pthread_t));
	if (thread_list == NULL) {
		fprintf(stderr, "cannot malloc thread table for %d threads\n", nthreads);
		exit(1);
	}

	while (nt--) {
		ret = pthread_create(&thread_list[nt], NULL, (void *(*)(void *))noploop, &loop);
		if (ret)
			err(1, "cannot create thread %d\n", nt);
		nthreads++;
	}
	noploop(&loop);

	while(nthreads--) {
		pthread_join(thread_list[nthreads], NULL);
	}
	exit(0);
}
