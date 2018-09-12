/*
 * pfmon_gen_ia32.c - Intel architectural PMU support
 * 		      Intel Core Solo/Core Duo
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

#include <ctype.h>
#include <perfmon/pfmlib_gen_ia32.h>

typedef struct {
	char *cnt_mask_arg;
	char *inv_arg;
	char *edge_arg;
	char *anythr_arg;
} pfmon_gen_ia32_args_t;

static void
pfmon_gen_ia32_setup_cnt_mask(pfmon_event_set_t *set)
{
	pfmlib_gen_ia32_input_param_t *param = set->setup->mod_inp;
	pfmon_gen_ia32_args_t *args;
	char *cnt_mask_str;
	char *p;
	unsigned int cnt_mask;
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
			param->pfp_gen_ia32_counters[i].cnt_mask = 0;
		return;
	}

	while (cnt_mask_str) {

		if (cnt == options.max_counters || cnt == set->setup->event_count)
			goto too_many;

		p = strchr(cnt_mask_str,',');

		if ( p ) *p++ = '\0';

		cnt_mask = atoi(cnt_mask_str);

		if (cnt_mask < 0 || cnt_mask >255) goto invalid;

		param->pfp_gen_ia32_counters[cnt++].cnt_mask= cnt_mask;

		cnt_mask_str = p;
	}
	return;
invalid:
	fatal_error("event %d: counter mask must be in [0-256)\n", cnt);
too_many:
	fatal_error("too many counter masks specified\n");
}

static void
pfmon_gen_ia32_setup_bool(pfmon_event_set_t *set, int flag, char *str, char *name)
{
	pfmlib_gen_ia32_input_param_t *param = set->setup->mod_inp;
	pfmon_gen_ia32_args_t *args;
	char *p, c;
	unsigned int cnt=0;
	unsigned int i;

	args = set->setup->mod_args;

	/*
	 * by default, clear the flag
	 */
	if (str == NULL) {
		for (i=0; i < set->setup->event_count; i++)
			param->pfp_gen_ia32_counters[i].flags &= ~flag;
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
			param->pfp_gen_ia32_counters[cnt].flags |= flag;	
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

static void
pfmon_gen_ia32_usage(void)
{
	printf( "--counter-mask=msk1,msk2,...\t\tSet event counter mask (0,1,2,3).\n"
		"--inv-mask=i1,i2,...\t\t\tSet event inverse counter mask\n"
		"\t\t\t\t\t (y/n,0/1).\n"
		"--edge-mask=e1,e2,...\t\t\tSet event edge detect (y/n,0/1).\n"
		"--anythr-mask=e1,e2,...\t\t\tSet anythread filter (y/n,0/1).\n"
	);
}

/*
 * Generic IA-32 options
 *
 * 000-255   reserved for generic options
 * 400-499   reserved for PMU specific options
 * 500-599   reserved for format specific options
 */
static struct option cmd_gen_ia32_options[]={
	{ "counter-mask", 1, 0, 400 },
	{ "inv-mask", 1, 0, 401 },
	{ "edge-mask", 1, 0, 402 },
	{ "anythr-mask", 1, 0, 403 },
	{ 0, 0, 0, 0}
};

static int
pfmon_gen_ia32_initialize(void)
{
	int r;

	r = pfmon_register_options(cmd_gen_ia32_options, sizeof(cmd_gen_ia32_options));
	if (r == -1) return -1;

	return 0;
}

/*
 * 0  means we understood the option
 * -1 unknown option
 */
static int
pfmon_gen_ia32_parse_options(int code, char *optarg)
{
	pfmon_gen_ia32_args_t *args;
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
		case  403:
			if (args->anythr_arg)
				fatal_error("anythr mask already defined\n");
			args->anythr_arg = optarg;
			break;
		default:
			return -1;
	}
	return 0;
}

static int
pfmon_gen_ia32_setup(pfmon_event_set_t *set)
{
	pfmon_gen_ia32_args_t *args;

	args = set->setup->mod_args;

	if (!args)
		return 0;
	/* 
	 * we systematically initialize thresholds to their minimal value
	 * or requested value
	 */
	pfmon_gen_ia32_setup_cnt_mask(set);

	pfmon_gen_ia32_setup_bool(set, PFM_GEN_IA32_SEL_EDGE, args->edge_arg, "edge");
	pfmon_gen_ia32_setup_bool(set, PFM_GEN_IA32_SEL_INV, args->inv_arg, "inv");
	pfmon_gen_ia32_setup_bool(set, PFM_GEN_IA32_SEL_ANYTHR, args->anythr_arg, "anythr");

	return 0;
}

static void
pfmon_gen_ia32_verify_cmdline(int argc, char **argv)
{
	if (options.dfl_plm & (PFM_PLM1|PFM_PLM2))
		fatal_error("-1 or -2 privilege levels are not supported by this PMU model\n");

	if (options.opt_data_trigger_ro)
		fatal_error("the --trigger-data-ro option is not supported by this processor\n");
}

pfmon_support_t pfmon_gen_ia32={
	.name				= "Intel architectural PMU",
	.pmu_type			= PFMLIB_GEN_IA32_PMU,
	.generic_pmu_type		= PFMLIB_NO_PMU,
	.pfmon_initialize		= pfmon_gen_ia32_initialize,		
	.pfmon_usage			= pfmon_gen_ia32_usage,	
	.pfmon_parse_options		= pfmon_gen_ia32_parse_options,
	.pfmon_setup			= pfmon_gen_ia32_setup,
	.pfmon_verify_cmdline		= pfmon_gen_ia32_verify_cmdline,
	.sz_mod_args			= sizeof(pfmon_gen_ia32_args_t),
	.sz_mod_inp			= sizeof(pfmlib_gen_ia32_input_param_t)
};

/*
 * Core Duo/Core Solo implement architectural PMU, with
 * additional events and unit masks
 */
pfmon_support_t pfmon_coreduo={
	.name				= "Intel Core Solo/Core Duo",
	.pmu_type			= PFMLIB_COREDUO_PMU,
	.generic_pmu_type		= PFMLIB_NO_PMU,
	.pfmon_initialize		= pfmon_gen_ia32_initialize,		
	.pfmon_usage			= pfmon_gen_ia32_usage,	
	.pfmon_parse_options		= pfmon_gen_ia32_parse_options,
	.pfmon_setup			= pfmon_gen_ia32_setup,
	.pfmon_verify_cmdline		= pfmon_gen_ia32_verify_cmdline,
	.sz_mod_args			= sizeof(pfmon_gen_ia32_args_t),
	.sz_mod_inp			= sizeof(pfmlib_gen_ia32_input_param_t)
};
