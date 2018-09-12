/*
 * inst_hist_mult.c - instruction-based histogram multi-event sampling for all PMUs
 *
 * Copyright (c) 2006-2007 Hewlett-Packard Development Company, L.P.
 * Contributed by Stephane Eranian <eranian@hpl.hp.com>
 * Parts contributed by Andrzej Nowak (CERN)
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
#include <perfmon/perfmon_dfl_smpl.h>

#define	SMPL_MOD_NAME "inst-hist"

typedef struct {
	uint64_t	total_count[PFMON_MAX_PMDS];
	uint64_t	max_count;
	hash_data_t 	**tab;
	unsigned long 	pos;
	unsigned int	event_count;
} hash_sort_arg_t;

pfmon_smpl_module_t inst_hist_smpl_module;

static int (*process)(pfmon_sdesc_t *sdesc, int pd_idx, size_t num, uint64_t entry, void *data);

static int
inst_hist_process_sample_raw(pfmon_sdesc_t *sdesc, int pd_idx, size_t num, uint64_t entry, void *data)
{
	pfm_dfl_smpl_entry_t *ent = data;
	FILE *fp = sdesc->csmpl.smpl_fp;
	pfmon_event_set_t *set;
	uint64_t *regs;
	int ret, n;

	ret = fwrite(data, sizeof(pfm_dfl_smpl_entry_t), 1, fp);
	if (ret != 1)
		return ret != 1 ? -1 : 0;

	/*
 	 * save counts in the order of the events
 	 */
	for (n = 0; ret == 1 && n < num; n++) {
		ret = fwrite(&regs[set->setup->rev_smpl_pmds[ent->ovfl_pmd].map_pmd_evt[n].off],
			     sizeof(uint64_t),
			     1,
			     fp);
	}
	return ret != 1 ? -1 : 0;
}

static int
inst_hist_process_sample_compact(pfmon_sdesc_t *sdesc, int pd_idx, size_t num, uint64_t entry, void *data)
{
	pfm_dfl_smpl_entry_t *ent = data;
	pfmon_event_set_t *set;
	FILE *fp = sdesc->csmpl.smpl_fp;
	uint64_t *regs;
	int ret, n;

	set = sdesc->sets;
	regs = (uint64_t *)(ent+1);

	ret = fprintf(fp,
			"%"PRIu64
			" %d"
			" %d"
			" %d"
			" 0x%"PRIx64
			" %d"
			" %"PRIu64
			" %u"
			" 0x%"PRIx64,
			entry,
			ent->tgid,
			ent->pid,
			ent->cpu,
			ent->tstamp,
			ent->ovfl_pmd, 
			-ent->last_reset_val,
			ent->set,
			ent->ip);

	/*
 	 * print counts in the order of the events
 	 */
	for (n = 0; ret > 0 && n < num; n++) {
		ret = fprintf(fp, " 0x%"PRIx64,
				 regs[set->setup->rev_smpl_pmds[ent->ovfl_pmd].map_pmd_evt[n].off]);
	}

	if (options.opt_addr2sym) {
		fputc(' ', fp);
		pfmon_print_address(fp,
				    sdesc->syms,
				    ent->ip,
				    ent->tgid,
				    syms_get_version(sdesc));
	}
	fputc('\n', fp);

	return ret > 0 ? 0 : -1;
}

static int
inst_hist_process_sample_prof(pfmon_sdesc_t *sdesc, int pd_idx, size_t num, uint64_t entry, void *data)
{
	pfm_dfl_smpl_entry_t *ent = data;
	hash_data_t *hash_entry;
	pfmon_hash_key_t key;
	void *hash_desc;
	int ret;

	hash_desc  = sdesc->csmpl.data;

	key.val = ent->ip;
	key.pid = options.opt_aggr && !options.opt_syst_wide ? 0 : ent->tgid; /* process id */
	key.tid = options.opt_aggr && !options.opt_syst_wide ? 0 : ent->pid; /* thread id */
	key.version = syms_get_version(sdesc);

	ret = pfmon_hash_find(hash_desc, key, &data);
	if (ret == -1) {
		pfmon_hash_add(hash_desc, key, &data);
		hash_entry = data;
		hash_entry->count[pd_idx] = 0;
		hash_entry->key = key;
	} else
		hash_entry = data;

	hash_entry->count[pd_idx]++;

	return 0;
}

static int
inst_hist_process_samples(pfmon_sdesc_t *sdesc)
{
	pfm_dfl_smpl_hdr_t *hdr;
	pfm_dfl_smpl_entry_t *ent;
	pfmon_smpl_desc_t *csmpl;
	uint64_t entry, count, skip;
	void *pos;
	int ret = 0, pd_idx;
	size_t num;
	uint16_t last_ovfl_pmd;

	csmpl = &sdesc->csmpl;
	hdr   = csmpl->smpl_hdr;

	if (hdr == NULL)
		return -1;
	
	ent        = (pfm_dfl_smpl_entry_t *)(hdr+1);
	pos	   = ent;
	entry      = options.opt_aggr ? *csmpl->aggr_count : csmpl->entry_count;
	count      = hdr->hdr_count;
	pd_idx     = 0;
	num	   = 0;
	last_ovfl_pmd = ~0;
	
	DPRINT(("count=%"PRIu64" entry=%"PRIu64"\n", count, entry));

	/*
 	 * check if we have new entries
 	 * if so skip the old entries and process only the new ones
 	 */
	if((csmpl->last_ovfl == hdr->hdr_overflows && csmpl->last_count <= count)
	  || ((csmpl->last_ovfl+1) == hdr->hdr_overflows && csmpl->last_count < count)) {
		skip = csmpl->last_count;
		vbprintf("[%d] skip %"PRIu64" samples out of %"PRIu64" (overflows: %"PRIu64")\n",
			  sdesc->tid,
			  skip,
			  count,
			  hdr->hdr_overflows);
	} else {
		skip = 0;
	}
	/*
 	 * only account for new entries, i.e., skip leftover entries
 	 */
	if (options.opt_aggr) {
		*csmpl->aggr_count += count - skip;
		if (csmpl->last_ovfl != ~0)
			*csmpl->aggr_ovfl += hdr->hdr_overflows - csmpl->last_ovfl;
	} else {
		csmpl->entry_count += count - skip;
	}
	csmpl->last_count = count;
	csmpl->last_ovfl = hdr->hdr_overflows;

	while(count--) {
		DPRINT(("entry %"PRIu64" PID:%d CPU:%d STAMP:0x%"PRIx64" OVF:%u IIP: %llx\n",
			entry,
			ent->pid,
			ent->cpu,
			ent->tstamp,
			ent->ovfl_pmd,
			(unsigned long long)ent->ip));

		if (ent->ovfl_pmd != last_ovfl_pmd) {
			pd_idx = sdesc->sets->setup->rev_smpl_pmds[ent->ovfl_pmd].pd_idx;
			last_ovfl_pmd = ent->ovfl_pmd;
			num = sdesc->sets->setup->rev_smpl_pmds[ent->ovfl_pmd].num_smpl_pmds;
		}

		/*
		 * in aggregation mode sample processing is serialized,
		 * therefore we are safe to use a single hash_table here
		 */
		if(skip) {
			skip--;
		} else {
			ret = (*process)(sdesc, pd_idx, num, entry, ent);
			if (ret)
				break;
			entry++;
		}
		/* skip over body */
		pos += sizeof(*ent) + num * sizeof(uint64_t);
		ent = pos;
	}
	return ret;
}

static int
inst_hist_print_header(pfmon_sdesc_t *sdesc)
{
	FILE *fp = sdesc->csmpl.smpl_fp;
	int i;

	fprintf(fp, "# description of columns:\n");

	if (options.opt_smpl_mode == PFMON_SMPL_DEFAULT) {
		fprintf(fp, "#\tcolumn  0: number of samples for event 0 (sorting key)\n"
			    "#\tcolumn  1: relative percentage of samples for event 0\n"
			    "#\tcolumn  2: cumulative percentage for event 0\n");

		for(i=1; i < sdesc->sets->setup->event_count; i++) {
			fprintf(fp, "#\tcolumn  %d: number of samples for event %d\n"
				    "#\tcolumn  %d: relative percentage of samples for event %d\n"
				    "#\tcolumn  %d: cumulative percentage for event %d\n",
				    3*i, i,
				    1+3*i, i,
				    2+3*i, i);

		}
		fputc('\n', fp);
		fprintf(fp, "#\tother columns are self-explanatory\n");
	} else if (options.opt_smpl_mode == PFMON_SMPL_COMPACT) {
		char *name;
		size_t l;
		int i, j;

		fprintf(fp, "#\tcolumn  0: sample number\n"
			    "#\tcolumn  1: process identification\n"
			    "#\tcolumn  2: thread identification\n"
			    "#\tcolumn  3: CPU\n"
			    "#\tcolumn  4: timestamp\n"
			    "#\tcolumn  5: overflowed PMD index\n"
			    "#\tcolumn  6: sampling period\n"
			    "#\tcolumn  7: event set\n"
			    "#\tcolumn  8: interrupted instruction address\n");

		pfm_get_max_event_name_len(&l);
		l++; /* for \0 */

		name = malloc(l);
		if (!name) {
			warning("cannot allocate string for header\n");
			return -1;
		}

		for(i=0, j=0; i < sdesc->sets->setup->event_count; i++) {
			if (sdesc->sets->setup->long_rates[i].flags & PFMON_RATE_VAL_SET)
				continue;

			pfm_get_full_event_name(&sdesc->sets->setup->inp.pfp_events[i], name, l);
			fprintf(fp, "#\tcolumn %2d: PMD%d = %s\n",
					8+j,
					sdesc->sets->setup->outp.pfp_pmds[i].reg_num,
					name);
			j++;
		}
	}
	return 0;
}

static int
inst_hist_initialize_session(pfmon_sdesc_t *sdesc)
{
	pfmon_smpl_desc_t *csmpl = &sdesc->csmpl;
	void *hash_desc;
	pfmon_hash_param_t param;

	param.hash_log_size = 12;
	param.max_entries   = ~0; /* unlimited */
	param.entry_size    = sizeof(hash_data_t);
#ifdef __ia64__
	param.shifter	    = 4;
#else
	param.shifter	    = 0;
#endif
	param.flags	    = PFMON_HASH_ACCESS_REORDER;

	pfmon_hash_alloc(&param, &hash_desc);

	csmpl->data = hash_desc;
	return 0;
}

static void
inst_hist_print_data(void *arg, void *data)
{
	hash_data_t *p = data;
	hash_sort_arg_t *sort_arg = arg;
	hash_data_t **tab = sort_arg->tab;
	unsigned long pos = sort_arg->pos;
	unsigned int i, cnt;

	cnt = sort_arg->event_count;

	tab[pos] = p;

	sort_arg->pos = ++pos;

	for(i=0; i < cnt; i++) {
		sort_arg->total_count[i] += p->count[i];
		if (p->count[i] > sort_arg->max_count)
			sort_arg->max_count = p->count[i];
	}
}

static int
inst_hist_show_results(pfmon_sdesc_t *sdesc)
{
	uint64_t top_num, cum_count[PFMON_MAX_PMDS];
	void *hash_desc;
	pfmon_smpl_desc_t *csmpl;
	FILE *fp;
	hash_data_t **tab;
	unsigned long ns = 0;
	uint64_t addr;
	unsigned long i, num_entries, j;
	unsigned long event_count;
	double d_cum, cum_total;
	hash_sort_arg_t arg;
	size_t len_count;
	int need_resolve;
	char buf[32];

	if (options.opt_smpl_mode != PFMON_SMPL_DEFAULT)
		return 0;

	csmpl = &sdesc->csmpl;
	fp = csmpl->smpl_fp;

	hash_desc = csmpl->data;

	if (fp == NULL || hash_desc == NULL)
		return -1;

	pfmon_hash_num_entries(hash_desc, &num_entries);

	tab = (hash_data_t **)malloc(sizeof(hash_data_t *)*num_entries);
	if (tab == NULL) {
		warning("cannot allocate memory to print %lu samples\n", num_entries);
		return -1;
	}
	event_count = sdesc->sets->setup->event_count;
	memset(&cum_count, 0, sizeof(cum_count));
	memset(&arg, 0, sizeof(arg));
	arg.tab = tab;
	arg.pos = 0;
	arg.max_count   = 0;
	arg.event_count = event_count;

	pfmon_hash_iterate(hash_desc, inst_hist_print_data, &arg);

	sprintf(buf, "%"PRIu64, arg.max_count);
	len_count = strlen(buf);

	/* adjust for column heading smpl_evXX */
	if (len_count < 10)
		len_count = 10;

	smpl_reduce(sdesc, tab, num_entries, event_count);

	need_resolve = options.opt_addr2sym;

	top_num = options.smpl_show_top;
	if (!top_num)
		top_num = num_entries;

	if (options.opt_syst_wide)
		if (options.opt_aggr)
			fprintf(fp, "# aggregated results\n");
		else
			fprintf(fp, "# results for CPU%u\n", sdesc->cpu);
	else
		fprintf(fp, "# results for [%d:%d<-[%d]] (%s)\n",
			sdesc->pid,
			sdesc->tid,
			sdesc->ppid,
			sdesc->cmd ? sdesc->cmd : "aggregated sessions");

	fprintf(fp, "# total samples          : %"PRIu64"\n", csmpl->entry_count);
	fprintf(fp, "# total buffer overflows : %"PRIu64"\n#\n#", csmpl->last_ovfl);


	for(j=0; j < event_count; j++)
		fprintf(fp, "%*s%02lu ", (int)len_count+16-2, "event", j);

	fprintf(fp, "\n# ");

	for(j=0; j < event_count; j++)
		fprintf(fp, "%*s   %%self    %%cum ", (int)len_count, "counts");

	fprintf(fp, "%*s ",
		(int)(2+(sizeof(uint64_t)<<1)),
		"code addr");

	if (need_resolve)
		fprintf(fp, "symbol");

	fputc('\n', fp);

	for(i=0; i < num_entries; i++) {
		uint64_t sum;

		addr = tab[i]->key.val;

		sum = 0;
		for(j=0; j < event_count; j++) {
			sum += tab[i]->count[j];
			cum_count[j] += tab[i]->count[j];
		}
		/*
		 * skip sample if fused with another one
		 */
		if (!sum)
			continue;
		
		fputc(' ', fp); fputc(' ', fp);

		for(j=0; j < event_count; j++) {

			if (arg.total_count[j]) {
				cum_total  = (double)cum_count[j]*100.0 / (double)arg.total_count[j];
				d_cum = (double)tab[i]->count[j]*100.0/(double)arg.total_count[j];
			} else {
				cum_total = d_cum = 0; 
			}

			if (cum_total > (double)options.smpl_cum_thres)
				goto out;

			fprintf(fp, "%*"PRIu64" %6.2f%% %6.2f%% ",
				(int)len_count,
				tab[i]->count[j],
				d_cum,
				cum_total);
		}

		/* unknown symbols are grouped only in --smpl-per-func mode */
		if (options.opt_smpl_per_func && is_unknown_cookie(tab[i]->cookie))
			addr = 0;

		fprintf(fp, "0x%0*"PRIx64" ", (int)(sizeof(uint64_t)<<1), addr);

		if (need_resolve)
			pfmon_print_address(fp,
					    sdesc->syms,
					    tab[i]->key.val,
					    tab[i]->key.pid,
					    tab[i]->key.version);

		fputc('\n', fp);
		/*
 		 * exit after n samples have been printed
 		 */
		if (++ns == top_num)
			break;
	}
out:
	free(tab);
	return 0;
}

static int
inst_hist_terminate_session(pfmon_sdesc_t *sdesc)
{
	inst_hist_show_results(sdesc);

	pfmon_hash_free(sdesc->csmpl.data);
	sdesc->csmpl.data = NULL;
	return 0;
}

pfmon_smpl_module_t inst_hist_smpl_module;
static void inst_hist_initialize_mask(void)
{
	pfmon_bitmask_setall(&inst_hist_smpl_module.pmu_mask);
}

static int
inst_hist_validate_events(pfmon_event_set_t *set)
{
	switch(options.opt_smpl_mode) {
	case PFMON_SMPL_RAW:
		process = inst_hist_process_sample_raw;
		inst_hist_smpl_module.flags |= PFMON_SMPL_MOD_FL_OUTFILE;
		break;
	case PFMON_SMPL_COMPACT:
		process = inst_hist_process_sample_compact;
		inst_hist_smpl_module.flags |= PFMON_SMPL_MOD_FL_OUTFILE;
		break;
	case PFMON_SMPL_DEFAULT:
		process = inst_hist_process_sample_prof;
		break;
	}
	return 0;
}

pfmon_smpl_module_t inst_hist_smpl_module ={
	.name		    = SMPL_MOD_NAME,
	.description	    = "IP-based histogram",
	.process_samples    = inst_hist_process_samples,
	.initialize_mask    = inst_hist_initialize_mask,
	.initialize_session = inst_hist_initialize_session,
	.validate_events    = inst_hist_validate_events,
	.terminate_session  = inst_hist_terminate_session,
	.print_header       = inst_hist_print_header,
	.flags		    = 0,
	.fmt_name	    = PFM_DFL_SMPL_NAME
};
