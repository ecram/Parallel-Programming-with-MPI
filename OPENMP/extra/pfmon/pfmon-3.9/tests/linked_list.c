/*
 * linked_list.c: a stupid linked list access program
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
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <sys/mman.h>

typedef struct _node {
	struct _node *next, *prev;
	unsigned long value1;
	unsigned long value2;
} node_t;

static void fatal_error(char *fmt,...);

static void
fatal_error(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	exit(1);
}

void
create_list(unsigned long count, node_t **list)
{
	node_t *p, *l;

	l = NULL;

	while(count) {
		p = malloc(sizeof(*p));
		if (p == NULL) fatal_error("cannot allocate node\n");
		memset(p, 0, sizeof(*p));
		p->value1 = count;
		p->value2 = count;
		p->next  = l;
		p->prev  = NULL;
		l = p;
		count--;
	}
	*list = l;
}

void
free_list(node_t *list)
{
	node_t *tmp;
	while(list) {
		tmp = list->next;
		free(list);
		list = tmp;
	}
}

unsigned long
walk_list(const node_t *list, unsigned long count)
{
	const node_t *p;
	unsigned long sum = 0;

	while(count) {
		p = list;
		while (p) {
			sum += p->value1 + p->value2;
			p = p->next;
		}
		count--;
	}
	return sum;
}

int 
main(int argc, char **argv)
{
	unsigned long count, count2;
	unsigned long npg;
	size_t pagesize;
	node_t *list;

	count  = argc > 1 ? strtoul(argv[1], NULL, 10) : 1000;
	count2 = argc > 2 ? strtoul(argv[2], NULL, 10) : 1;

	pagesize = getpagesize();
	npg      = sysconf(_SC_AVPHYS_PAGES);

	if ((count*sizeof(node_t)) >= (npg*pagesize)) {
		fatal_error("too many nodes, you will swap like crazy!\n");
	}
	printf("sizeof(node_t)=%zu used memory=%luMB\n", sizeof(node_t), (count*sizeof(node_t))>>20);
	printf("count=%lu count2=%lu\n", count, count2);

	create_list(count, &list);
	walk_list(list, count2);
	free_list(list);
	return 0;
}
