/*
 * pfmon_intel_nhm.c - Intel Nehalem PMU support
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
#include "pfmon.h"
#include <fcntl.h>
#include <perfmon/pfmlib_intel_nhm.h>

typedef struct {
	char *cnt_mask_arg;
	char *inv_arg;
	char *edge_arg;
	char *occ_arg;
	char *anythr_arg;
	char *ld_lat_thres_arg;
} pfmon_nhm_args_t;

typedef struct {
	pfmlib_nhm_input_param_t	inp;
	pfmon_nhm_args_t		args;
} pfmon_nhm_param_t;

static void
pfmon_nhm_setup_cnt_mask(pfmon_event_set_t *set)
{
	pfmlib_nhm_input_param_t *param = set->setup->mod_inp;
	pfmon_nhm_args_t *args;
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
			param->pfp_nhm_counters[i].cnt_mask = 0;
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

		param->pfp_nhm_counters[cnt++].cnt_mask = (unsigned int)l;

		cnt_mask_str = p;
	}
	return;
invalid:
	fatal_error("event %d: counter mask must be in [0-256)\n", cnt);
too_many:
	fatal_error("too many counter masks specified\n");
}

static void
pfmon_nhm_setup_bool(pfmon_event_set_t *set, int flag, char *str, char *name)
{
	pfmlib_nhm_input_param_t *param = set->setup->mod_inp;
	pfmon_nhm_args_t *args;
	char *p, c;
	unsigned int cnt=0;
	unsigned int i;

	args = set->setup->mod_args;

	/*
	 * by default, clear the flag
	 */
	if (str == NULL) {
		for (i=0; i < set->setup->event_count; i++)
			param->pfp_nhm_counters[i].flags &= ~flag;
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
			param->pfp_nhm_counters[cnt].flags |= flag;	
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
pfmon_nhm_prepare_registers(pfmon_event_set_t *set)
{
	pfmlib_nhm_input_param_t *param = set->setup->mod_inp;

	if (param->pfp_nhm_pebs.pebs_used)
		set->setup->master_pc[0].reg_flags |= PFM_REGFL_NO_EMUL64;

	return 0;
}

static void
pfmon_nhm_usage(void)
{
	printf( "--counter-mask=msk1,msk2,...\t\tSet event counter mask (0,1,2,3).\n"
		"--inv-mask=i1,i2,...\t\t\tSet event inverse counter mask\n"
		"\t\t\t\t\t (y/n,0/1).\n"
		"--edge-mask=e1,e2,...\t\t\tSet event edge detect (y/n,0/1).\n"
		"--anythread-mask=e1,e2,...\t\tSet anythread filter (y/n,0/1).\n"
		"--occ-mask=e1,e2,...\t\t\tSet occupancy reset filter (y/n,0/1).\n"
		"--ld-lat-threshold=l\t\t\tSet load latency threshold to l cycles.\n"
	);
}

/*
 * Intel Core options
 *
 * 000-255   reserved for generic options
 * 400-499   reserved for PMU specific options
 * 500-599   reserved for format specific options
 */
static struct option cmd_nhm_options[]={
	{ "counter-mask", 1, 0, 400 },
	{ "inv-mask", 1, 0, 401 },
	{ "edge-mask", 1, 0, 402 },
	{ "ld-lat-threshold", 1, 0, 403 },
	{ "anythread-mask", 1, 0, 404 },
	{ "occ-mask", 1, 0, 405 },
	{ 0, 0, 0, 0}
};

static int
pfmon_nhm_initialize(void)
{
	int r;

	r = pfmon_register_options(cmd_nhm_options, sizeof(cmd_nhm_options));
	if (r == -1) return -1;

	return 0;
}

/*
 * 0  means we understood the option
 * -1 unknown option
 */
static int
pfmon_nhm_parse_options(int code, char *optarg)
{
	pfmon_nhm_args_t *nhm_args;

	nhm_args = options.last_set->setup->mod_args;

	switch(code) {
		case  400:
			if (nhm_args->cnt_mask_arg)
				fatal_error("counter masks already defined\n");
			nhm_args->cnt_mask_arg = optarg;
			break;
		case  401:
			if (nhm_args->inv_arg)
				fatal_error("inverse mask already defined\n");
			nhm_args->inv_arg = optarg;
			break;
		case  402:
			if (nhm_args->edge_arg)
				fatal_error("edge detect mask already defined\n");
			nhm_args->edge_arg = optarg;
			break;
		case  403:
			if (nhm_args->ld_lat_thres_arg)
				fatal_error("load latency threshold already defined\n");
			nhm_args->ld_lat_thres_arg = optarg;
			break;
		case  404:
			if (nhm_args->anythr_arg)
				fatal_error("anythread parameters already defined\n");
			nhm_args->anythr_arg = optarg;
			break;
		case  405:
			if (nhm_args->occ_arg)
				fatal_error("occupancy reset parameters already defined\n");
			nhm_args->occ_arg = optarg;
			break;
		default:
			return -1;
	}
	return 0;
}

static void
pfmon_nhm_setup_pebs(pfmon_event_set_t *set)
{
	pfmlib_nhm_input_param_t *param = set->setup->mod_inp;
	param->pfp_nhm_pebs.pebs_used = 1;
}

static void
pfmon_nhm_setup_ld_lat(pfmon_event_set_t *set)
{
	pfmlib_nhm_input_param_t *param = set->setup->mod_inp;
	pfmon_nhm_args_t *args;
	unsigned int lat;

	args = set->setup->mod_args;

	if (args->ld_lat_thres_arg)
		fatal_error("load latency threshold not yet supported\n");

	return;
#if 0
	if (!(options.smpl_mod->flags & PFMON_SMPL_MOD_FL_LD_LAT))
		return;
#endif
	args = set->setup->mod_args;

	if (!args->ld_lat_thres_arg)
		fatal_error("load latency threshold must be used with this sampling module, use --ld-lat-threshold\n");

	lat = atoi(args->ld_lat_thres_arg);
	if (lat < 3 || lat > 0xffff)
		fatal_error("load latency threshold must be in [3:65535]\n");

	param->pfp_nhm_pebs.ld_lat_thres  = lat;
}

static void
pfmon_nhm_show_socket_map(void)
{
        int fd, i, j, s;
        ssize_t r;
        char filename[64], str[8];

        warning("there can only be one session per processor socket "
                "with uncore events, use --cpu-list to specify one "
                "core/socket. Processor socket map is:\n");
        for(i=0, j= 0; j < options.selected_cpus; i++) {
                if (!pfmon_bitmask_isset(&options.virt_cpu_mask, i))
                        continue;
                j++;

                sprintf(filename, "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", i);
                r = -1;
                fd = open(filename, O_RDONLY);
                if (fd > -1) {
                        r = read(fd, str, 8);
                        close(fd);
                }

                if (r < 0) {
                        warning("CPU%-4d : cannot determine socket\n", i);
                        continue;
                }

                s = atoi(str);

                warning("CPU%-4d : socket %d\n", i, s);
        }
        fatal_error("");
}

static void
pfmon_nhm_uncore_socket_check(void)
{
	int fd, i, j, l, s, n = 0;
	int *sockets;
	ssize_t r;
	char filename[64], str[8];

	/*
 	 * approximate number of sockets using number of cores
 	 * we oversize but that is the easiest thing we can do
 	 */
	sockets = malloc(options.selected_cpus * sizeof(int));
	if (!sockets)
		return; /* let the kernel check */

	for(i=0, j=0; j < options.selected_cpus; i++) {

		if (!pfmon_bitmask_isset(&options.virt_cpu_mask, i))
			continue;

		/*
 		 * assume topology and /sys are available
 		 * worst can open fails, and the kernel will check
 		 */
		sprintf(filename, "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", i);
		fd = open(filename, O_RDONLY);
		if (fd == -1)
			return; /* let the kernel check */

		r = read(fd, str, 8);

		close(fd);

		if (r < 0)
			return; /* let the kernel check */

		/*
		 * extract socket id
		 */
		s = atoi(str);

		/*
 		 * check if we have already seen this socket id
 		 * cannot use bitmask here because socket id can vary
 		 * from 0 to 65535, so instead use a simple looked table
 		 */
		for(l=0; l < n; l++)
			if (sockets[l] == s)
				pfmon_nhm_show_socket_map();
		sockets[n] = s;
		n++;
		j++;
	}
	free(sockets);
}

/*
 * check offcore_response_0
 * all unit masks must be the same
 */
static void
pfm_nhm_offcore_check(pfmlib_event_t *prev_e, pfmlib_event_t *e)
{
	unsigned int u[2], c;
	int i, k;
	pfmlib_event_t *ee[2];
	int ret;


	/*
 	 * those events need umask, let libpfm report the error
 	 * pfmon will print the missng umasks
 	 */
	if (! (e->num_masks && prev_e->num_masks))
		return;

	u[0] = u[1] = 0;
	ee[0] = prev_e;
	ee[1] = e;

	for(k=0; k < 2; k++) {
		for(i=0; i < ee[k]->num_masks; i++) {
			ret = pfm_get_event_mask_code(ee[k]->event, ee[k]->unit_masks[i], &c);
			/*
			 * let libpfm catch the invalid umask
			 */
			if (ret != PFMLIB_SUCCESS)
				return;

			u[k] |= c;
		}
	}

	if (u[0] != u[1])
		fatal_error("when the OFFCORE_RESPONSE_0 event is used multiple times, all unit masks must be identical as they are stored in another shared register\n");
}


/*
 * run a series of validation on event set and print
 * more useful messages than what libpfm or the kernel
 * can do
 */
static void
pfmon_nhm_setup_check(pfmon_event_set_t *set)
{
	pfmlib_nhm_input_param_t *args;
	unsigned int i, dfl_plm;
	pfmlib_event_t *e, unc, off;
	pfmlib_event_t *prev_e = NULL;

	args = set->setup->mod_inp;

	dfl_plm = set->setup->inp.pfp_dfl_plm;

	memset(&unc, 0, sizeof(unc));
	pfm_find_full_event("unc_clk_unhalted", &unc);

	memset(&off, 0, sizeof(off));
	pfm_find_full_event("offcore_response_0", &off);

	/*
 	 * validate events. The following constraints apply:
 	 *
 	 * - uncore event are restricted to system-wide (one per socket)
 	 * - uncore events have no priv level filtering so enforce -u -k
 	 *   to ensure user if aware of limitation. Can be done globally
 	 *   or for each individual event
 	 * - anythr only for core counters
 	 * - occupancy only for uncore counters
 	 */
	for(i=0; i < set->setup->event_count; i++) {

		e = &set->setup->inp.pfp_events[i];

		if (pfm_nhm_is_uncore(e)) {

			if (!options.opt_syst_wide)
				fatal_error("uncore events require system-wide mode\n");

			if (e->plm != (PFM_PLM0|PFM_PLM3) && dfl_plm != (PFM_PLM0|PFM_PLM3))
				fatal_error("must use -u -k or --priv-level=uk for uncore events\n");

			if (args && args->pfp_nhm_counters[i].flags & PFM_NHM_SEL_ANYTHR)
				fatal_error("anythread filter only available for core events\n");

			if (e->event == unc.event && args &&
			    (args->pfp_nhm_counters[i].flags || args->pfp_nhm_counters[i].cnt_mask))
				fatal_error("UNC_CLK_UNHALTED does not take any filter\n");

			/*
			 * check socket restriction, only do it once
			 * use set0 to identify first call
			 */
			if (set->setup->id == 0)
				pfmon_nhm_uncore_socket_check();

		} else {
			if (args && args->pfp_nhm_counters[i].flags & PFM_NHM_SEL_OCC_RST)
				fatal_error("occupancy reset filter only available for uncore events\n");

			if (e->event == off.event) {
				if (prev_e)
					pfm_nhm_offcore_check(prev_e, e);
				prev_e = e;
			}
		}
	}
}

static int
pfmon_nhm_setup(pfmon_event_set_t *set)
{
	pfmon_nhm_args_t *args;

	args = set->setup->mod_args;
	if (!args) 
		goto end;

	if (options.smpl_mod->flags & PFMON_SMPL_MOD_FL_PEBS) {

		if (set->setup->event_count > 1)
			fatal_error("with PEBS sampling, only one event must be used\n");

		if (!pfm_nhm_is_pebs(set->setup->inp.pfp_events))
			fatal_error("event must be PEBS capable, check with pfmon -i\n");

		pfmon_nhm_setup_pebs(set);
	}

	pfmon_nhm_setup_cnt_mask(set);

	pfmon_nhm_setup_bool(set, PFM_NHM_SEL_EDGE, args->edge_arg, "edge");
	pfmon_nhm_setup_bool(set, PFM_NHM_SEL_INV, args->inv_arg, "inv");
	pfmon_nhm_setup_bool(set, PFM_NHM_SEL_ANYTHR, args->anythr_arg, "anythr");
	pfmon_nhm_setup_bool(set, PFM_NHM_SEL_OCC_RST, args->occ_arg, "occ");

	pfmon_nhm_setup_ld_lat(set);
end:
	pfmon_nhm_setup_check(set);
	return 0;
}

static void
pfmon_nhm_verify_cmdline(int argc, char **argv)
{
	if (options.dfl_plm & (PFM_PLM1|PFM_PLM2))
		fatal_error("-1 or -2 privilege levels are not supported by this PMU model\n");

	if (options.opt_data_trigger_ro)
		fatal_error("the --trigger-data-ro option is not supported by this processor\n");

}

static void
pfmon_nhm_show_event_info(unsigned int idx)
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
			if (pfm_nhm_is_pebs(&e)) {
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
		if (pfm_nhm_is_pebs(&e))
			puts("Yes");
		else
			puts("No");
	}

	printf("Uncore   : ");
	if (pfm_nhm_is_uncore(&e))
		puts("Yes");
	else
		puts("No");
}

/*
 * Intel Nehalem-based processors
 */
pfmon_support_t pfmon_intel_nhm={
	.name				= "Intel Nehalem",
	.pmu_type			= PFMLIB_INTEL_NHM_PMU,
	.generic_pmu_type		= PFMLIB_NO_PMU,
	.pfmon_initialize		= pfmon_nhm_initialize,		
	.pfmon_usage			= pfmon_nhm_usage,	
	.pfmon_parse_options		= pfmon_nhm_parse_options,
	.pfmon_setup			= pfmon_nhm_setup,
	.pfmon_prepare_registers	= pfmon_nhm_prepare_registers,
	.pfmon_verify_cmdline		= pfmon_nhm_verify_cmdline,
	.pfmon_show_event_info		= pfmon_nhm_show_event_info,
	.sz_mod_args			= sizeof(pfmon_nhm_args_t),
	.sz_mod_inp			= sizeof(pfmlib_nhm_input_param_t),
};
