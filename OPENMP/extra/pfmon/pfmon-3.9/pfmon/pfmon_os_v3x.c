/*
 * pfmon_os_v3x.c 
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
probe_v3x(void)
{
	return PFM_VERSION_MAJOR(options.pfm_version) == 3;
}


static int
create_sets_v3x(int fd, pfmon_setdesc_t *sets, int n, int *err)
{
	pfarg_set_desc_t *s, *p;
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
		ret = pfm_create_sets(fd, 0, s, n * sizeof(*s));
	} else {
		p = s;
		c = options.arg_mem_max / sizeof(*p);
		while(n) {
			nn = n *sizeof(*p) > options.arg_mem_max ? c : n;
			ret = pfm_create_sets(fd, 0, p, nn * sizeof(*p));
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
getinfo_sets_v3x(int fd, pfmon_setinfo_t *sets, int n, int *err)
{
	pfarg_set_info_t *s, *p;
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
		ret = pfm_getinfo_sets(fd, 0, s, n * sizeof(*s));
	} else {
		p = s;
		c = options.arg_mem_max / sizeof(*p);
		while(n) {
			nn = n *sizeof(*p) > options.arg_mem_max ? c : n;
			ret = pfm_getinfo_sets(fd, 0, p, nn * sizeof(p));
			if (ret)
				break;
			n -= nn;
			p += nn;
		}
	}
	*err = errno;
	for(i=0; i < n; i++) {
		sets[i].set_runs = s[i].set_runs;
		sets[i].set_act_duration = s[i].set_duration;
	}
	free(s);
	return ret;
}

static int 
rd_pmds_v3x(int fd, pfmon_pmd_t *pmds, int n, int *err)
{
	pfarg_pmr_t *pds, *p;
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
		ret = pfm_read(fd, 0, PFM_RW_PMD, pds, n * sizeof(*pds));
	} else {
		p = pds;
		c = options.arg_mem_max / sizeof(*p);
		while(n) {
			nn = n *sizeof(*p) > options.arg_mem_max ? c : n;
			ret = pfm_read(fd, 0, PFM_RW_PMD, p, nn * sizeof(*p));
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
start_v3x(int fd, int *err)
{
	int ret;
	ret = pfm_set_state(fd, 0, PFM_ST_START);
	*err = errno;
	return ret;
}

static int
stop_v3x(int fd, int *err)
{
	int ret;
	ret = pfm_set_state(fd, 0, PFM_ST_STOP);
	*err = errno;
	return ret;
}

static int
restart_v3x(int fd, int *err)
{
	int ret;
	ret = pfm_set_state(fd, 0, PFM_ST_RESTART);
	*err = errno;
	return ret;
}

static int
create_v3x(pfmon_ctx_t *ctx, void **smpl_hdr, int *err)
{
	int flags;
	int ret = 0;

	flags = ctx->ctx_flags;
	if (options.opt_use_smpl)
		flags |= PFM_FL_SMPL_FMT;
		
	ret = pfm_create(flags, NULL, ctx->fmt_name, ctx->ctx_arg, ctx->ctx_arg_size);
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
	return ret;
}

static int
load_v3x(int fd, int tgt, int *err)
{
	int ret;

	ret = pfm_attach(fd, 0, tgt);
	*err = errno;
	return ret;
}

static int
unload_v3x(int fd, int *err)
{
	int ret;

	ret = pfm_attach(fd, 0, PFM_NO_TARGET);
	*err = errno;
	return ret;
}

static int
wr_pmcs_v3x(int fd, pfmon_event_set_t *sets, pfmon_pmc_t *pmcs, int n, int *err)
{
	pfarg_pmr_t *pcs, *p;
	int i, ret = 0;
	int c, nn;

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
		ret = pfm_write(fd, 0, PFM_RW_PMC, pcs, n * sizeof(*pcs));
	} else {
		p = pcs;
		c = options.arg_mem_max / sizeof(*p);
		while(n) {
			nn = n *sizeof(*p) > options.arg_mem_max ? c : n;
			ret = pfm_write(fd, 0, PFM_RW_PMC, p, nn * sizeof(*p));
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
wr_pmds_v3x(int fd, pfmon_pmd_t *pmds, int n, int *err)
{
	pfarg_pmd_t *pds;
	pfarg_pmd_attr_t *pas;
	void *addr, *p;
	size_t sz;
	int i, ret = 0, type;
	int c, nn;

	if (options.opt_use_smpl) {
		sz = sizeof(*pas);	
		type = PFM_RW_PMD_ATTR;
		c = options.arg_mem_max / sizeof(*pas);
	} else {
		sz = sizeof(*pds);
		type = PFM_RW_PMD;
		c = options.arg_mem_max / sizeof(*pds);
	}
	addr = calloc(n, sz);
	if (!addr) {
		*err = errno;
		return -1;
	}
	if (options.opt_use_smpl) {
		pas = addr;
		for(i=0; i < n ; i++) {
			pas[i].reg_num = pmds[i].reg_num;
			pas[i].reg_set = pmds[i].reg_set;
			pas[i].reg_flags = pmds[i].reg_flags;
			pas[i].reg_value = pmds[i].reg_value;

			pas[i].reg_long_reset = pmds[i].reg_long_reset;
			pas[i].reg_short_reset = pmds[i].reg_short_reset;
			pas[i].reg_random_mask = pmds[i].reg_random_mask;
			memcpy(pas[i].reg_reset_pmds, pmds[i].reg_reset_pmds,
		       		sizeof(pmds[i].reg_reset_pmds));
			memcpy(pas[i].reg_smpl_pmds, pmds[i].reg_smpl_pmds,
		       		sizeof(pmds[i].reg_smpl_pmds));
		}
	} else {
		pds = addr;
		for(i=0; i < n ; i++) {
			pds[i].reg_num = pmds[i].reg_num;
			pds[i].reg_set = pmds[i].reg_set;
			pds[i].reg_flags = pmds[i].reg_flags;
			pds[i].reg_value = pmds[i].reg_value;
		}
	}

	if ((n * sz) <= options.arg_mem_max) {
		ret = pfm_write(fd, 0, type, addr, n * sz);
	} else {
		p = addr;
		while(n) {
			nn = n * sz > options.arg_mem_max ? c : n;
			ret = pfm_write(fd, 0, type, p, n * sz);
			if (ret)
				break;
			n -= nn;
			p += nn * sz;
		}
	}

	*err = errno;
	free(addr);
	return ret;
}

int
wr_ibrs_v3x(int fd, pfmon_pmc_t *pmcs, int n, int *err)
{
	return wr_pmcs_v3x(fd, NULL, pmcs, n, err);
}

int
wr_dbrs_v3x(int fd, pfmon_pmc_t *pmcs, int n, int *err)
{
	return wr_pmcs_v3x(fd, NULL, pmcs, n, err);
}

static int
get_unavail_regs_v3x(int type, pfmlib_regmask_t *r_pmcs, pfmlib_regmask_t *r_pmds)
{
	pfarg_sinfo_t sif;
	int fd, i, j;

	memset(&sif, 0, sizeof(sif));

	fd = pfm_create(type, &sif, NULL, NULL, 0);

	if (fd == -1)
		return -1;

	if (r_pmcs)
		for(i=0; i < PFM_PMC_BV; i++) {
			for(j=0; j < 64; j++) {
				if ((sif.sif_avail_pmcs[i] & (1ULL << j)) == 0)
					pfm_regmask_set(r_pmcs, (i<<6)+j);
			}
		}

	if (r_pmds)
		for(i=0; i < PFM_PMD_BV; i++) {
			for(j=0; j < 64; j++) {
				if ((sif.sif_avail_pmds[i] & (1ULL << j)) == 0)
					pfm_regmask_set(r_pmds, (i<<6)+j);
			}
		}

	close(fd);
	return 0;
}

pfmon_api_t pfmon_api_v3x={
	.probe = probe_v3x,
	.create = create_v3x,
	.load = load_v3x,
	.start = start_v3x,
	.stop = stop_v3x,
	.restart = restart_v3x,
	.unload = unload_v3x,
	.wr_pmds = wr_pmds_v3x,
	.wr_pmcs = wr_pmcs_v3x,
	.rd_pmds = rd_pmds_v3x,
	.wr_ibrs = wr_ibrs_v3x,
	.wr_dbrs = wr_dbrs_v3x,
	.create_sets = create_sets_v3x,
	.getinfo_sets = getinfo_sets_v3x,
	.get_unavail_regs = get_unavail_regs_v3x
};
