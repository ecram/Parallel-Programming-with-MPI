/*
 * pong.c - dual process ping-pong example
 *
 * Copyright (c) 2009 Google, Inc
 * Contributed by Stephane Eranian <eranian@gmail.com>
 *
 * Based on:
 * Copyright (c) 2008 Stephane Eranian
 * Contributed by Stephane Eranian <eranian@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file is part of libpfm, a performance monitoring support library for
 * applications on Linux.
 */
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <syscall.h>
#include <err.h>

/*
 * pin task to CPU
 */
#ifndef __NR_sched_setaffinity
#error "you need to define __NR_sched_setaffinity"
#endif

#define MAX_CPUS	2048
#define NR_CPU_BITS	(MAX_CPUS>>3)
int
pin_cpu(pid_t pid, unsigned int cpu)
{
	uint64_t my_mask[NR_CPU_BITS];

	if (cpu >= MAX_CPUS)
		errx(1, "this program supports only up to %d CPUs", MAX_CPUS);

	my_mask[cpu>>6] = 1ULL << (cpu&63);

	return syscall(__NR_sched_setaffinity, pid, sizeof(my_mask), &my_mask);
}


static volatile int quit;
void sig_handler(int n)
{
	quit = 1;
}

static void
do_child(int fr, int fw)
{
	char c;
	ssize_t ret;

	for(;;) {
		ret = read(fr, &c, 1);	
		if (ret < 0)
			break;
		ret = write(fw, "c", 1);
		if (ret < 0)
			break;
		
	}
	printf("child exited\n");
	exit(0);
}


int
main(int argc, char **argv)
{
	int ret;
	int pr[2], pw[2];
	int which_cpu;
	pid_t pid;
	ssize_t nbytes;
	char c = '0';
	int delay;

	delay = argc > 1 ? atoi(argv[1]) : 10;
	
	srandom(getpid());
	which_cpu = random() % sysconf(_SC_NPROCESSORS_ONLN);

	ret = pipe(pr);
	if (ret)
		err(1, "cannot create read pipe");

	ret = pipe(pw);
	if (ret)
		err(1, "cannot create write pipe");

	/*
 	 * Pin to CPU0, inherited by child process. That will enforce
 	 * the ping-pionging and thus stress the PMU context switch 
 	 * which is what we want
 	 */
	ret = pin_cpu(getpid(), which_cpu);
	if (ret)
		err(1, "cannot pin to CPU%d", which_cpu);

	printf("Both processes pinned to CPU%d, running for %ds\n", which_cpu, delay);

	/*
 	 * create second process which is not monitoring at the moment
 	 */
	switch(pid=fork()) {
		case -1:
			err(1, "cannot create child");
		case 0:
			/* pr[]: write master, read child */
			/* pw[]: read master, write child */
			close(pr[1]); close(pw[0]);
			do_child(pr[0], pw[1]);
			exit(1);
	}

	close(pr[0]);
	close(pw[1]);

	signal(SIGALRM, sig_handler);
	alarm(delay);

	/*
	 * ping pong loop
	 */
	while(!quit) {
		nbytes = write(pr[1], "c", 1);
		nbytes = read(pw[0], &c, 1);	
	}

	/*
 	 * close pipes
 	 */
	close(pr[1]);
	close(pw[0]);
	return 0;
}
