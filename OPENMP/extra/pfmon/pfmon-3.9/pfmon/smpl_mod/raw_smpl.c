/*
 * raw_smpl.c - write buffer in binary form into a file for all PMU models
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

static int
raw_process_samples(pfmon_sdesc_t *sdesc)
{
	/* obsolete */
	return -1;
}

pfmon_smpl_module_t raw_smpl_module;
static void raw_old_initialize_mask(void)
{
	pfmon_bitmask_setall(&raw_smpl_module.pmu_mask);
}

static int
raw_validate_events(pfmon_event_set_t *set)
{
	warning("raw sampling module, obsolete, use --smpl-raw instead\n");
	return -1;
}


pfmon_smpl_module_t raw_smpl_module ={
	.name		    = "raw",
	.description	    = "dump buffer in binary format",
	.process_samples    = raw_process_samples,
	.initialize_mask    = raw_old_initialize_mask,
	.validate_events    = raw_validate_events,
	.fmt_name	    = PFM_DFL_SMPL_NAME
};
