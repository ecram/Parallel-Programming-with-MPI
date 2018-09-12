/*
 * pfmon_os_v2x.c 
 *
 * Copyright (c) 2008 Google, Inc
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
#include <sys/types.h>
#include <sys/mman.h>

#include "pfmon.h"

static int
probe_v2x(void)
{
	return PFM_VERSION_MAJOR(options.pfm_version) == 2
	    && PFM_VERSION_MINOR(options.pfm_version) > 0;
}

static int
create_sets_v2x(int fd, pfmon_setdesc_t *sets, int n, int *err)
{
	pfarg_setdesc_t *s, *p;
	int i, ret = 0;
	int c, nn;

	s = calloc(n, sizeof(*s));
	if (!s) {
		*err = errno;
		return -1;
	}
	for(i=0; i < n ; i++) {
		s[i].set_id = sets[i].set_id;
		s[i].set_flags = sets[i].set_flags;
		s[i].set_timeout = sets[i].set_timeout;
	}
	if ((n * sizeof(*s))<= options.arg_mem_max) {
		ret = pfm_create_evtsets(fd, s, n);
	} else {
		p = s;
		c = options.arg_mem_max / sizeof(*p);
		while(n) {
			nn = n *sizeof(*p) > options.arg_mem_max ? c : n;
			ret = pfm_create_evtsets(fd, p, nn);
			if (ret)
				break;
			n -= nn;
			p += nn;
		}
	}

	*err = errno;
	free(s);
	return ret;
}

static int
getinfo_sets_v2x(int fd, pfmon_setinfo_t *sets, int n, int *err)
{
	pfarg_setinfo_t *s, *p;
	int i, ret = 0;
	int c, nn;

	s = calloc(n, sizeof(*s));
	if (!s) {
		*err = errno;
		return -1;
	}
	for(i=0; i < n ; i++) {
		s[i].set_id = sets[i].set_id;
	}
	if ((n * sizeof(*s))<= options.arg_mem_max) {
		ret = pfm_getinfo_evtsets(fd, s, n);
	} else {
		p = s;
		c = options.arg_mem_max / sizeof(*p);
		while(n) {
			nn = n *sizeof(*p) > options.arg_mem_max ? c : n;
			ret = pfm_getinfo_evtsets(fd, p, nn);
			if (ret)
				break;
			n -= nn;
			p += nn;
		}
	}
	*err = errno;
	for(i=0; i < n; i++) {
		sets[i].set_runs = s[i].set_runs;
		sets[i].set_act_duration = s[i].set_act_duration;
		memcpy(sets[i].set_avail_pmcs, s[i].set_avail_pmcs,
			sizeof(s[i].set_avail_pmcs));
		memcpy(sets[i].set_avail_pmds, s[i].set_avail_pmds,
			sizeof(s[i].set_avail_pmds));
	}
	free(s);
	return ret;
}

static int 
rd_pmds_v2x(int fd, pfmon_pmd_t *pmds, int n, int *err)
{
	pfarg_pmd_t *pds, *p;
	int i, ret = 0;
	int c, nn, j;

	pds = calloc(n, sizeof(*pds));
	if (!pds) {
		*err = errno;
		return -1;
	}
	j = n;
	for(i=0; i < j; i++) {
		pds[i].reg_num  = pmds[i].reg_num;
		pds[i].reg_set  = pmds[i].reg_set;
	}
	if ((n * sizeof(*pds))<= options.arg_mem_max) {
		ret = pfm_read_pmds(fd, pds, n);
	} else {
		p = pds;
		c = options.arg_mem_max / sizeof(*p);
		while(n) {
			nn = n *sizeof(*p) > options.arg_mem_max ? c : n;
			ret = pfm_read_pmds(fd, p, nn);
			if (ret)
				break;
			n -= nn;
			p += nn;
		}
	}

	*err = errno;
	for(i=0; i < j; i++) {
		pmds[i].reg_value = pds[i].reg_value;
		/* other fields are not used by pfmon */
	}
	free(pds);
	return ret;
}

static int
start_v2x(int fd, int *err)
{
	int ret;
	ret = pfm_start(fd, NULL);
	*err = errno;
	return ret;
}

static int
stop_v2x(int fd, int *err)
{
	int ret;
	ret = pfm_stop(fd);
	*err = errno;
	return ret;
}

static int
restart_v2x(int fd, int *err)
{
	int ret;
	ret = pfm_restart(fd);
	*err = errno;
	return ret;
}

static int
create_v2x(pfmon_ctx_t *ctx, void **smpl_hdr, int *err)
{
	pfarg_ctx_t *new_ctx;
	void *addr;
	int ret;

	addr = calloc(1, sizeof(*new_ctx));
	if (addr == NULL) {
		*err = errno;
		return -1;
	}
	new_ctx = addr;

	new_ctx->ctx_flags = ctx->ctx_flags;

	ret = pfm_create_context(new_ctx, ctx->fmt_name, ctx->ctx_arg, ctx->ctx_arg_size);
	*err = errno;
	if (ret == -1)
		goto error;

	if (options.opt_use_smpl && ctx->ctx_map_size) {
		*smpl_hdr = mmap(NULL,
				 ctx->ctx_map_size,
				 PROT_READ, MAP_PRIVATE,
				 ret,
				 0);
		*err = errno;

		if (*smpl_hdr == MAP_FAILED) {
			DPRINT(("cannot mmap buffer errno=%d\n", errno));
			close(ret);
			ret = -1;
			goto error;
		}
		DPRINT(("mmap @%p size=%zu\n",
			*smpl_hdr,
			ctx->ctx_map_size));
	}
error:
	free(addr);
	return ret;
}

static int
load_v2x(int fd, int tgt, int *err)
{
	pfarg_load_t load_args;
	int ret;

	memset(&load_args, 0, sizeof(load_args));

	load_args.load_pid = tgt;

	ret = pfm_load_context(fd, &load_args);
	*err = errno;
	return ret;
}

static int
unload_v2x(int fd, int *err)
{
	int ret;

	ret = pfm_unload_context(fd);
	*err = errno;
	return ret;
}

static int
wr_pmcs_v2x(int fd, pfmon_event_set_t *sets, pfmon_pmc_t *pmcs, int n, int *err)
{
	pfarg_pmc_t *pcs, *p;
	int i, ret = 0;
	int nn, c;

	pcs = calloc(n, sizeof(*pcs));
	if (!pcs) {
		*err = errno;
		return -1;
	}
	for(i=0; i < n ; i++) {
		pcs[i].reg_num = pmcs[i].reg_num;
		pcs[i].reg_set = pmcs[i].reg_set;
		pcs[i].reg_flags = pmcs[i].reg_flags;
		pcs[i].reg_value = pmcs[i].reg_value;
	}

	if ((n * sizeof(*pcs))<= options.arg_mem_max) {
		ret = pfm_write_pmcs(fd, pcs, n);
	} else {
		p = pcs;
		c = options.arg_mem_max / sizeof(*p);
		while(n) {
			nn = n *sizeof(*p) > options.arg_mem_max ? c : n;
			ret = pfm_write_pmcs(fd, p, nn);
			if (ret)
				break;
			n -= nn;
			p += nn;
		}
	}

	*err = errno;
	free(pcs);
	return ret;
}

static int 
wr_pmds_v2x(int fd, pfmon_pmd_t *pmds, int n, int *err)
{
	pfarg_pmd_t *pds, *p;
	int i, ret = 0;
	int c, nn;

	pds = calloc(n, sizeof(*pds));
	if (!pds) {
		*err = errno;
		return -1;
	}
	for(i=0; i < n ; i++) {
		pds[i].reg_num = pmds[i].reg_num;
		pds[i].reg_set = pmds[i].reg_set;
		pds[i].reg_flags = pmds[i].reg_flags;
		pds[i].reg_value = pmds[i].reg_value;

		/* only when really needed */
		if (options.opt_use_smpl) {
			pds[i].reg_long_reset = pmds[i].reg_long_reset;
			pds[i].reg_short_reset = pmds[i].reg_short_reset;
			pds[i].reg_random_mask = pmds[i].reg_random_mask;
			memcpy(pds[i].reg_reset_pmds, pmds[i].reg_reset_pmds,
		       		sizeof(pmds[i].reg_reset_pmds));
			memcpy(pds[i].reg_smpl_pmds, pmds[i].reg_smpl_pmds,
		       		sizeof(pmds[i].reg_smpl_pmds));
		}
	}
	if ((n * sizeof(*pds))<= options.arg_mem_max) {
		ret = pfm_write_pmds(fd, pds, n);
	} else {
		p = pds;
		c = options.arg_mem_max / sizeof(*p);
		while(n) {
			nn = n *sizeof(*p) > options.arg_mem_max ? c : n;
			ret = pfm_write_pmds(fd, p, nn);
			if (ret)
				break;
			n -= nn;
			p += nn;
		}
	}

	*err = errno;
	free(pds);
	return ret;
}

int
wr_ibrs_v2x(int fd, pfmon_pmc_t *pmcs, int n, int *err)
{
	return wr_pmcs_v2x(fd, NULL, pmcs, n, err);
}

int
wr_dbrs_v2x(int fd, pfmon_pmc_t *pmcs, int n, int *err)
{
	return wr_pmcs_v2x(fd, NULL, pmcs, n, err);
}

static int
get_unavail_regs_v2x(int type, pfmlib_regmask_t *r_pmcs, pfmlib_regmask_t *r_pmds)
{
	pfarg_ctx_t ctx;
	pfarg_setinfo_t	setf;
	int fd, i, j, ret;

	memset(&ctx, 0, sizeof(ctx));
	memset(&setf, 0, sizeof(setf));

	ctx.ctx_flags = type;

	fd = pfm_create_context(&ctx, NULL, NULL, 0);
	if (fd == -1)
		return -1;

	ret = pfm_getinfo_evtsets(fd, &setf, 1);
	if (ret == -1) {
		close(fd);
		return -1;
	}

	if (r_pmcs)
		for(i=0; i < PFM_PMC_BV; i++) {
			for(j=0; j < 64; j++) {
				if ((setf.set_avail_pmcs[i] & (1ULL << j)) == 0)
					pfm_regmask_set(r_pmcs, (i<<6)+j);
			}
		}

	if (r_pmds)
		for(i=0; i < PFM_PMD_BV; i++) {
			for(j=0; j < 64; j++) {
				if ((setf.set_avail_pmds[i] & (1ULL << j)) == 0)
					pfm_regmask_set(r_pmds, (i<<6)+j);
			}
		}
	close(fd);
	return ret;
}

pfmon_api_t pfmon_api_v2x={
	.probe = probe_v2x,
	.create = create_v2x,
	.load = load_v2x,
	.start = start_v2x,
	.stop = stop_v2x,
	.restart = restart_v2x,
	.unload = unload_v2x,
	.wr_pmds = wr_pmds_v2x,
	.wr_pmcs = wr_pmcs_v2x,
	.rd_pmds = rd_pmds_v2x,
	.wr_ibrs = wr_ibrs_v2x,
	.wr_dbrs = wr_dbrs_v2x,
	.create_sets = create_sets_v2x,
	.getinfo_sets = getinfo_sets_v2x,
	.get_unavail_regs = get_unavail_regs_v2x
};
