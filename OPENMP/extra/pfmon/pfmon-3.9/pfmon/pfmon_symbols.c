/*
 * pfmon_symbols.c  - management of symbol tables
 *
 * Copyright (c) 2002-2006 Hewlett-Packard Development Company, L.P.
 * Contributed by Stephane Eranian <eranian@hpl.hp.com>
 * Parts contributed by Andrzej Nowak (CERN)
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
 *
 *                   module_map_list_t
 *                  -------------------
 * sdesc ->syms -->|               map |--\
 *                  -------------------   |
 *                                        |
 *          module_map_t                  |
 *       ----------------- <--------------/
 *      |            path |
 *      |         version |
 *      |             mod |     
 *      |             pid |  --------------------------------------------------
 *       -----------------   |ALL STRUCTS ARE SHARED BETWEEN SDESC IN THIS BOX|
 *      |            path |  |                                                |
 *      |         version |  |    module_symbols_t                            |
 *      |             mod |---> -------------------        symbol_t           |
 *      |             pid |  | |           sym_tab | -->  ----------          |
 *       -----------------   | |             nsyms |     |    name  |--> foo  |
 *      |           NULL  |  | |                   |     |   value  |         |
 *      |             -1  |  | |                   |      ----------          |
 *       -----------------   |  --------------------     |    name  |--> bar  |
 *                           |                           |   value  |         |
 *                           |                            ----------          |
 *                           --------------------------------------------------
 *
 * module_map_list_t is shared between threads of the same process
 * sym_tab = text and data symbols
 * nsyms = number of text and data symbols
 */
#include <ctype.h>
#include <fcntl.h>
#include <libelf.h>

#include "pfmon.h"

#define PFMON_KALLSYMS		"/proc/kallsyms"

#define PFMON_NM_CMD    "sh -c \"nm -f sysv -C --defined -D %s 2>/dev/null; nm -a -f sysv -C --defined %s 2>/dev/null\" | sort -t'|' -k 2 | uniq"
#define PFMON_READELF_CMD "sh -c \"readelf -l %s 2>/dev/null | fgrep LOAD 2>/dev/null | head -1 2>/dev/null\" "
#define NTOK 8

#define KERNEL_MOD_ID0	(~0UL)
#define KERNEL_MOD_ID1	(~0UL)

/* description of one symbol */
typedef struct {
	char			*name;		/* symbol name */
	uint64_t		value;		/* symbol address */
	uint64_t		size;		/* optional symbol size */
} symbol_t;

/* description of one module text and data symbol tables */
typedef struct _module_symbols_t {
	struct _module_symbols_t *next;		/* linked list */
	struct _module_symbols_t *prev;		/* linked list */
	uint64_t		id[2];		/* unique identifier */
	uint64_t		pt_load;	/* pt_load base address */
	symbol_t		*sym_tab;	/* symbol table */
	unsigned long		nsyms;		/* number of text symbols */
	unsigned long		refcnt;		/* reference count */
} module_symbols_t;

/* description of one line in /proc/PID/maps */
typedef struct {
	module_symbols_t	*mod;		/* which module_symbols_t */
	unsigned int		version;	/* snapshot version (per task) */
	uint64_t		base;		/* base addresses (code, data) */
	uint64_t		ref_base;	/* common base for same module */
	uint64_t		id[2];		/* module unique identifier */
	uint64_t		max;		/* max addr (code, data) */
	char			*path;		/* file path */
	pid_t			pid;		/* owner */
} module_map_t;

typedef struct {
	pthread_mutex_t lock;
	unsigned int refcnt;
	unsigned int version;	/* current module map generation of /prod/PID/maps snapshot */
	module_map_t *map;
} module_map_list_t;


static module_symbols_t *module_list;
static pthread_mutex_t  module_list_lock = PTHREAD_MUTEX_INITIALIZER;

void show_module_map(pfmon_sdesc_t *sdesc);

void *
syms_get(pfmon_sdesc_t *sdesc)
{
	module_map_list_t *list = sdesc->syms;
	if (sdesc->syms) {
		pthread_mutex_lock(&list->lock);
		list->refcnt++;
		pthread_mutex_unlock(&list->lock);
	}
	return sdesc->syms;
}

unsigned int
syms_get_version(pfmon_sdesc_t *sdesc)
{
	module_map_list_t *list = sdesc->syms;
	return list ? list->version : 0;
}

/*
 * find module_symbol by id
 * caller must have module_list_lock held
 *
 * by having caller control lock, we make it possible to
 * atomically find and add new module_symbols
 */
static module_symbols_t *
find_module_syms(uint64_t *id)
{
	module_symbols_t *p;
	
	for(p = module_list; p ; p = p ->next) {
		if (p->id[0] == id[0] && p->id[1] == id[1]) {
			p->refcnt++;
			break;
		}
	}
	return p;
}

/*
 * add module_symbol p to module list
 * caller must have module_list_lock held
 */
static void
add_module_syms(module_symbols_t *p)
{
	if (module_list)
		module_list->prev = p;

	p->next = module_list;
	p->prev = NULL;

	module_list = p;
}

/*
 * qsort expects:
 *      < 0 if a < b 
 *      > 0 if a > b
 *      = 0 if a = b
 * do not use simpler substraction because of type casting
 */
static int
symcmp(const void *a, const void *b)
{
        symbol_t *ap = (symbol_t *)a;
        symbol_t *bp = (symbol_t *)b;

        /*
	 * sort by value first,
	 * then by size,
	 * then by reverse sort by name (so __prefix are at teh end)
	 */
        if (ap->value == bp->value) {
                if (ap->size == bp->size)
                        return strcmp(bp->name, ap->name);
                if (ap->size == bp->size)
                        return 0;
                return ap->size > bp->size ? 1 : -1;
        }
        return ap->value > bp->value ? 1 : -1;
}

/*
 * symbol.size == 0 can only happen for the last
 * symbol of a module.
 * In that case we estimate the size using the mapping
 * range it belong to. We may overestimate but that is
 * fine because we are unlikely to get samples in the
 * overestimated section
 */
static uint64_t
sym_size(module_map_t *map, module_symbols_t *mod, unsigned long i)
{
	uint64_t addr;

	if (mod->sym_tab[i].size)
		return mod->sym_tab[i].size;

	addr = map->ref_base + mod->sym_tab[i].value - mod->pt_load;
	return map->max - addr;
}

/*
 * add kernel module_map in p for sdesc
 */
static int
add_kernel_module_map(pfmon_sdesc_t *sdesc, unsigned int version, module_map_t *p)
{
	module_symbols_t *mod;
	uint64_t id[2];

	id[0] = KERNEL_MOD_ID0;
	id[1] = KERNEL_MOD_ID1;
	mod = find_module_syms(id);
	if (!mod)
		return -1;

	DPRINT(("[%d] ADD KERNEL refcnt=%lu\n", sdesc->tid, mod->refcnt));

	/* add kernel to map */
	p->mod = mod;
	p->path = strdup("kernel");
	p->version = version;
	p->pid = sdesc->pid;
	p->id[0] = KERNEL_MOD_ID0;
	p->id[1] = KERNEL_MOD_ID1;

	p->ref_base = mod->sym_tab[0].value;
	p->base = mod->sym_tab[0].value;

	p->max  = mod->sym_tab[mod->nsyms-1].value
		+ mod->sym_tab[mod->nsyms-1].size;

	vbprintf("[%u] loaded map version %d, 0x%"PRIx64"-0x%"PRIx64" ref=0x%"PRIx64" %s\n",
		sdesc->pid,
		version,
		p->base,
		p->max,
		p->ref_base,
		p->path);

	return 0;
}

/*
 * extract first PT_LOAD segment load address from ELF image
 * The value is needed to offset symbol addresses collected
 * from symbol table
 */
static int
extract_module_ptload(const char *filename, uint64_t *addr)
{
	FILE *fp;
	char *buf;
	size_t sz;
	int ret = -1;

	sz = strlen(filename) + strlen(PFMON_READELF_CMD) + 1;

	buf = malloc(sz);
	if (!buf)
		return -1;

	sprintf(buf, PFMON_READELF_CMD, filename);

	fp = popen(buf, "r");
	if (!fp)
		goto out;

	ret = getline(&buf, &sz, fp);
	if (ret == EOF)
		goto out;

	sscanf(buf, "%*s 0x%*x 0x%"PRIx64, addr);


	ret = 0;
out:
	if (fp)
		fclose(fp);
	free(buf);
	return ret;
}

/*
 * correct text sizes if:
 * 	- size was missing from symbol table
 * 	- reported size is bigger than computed size (nesting)
 */
static void
sort_and_size(symbol_t *tab, unsigned long nsyms, int dosz)
{
	uint64_t sss;
	unsigned int i;

 	/*
 	 * sort addresses
 	 */
	qsort(tab, nsyms, sizeof(symbol_t), symcmp);
	
	if (!dosz)
		return;

	/*
 	 * correct text sizes if:
 	 * 	- size was missing from symbol table
 	 * 	- reported size is bigger them computed size (nesting)
 	 */
	for(i=1; i < nsyms; i++) {
		sss  = tab[i].value - tab[i-1].value;
		if (sss < tab[i-1].size)
			DPRINT(("==> overlapping symbols : 0x%"PRIx64" %"PRIu64" %s 0x%"PRIx64" %s\n", tab[i-1].value, tab[i-1].size, tab[i-1].name, tab[i].value, tab[i].name));
		if (tab[i-1].size == 0) {
			DPRINT(("size adjust : %"PRIu64" %s\n", sss, tab[i-1].name));
			tab[i-1].size  = sss;
		}
	}
}

/*
 * load symbols from image file using nm
 */
static int
load_module_syms(const char *filename, module_symbols_t *mod)
{
	FILE *fp;
	size_t max_syms;
	char *buf, *line, *saved_line, *p, *q;
	char *nm_toks[NTOK];
	size_t sz, ssz;
	unsigned long i;
	uint64_t addr;
        int breakflag;

	if (extract_module_ptload(filename, &mod->pt_load))
		return -1;

	buf = malloc(2 * strlen(filename) + strlen(PFMON_NM_CMD)+1);
	if (!buf)
		return -1;

	sprintf(buf, PFMON_NM_CMD, filename, filename);

	fp = popen(buf, "r");
	free(buf);
	if (!fp)
		return -1;

	max_syms = 2048;
	line = NULL, sz = 0; saved_line = NULL;
	while (getline(&line, &sz, fp) != EOF) {
		p = q = line; i = 0;
		nm_toks[6] = NULL;
		while(i < NTOK && (p = strtok_r(q, "|", &saved_line))) {
			if (*p == '\n' || *p == '\0')
				break;
			nm_toks[i++] = p;
			q = NULL;
		}
		if (!nm_toks[6])
			continue;

		/*
 		 * skip undesirable symbols types
 		 */
		p = nm_toks[3];
		while (isspace(*p)) p++;

		//if (!strcmp(p, "FILE") ||!strcmp(p, "SECTION"))
		if (!strcmp(p, "FILE"))
			continue;

		addr = strtoull(nm_toks[1], NULL, 16);
		ssz = strtoul(nm_toks[4], NULL, 16);

		if (!mod->nsyms || mod->nsyms == max_syms) {
			/* exponential growth */
			max_syms <<=1;
			mod->sym_tab = realloc(mod->sym_tab, max_syms * sizeof(symbol_t));
			if (!mod->sym_tab)
				goto error;
		}

                p = strchr(nm_toks[0], '|');
                if (p)
                        *p = '\0';

                for(i=strlen(nm_toks[0]); nm_toks[0][i-1] == ' '; i--);
                nm_toks[0][i] = '\0';

                breakflag = 0;
                if(mod->nsyms && (mod->sym_tab[mod->nsyms-1].value == addr)) {
                        if(strncmp(mod->sym_tab[mod->nsyms-1].name, "_Z", 2)) {
                                //DPRINT(("skipping %s/%"PRIx64" because %s/%"PRIx64" already inside\n", nm_toks[0], addr, mod->sym_tab[stype][index].name, mod->sym_tab[stype][index].value));
                                breakflag = 1;
                        }
                }

		if(breakflag == 1)
			continue;

		mod->sym_tab[mod->nsyms].name  = strdup(nm_toks[0]);
		mod->sym_tab[mod->nsyms].value = addr;
		mod->sym_tab[mod->nsyms].size  = ssz;
		mod->nsyms++;
	}

	if (line)
		free(line);

	fclose(fp);

	sort_and_size(mod->sym_tab, mod->nsyms, 0);
	vbprintf("loaded %lu symbols, load offset 0x%"PRIx64", %s\n",
	 	mod->nsyms,
		mod->pt_load,
		filename);

	return 0;
error:
	fclose(fp);
	free(line);
	return -1;
}

/*
 * load kernel symbols using /proc/kallsyms.
 * This file does not contains kernel data symbols but includes code/data
 * symbols from modules. Code symbol size is not provided.
 */
static int
load_kallsyms_symbols(module_symbols_t *mod)
{
	FILE *fp;
	char *s, *str_addr, *sym_start, *mod_start, *endptr;
	uint64_t sym_len, mod_len;
	uint64_t line = 1UL;
	uint64_t addr;
	uint64_t sym_count;
	size_t sz;
	char *line_str;
	char addr_str[24]; /* cannot be more than 16+2 (for 0x) */
	int type, ret;

	fp = fopen(PFMON_KALLSYMS, "r");
	if (fp == NULL) {
		DPRINT(("file %s not found\n", PFMON_KALLSYMS));
		return -1;
	}

	/*
	 * allocate a default-sized symbol table 
	 */
	sym_count = 8192;
	mod->nsyms = 0;
	mod->sym_tab = NULL;

	line_str = NULL; sz = 0;
	ret = 0;
	while(getline(&line_str, &sz, fp)>0) {

		s = line_str;

		while(*s != ' ' && *s !='\0')
			s++;

		if (*s == '\0')
			break;

		if (s-line_str > 16+2) {
			ret = -1;
			break;
		}

		strncpy(addr_str, line_str, s-line_str);
		addr_str[s-line_str] = '\0';

		/* point to object type */
		s++;
		type = tolower(*s);

		/*
 		 * drop non data and text symbols
 		 */
		if (type != 's' && type != 'd' && type != 'D'
		    && type != 't' && type != 'T')
			continue;


		/* look for space separator */
		s++;
		if (*s != ' ') {
			ret = -1;
			break;
		}

		if (!mod->nsyms || mod->nsyms == sym_count) {
			/* exponential growth */
			sym_count <<=1;
			mod->sym_tab = (symbol_t *)realloc(mod->sym_tab, sym_count*sizeof(symbol_t));
			if (!mod->sym_tab) {
				ret = -1;
				break;
			}
		}

		/* compute address */
		endptr = NULL;
		addr  = strtoull(addr_str, &endptr, 16);
		if (*endptr != '\0') {
			ret = -1;
			break;
		}
		/* skip aliased symbols */
		if (mod->nsyms && addr == mod->sym_tab[mod->nsyms-1].value) {
			while (*s++ != '\n');
			line++;
			continue;
		}
			
		/* advance to symbol name */
		sym_start = ++s;

		/* look for end-of-string */
		while(*s != '\n' && *s != '\0' && *s != ' ' && *s != '\t')
			s++;

		if (*s == '\0') {
			ret = -1;
			break;
		}
		sym_len = s - sym_start;

		/* check for module */
		while(*s != '\n' && *s != '\0' && *s != '[')
			s++;

		/* symbol belongs to a kernel module */
		if (*s == '[') {
			mod_start = s++;
			while(*s != '\n' && *s != '\0' && *s != ']')
				s++;
			if (*s != ']') {
				ret = -1;
				break;
			}
			mod_len = s - mod_start + 1;
		} else {
			mod_len   = 0;
			mod_start = NULL;
		}

		line++;

		/*
		 * place string in our memory pool
		 * +1 for '\0'
		 */
		//str_addr = place_str(mod_len + sym_len + 1);
		str_addr = malloc(mod_len + sym_len + 1);
		if (str_addr == NULL) {
			ret = -1;
			break;
		}

		strncpy(str_addr, sym_start, sym_len);
		if (mod_len)
			strncpy(str_addr+sym_len, mod_start, mod_len);
		str_addr[sym_len+mod_len] = '\0';

		mod->sym_tab[mod->nsyms].value = addr;
    		mod->sym_tab[mod->nsyms].size  = 0; /* computed later */
    		mod->sym_tab[mod->nsyms].name  = str_addr;
		mod->nsyms++;
	}
	if (line_str)
		free(line_str);

	/*
	 * normally a kallsyms is already sorted
	 * so we should not have to do this
	 */
	if (ret == 0) {
		sort_and_size(mod->sym_tab, mod->nsyms, 1);
		/*
		 * kernel symbol address are absolute
		 */
		mod->pt_load = mod->sym_tab[0].value;

		vbprintf("loaded %lu symbols, load offset 0x%"PRIx64", kernel\n",
	 		mod->nsyms,
			mod->pt_load);
	}
	fclose(fp);
	return ret;
}

int
load_kernel_syms(void)
{
	module_symbols_t *mod;
	char *from;
	int ret;

	/*
 	 * don't bother if not use --resolv
 	 * it is not possible to set triggers
 	 * in the kernel anyway
 	 */
	if (!options.opt_addr2sym)
		return 0;

	mod = calloc(1, sizeof(module_symbols_t));
	if (!mod)
		return -1;

	mod->id[0] = KERNEL_MOD_ID0;
	mod->id[1] = KERNEL_MOD_ID1;
	/* special refcnt for kernel so it gets freed at the very end */
	mod->refcnt = 1;

	/* 
	 * Despite /proc/kallsyms, System.map is still useful because it includes data symbols
	 * We use System.map if specified, otherwise we default to /proc/kallsyms
	 */
	if (options.opt_sysmap_syms) {
		ret  = -1; //load_sysmap_symbols(&kernel_syms);
		from = options.symbol_file;
	} else {
		ret  = load_kallsyms_symbols(mod);
		from = PFMON_KALLSYMS;
	}
	if (!ret) {
		vbprintf("loaded %lu symbols from %s\n", mod->nsyms, from);

		pthread_mutex_lock(&module_list_lock);
		add_module_syms(mod);
		pthread_mutex_unlock(&module_list_lock);
	} 
	return ret;
}

// attaches the kernel symbols to the options.primary_syms table in
// system-wide mode to enable kernel-level symbol resolution
void attach_kernel_syms(pfmon_sdesc_t *sdesc)
{
	module_map_list_t *list;
	module_map_t *map;
	
	/* don't bother whenever unecessary */
	if (!(options.opt_addr2sym || options.opt_triggers))
		return;

	list = calloc(1, sizeof(module_map_list_t));
	if (!list)
		fatal_error("cannot allocate module_map_list\n");

	/* 2 entries, last is end marker */
	map = calloc(2, sizeof(module_map_t));
	if (!map)
		fatal_error("cannot allocate module_map\n");
	
	pthread_mutex_init(&list->lock, PTHREAD_MUTEX_TIMED_NP);
	list->refcnt = 1;
	list->map = map;
	list->version = 1;

	if (add_kernel_module_map(sdesc, 1, &map[0]))
		fatal_error("cannot add kernel map\n");

	/* end marker */
	map[1].mod = NULL;
	map[1].version = 0;
	map[1].pid = -1;

	sdesc->syms = list;
}

/*
 * load or connect missing module_symbols_t for sdesc
 * special module_symbols ([vdso], ...) have already
 * been loaded
 */
void
pfmon_gather_module_symbols(pfmon_sdesc_t *sdesc)
{
	module_map_list_t *list;
	module_map_t *map;
	module_symbols_t *mod;
	struct stat st;
	char *path;
	uint64_t id[2];
	
	if (!sdesc->syms)
		return;

	list = sdesc->syms;
	pthread_mutex_lock(&module_list_lock);

	pthread_mutex_lock(&list->lock);

	for(map = list->map; map->pid != -1; map++) {

		path = map->path;
		mod = map->mod;

		/*
 		 * look if file-based module exists
 		 */
		if(!mod && *path == '/' && !stat(path, &st)) {

			id[0] = st.st_dev;
			id[1] = st.st_ino;

			/* look if module exists, and increment refcnt */
			mod = find_module_syms(id);
			if (!mod) {
				mod = calloc(1, sizeof(module_symbols_t));
				if (!mod)
					fatal_error("gather_module_symbol out of memory\n");
				mod->id[0] = id[0];
				mod->id[1] = id[1];
				mod->refcnt = 1;
				/*
				 * if we fail, this maybe because this is not
				 * an ELF file, so just keep the map infos
				 */
				if (load_module_syms(path, mod)) {
					free(mod);
					mod = NULL;
				} else {
					add_module_syms(mod);
				}
			}
		}
		map->mod = mod;
	}
	pthread_mutex_unlock(&list->lock);
	pthread_mutex_unlock(&module_list_lock);
}

/*
 * append k module_maps from map to end of sdesc->syms
 */
static int
append_module_maps(pfmon_sdesc_t *sdesc, module_map_t *map, int k)
{
	module_map_list_t *list;
	module_map_t *x;
	int i = 0;

	list = sdesc->syms;
	/* if list not empty, first find the end */
	if(list) {
		/* i points to last element */
		for(x = list->map; x->pid != -1; x++)
			i++; 
	} else {
		sdesc->syms = list = calloc(1, sizeof(module_map_list_t));
		if (!list)
			return -1;
		list->refcnt = 1;
		list->version = 1;
		pthread_mutex_init(&list->lock, PTHREAD_MUTEX_TIMED_NP);
	}
	/* realloc for old + new module_maps */
	list->map = realloc(list->map, (i+k) * sizeof(module_map_t));
	if (!list->map)
		return -1;

	x = list->map + i;
	memcpy(x, map, k * sizeof(module_map_t));

	return 0;
}

/*
 * mode = 0 : only map is loaded
 * mode = 1 : map + actual symbol table are loaded
 */
int
load_sdesc_symbols(pfmon_sdesc_t *sdesc, int mode)
{
	module_map_list_t *list;
	module_map_t *p;
	module_symbols_t *mod;
	pid_t pid;
	size_t szl;
	FILE *fp;
	uint64_t id[2];
	uint64_t start;
	uint64_t end;
	unsigned int version, d1, d2; /* dev_t is unsigned long */
	uint64_t ino; /* inot_t is uint64_t */
	size_t max_count;
	int ret, n, k;
	char perm[8];
	char filename[32];
	char *line, *path, *c;

	/* don't bother whenever unecessary */
	if (!(options.opt_addr2sym || options.opt_triggers))
		return 0;

	list = sdesc->syms;
	/*
 	 * flush remaining samples, if any
 	 *
 	 * in per-thread mode, monitored thread is necessary
 	 * stopped because we come here on breakpoints only.
 	 */
	pfmon_process_smpl_buf(sdesc, 0);
	
	pid = sdesc->tid;

	line = path = NULL;
	
	if (list)
		version = ++list->version;
	else
		version = 1; /* anticipate first version */

	DPRINT(("[%d] SDESC_SYMS mode=%d version=%d\n", pid, mode, version));
	
	sprintf(filename, "/proc/%d/maps", pid);
	fp = fopen(filename, "r");
	if (fp == NULL) 
		return -1;

	szl = 0;

	p = NULL;
	max_count = 8;
	k = 0;
	ret = 0;
	while(getline(&line, &szl, fp) >0) {
		n = sscanf (line, "%"PRIx64"-%"PRIx64" %s %*x %d:%d %"PRIu64" %*s", &start, &end, perm, &d1, &d2, &ino);

		mod = NULL;
		if (n != 6 || perm[3] != 'p')
			continue;

		path = strchr(line, '/');
		if (!path)
			path = strchr(line, '[');
		/* remove trailing \n */
		if (path) {
			path[strlen(path)-1] = '\0';
			/*
			 * we skip stack because the current data structures
			 * do not allow copying of stack between different
			 * threads in the same process.
			 */
			if (!strcmp(path, "[stack]"))
				continue;

			/*
			 * handle the case where /proc/maps reports the library
			 * path as deleted (from the dcache)
			 */
			c = strchr(path, ';');
			if (c)
				*c = '\0';

			if (k == 0 || k == max_count) {
				max_count <<=1;
				p = realloc(p, max_count * sizeof(module_map_t));
				if (!p) {
					ret = -1;
					break;
				}
			}
			id[0] = id[1] = 0;
			/*
 			 *  get the inode number and assign it as an ID
 			 */
			if (*path =='/') {
				id[0] = (uint64_t)d1 << 32 |  d2;
				id[1] = ino;

				/*
 				 * look if we already have the module
 				 * id=0 guaranteed not to exist
 				 */
				mod = find_module_syms(id);
			}

			p[k].pid = pid;
			p[k].id[0] = id[0];
			p[k].id[1] = id[1];
			p[k].version = version;
                        p[k].path = strdup(path);
			
			p[k].base = start;
			p[k].max = end;
			p[k].ref_base = start;

			if (id[0] || id[1]) {
				int j;
				for (j=0; j < k; j++)
					if (p[j].id[0] == id[0] && p[j].id[1] == id[1])
						break;
				if (j < k)
					p[k].ref_base = p[j].base;
			}

			/*
			 * if module was not found and exists
			 * in the filesystem, then load symbol information
			 */
			if (mode == 1 && !mod && (id[0] || id[1])) {
				mod = calloc(1, sizeof(module_symbols_t));
				if (mod == NULL) {
					ret = -1;
					break;
				}
				mod->id[0] = id[0];
				mod->id[1] = id[1];
				mod->refcnt = 1;
				if (load_module_syms(path, mod)) {
					free(mod);
					mod = NULL;
				} else
					add_module_syms(mod);
			}

			/* non ELF and non file mappings have no mod */
			p[k].mod = mod;

			vbprintf("[%u] loaded map version %d, 0x%"PRIx64"-0x%"PRIx64" ref=0x%"PRIx64" %s\n",
				pid,
				version,
				start,
				end, p[k].ref_base, path);
			k++;
		}
	}

	if (!ret) {
		if (k == 0 || (k+2) >= max_count) {
			max_count+=2;
			p = realloc(p, max_count * sizeof(module_map_t));
			if (!p) {
				ret = -1;
				goto error;
			}
		}
		ret = add_kernel_module_map(sdesc, version, p+k);
		if (ret)
			goto error;
		k++;

		/* end marker */		
		p[k].mod = NULL;
		p[k].pid = -1;
		p[k].version = 0;
		k++;

		/* may create sdesc->syms */
		ret = append_module_maps(sdesc, p, k);
		if (ret)
			goto error;
	}
error:
	if (line)
		free(line);

	fclose(fp);
	free(p);

	if (ret == -1)
		warning("abort loading symbols from %s\n", filename);

	return ret;
}

int
find_sym_addr(char *name, unsigned int version, void *syms,
	      uint64_t *start, uint64_t *end)
{
	module_map_list_t *list = syms;
	module_map_t *map;
	module_symbols_t *mod;
	char *p;
	unsigned long i;
	char mod_name[32];

	if (name == NULL || start == NULL || list == NULL) 
		return -1;

	/*
	 * check for module name
	 */
	mod_name[0] = '\0';
	p = strchr(name, ':');
	if (p && *(p+1) != ':') {
		strncpy(mod_name, name, p - name); 
		mod_name[p-name] = '\0';
		name = p + 1;
	}

	for(mod = module_list; mod ; mod = mod ->next) {
		for (i = 0; i < mod->nsyms; i++) {
			if (!strcmp(name, mod->sym_tab[i].name))
				break;
		 }
		/* not found? */
		if (i == mod->nsyms)
			continue;

		for(map = list->map; map->pid != -1; map++) {
			if (map->version != version)
				continue;	
			/*
 			 * stop at the first map match regardless
 			 * of symbol type. It contains the right
 			 * base to use with pt_load
 			 */
			if (map->mod == mod)
				goto found;
		}
	}
	return -1;
found:
	*start = map->ref_base + mod->sym_tab[i].value - mod->pt_load;

	if (end) {
		
		*end = *start + sym_size(map, mod, i);
	}
	return 0;
}

static int
bsearch_sym_cmp(uint64_t base, uint64_t addr, uint64_t offs, symbol_t *sym, uint64_t limit)
{
	uint64_t s, e, sz;

	s = base + sym->value - offs;
	/*
 	 * the last symbol of a module may not have a size (e.g. kernel)
 	 * we compute a logical size by substracting the start address
 	 * to the end address of the map. That may be over-estmating
 	 * but will always be enough to qualify addr
 	 */
	sz = sym->size ? sym->size : (limit ? limit - s : 0);
	e = s + sz;

	DPRINT(("==> addr=0x%"PRIx64" s=0x%"PRIx64" e=0x%"PRIx64" %s\n", addr, s, e, sym->name));

	/* match */
	if (s <= addr && addr < e)
		return 0;

	/* less than */
	if (addr < s)
		return -1;

	/* greater than */
	return 1;
}

int
find_sym_by_av(uint64_t addr, unsigned int version, void *syms,
	       char **name, char **module,
	       uint64_t *start, uint64_t *end, uint64_t *cookie)
{
	module_map_list_t *list = syms;
	module_map_t *map;
	module_symbols_t *mod;
	symbol_t *s = NULL, *lim, *t;
	long l, h, m;
	uint64_t base;
	int r;

	if (!list)
		return -1;

	for(map = list->map; map->pid != -1 ; map++) {

		/*
		 * find matching mapping version
		 */
		if (map->version != version)
			continue;

		mod = map->mod;

		DPRINT(("\n0x%"PRIx64" base=0x%"PRIx64" max=0x%"PRIx64" load=0x%"PRIx64" ref=0x%"PRIx64" nsyms=%lu %s\n",
			addr,
			map->base,
			map->max,
			mod ? mod->pt_load : -1,
			map->ref_base,
			mod ? mod->nsyms : 0,
			map->path));
		/*
		 * check for valid range
		 */
		if (map->base > addr || addr >= map->max)
			continue;

		base = map->ref_base;
		/*
		 * binary search
		 * cannot use bsearch because of base adjustment
		 * s = bsearch(&addr, mod->sym_tab[type], mod->nsyms[type], sizeof(symbol_t), bsearch_sym_cmp);
		 */
		if (mod && mod->nsyms) {
			l = 0;
			h = mod->nsyms-1;
			while (l <= h) {
				m = (l + h) / 2;
				s = &mod->sym_tab[m];
				r = bsearch_sym_cmp(base, addr, mod->pt_load, s, m == mod->nsyms-1 ? map->max: 0);
				if (r > 0)
					l = m + 1;
				else if (r < 0)
					h = m - 1;
				else
					goto found;
			}
			/*
			 * hit a hole in the symbol table.
			 * That could be caused by stripped images
			 * with static functions or bogus size information
			 *
			 * return unknown symbol
			 */
		}
		/*
		 * We come here if:
		 *      - mapping does not correspond to an ELF file
		 *      - mapping has no symbols
		 *      - hit a hole in the symbol table (stripped)
		 */
		if(name)
			*name = "UNKNOWN_SYMBOL";
		if(start)
			*start = addr;
		if(end)
			*end = addr;
		if(module)
			*module = map->path;
		if(cookie)
			*cookie = PFMON_COOKIE_UNKNOWN_SYM;
		return 0;
	}
	return -1;
found:
	/*
	 * in some symbol tables, you may have multiple symbols
	 * with the same address, sometimes the same size.
	 * 
	 * To avoid having to repack symbols if we find multiple
	 * matches for the same address, we keep those symbols
	 * in the table. But during search we try to return the more
	 * meaningful symbol.
	 * By meaningful we mean:
	 *      - if size is 0, then the first symbol at the same address
	 *        with non-zero size, if any
	 *      - the symbol wth the same size. Symbols with the same address
	 *        and size are sorted by name (reverse order).
	 */
	if (s->size == 0) {
		lim = mod->sym_tab + mod->nsyms;
		t = s+1;
		while (t != lim && !t->size && t->value == s->value)
			t++;

		if (t != lim && t->value == s->value)
			s = t;
	} else if (0) {
		t = s-1;
		lim = mod->sym_tab - 1;
		while (t != lim && t->size == s->size && t->value == s->value)
			t--;
		if (t != lim && t->value == s->value)
			s = t;
	}
	if (name)
		*name = s->name;
	if (start)
		*start = base + s->value - mod->pt_load;
	if (end)
		*end = base + s->value - mod->pt_load + s->size;
	if (module)
		*module = map->path;
	if(cookie)
		*cookie = s->value;
	return 0;
}

/*      
 *       * mostly for debug     
 *        */     
void                    
print_syms(pfmon_sdesc_t *sdesc)
{               
	module_map_list_t *list;
	module_map_t *map;
	module_symbols_t *mod;
	symbol_t *symbol_tab; 
	uint64_t sz, addr;
	unsigned long i;

	list = sdesc->syms;

	if (!list) {
		printf("no symbols defined\n");
		return; 
	}                       

	for (map = list->map; map->pid != -1 ; map++) {
		mod = map->mod; 
		/*      
		 * some mappings do not point to ELF files, e.g.,
		 * mmapped files. Here we simply print the address
		 * range and the file path
		 */
		if (!mod) {
			printf("%8d %"PRIx64" %16"PRIu64"<%s>\n",
					map->version,
					map->base,
					map->max,
					map->path);
			continue;
		}
		/*      
		 * multiple maps may point to the same module, e.g.,
		 * text and data. But we want to print only once for
		 * the reference map. Otherwise we get symbols multiple
		 * times        
		 */             
		if (map->base != map->ref_base)
			continue;
		symbol_tab = mod->sym_tab;

		for (i = 0; i < mod->nsyms; i++) {
			addr = map->ref_base + symbol_tab[i].value - mod->pt_load;
			sz = symbol_tab[i].size;

			printf("%8d %"PRIx64" %"PRIx64" %s<%s>\n",
					map->version,
					addr,
					sz,
					symbol_tab[i].name, map->path);
		}
	}
}

/*
 * read ELF program header to return entry point address
 * return:
 * 	0 if error
 * 	addr otherwise
 */
uint64_t
get_entry_point(char *filename)
{
	Elf *elf;
	Elf64_Ehdr *ehdr64;
	Elf32_Ehdr *ehdr32;
	char *eident;
	uint64_t addr = 0;
	int fd;

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		DPRINT(("symbol file for %s not found\n", filename));
		return 0;
	}

  	/* initial call to set internal version value */
	if (elf_version(EV_CURRENT) == EV_NONE) {
		DPRINT(("ELF library out of date"));
		goto end2;
	}

  	/* prepare to read the entire file */
	elf = elf_begin(fd, ELF_C_READ, NULL);
	if (elf == NULL) {
		DPRINT(("cannot read %s\n", filename));
		goto end2;
	}

	/* error checking */
	if (elf_kind(elf) != ELF_K_ELF) {
		DPRINT(("%s is not an ELF file\n", filename));
		goto end;
	}
  
	eident = elf_getident(elf, NULL);
	switch (eident[EI_CLASS]) {
  		case ELFCLASS64:
			ehdr64 = elf64_getehdr(elf);
			if (ehdr64)
				addr = ehdr64->e_entry;
			break;
		case ELFCLASS32:
			ehdr32 = elf32_getehdr(elf);
			if (ehdr32)
				addr = ehdr32->e_entry;
			break;
		default:
			addr = 0;
	}
end:
	elf_end(elf);
end2:
	close(fd);

	return addr;
}

/*
 * check whether filename is ELF32 ABI
 * return:
 * 	0 not ELF32
 * 	1 is ELF32
 */
int
program_is_abi32(char *filename)
{
	Elf *elf;
	char *eident;
	int fd, ret = 0;

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		DPRINT(("symbol file for %s not found\n", filename));
		return 0;
	}

  	/* initial call to set internal version value */
	if (elf_version(EV_CURRENT) == EV_NONE) {
		DPRINT(("ELF library out of date"));
		goto end2;
	}

  	/* prepare to read the entire file */
	elf = elf_begin(fd, ELF_C_READ, NULL);
	if (elf == NULL) {
		DPRINT(("cannot read %s\n", filename));
		goto end2;
	}

	/* error checking */
	if (elf_kind(elf) != ELF_K_ELF) {
		DPRINT(("%s is not an ELF file\n", filename));
		goto end;
	}
  
	eident = elf_getident(elf, NULL);
	if (eident[EI_CLASS] == ELFCLASS32)
		ret = 1;
end:
	elf_end(elf);
end2:
	close(fd);
	return ret;
}

static void
free_module_symbols(module_symbols_t *p)
{
	symbol_t *s;
	int j;

	s = p->sym_tab;
	for(j=0; j < p->nsyms; j++, s++)
		free(s->name);
	free(p->sym_tab);

	/*
 	 * only actual files are present in
 	 * module list
 	 *
 	 * id == 0 denote special symbols ([heap], [stack], ...)
 	 *
 	 */
	if (p->id) {
		if (p->prev)
			p->prev->next = p->next;
		else
			module_list = p->next;

		if (p->next)
			p->next->prev = p->prev;
	}
	free(p);
}

void
free_module_map_list(void *syms)
{
	module_map_list_t *list = syms;
	module_map_t *map;

	if (!list)
		return;

	pthread_mutex_lock(&list->lock);

	if (--list->refcnt == 0) {

		pthread_mutex_lock(&module_list_lock);

		for(map = list->map; map->pid != -1; map++) {
			if (map->mod) {
				DPRINT(("REF %s %lu %p\n", map->path, map->mod->refcnt, map->mod));
				map->mod->refcnt--;
				if (map->mod->refcnt == 0)
					free_module_symbols(map->mod);
				else
					DPRINT(("STILL REF %s %lu\n", map->path, map->mod->refcnt));
			}
			/* was strdup'd */
			free(map->path);
		} 
		pthread_mutex_unlock(&module_list_lock);
		/* was realloc'd */
		free(list->map);
		free(list);
	} else
		pthread_mutex_unlock(&list->lock);
}

void
show_module_map(pfmon_sdesc_t *sdesc)
{
	module_map_list_t *list;
	module_map_t *map;

	list = sdesc->syms;
	if (!list)
		return;

	pthread_mutex_lock(&list->lock);

	for(map = list->map; map->pid != -1; map++) {
		printf("mod version=%d base=%"PRIx64" max=%"PRIx64" ref=%"PRIx64"pt_load=%"PRIx64" %s\n",
			map->version,
			map->base,
			map->max,
			map->ref_base,
			map->mod ? map->mod->pt_load : -1,
			map->path);
	}

	pthread_mutex_unlock(&list->lock);
}
