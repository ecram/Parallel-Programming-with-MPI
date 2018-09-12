/*
 * compact_smpl.c - compact output sampling module for all PMU  models
 *
 * Copyright (c) 2002-2006 Hewlett-Packard Development Company, L.P.
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

#include <perfmon/perfmon_dfl_smpl.h>

#define SMPL_MOD_NAME	"compact"

/*
 * forward declaration
 */
pfmon_smpl_module_t compact_smpl_module;

static void
compact_initialize_mask(void)
{
	pfmon_bitmask_setall(&compact_smpl_module.pmu_mask);
}

/*
 * module has been obsoleted
 */
static int
compact_validate_events(pfmon_event_set_t *set)
{
	if (options.opt_smpl_mode == PFMON_SMPL_RAW)
		warning("use inst-hist (default) module and --smpl-raw for raw mode output\n");
	else
		warning("use inst-hist (default) module and --smpl-compact for compact mode output\n");

	return -1;
}

static int
compact_process_samples(pfmon_sdesc_t *sdesc)
{
	return -1;
}


pfmon_smpl_module_t compact_smpl_module ={
	.name		    = SMPL_MOD_NAME,
	.description	    = "Column-style raw values (obsoleted)",
	.initialize_mask    = compact_initialize_mask,
	.validate_events    = compact_validate_events,
	.process_samples    = compact_process_samples,
	.fmt_name	    = PFM_DFL_SMPL_NAME
};
