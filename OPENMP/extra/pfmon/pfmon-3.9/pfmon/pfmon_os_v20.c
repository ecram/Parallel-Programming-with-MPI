/*
 * pfmon_os_v20.c 
 *
 * Based on:
 * Copyright (c) 2001-2006 Hewlett-Packard Development Company, L.P.
 * Contributed by Stephane Eranian <eranian@hpl.hp.com>
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
probe_v20(void)
{
	return PFM_VERSION_MAJOR(options.pfm_version) == 2
	    &&  PFM_VERSION_MINOR(options.pfm_version) == 0;
}

/*
 * for perfmon v2.0
 */
static int
create_v20(pfmon_ctx_t *ctx, void **smpl_hdr, int *err)
{
	pfarg_context_t *old_ctx;
	void *addr;
	int ret;

	addr = calloc(1, sizeof(*old_ctx)+ctx->ctx_arg_size);
	if (addr == NULL) {
		*err = errno;
		return -1;
	}
	old_ctx = addr;

	old_ctx->ctx_flags = ctx->ctx_flags;
	memcpy(old_ctx->ctx_smpl_buf_id, ctx->ctx_uuid, sizeof(pfm_uuid_t));

	memcpy(old_ctx+1, ctx->ctx_arg, ctx->ctx_arg_size);

	ret = perfmonctl(0, PFM_CREATE_CONTEXT, old_ctx, 1);
	*err = errno;
	if (ret == -1)
		goto error;

	*smpl_hdr = old_ctx->ctx_smpl_vaddr;
	ret = old_ctx->ctx_fd;
	ctx->ctx_map_size = 0;
error:
	free(addr);
	return ret;
}

static int
wr_pmds_v20(int fd, pfmon_pmd_t *pmds, int n, int *err)
{
	pfarg_reg_t *old_pmds;
	int i, ret;

	old_pmds = calloc(1, sizeof(*old_pmds)*n);
	if (old_pmds == NULL) {
		*err = errno;
		return -1;
	}

	for(i=0; i < n; i++) {
		/*
		 * reg_long_reset     -> must do
		 * reg_short_reset    -> must do
		 * reg_last_reset_val -> unused
		 * reg_ovfl_switch    -> unused in v2.0
		 * reg_reset_pmds     -> write_pmcs_old
		 * reg_smpl_pmds      -> write_pmcs_old
		 * reg_smpl_eventid   -> unused
		 * reg_random_mask    -> must do
		 * reg_random_seed    -> must do
		 */
		old_pmds[i].reg_num          = pmds[i].reg_num;
		old_pmds[i].reg_value        = pmds[i].reg_value;
		old_pmds[i].reg_long_reset   = pmds[i].reg_long_reset;
		old_pmds[i].reg_short_reset  = pmds[i].reg_short_reset;
		old_pmds[i].reg_random_mask  = pmds[i].reg_random_mask;
		old_pmds[i].reg_random_seed  = pmds[i].reg_random_seed;
	}
	ret = perfmonctl(fd, PFM_WRITE_PMDS, old_pmds, n);
	*err = errno;

	/*
	 * pfmon does not look at per-register error flags so we don't need
	 * to copy reg_flags back.
	 */
	free(old_pmds);

 	return ret;
}

static int
wr_pmcs_v20(int fd, pfmon_event_set_t *set, pfmon_pmc_t *pmcs, int n, int *err)
{
	pfarg_reg_t *old_pmcs;
	int i, ret;

	*err = 0;

	old_pmcs = calloc(1, sizeof(*old_pmcs)*n);
	if (old_pmcs == NULL) {
		*err = errno;
		return -1;
	}

	for(i=0; i < n; i++) {
		/*
		 * reg_flags          -> must do
		 * reg_long_reset     -> write_pmds_old
		 * reg_short_reset    -> write_pmds_old
		 * reg_last_reset_val -> unused
		 * reg_ovfl_switch    -> unused in v2.0
		 * reg_reset_pmds     -> write_pmcs_old
		 * reg_smpl_pmds      -> write_pmcs_old
		 * reg_smpl_eventid   -> unused
		 * reg_random_mask    -> write_pmds_old
		 * reg_random_seed    -> write_pmds_old
		 */
		old_pmcs[i].reg_num   = pmcs[i].reg_num;
		old_pmcs[i].reg_value = pmcs[i].reg_value;
		old_pmcs[i].reg_flags = pmcs[i].reg_flags;

		/*
		 * the reg_smpl_pmds,reg_reset_pmds bitvector have been moved
		 * to the PMD side in the new interface. This is the only way
		 * to put them back where they belong for perfmon v2.0
		 * We only do this for actual counter events.
		 */
		if (i < set->setup->event_count) {
			old_pmcs[i].reg_smpl_pmds[0] = set->master_pd[i].reg_smpl_pmds[0];
			old_pmcs[i].reg_reset_pmds[0] = set->master_pd[i].reg_reset_pmds[0];
		}
	}

	ret = perfmonctl(fd, PFM_WRITE_PMCS, old_pmcs, n);
	*err = errno;

	free(old_pmcs);

 	return ret;
}

static int
rd_pmds_v20(int fd, pfmon_pmd_t *pmds, int n, int *err)
{
	pfarg_reg_t *old_pmds;
	int i, ret;

	old_pmds = calloc(1, sizeof(*old_pmds)*n);
	if (old_pmds == NULL) {
		*err = errno;
		return -1;
	}

	for(i=0; i < n; i++) {
		old_pmds[i].reg_num  = pmds[i].reg_num;
	}

	ret = perfmonctl(fd, PFM_READ_PMDS, old_pmds, n);
	*err = errno;

	/*
	 * XXX: do the minimum here. pfmon only looks at the reg_value field
	 * XXX: we do this even when the call fails, because it propagates retflags
	 */
	for(i=0; i < n; i++) {
		pmds[i].reg_value = old_pmds[i].reg_value;
	}

	free(old_pmds);

 	return ret;
}

static int
wr_ibrs_v20(int fd, pfmon_pmc_t *pmcs, int n, int *err)
{
	pfarg_dbreg_t *ibrs;
	int i, ret;

	ibrs = calloc(1, sizeof(*ibrs)*n);
	if (ibrs == NULL) {
		*err = errno;
		return -1;
	}

	for(i=0; i < n; i++) {
		ibrs[i].dbreg_num   = pmcs[i].reg_num - 256;
		ibrs[i].dbreg_value = pmcs[i].reg_value;
	}

	ret = perfmonctl(fd, PFM_WRITE_IBRS, ibrs, n);
	*err = errno;

	free(ibrs);

 	return ret;
}

static int
wr_dbrs_v20(int fd, pfmon_pmc_t *pmcs, int n, int *err)
{
	pfarg_dbreg_t *dbrs;
	int i, ret;

	dbrs = calloc(1, sizeof(*dbrs)*n);
	if (dbrs == NULL) {
		*err = errno;
		return -1;
	}

	for(i=0; i < n; i++) {
		dbrs[i].dbreg_num   = pmcs[i].reg_num - 264;
		dbrs[i].dbreg_value = pmcs[i].reg_value;
	}

	ret = perfmonctl(fd, PFM_WRITE_DBRS, dbrs, n);
	*err = errno;

	free(dbrs);

 	return ret;
}

static int
load_v20(int fd, int tgt, int *err)
{
	pfarg_load_t load_args;
	int ret;

	memset(&load_args, 0, sizeof(load_args));

	/* in v2.0 system-wide the load_pid must be the thread id of caller */
	 if (options.opt_syst_wide)  
		 tgt = gettid();

	load_args.load_pid = tgt;

	ret = perfmonctl(fd, PFM_LOAD_CONTEXT, &load_args, 1);
	*err = errno;
	return ret;
}

static int
unload_v20(int fd, int *err)
{
	int ret;

	ret = perfmonctl(fd, PFM_UNLOAD_CONTEXT, NULL, 0);
	*err = errno;
	return ret;
}

static int
start_v20(int fd, int *err)
{
	int ret;
	ret = perfmonctl(fd, PFM_START, NULL, 0);
	*err = errno;
	return ret;
}

int
stop_v20(int fd, int *err)
{
	int ret;
	ret = perfmonctl(fd, PFM_STOP, NULL, 0);
	*err = errno;
	return ret;
}

int
restart_v20(int fd, int *err)
{
	int ret;
	ret = perfmonctl(fd, PFM_RESTART, NULL, 0);
	*err = errno;
	return ret;
}

static int
create_sets_v20(int fd, pfmon_setdesc_t *sets, int n, int *err)
{
	*err = ENOSYS;
	return -1;
}

static int
getinfo_sets_v20(int fd, pfmon_setinfo_t *sets, int n, int *err)
{
	*err = ENOSYS;
	return -1;
}

static int
get_unavail_v20(int type, pfmlib_regmask_t *r_pmcs, pfmlib_regmask_t *r_pmds)
{
	return -1;
}

pfmon_api_t pfmon_api_v20={
	.probe = probe_v20,
	.create = create_v20,
	.load = load_v20,
	.unload = unload_v20,
	.start = start_v20,
	.stop = stop_v20,
	.restart = restart_v20,
	.wr_pmds = wr_pmds_v20,
	.wr_pmcs = wr_pmcs_v20,
	.rd_pmds = rd_pmds_v20,
	.wr_ibrs = wr_ibrs_v20,
	.wr_dbrs = wr_dbrs_v20,
	.create_sets = create_sets_v20,
	.getinfo_sets = getinfo_sets_v20,
	.get_unavail_regs = get_unavail_v20
};
