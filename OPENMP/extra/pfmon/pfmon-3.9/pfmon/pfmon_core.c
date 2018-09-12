/*
 * pfmon_core.c - Intel Core processors PMU support
 *
 * Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
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
#include "pfmon.h"
#include <perfmon/pfmlib_core.h>

typedef struct {
	char *cnt_mask_arg;
	char *inv_arg;
	char *edge_arg;
} pfmon_core_args_t;

typedef struct {
	pfmlib_core_input_param_t	inp;
	pfmon_core_args_t		args;
} pfmon_core_param_t;

static void
pfmon_core_setup_cnt_mask(pfmon_event_set_t *set)
{
	pfmlib_core_input_param_t *param = set->setup->mod_inp;
	pfmon_core_args_t *args;
	char *cnt_mask_str;
	char *p, *endptr = NULL;
	unsigned long l;
	unsigned int cnt=0;
	unsigned int i;

	args = set->setup->mod_args;

	cnt_mask_str = args->cnt_mask_arg;

	/*
	 * the default value for cnt_mask is 0: this means at least once 
	 * per cycle.
	 */
	if (cnt_mask_str == NULL) {
		for (i=0; i < set->setup->event_count; i++)
			param->pfp_core_counters[i].cnt_mask = 0;
		return;
	}

	while (cnt_mask_str) {

		if (cnt == options.max_counters || cnt == set->setup->event_count)
			goto too_many;

		p = strchr(cnt_mask_str,',');

		if ( p ) *p = '\0';

		l = strtoul(cnt_mask_str, &endptr, 10);
		if (*endptr || l < 0 || l >255)
			goto invalid;

		if ( p ) *p++ = ',';

		param->pfp_core_counters[cnt++].cnt_mask = (unsigned int)l;

		cnt_mask_str = p;
	}
	return;
invalid:
	fatal_error("event %d: counter mask must be in [0-256)\n", cnt);
too_many:
	fatal_error("too many counter masks specified\n");
}

static void
pfmon_core_setup_bool(pfmon_event_set_t *set, int flag, char *str, char *name)
{
	pfmlib_core_input_param_t *param = set->setup->mod_inp;
	pfmon_core_args_t *args;
	char *p, c;
	unsigned int cnt=0;
	unsigned int i;

	args = set->setup->mod_args;

	/*
	 * by default, clear the flag
	 */
	if (str == NULL) {
		for (i=0; i < set->setup->event_count; i++)
			param->pfp_core_counters[i].flags &= ~flag;
		return;
	}

	while (str) {
		if (cnt == options.max_counters || cnt == set->setup->event_count)
			goto too_many;

		p = strchr(str,',');
		if ( p )
			*p = '\0';

		if (strlen(str) > 1)
			goto invalid;

		c = *str;

		if ( p )
			*p++ = ',';

		if (c == 'y' || c == 'Y' || c == '1')
			param->pfp_core_counters[cnt].flags |= flag;	
		else if (c != 'n' &&  c != 'N' && c != '0')
			goto invalid;
		cnt++;
		str = p;
	}
	return;
invalid:
	fatal_error("event %d: %s value is any one of y,Y,n,N,0,1\n", name, cnt);
too_many:
	fatal_error("too many %s values specified\n", name);
}

static int
pfmon_core_prepare_registers(pfmon_event_set_t *set)
{
	pfmlib_core_input_param_t *param = set->setup->mod_inp;
	int i;

	if (param->pfp_core_pebs.pebs_used) {
		/*
 		 * For PEBS to work efficently, you have
 		 * to disable 64-bit virtualization of the
 		 * counter, otherwise you get one interrupt
 		 * per overflow, instead of one interrupt per
 		 * PEBS buffer overflow
 		 *
 		 * on Core, only counter0 has PEBS support
 		 */
		for (i=0; i < set->setup->pc_count; i++)
			if (set->setup->master_pc[i].reg_num == 0)
				set->setup->master_pc[i].reg_flags |= PFM_REGFL_NO_EMUL64;
	}
	return 0;
}

static void
pfmon_core_usage(void)
{
	printf( "--counter-mask=msk1,msk2,...\t\tSet event counter mask (0,1,2,3).\n"
		"--inv-mask=i1,i2,...\t\t\tSet event inverse counter mask\n"
		"\t\t\t\t\t (y/n,0/1).\n"
		"--edge-mask=e1,e2,...\t\t\tSet event edge detect (y/n,0/1).\n"
	);
}

/*
 * Intel Core options
 *
 * 000-255   reserved for generic options
 * 400-499   reserved for PMU specific options
 * 500-599   reserved for format specific options
 */
static struct option cmd_core_options[]={
	{ "counter-mask", 1, 0, 400 },
	{ "inv-mask", 1, 0, 401 },
	{ "edge-mask", 1, 0, 402 },
	{ 0, 0, 0, 0}
};

static int
pfmon_core_initialize(void)
{
	int r;

	r = pfmon_register_options(cmd_core_options, sizeof(cmd_core_options));
	if (r == -1) return -1;

	return 0;
}

/*
 * 0  means we understood the option
 * -1 unknown option
 */
static int
pfmon_core_parse_options(int code, char *optarg)
{
	pfmon_core_args_t *args;
	pfmon_event_set_t *set;

	set = options.last_set;

	args = set->setup->mod_args;

	switch(code) {
		case  400:
			if (args->cnt_mask_arg)
				fatal_error("counter masks already defined\n");
			args->cnt_mask_arg = optarg;
			break;
		case  401:
			if (args->inv_arg)
				fatal_error("inverse mask already defined\n");
			args->inv_arg = optarg;
			break;
		case  402:
			if (args->edge_arg)
				fatal_error("edge detect mask already defined\n");
			args->edge_arg = optarg;
			break;
		default:
			return -1;
	}
	return 0;
}

static void
pfmon_core_setup_pebs(pfmon_event_set_t *set)
{
	pfmlib_core_input_param_t *param = set->setup->mod_inp;
	param->pfp_core_pebs.pebs_used = 1;
}

static int
pfmon_core_setup(pfmon_event_set_t *set)
{
	pfmon_core_args_t *args;

	args = set->setup->mod_args;

	if (!args)
		return 0;

	/* 
	 * we systematically initialize thresholds to their minimal value
	 * or requested value
	 */
	if (options.smpl_mod->flags & PFMON_SMPL_MOD_FL_PEBS) {
#if 0
		if (set->event_count > 1)
			fatal_error("with PEBS sampling, only one event must be used\n");
#endif
		if (!pfm_core_is_pebs(set->setup->inp.pfp_events))
			fatal_error("event must be PEBS capable, check with pfmon -i\n");
		pfmon_core_setup_pebs(set);
	}
	pfmon_core_setup_cnt_mask(set);

	pfmon_core_setup_bool(set, PFM_CORE_SEL_EDGE, args->edge_arg, "edge");
	pfmon_core_setup_bool(set, PFM_CORE_SEL_INV, args->inv_arg, "inv");

	return 0;
}

static void
pfmon_core_verify_cmdline(int argc, char **argv)
{
	if (options.dfl_plm & (PFM_PLM1|PFM_PLM2))
		fatal_error("-1 or -2 privilege levels are not supported by this PMU model\n");

	if (options.opt_data_trigger_ro)
		fatal_error("the --trigger-data-ro option is not supported by this processor\n");

	if (options.smpl_mod->flags & PFMON_SMPL_MOD_FL_PEBS) {
		if (!options.opt_use_smpl)
			fatal_error("PEBS module can only be used when sampling\n");
	}
}

static void
pfmon_core_show_event_info(unsigned int idx)
{
	pfmlib_event_t e;
	unsigned int n, np, i;
	char name[PFMON_MAX_EVTNAME_LEN];


	memset(&e, 0, sizeof(e));
	e.event = idx;

	pfm_get_num_event_masks(idx, &n);

	printf("PEBS     : ");

	if (n) {
		np = 0;
		for(i=0; i < n; i++) {
			e.num_masks = 1;
			e.unit_masks[0] = i;
			if (pfm_core_is_pebs(&e)) {
				pfm_get_event_mask_name(idx, i, name, PFMON_MAX_EVTNAME_LEN);
				printf("[%s] ", name);
				np++;
			}
		}
		if (np == 0)
			puts("No");
		else	
			putchar('\n');
	} else {
		if (pfm_core_is_pebs(&e))
			puts("Yes");
		else
			puts("No");
	}
}

/*
 * Intel Core-based processors
 */
pfmon_support_t pfmon_core={
	.name				= "Intel Core",
	.pmu_type			= PFMLIB_CORE_PMU,
	.generic_pmu_type		= PFMLIB_NO_PMU,
	.pfmon_initialize		= pfmon_core_initialize,		
	.pfmon_usage			= pfmon_core_usage,	
	.pfmon_parse_options		= pfmon_core_parse_options,
	.pfmon_setup			= pfmon_core_setup,
	.pfmon_prepare_registers	= pfmon_core_prepare_registers,
	.pfmon_verify_cmdline		= pfmon_core_verify_cmdline,
	.pfmon_show_event_info		= pfmon_core_show_event_info,
	.sz_mod_args			= sizeof(pfmon_core_args_t),
	.sz_mod_inp			= sizeof(pfmlib_core_input_param_t),
};
