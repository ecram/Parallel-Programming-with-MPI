/*
 * common sampling module code for sampling
 *
 * Copyright (c) 2009 Goole, Inc
 * Contributed by Stephane Eranian <eranian@gmail.com>
 *
 * This file is part of pfmon, a sample tool to measure performance 
 * of applications on Linux.
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
#include "pfmon.h"
#include "pfmon_smpl_util.h"

/*
 * qsort expects:
 *      < 0 if e1 < e2
 *      > 0 if e1 > e2
 *      = 0 if e1 = e2
 *
 * We invert this because we want entries sorted in reverse
 * order: larger count first
 *
 * do not use simpler substraction because of type casting
 */
static int
hash_data_sort_bycount(const void *a, const void *b)
{
        hash_data_t **e1 = (hash_data_t **)a;
        hash_data_t **e2 = (hash_data_t **)b;
                        
        if ((*e1)->count[0] < (*e2)->count[0])
                return 1;
        
        return (*e1)->count[0] > (*e2)->count[0] ? -1 : 0;
}

static void
smpl_symbolize(pfmon_sdesc_t *sdesc, hash_data_t **tab, unsigned long num_entries, unsigned int event_count)
{
	unsigned long i;
	hash_data_t *p;
	/*
 	 * assign symbol name, module name, start, end to each sample
 	 */
	for(i=0; i < num_entries; i++) {
		p = tab[i];
		/* ignore errors and symbol name */
		find_sym_by_av(p->key.val, p->key.version, sdesc->syms,
				NULL, &p->mod, &p->start, &p->end, &p->cookie);
	}
}

static inline int
is_unknown(hash_data_t *p)
{
	return p->cookie == PFMON_COOKIE_UNKNOWN_SYM;
}

static inline int
is_same_instr_or_func(hash_data_t *p, hash_data_t *q)
{
	if (options.opt_smpl_per_func) {
		       /* same cookie, same module */
		return q->cookie == p->cookie && !strcmp(p->mod, q->mod);
	}
		/* same address, same cookie, same module */
	return p->key.val == q->key.val && q->cookie == p->cookie && !strcmp(p->mod, q->mod);
}

void
smpl_reduce(pfmon_sdesc_t *sdesc, hash_data_t **tab, unsigned long num_entries, unsigned int event_count)
{
	hash_data_t *p;
	unsigned long i, j;
	int k;


	/*
	 * cannot proceed if symbol tables are not loaded
	 */
	if (!options.opt_addr2sym)
		goto skip;

	smpl_symbolize(sdesc, tab, num_entries, event_count);

	for(i=0; i < num_entries; i++) {
		p = tab[i];

		if (!p->mod)
			continue;

		/*
 		 * In per-function mode, we cannot fuse samples
 		 * which are not associated with a symbol. This 
 		 * is the case for stripped images in which not
 		 * all symbol informatin may be available.
 		 * Fusing would aggregate samples to the nearest
 		 * symbol which may not be related.
 		 */
		if (options.opt_smpl_per_func) {
			/* round to symbol start */
			p->key.val = p->start;
		}

		/*
 		 * look for all matching samples and
 		 * fuse
 		 */
		for(j=i+1; j < num_entries; j++) {

			/*
 			 * symbol already processed
 			 */
			if (!tab[j]->mod)
				continue;
		
			/*
 			 * instruction mode, fuse iff:
 			 *  A,v1,cookie1,mod1
 			 *  A,v2,cookie1,mod1
 			 *  Same address, same module, same cookie
 			 *
 			 * function mode, fuse iff:
 			 * A,v2,cookie1,mod1
 			 * B,v2,cookie,mod1
 			 * Same module, same cookie (unknowns skipped)
 			 */
			if (is_same_instr_or_func(p, tab[j])) {
				for(k=0; k < event_count; k++) {
					p->count[k] += tab[j]->count[k];
					tab[j]->count[k] = 0;
				}
				tab[j]->mod = NULL;
			}
		}
	}
skip:
	qsort(tab, num_entries, sizeof(hash_data_t *), hash_data_sort_bycount);
}


