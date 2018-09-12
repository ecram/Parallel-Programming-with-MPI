/*
 * pfdbg.c: toggle the debug behavior of the kernel perfmon system
 *
 * Copyright (c) 2001-2006 Hewlett-Packard Development Company, L.P.
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
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <perfmon/perfmon.h>

#define PFDBG_VERSION	"0.04"
#define VERSION_22 (2U<<16|2U)

static int debug = -1, debug_ovfl = -1, printk, reset_stat;
static int pfm_version;

static struct option cmd_options[]={
	{ "version", 0, 0, 1},
	{ "help", 0, 0, 2},

	{ "on", 0, &debug, 1},
	{ "off", 0, &debug, 0},
	{ "ovfl-on", 0, &debug_ovfl, 1},
	{ "ovfl-off", 0, &debug_ovfl, 0},
	{ "reset-stats", 0, &reset_stat, 1},
	{ "printk-off", 0, &printk, 1},
	{ "help", 0, 0, 3},
	{ 0, 0, 0, 0}
};

static void fatal_error(char *fmt,...) __attribute__((noreturn));

static void
warning(char *fmt, ...) 
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static void
fatal_error(char *fmt, ...) 
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	exit(1);
}

/*
 * on IA-64 with perfmon-2.0, we used to get the information from /proc/perfmon
 */
int
pfm_get_version_legacy(void)
{
	FILE *fp;
	char *buf = NULL;
	char *p, *s;
	size_t len = 0;
	int ret;


	fp = fopen("/proc/perfmon", "r");
	if (fp == NULL)
		return -1;

	ret = getline(&buf, &len, fp);
	if (ret < 0) fatal_error("cannot extract perfmon version\n");

	if (strncmp(buf, "perfmon version", 15)) goto invalid_format;

	p = strchr(buf+15, ':');
	if (p == NULL) goto invalid_format;
	s = p+1;
	p = strchr(s, '.');
	if (p == NULL) goto invalid_format;
	*p = '\0';
	pfm_version = atoi(s) << 16;
	s  = p+1;
	pfm_version |= atoi(s);

	free(buf);

	fclose(fp);

	return 0;

invalid_format:
	fatal_error("cannot parse perfmon version number\n");
	/* never reached */
	return -1;
}

void
pfm_get_version(void)
{
	FILE *fp;
	char *buf = NULL;
	size_t len = 0;
	char *p;
	int ret;

	if (pfm_get_version_legacy() == 0)
		return;

	fp = fopen("/sys/kernel/perfmon/version", "r");
	if (fp == NULL)
		fatal_error("host kernel does not have perfmon support\n");

	ret = getline(&buf, &len, fp);
	if (ret < 0) fatal_error("cannot extract perfmon version\n");

	p = strchr(buf, '.');
	if (p == NULL) goto invalid_format;
	*p = '\0';
	pfm_version = atoi(buf) << 16;
	p++;
	pfm_version |= atoi(p);

	free(buf);

	fclose(fp);

	return;

invalid_format:
	fatal_error("cannot parse perfmon version number\n");
}

void
usage(char **argv)
{
	printf("Usage: %s [OPTIONS]... COMMAND\n", argv[0]);

	printf(	"-h, --help\tdisplay this help and exit\n"
		"--version\toutput version information and exit\n"
		"--on\t\tturn on debugging\n"
		"--off\t\tturn off debugging\n"
		"--ovfl-on\tturn on overflow debugging\n"
		"--ovfl-off\tturn off overflow debugging\n"
		"-printk-off\t\tturn off printk throttling\n"
	);
}

#ifdef __ia64__
/*
 * for perfmon v2.0
 */
int
debug_on_off_old(int mode)
{
	return perfmonctl(0, PFM_DEBUG, &mode, 1);
}
#endif

static int
write_proc(char *file, int mode)
{
	char mode_str[8];
	int len, fd, ret = 0;

	len = sprintf(mode_str, "%d", mode);

	fd = open(file, O_WRONLY);
	if (fd == -1)
		return -1;

	if (write(fd, mode_str, len) < len) {
		warning("cannot write to %s: %s\n", file, strerror(errno));
		ret = -1;
	}
	close(fd);

	return ret;
}

static int
debug_on_off(int mode)
{
	if (pfm_version >= VERSION_22)
		return write_proc("/sys/kernel/perfmon/debug", mode);
#ifdef __ia64__
	return debug_on_off_old(mode);
#endif
	return 0;
}

static int
debug_ovfl_on_off(int mode)
{
	if (pfm_version >= VERSION_22)
		return write_proc("/sys/kernel/perfmon/debug_ovfl", mode);
#ifdef __ia64__
	return write_proc("/proc/sys/kernel/perfmon/debug_ovfl", mode);
#endif
	return 0;
}

static int
reset_stats(void)
{
	if (pfm_version >= VERSION_22)
		return write_proc("/sys/kernel/perfmon/reset_stats", 1);
#ifdef __ia64__
	return debug_on_off(0);
#endif
	return 0;
}

int
main(int argc, char **argv)
{
	int c, ret;

	pfm_get_version();

	while ((c=getopt_long(argc, argv,"vVhk", cmd_options, 0)) != -1) {
		switch(c) {
			case   0:
				continue;
			case 1:
			case 'v':
			case 'V':
				printf("Version %s Date: %s\n", PFDBG_VERSION, __DATE__);
				printf("perfmon version: %u.%u\n",
						pfm_version>>16 & 0xffff,
						pfm_version & 0xffff);
				exit(0);
			case 'h':
			case   2:
				usage(argv);
				break;
			case 'k':
				printk = 1;
				break;
			default:
				fatal_error("Unknown option\n");
		}
	}
	if (printk)
		write_proc("/proc/sys/kernel/printk_ratelimit", 0);

	if (debug > -1)
		debug_on_off(debug);

	if (debug_ovfl > -1) {
		ret = debug_ovfl_on_off(debug_ovfl);
		if (ret) {
			if (debug == -1)
				debug = 0;
			if (debug_ovfl == 1)
				debug |= 1<<1;
			debug_on_off(debug);
		}
	}
	if (reset_stat)
		reset_stats();
	return 0;
}
