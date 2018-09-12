/*
 * pebs_smpl.c - Intel Core PEBS support IP-based histogram
 *
 * Supported processors: Intel Core, Intel Atom 
 *
 * Copyright (c) 2006-2007 Hewlett-Packard Development Company, L.P.
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
#include "pfmon.h"
#include "pfmon_smpl_util.h"
#include <perfmon/perfmon_pebs_core_smpl.h>

typedef struct {
	hash_data_t 	**tab;
	unsigned long 	pos;
	uint64_t	total_count;
	uint64_t	max_count;
} hash_sort_arg_t;

typedef struct {
	size_t entry_sz;
	size_t hdr_sz;
	size_t arg_sz;
	uint64_t (*get_count)(void *p);
	uint64_t (*get_ovfls)(void *p);
	int (*process_raw)(pfmon_sdesc_t *sdesc, uint64_t entry, void *p);
	int (*process_compact)(pfmon_sdesc_t *sdesc, uint64_t entry, void *p);
	int (*process_prof)(pfmon_sdesc_t *sdesc, uint64_t entry, void *p);
	void (*ctx_arg)(void *p, size_t buf_size);
} pebs_func_t;

pfmon_smpl_module_t pebs_smpl_module;

static pebs_func_t *pebs_func;
static int (*process)(pfmon_sdesc_t *sdesc, uint64_t entry, void *p);

static int
pebs_process_raw(pfmon_sdesc_t *sdesc, uint64_t entry, void *data)
{
	int ret;
	ret = fwrite(data, pebs_func->entry_sz, 1, sdesc->csmpl.smpl_fp);
	return ret != 1 ? -1 : 0;
}

static int
pebs_process_compact(pfmon_sdesc_t *sdesc, uint64_t entry, void *data)
{
	pfm_pebs_core_smpl_entry_t *ent = data;
	FILE *fp = sdesc->csmpl.smpl_fp;
	int ret;

	ret = fprintf(fp,"%"PRIu64
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			" 0x%"PRIx64""
			,entry,
		ent->ip,
		ent->eflags,
		ent->eax,
		ent->ebx,
		ent->ecx,
		ent->edx,
		ent->esi,
		ent->edi,
		ent->edi,
		ent->ebp,
		ent->esp,
		ent->r8,
		ent->r9,
		ent->r10,
		ent->r11,
		ent->r12,
		ent->r13,
		ent->r14,
		ent->r15);

	/*
	 * PEBS does not record pid so in system wide
	 * we cannot correlate a sample with a PID
	 */
	if (options.opt_addr2sym) {
		fputc(' ', fp);
		pfmon_print_address(fp,
				    sdesc->syms,
				    ent->ip,
				    sdesc->pid,
				    syms_get_version(sdesc));
	}
	fputc('\n', fp);
	return ret > 0 ? 0 : -1;
}


static uint64_t
pebs_core_get_count(void *h)
{
	pfm_pebs_core_smpl_hdr_t *hdr = h;
	return (hdr->ds.pebs_index - hdr->ds.pebs_buf_base)/pebs_func->entry_sz;
}

static uint64_t
pebs_core_get_ovfls(void *h)
{
	pfm_pebs_core_smpl_hdr_t *hdr = h;
	return hdr->overflows;
}

static void
pebs_core_ctx_arg(void *p, size_t buf_size)
{
	pfm_pebs_core_smpl_arg_t *arg_core = p;

	arg_core->buf_size = buf_size;
	arg_core->intr_thres = options.smpl_entries; /* whole buffer */
	arg_core->cnt_reset = options.sets->setup->long_rates[0].value;
}

static int
pebs_process_prof(pfmon_sdesc_t *sdesc, uint64_t entry, void *data)
{
	pfmon_hash_key_t key;
	hash_data_t *hash_entry;
	uint64_t *regs = data;
	void *addr, *hash_desc;
	int ret;

	hash_desc  = sdesc->csmpl.data;

	key.val = regs[1];
	key.version = syms_get_version(sdesc);

	/*
	 * PEBS does not record pid, therefore we need to ignore it
	 * in system-wide mode
	 */
	if (options.opt_syst_wide) {
		key.pid = 0;
		key.tid = 0;
	} else {
		key.pid = options.opt_aggr ? 0 : sdesc->pid;
		key.tid = options.opt_aggr ? 0 : sdesc->tid;
	}

	ret = pfmon_hash_find(hash_desc, key, &addr);
	if (ret == -1) {
		pfmon_hash_add(hash_desc, key, &addr);
		hash_entry = addr;
		hash_entry->count[0] = 0;
		hash_entry->key = key;
	} else
		hash_entry = addr;

	hash_entry->count[0]++;

	return 0;
}

static int
pebs_process_samples(pfmon_sdesc_t *sdesc)
{
	pfmon_smpl_desc_t *csmpl;
	FILE *fp;
	uint64_t count, entry, skip, ovfls;
	void *pos;
	int ret = 0;

	csmpl      = &sdesc->csmpl;
	fp         = csmpl->smpl_fp;
	entry      = csmpl->entry_count;

	if (!csmpl->smpl_hdr)
		return -1;

	count = pebs_func->get_count(csmpl->smpl_hdr);
	ovfls = pebs_func->get_ovfls(csmpl->smpl_hdr);
	pos = csmpl->smpl_hdr + pebs_func->hdr_sz;


	DPRINT(("count=%"PRIu64"\n", count));

	/*
 	 * check if we have new entries
 	 * if so skip the old entries and process only the new ones
 	 */
	if((csmpl->last_ovfl == ovfls && csmpl->last_count <= count)
	  || ((csmpl->last_ovfl+1) == ovfls && csmpl->last_count < count)) {
		skip = csmpl->last_count;
		vbprintf("skip %"PRIu64" samples out of %"PRIu64" (overflows: %"PRIu64")\n",
			  skip,
			  count,
			  ovfls);
	} else {
		skip = 0;
	}
	/*
 	 * only account for new entries, i.e., skip leftover entries
 	 */
	if (options.opt_aggr) {
		*csmpl->aggr_count += count - skip;
		if (csmpl->last_ovfl != ~0)
			*csmpl->aggr_ovfl += ovfls - csmpl->last_ovfl;
	} else {
		csmpl->entry_count += count - skip;
	}

	csmpl->last_count = count;
	csmpl->last_ovfl = ovfls;

	while(count--) {
		if(skip){
			skip--;
			pos += pebs_func->entry_sz;
			continue;
		}
		ret = (*process)(sdesc, entry, (void *)pos);
		if (ret)
			break;
		entry++;
		pos += pebs_func->entry_sz;
	}
	return ret;
}

pfmon_smpl_module_t pebs_smpl_module;
static void pebs_initialize_mask(void)
{
	pfmon_bitmask_set(&pebs_smpl_module.pmu_mask, PFMLIB_CORE_PMU);
	pfmon_bitmask_set(&pebs_smpl_module.pmu_mask, PFMLIB_INTEL_ATOM_PMU);
}

/*
 * never called when smpl_mode = PFMON_SMPL_RAW
 */
static int
pebs_hist_print_header(pfmon_sdesc_t *sdesc)
{
	FILE *fp = sdesc->csmpl.smpl_fp;

        if (options.opt_smpl_mode == PFMON_SMPL_DEFAULT)
                fprintf(fp, "# description of columns:\n"
                    "#\tcolumn  1: number of samples for this address\n"
                    "#\tcolumn  2: relative percentage for this address\n"
                    "#\tcolumn  3: cumulative percentage up to this address\n"
                    "#\tcolumn  4: symbol name or address\n");
        else if (options.opt_smpl_mode == PFMON_SMPL_COMPACT)
                fprintf(fp, "# description of columns:\n"
                    "#\tcolumn  1: sample number\n"
                    "#\tcolumn  2: instruction address\n"
                    "#\tcolumn  3: EFLAGS\n"
                    "#\tcolumn  4: EAX\n"
                    "#\tcolumn  5: EBX\n"
                    "#\tcolumn  6: ECX\n"
                    "#\tcolumn  7: EDX\n"
                    "#\tcolumn  8: ESI\n"
                    "#\tcolumn  9: EDI\n"
                    "#\tcolumn 10: EBP\n"
                    "#\tcolumn 11: ESP\n"
                    "#\tcolumn 12: R8\n"
                    "#\tcolumn 13: R9\n"
                    "#\tcolumn 14: R10\n"
                    "#\tcolumn 15: R11\n"
                    "#\tcolumn 16: R12\n"
                    "#\tcolumn 17: R13\n"
                    "#\tcolumn 18: R14\n"
                    "#\tcolumn 19: R15\n"
                );
	return 0;
}

static int
pebs_hist_validate_events(pfmon_event_set_t *set)
{
	/*
	 * must be sampling with one event only for PEBS
	 */
	if (set->setup->inp.pfp_event_count > 1) {
		warning("the sampling module works with 1 event at a time only\n");
		return -1;
	}
	/*
 	 * PEBS hardware does write samples directly into a memory region with OS
 	 * intervention. PEBS hardware does not randomize the sampling period.
 	 * Thus, at best, we could randomize on buffer overflows. Currently, the
 	 * PEBS kernel sampling format does ntot support this mode. Thus we cannot
 	 * really support randomization.
 	 */
	if (set->master_pd[0].reg_flags & PFM_REGFL_RANDOM) {
		warning("by construction, randomization cannot be used with PEBS\n");
		return -1; 
	}
	/*
	 * PEBS does not record pid in each sample, thus there is no way to reliably
	 * correlate addresses to samples in system-wide mode.
	 */
	if (options.opt_syst_wide && options.opt_addr2sym) {
		if (set->setup->inp.pfp_events[0].plm != PFM_PLM0) {
			warning("In system-wide mode, the PEBS kernel module does not record"
				" PID information thus, only kernel symbols can be resolved, use -k to monitor kernel only\n");
			return -1;
		}
	}
	switch(options.opt_smpl_mode) {
	case PFMON_SMPL_RAW:
		process = pebs_func->process_raw;
		pebs_smpl_module.flags |= PFMON_SMPL_MOD_FL_OUTFILE;
		break;
	case PFMON_SMPL_COMPACT:
		process = pebs_func->process_compact;
		pebs_smpl_module.flags |= PFMON_SMPL_MOD_FL_OUTFILE;
		break;
	case PFMON_SMPL_DEFAULT:
		process = pebs_func->process_prof;
		break;
	}
	return 0;
}

static int
pebs_hist_initialize_session(pfmon_sdesc_t *sdesc)
{
	pfmon_smpl_desc_t *csmpl = &sdesc->csmpl;
	void *hash_desc;
	pfmon_hash_param_t param;

	param.hash_log_size = 12;
	param.max_entries   = ~0;
	param.entry_size    = sizeof(hash_data_t);
	param.shifter	    = 0;
	param.flags	    = PFMON_HASH_ACCESS_REORDER;

	pfmon_hash_alloc(&param, &hash_desc);

	csmpl->data = hash_desc;
	DPRINT(("initialized session for csmpl=%p data=%p\n", csmpl, csmpl->data));
	return 0;
}

static void
pebs_hist_print_data(void *arg, void *data)
{
	hash_data_t *p = (hash_data_t *)data;
	hash_sort_arg_t *sort_arg = (hash_sort_arg_t *)arg;
	hash_data_t **tab = sort_arg->tab;
	unsigned long pos = sort_arg->pos;
	uint64_t count;

	count = p->count[0];
	tab[pos] = p;

	sort_arg->pos = ++pos;
	sort_arg->total_count += count;

	if (count > sort_arg->max_count) sort_arg->max_count = count;
}

static int
pebs_hist_show_results(pfmon_sdesc_t *sdesc)
{
	pfmon_smpl_desc_t *csmpl;
	uint64_t total_count, cum_count, count, top_num;
	FILE *fp;
	void *hash_desc;
	double d_cum, cum_total;
	hash_data_t **tab;
	uint64_t addr;
	unsigned long ns = 0;
	unsigned long i, num_entries;
	hash_sort_arg_t arg;
	size_t len;
	int need_resolve;
	char buf[32];

	if (options.opt_smpl_mode != PFMON_SMPL_DEFAULT)
		return 0;

	csmpl = &sdesc->csmpl;
	hash_desc = csmpl->data;
	fp = csmpl->smpl_fp;

	if (hash_desc == NULL)
		return -1;

	pfmon_hash_num_entries(hash_desc, &num_entries);

	tab = (hash_data_t **)malloc(sizeof(hash_data_t *)*num_entries);
	if (tab == NULL) {
		warning("cannot allocate memory to print %lu samples\n", num_entries);
		return -1;
	}
	memset(&cum_count, 0, sizeof(cum_count));
	memset(&arg, 0, sizeof(arg));
	arg.tab = tab;
	arg.pos = 0;
	arg.total_count = 0;
	arg.max_count   = 0;

	pfmon_hash_iterate(csmpl->data, pebs_hist_print_data, &arg);

	total_count = arg.total_count;
	cum_count   = 0;

	sprintf(buf, "%"PRIu64, arg.max_count);
	len = strlen(buf);
	/* adjust for column heading */
	if (len < 6)
		len = 6;

	smpl_reduce(sdesc, tab, num_entries, 1);

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
			sdesc->cmd);

	fprintf(fp, "# total samples          : %"PRIu64"\n", csmpl->entry_count);
	fprintf(fp, "# total buffer overflows : %"PRIu64"\n#\n#", csmpl->last_ovfl);

	if (need_resolve)
		fprintf(fp, "# %*s   %%self    %%cum %*s symbol\n",
			(int)len, "counts",
			(int)(2+(sizeof(uint64_t)<<1)),
			"code addr");
	else
		fprintf(fp, "# %*s   %%self    %%cum %*s\n",
			(int)len, "counts",
			(int)(2+(sizeof(uint64_t)<<1)),
			"code addr");

	len+=3;
	for(i=0; i < num_entries; i++) {

		addr       = tab[i]->key.val;
		count      = tab[i]->count[0];

		/*
		 * skip sample if fused with another one
		 */
		if (!count)
			continue;

		if (count == 0)
			continue; /* can happen in per-function mode */

		cum_count += count;
		d_cum	   = (double)count*100.0 / (double)total_count;
		cum_total  = (double)cum_count*100.0 / (double)total_count;

		if (cum_total > (double)options.smpl_cum_thres)
			break;

		/* unknown symbols are grouped only in --smpl-per-func mode */
		if (options.opt_smpl_per_func && is_unknown_cookie(tab[i]->cookie))
			addr = 0;

		fprintf(fp, "%*"PRIu64" %6.2f%% %6.2f%% 0x%0*"PRIx64" ",
			    (int)len, 
			    count, 
			    d_cum,
			    (double)cum_count*100.0 / (double)total_count,
			    (int)(sizeof(uint64_t)<<1),
			    addr);

		if (need_resolve)
			pfmon_print_address(fp,
					    sdesc->syms,
					    tab[i]->key.val,
					    0,
					    tab[i]->key.version);
		fputc('\n', fp);

		/*
 		 * exit after n samples have been printed
 		 */
		if (++ns == top_num)
			break;
	}

	free(tab);

	return 0;
}

static int
pebs_hist_terminate_session(pfmon_sdesc_t *sdesc)
{
	pebs_hist_show_results(sdesc);
	pfmon_hash_free(sdesc->csmpl.data);
	sdesc->csmpl.data = NULL;
	return 0;
}

static int
pebs_init_ctx_arg(pfmon_ctx_t *ctx, unsigned int max_pmds_sample)
{
	void *p;
	uint64_t buf_size;
	int ret;

	/*
 	 * PEBS is using fixed size samples. Therefore we can use a 0 slack
 	 * argument.
	 */
	ret = pfmon_compute_smpl_entries(pebs_func->hdr_sz, pebs_func->entry_sz, 0);
	if (ret == -1)
		return -1;

	buf_size = pebs_func->hdr_sz + options.smpl_entries*pebs_func->entry_sz;

	vbprintf("sampling buffer #entries=%lu size=%llu, max_entry_size=%zu SS=%zu\n",
		 options.smpl_entries,
		(unsigned long long)buf_size,
		pebs_func->entry_sz);
	/*
	 * ctx_arg is freed in pfmon_create_context().
	 */
	p = calloc(1, pebs_func->arg_sz);
	if (!p) {
		warning("cannot allocate format argument\n");
		return -1;
	}
	ctx->ctx_arg = p;
	ctx->ctx_arg_size = pebs_func->arg_sz;
	ctx->ctx_map_size = buf_size;

	pebs_func->ctx_arg(p, buf_size);

	return 0;
}

static pebs_func_t pebs_core_func = {
	.entry_sz = sizeof(pfm_pebs_core_smpl_entry_t),
	.hdr_sz = sizeof(pfm_pebs_core_smpl_hdr_t),
	.arg_sz = sizeof(pfm_pebs_core_smpl_arg_t),
	.process_raw = pebs_process_raw, /* shared */
	.process_compact = pebs_process_compact,
	.process_prof = pebs_process_prof, /* shared */
	.get_count = pebs_core_get_count,
	.get_ovfls = pebs_core_get_ovfls,
	.ctx_arg = pebs_core_ctx_arg
};

static int
pebs_hist_initialize_module(void)
{
	switch(options.pmu_type) {
	case PFMLIB_INTEL_ATOM_PMU:
	case PFMLIB_INTEL_CORE_PMU:
		pebs_func = &pebs_core_func;
		break;
	default:	
		fatal_error("unsupported PEBS PMU=%d\n", options.pmu_type);
	}
        return 0;
}

pfmon_smpl_module_t pebs_smpl_module = {
	.name		    = "pebs",
	.description	    = "Intel Core/Atom PEBS sampling",
	.process_samples    = pebs_process_samples,
	.initialize_mask    = pebs_initialize_mask,
	.init_ctx_arg	    = pebs_init_ctx_arg,
	.initialize_session = pebs_hist_initialize_session,
	.terminate_session  = pebs_hist_terminate_session,
        .initialize_module  = pebs_hist_initialize_module,
	.print_header       = pebs_hist_print_header,
	.validate_events    = pebs_hist_validate_events,
	.flags		    = PFMON_SMPL_MOD_FL_PEBS,
	.fmt_name	    = PFM_PEBS_CORE_SMPL_NAME /* changed dynamically */
};
