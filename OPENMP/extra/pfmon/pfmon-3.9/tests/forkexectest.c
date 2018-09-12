/*
 * forkexectest.c: a stupid test combining fork/exec
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/wait.h>

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
	uint64_t loop;
	int nfork;
	char *cmd;
	char fake_cmd[128];

	loop  = argc > 1 ? strtoull(argv[1], NULL, 0) : 1000;
	nfork = argc > 2 ? atoi(argv[2]) : 0;

	if (*argv[0] == '-') {
		noploop(loop);
		exit(0);
	}

	/*
	 * add a '-' in front of child process command name
	 */
	cmd = argv[0];
	strcpy(fake_cmd+1, argv[0]);
	fake_cmd[0] = '-';
	argv[0] = fake_cmd;

	printf("creating %d additional process(es)\n", nfork);
	printf("%"PRIu64" iterations\n", loop);

	while (nfork--) {
		switch(fork()) {
			case -1: perror("fork"); goto cleanup;
				 /* execute twice to make the fork-exec section
				  * appear bigger then the exec section when
				  * monitoring
				  */
			case  0: noploop(loop);noploop(loop);
				 execvp(cmd, argv); exit(1);
		}
	}
	noploop(loop);
cleanup:
	while(wait4(-1, NULL, 0 , NULL) > 0);
	exit(0);
}
