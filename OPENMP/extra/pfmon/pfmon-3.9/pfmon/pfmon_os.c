/*
 * pfmon_os.c 
 *
 * Copyright (c) 2008 Google, Inc
 * Contributed by Stephane Eranian <eranian@hpl.hp.com>
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

static pfmon_api_t *pfmon_apis[]={
#ifdef __ia64__
	&pfmon_api_v20,
#endif
	&pfmon_api_v2x,
#ifdef CONFIG_PFMON_PFMV3
	&pfmon_api_v3x,
#endif
	NULL
};

static pfmon_api_t *pfmon_api;

int
pfmon_api_probe(void)
{
	pfmon_api_t **p;

	for( p = pfmon_apis; *p; p++) {
		if ((*p)->probe())
			break;
	}
	if (*p) {
		pfmon_api = *p;
		return 0;
	}
	return -1;
}

int
pfmon_load_context(int fd, pid_t tid, int *err)
{
	return pfmon_api->load(fd, tid, err);
}

int
pfmon_write_ibrs(int fd, pfmon_pmc_t *pmcs, int n, int *err)
{
	return pfmon_api->wr_ibrs(fd, pmcs, n, err);
}

int
pfmon_write_dbrs(int fd, pfmon_pmc_t *pmcs, int n, int *err)
{
	return pfmon_api->wr_dbrs(fd, pmcs, n, err);
}

int
pfmon_unload_context(int fd, int *err)
{
	return pfmon_api->unload(fd, err);
}

int
pfmon_write_pmds(int fd, pfmon_event_set_t *set, pfmon_pmd_t *pmds, int n, int *err)
{
	return pfmon_api->wr_pmds(fd, pmds, n, err);
}

int
pfmon_write_pmcs(int fd, pfmon_event_set_t *set, pfmon_pmc_t *pmcs, int n, int *err)
{
	return pfmon_api->wr_pmcs(fd, set, pmcs, n, err);
}

int
pfmon_read_pmds(int fd, pfmon_event_set_t *set, pfmon_pmd_t *pmds, int n, int *err)
{
	return pfmon_api->rd_pmds(fd, pmds, n, err);
}

int
pfmon_create_evtsets(int fd, pfmon_setdesc_t *sets, int n, int *err)
{
	return pfmon_api->create_sets(fd, sets, n, err);
}

int
pfmon_getinfo_evtsets(int fd, pfmon_setinfo_t *sets, int n, int *err)
{
	return pfmon_api->getinfo_sets(fd, sets, n, err);
}

int
pfmon_start(int fd, int *err)
{
	return pfmon_api->start(fd, err);
}

int
pfmon_stop(int fd, int *err)
{
	return pfmon_api->stop(fd, err);
}

int
pfmon_restart(int fd, int *err)
{
	return pfmon_api->restart(fd, err);
}

int
pfmon_create_context(pfmon_ctx_t *ctx, void **smpl_hdr, int *err)
{
	int ret;

	ret = pfmon_api->create(ctx, smpl_hdr, err);
	*err = errno;
	return ret;	
}

int
pfmon_get_unavail_regs(int type, pfmlib_regmask_t *r_pmcs, pfmlib_regmask_t *r_pmds)
{
	/*
 	 * mark everything available by default, worst case, kernel will reject
 	 */
	if (r_pmcs)
		memset(r_pmcs, 0, sizeof(*r_pmcs));
	if (r_pmds)
		memset(r_pmds, 0, sizeof(*r_pmds));

	return pfmon_api->get_unavail_regs(type, r_pmcs, r_pmds);
}

