/*
 * pfmon_task.c : handles per-task measurements
 *
 * Copyright (c) 2001-2006 Hewlett-Packard Development Company, L.P.
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
 */
#include "pfmon.h"

#include <fcntl.h>
#include <regex.h>
#include <syscall.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/ptrace.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include <dirent.h>

/*
 * This belongs to some LIBC header files for 2.6
 */
#ifndef PTRACE_SETOPTIONS

/* 0x4200-0x4300 are reserved for architecture-independent additions.  */
#define PTRACE_SETOPTIONS	0x4200
#define PTRACE_GETEVENTMSG	0x4201
#define PTRACE_GETSIGINFO	0x4202
#define PTRACE_SETSIGINFO	0x4203

/* options set using PTRACE_SETOPTIONS */
#define PTRACE_O_TRACESYSGOOD	0x00000001
#define PTRACE_O_TRACEFORK	0x00000002
#define PTRACE_O_TRACEVFORK	0x00000004
#define PTRACE_O_TRACECLONE	0x00000008
#define PTRACE_O_TRACEEXEC	0x00000010
#define PTRACE_O_TRACEVFORKDONE	0x00000020
#define PTRACE_O_TRACEEXIT	0x00000040

/* Wait extended result codes for the above trace pt_options.  */
#define PTRACE_EVENT_FORK	1
#define PTRACE_EVENT_VFORK	2
#define PTRACE_EVENT_CLONE	3
#define PTRACE_EVENT_EXEC	4
#define PTRACE_EVENT_VFORK_DONE	5
#define PTRACE_EVENT_EXIT	6
#endif /* PTRACE_OPTIONS */

#define PFMON_SDESC_PID_HASH_SIZE	256
#define PFMON_SDESC_PID_HASH(x)		((x) & (PFMON_SDESC_PID_HASH_SIZE-1))

#ifndef __WNOTHREAD
#define __WNOTHREAD     0x20000000
#endif

/*
 * better is cache line size aligned
 */
typedef struct {
	pthread_t	thread_id;	/* worker's thread id */
	unsigned int	cpu_id;		/* worker's assigned CPU */
	unsigned int	max_fds;	/* max number of fds to be managed by worker */
	int		to_worker[2];	/* worker's 1-way communication frofromm master */
	int		from_worker[2];	/* worker's 1-way communication back to master */
} task_worker_t;

typedef enum { 
	PFMON_TASK_MSG_ADD_TASK,	/* new task to handle */
	PFMON_TASK_MSG_REM_TASK,	/* new task to handle */
} pfmon_worker_msg_type_t;

typedef struct {
	pfmon_worker_msg_type_t	type;
	void			*data;
} task_worker_msg_t;

typedef struct {
	unsigned long num_sdesc;	/* number of sdesc allocated at a particular time */
	unsigned long max_sdesc;	/* max number of allocated sdesc at a particular time */
	unsigned long num_active_sdesc; /* number of sdesc which are actively monitoring */
	unsigned long max_active_sdesc; /* max number of sdesc which are actively monitoring at a particular time */
	unsigned long total_sdesc;	/* total number of sdesc created for the entire session */
} task_info_t;

static pthread_key_t		arg_key;
static pthread_mutex_t		pfmon_hash_pid_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t		task_info_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t		task_aggr_lock = PTHREAD_MUTEX_INITIALIZER;
static pfmon_sdesc_t		sdesc_task_aggr;
static regex_t 			follow_exec_preg;
static task_worker_t		*workers;
static pid_t 			master_tid;
static pfmon_sdesc_t 		*sdesc_pid_hash[PFMON_SDESC_PID_HASH_SIZE];
static int volatile 		time_to_quit;
static pfmon_quit_reason_t	quit_reason;
static int 			work_todo;
static task_info_t		task_info;
static sem_t			master_work_sem;

static void pfmon_sdesc_chain_append(pfmon_sdesc_t *sdesc);
static int task_pfm_init(pfmon_sdesc_t *sdesc, int from_exec, pfmon_ctx_t *ctx);

static const char *trigger_strs[]={
	"entry",
	"start",
	"stop",
	"dlopen"
};

/*
 * must be called with aggr_lock held
 */
static inline void
task_aggregate_results(pfmon_sdesc_t *sdesc)
{
	pfmon_event_set_t *set_aggr, *set;
	unsigned int i, count;
	
	for (set_aggr = sdesc_task_aggr.sets,
	     set = sdesc->sets;
	     set_aggr;
	     set_aggr = set_aggr->next,
	     set = set->next) {

		count = set_aggr->setup->event_count;

		for (i=0; i < count; i++) {
			set_aggr->master_pd[i].reg_value += set->master_pd[i].reg_value;
		}
	}
}

static void
task_sigalarm_handler(int n, struct siginfo *info, void *sc)
{
	if (quit_reason == QUIT_NONE)
		quit_reason  = QUIT_TIMEOUT;
	time_to_quit = 1;
	sem_post(&master_work_sem);
}

static void
task_sigint_handler(int n, struct siginfo *info, void *sc)
{
	if (gettid() != master_tid) return;

	if (quit_reason == QUIT_NONE)
		quit_reason  = QUIT_ABORT;
	time_to_quit = 1;
	sem_post(&master_work_sem);
}

static void
task_sigchild_handler(int n, struct siginfo *info, void *sc)
{
	sem_post(&master_work_sem);
}

/* for debug only */
static void
task_sigterm_handler(int n, struct siginfo *info, void *sc)
{
	if (quit_reason == QUIT_NONE)
		quit_reason  = QUIT_TERM;
	time_to_quit = 1;
	sem_post(&master_work_sem);
}

static void
mask_global_signals(void)
{
	sigset_t my_set;

	sigemptyset(&my_set);
	sigaddset(&my_set, SIGINT);
	sigaddset(&my_set, SIGCHLD);
	sigaddset(&my_set, SIGALRM);
	sigaddset(&my_set, SIGTERM);
	/*
	 * we want to affect the caller's thread only, not the entire process
	 */
        pthread_sigmask(SIG_BLOCK, &my_set, NULL);
}

static void
unmask_global_signals(void)
{
	sigset_t my_set;

	sigemptyset(&my_set);
	sigaddset(&my_set, SIGINT);
	sigaddset(&my_set, SIGCHLD);
	sigaddset(&my_set, SIGALRM);
	sigaddset(&my_set, SIGTERM);

	/*
	 * we want to affect the caller's thread only, not the entire process
	 */
        pthread_sigmask(SIG_UNBLOCK, &my_set, NULL);
}

/*
 * signal handlers are shared by all pfmon threads
 */
static void
setup_global_signals(void)
{
	struct sigaction act;
	sigset_t my_set;

	memset(&act,0,sizeof(act));
	sigemptyset(&my_set);
	sigaddset(&my_set, SIGINT);
	sigaddset(&my_set, SIGTERM);

	act.sa_mask    = my_set;
	act.sa_flags   = SA_SIGINFO;
	act.sa_handler = (__sighandler_t)task_sigalarm_handler;
	sigaction (SIGALRM, &act, 0);

	memset(&act,0,sizeof(act));
	sigemptyset(&my_set);
	sigaddset(&my_set, SIGALRM);
	sigaddset(&my_set, SIGTERM);

	act.sa_mask    = my_set;
	act.sa_handler = (__sighandler_t)task_sigint_handler;
	act.sa_flags   = SA_SIGINFO;
	sigaction (SIGINT, &act, 0);

	memset(&act,0,sizeof(act));
	sigemptyset(&my_set);
	sigaddset(&my_set, SIGALRM);
	sigaddset(&my_set, SIGINT);

	act.sa_mask    = my_set;
	act.sa_handler = (__sighandler_t)task_sigterm_handler;
	act.sa_flags   = SA_SIGINFO;
	sigaction (SIGTERM, &act, 0);

	memset(&act, 0, sizeof(act));
	sigemptyset(&my_set);
	sigaddset(&my_set, SIGINT);
	sigaddset(&my_set, SIGALRM);
	sigaddset(&my_set, SIGTERM);

	act.sa_mask    = my_set;
	act.sa_flags   = SA_SIGINFO;
	act.sa_handler = (__sighandler_t)task_sigchild_handler;
	sigaction (SIGCHLD, &act, 0);
}

static inline int
pfmon_continue(pid_t pid, unsigned long sig)
{
	int r;

	r = ptrace(PTRACE_CONT, pid, NULL, (void *)sig);
	if (r == -1) {
		warning("cannot restart [%d]: %s\n", pid, strerror(errno));
	}
	return r;
}

static inline int
pfmon_detach(pid_t pid)
{
	int r;

	r = ptrace(PTRACE_DETACH, pid, NULL, NULL);
	if (r == -1) {
		warning("cannot detach [%d]: %s\n", pid, strerror(errno));
	}
	return r;
}

static int
install_code_triggers(pfmon_sdesc_t *sdesc)
{
	unsigned int i, k=0;
	pfmon_trigger_t *trg;
	pid_t pid;
	int ret;

	trg = sdesc->code_triggers;
	pid = sdesc->tid;

	for (i=0; i < sdesc->num_code_triggers; i++, trg++) {
		trg->brk_idx = k++;
		if (trg->brk_address) {
			ret = pfmon_set_code_breakpoint(pid, trg);
			if (ret) {
				warning("cannot install code breakpoints @ %p\n", trg->brk_address);
				return -1;
			}
			vbprintf("[%d] installed %-5s code breakpoint (db%u) at %p\n",
				pid, 
				trigger_strs[trg->brk_type],
				i,
				trg->brk_address);
		}
	}
	return 0;
}

static int
install_data_triggers(pfmon_sdesc_t *sdesc)
{
	pfmon_trigger_t *trg;
	pid_t pid;
	unsigned int i;
	int rw, ret;

	trg = sdesc->data_triggers;
	pid = sdesc->tid;

	for (i=0; i < sdesc->num_data_triggers; i++, trg++) {
		rw = trg->trg_attr_rw;
		trg->brk_idx = i;
		ret = pfmon_set_data_breakpoint(pid, trg);
		if (ret) {
			warning("cannot install data breakpoints\n");
			return -1;
		}
		vbprintf("[%d] installed %-5s data breakpoint at %p\n", 
			pid, 
			trigger_strs[trg->brk_type],
			trg->brk_address);

	}
	return 0;
}

static int
uninstall_code_triggers(pfmon_sdesc_t *sdesc)
{
	unsigned int i;
	pfmon_trigger_t *trg;
	pid_t pid;
	int ret;

	trg = sdesc->code_triggers;
	pid = sdesc->tid;

	for (i=0; i < sdesc->num_code_triggers; i++, trg++) {
		if (trg->brk_address == 0)
			continue;

		ret = pfmon_clear_code_breakpoint(pid, trg);
		if (ret)
			warning("[%d] cannot uninstall code breakpoint @ %p : %s\n",
				pid,
				trg->brk_address, strerror(errno));
		else
			vbprintf("[%d] uninstalled %-5s code breakpoint (db%u) at %p\n",
				pid,
				trigger_strs[trg->brk_type],
				i,
				trg->brk_address);
	}
	return 0;
}

static int
uninstall_data_triggers(pfmon_sdesc_t *sdesc)
{
	pfmon_trigger_t *trg;
	pid_t pid;
	unsigned int i;
	int ret;

	trg = sdesc->data_triggers;
	pid = sdesc->tid;

	for (i=0; i < sdesc->num_data_triggers; i++, trg++) {
		if (trg->brk_address == 0)
			continue;

		ret = pfmon_clear_data_breakpoint(pid, trg);
		if (ret)
			warning("cannot uninstall data breakpoint @ %p\n", trg->brk_address);
		else
			vbprintf("[%d] uninstalled %-5s data breakpoint at %p\n",
				pid,
				trigger_strs[trg->brk_type],
				trg->brk_address);
	}
	return 0;
}

static pfmon_trigger_t *
find_trigger(pfmon_sdesc_t *sdesc, pfmon_trigger_t *triggers, unsigned int num, unsigned long addr)
{
	unsigned int i;

	for (i=0; i < num; i++) {
		if (addr == triggers[i].brk_address)
			return triggers+i;
	}
	return NULL;
}

static int 
task_setup_pfm_context(pfmon_sdesc_t *sdesc, pfmon_ctx_t *ctx)
{
	pfmon_smpl_desc_t *csmpl = &sdesc->csmpl;
	pfmon_ctxid_t id;
	int ret, error;

	pfmon_clone_sets(options.sets, sdesc);

	memset(csmpl, 0, sizeof(pfmon_smpl_desc_t));

	sdesc->ctxid = pfmon_create_context(ctx, &csmpl->smpl_hdr, &error);
	if (sdesc->ctxid == -1 ) {
		if (error == ENOMEM && options.opt_use_smpl) {
			warning("Not enough memory to create perfmon context for [%d],\ncheck your locked memory "
				" resource limit with limit or ulimit\n", sdesc->tid);
		} else {
			warning("can't create perfmon context: %s\n", strerror(error));
			if (errno == EMFILE)
				warning("try increasing resource limit (ulimit -n) or use the --smpl-eager-save option\n");
		}
		return -1;
	}
	id = sdesc->ctxid;

	/*
	 * set close-on-exec because monitored task
	 * should not access the fd
	 */
	ret = fcntl(id, F_SETFD, FD_CLOEXEC);
	if (ret) {
		warning("cannot set CLOEXEC: %s\n", strerror(errno));
		return -1;
	}

	if (open_results(sdesc) == -1)
		return -1;

	if (options.opt_use_smpl) {
		if (pfmon_setup_smpl(sdesc, &sdesc_task_aggr) == -1)
			return -1;
		sdesc->csmpl.map_size = ctx->ctx_map_size;
	}

	if (install_event_sets(sdesc) == -1)
		return -1;

	if (pfmon_load_context(sdesc->ctxid, sdesc->tid, & error) == -1) {
		if (error == EBUSY)
			warning("error conflicting monitoring session exists\n");
		else
			warning("cannot attach context to %d: %s\n", sdesc->tid, strerror(error));

		return -1;
	}

	return 0;
}

static int 
task_reset_pfm_context(pfmon_sdesc_t *sdesc)
{
	pfmon_event_set_t *set;
	pfmon_pmd_t *pd;
	unsigned int i, count;
	int ret = -1, error;

	vbprintf("[%d] resetting perfmon state hdr=%p\n", sdesc->tid, sdesc->csmpl.smpl_hdr);

	for (set = sdesc->sets; set; set = set->next) {
		pd = set->master_pd;
		count = set->setup->event_count;
		for(i=0; i < count; i++) {
			pd[i].reg_value = set->setup->long_rates[i].value;
		}
	}

	/*
	 * task is stopped but we need to unload because we reprogram
	 * the event sets
	 */
	if (pfmon_unload_context(sdesc->ctxid, &error) == -1)
		return -1;

	install_event_sets(sdesc);

	if (pfmon_load_context(sdesc->ctxid, sdesc->tid, &error) == -1)
		return -1;

	/* monitoring is always stopped on reload */

	if (options.opt_use_smpl) {
		if (pfmon_reset_smpl(sdesc) == -1) goto error;
		if (pfmon_setup_smpl(sdesc, &sdesc_task_aggr) == -1) goto error;
		DPRINT(("reset setup sampling buffer for [%d]\n", sdesc->tid));
	}
	ret = 0;
error:
	return ret;
}

/*
 * is_final = 0 : collect intermediate results (split-exec)
 * is_final = 1 : collect final results
 *
 * collect final counts, samples. Close count output files.
 * Sampling output files and processing not done here.
 */
static int
task_collect_results(pfmon_sdesc_t *sdesc, int is_final)
{
	/*
	 * no more context attached, there is nothing we can do here
	 */
	if (sdesc->ctxid == -1)
		return 0;

	/*
	 * read the last known values for the counters
	 */
	if (options.opt_use_smpl == 0 || options.opt_smpl_print_counts) {
		if (read_results(sdesc) == -1) {
			warning("read_results error\n");
			return -1;
		}
	}

	if (options.opt_aggr) {
		pthread_mutex_lock(&task_aggr_lock);

		task_aggregate_results(sdesc);

		/* process last samples */
		if (options.opt_use_smpl)
			pfmon_process_smpl_buf(sdesc, 1);

		pthread_mutex_unlock(&task_aggr_lock);
	} else {
		/* process last samples */
		if (options.opt_use_smpl)
			pfmon_process_smpl_buf(sdesc, 1);

		/*
 		 * when sampling, this function only prints
 		 * data if --smpl-print-counts is present
 		 */
		show_results(sdesc, 0);

		/* close generic count output (--outfile) */
		close_results(sdesc);
	}
	return 0;
}

/*
 * allocates sdesc with accompanying ctx_arg area
 */
static pfmon_sdesc_t *
pfmon_sdesc_alloc(void)
{
	pfmon_sdesc_t *tmp;

	tmp = calloc(1, sizeof(pfmon_sdesc_t) + options.ctx_arg_size);
	if (tmp == NULL)
		fatal_error("cannot allocate sdesc\n");

	pthread_mutex_init(&tmp->lock, PTHREAD_MUTEX_TIMED_NP);

	return tmp;
}

static void
pfmon_sdesc_free(pfmon_sdesc_t *sdesc)
{
	if (!sdesc)
		return;

	DPRINT(("[%d] FREE SDESC syms=%p\n", sdesc->tid, sdesc->syms));

	if (sdesc->sets)
		pfmon_free_sets(sdesc);

	free_module_map_list(sdesc->syms);
	free(sdesc->cmd);
	free(sdesc);
}

static void
pfmon_sdesc_pid_hash_add(pfmon_sdesc_t **hash, pfmon_sdesc_t *t)
{
	int slot = PFMON_SDESC_PID_HASH(t->tid);

	pthread_mutex_lock(&pfmon_hash_pid_lock);

	t->next    = hash[slot];
	hash[slot] = t;

	pthread_mutex_unlock(&pfmon_hash_pid_lock);

}

static pfmon_sdesc_t *
pfmon_sdesc_pid_hash_find(pfmon_sdesc_t **hash, pid_t pid)
{
	int slot = PFMON_SDESC_PID_HASH(pid);
	pfmon_sdesc_t *q;

	pthread_mutex_lock(&pfmon_hash_pid_lock);

	q = hash[slot];
	while (q) {
		if ((q)->tid == pid) break;
		q = q->next;
	}
	pthread_mutex_unlock(&pfmon_hash_pid_lock);

	return q;
}

static int
pfmon_sdesc_pid_hash_remove(pfmon_sdesc_t **hash, pfmon_sdesc_t *t)
{
	pfmon_sdesc_t *q, *prev = NULL;
	int slot = PFMON_SDESC_PID_HASH(t->tid);

	pthread_mutex_lock(&pfmon_hash_pid_lock);

	q = hash[slot];
	while (q) {
		if (q == t) goto found;
		prev = q;
		q = q->next;
	}
	pthread_mutex_unlock(&pfmon_hash_pid_lock);

	fatal_error("cannot find [%d] in hash queue\n", t->tid);
	return -1;
found:
	if (prev)
		prev->next = t->next;
	else 
		hash[slot] = t->next;

	pthread_mutex_unlock(&pfmon_hash_pid_lock);

	return 0;
}
	
static int
pfmon_setup_ptrace(pid_t pid)
{
	unsigned long ptrace_flags;
	int ret;

	ptrace_flags = 0UL;

	/*
	 * we need this notifcation to stop monitoring on exec when
	 * no "follow" option is specified
	 */
	ptrace_flags |= PTRACE_O_TRACEEXEC;

	if (options.opt_follow_vfork)
		ptrace_flags |= PTRACE_O_TRACEVFORK;
	if (options.opt_follow_fork)
		ptrace_flags |= PTRACE_O_TRACEFORK;
	if (options.opt_follow_pthread)
		ptrace_flags |= PTRACE_O_TRACECLONE;


	vbprintf("follow_exec=%c follow_vfork=%c follow_fork=%c follow_pthread=%c\n",
		options.opt_follow_exec  ? 'y' : 'n',
		options.opt_follow_vfork ? 'y' : 'n',
		options.opt_follow_fork  ? 'y' : 'n',
		options.opt_follow_pthread ? 'y' : 'n');

	if (ptrace_flags == 0UL) return 0;

	/*
	 * update the options
	 */
	ret = ptrace(PTRACE_SETOPTIONS, pid, NULL, (void *)ptrace_flags);
	if (ret == -1) warning("cannot set ptrace options on [%d], check PTRACE_SETOPTIONS support: %s\n", pid, strerror(errno));
	return ret;
}

static void
pfmon_sdesc_exit(pfmon_sdesc_t *sdesc)
{
	int can_exit = 0;

	LOCK_SDESC(sdesc);

	sdesc->refcnt--;

	if (!sdesc->refcnt) {

		pfmon_sdesc_pid_hash_remove(sdesc_pid_hash, sdesc);

		pthread_mutex_lock(&task_info_lock);

		task_info.num_sdesc--;

		if (sdesc->ctxid != -1)
			task_info.num_active_sdesc--;


		if (task_info.num_sdesc == 0)
			can_exit = 1;

		pthread_mutex_unlock(&task_info_lock);

		vbprintf("[%d] detached\n", sdesc->tid);
		/*
 		 * postpone freeing/closing when all sampling sessions done
 		 * must be done before we post the semaphore to avoid a
 		 * race with sdesc_chain_append() and sdesc_chain_process()
 		 */
		if (options.opt_use_smpl && !options.opt_eager)
			pfmon_sdesc_chain_append(sdesc);
		else
			pfmon_sdesc_free(sdesc);

		if (can_exit) {
			work_todo = 0;
			sem_post(&master_work_sem);
		}	
	} else {
		if (sdesc->refcnt < 1) 
			fatal_error("invalid refcnt=%d for [%d]\n", sdesc->refcnt, sdesc->tid); 

		DPRINT(("deferring remove tid=%d refcnt=%d\n", sdesc->tid, sdesc->refcnt));

		UNLOCK_SDESC(sdesc);
	}
}

static const char *sdesc_type_str[]= {
	"attached",
	"fork",
	"vfork",
	"clone"
};

static inline void
pfmon_sdesc_set_pid(pfmon_sdesc_t *sdesc, pid_t new_tid)
{
	sdesc->pid = find_pid_attr(new_tid, "Tgid");
	sdesc->ppid = find_pid_attr(new_tid, "PPid");
	sdesc->tid = new_tid;
}

/*
 * perform sdesc setup actions which require task to be stopped
 */
static void
pfmon_sdesc_update_stopped(pfmon_sdesc_t *sdesc)
{
	pfmon_disable_all_breakpoints(sdesc->tid);	

	if (sdesc->num_code_triggers) {
		/* uninstall what was inherited from parent */
		uninstall_code_triggers(sdesc);

		/* only keep dlopen if inheritance is disabled */
		if (!options.opt_code_trigger_follow)
			sdesc->num_code_triggers = 1;
	}

	if (sdesc->num_data_triggers) {
		/* uninstall what was inherited from parent */
		uninstall_data_triggers(sdesc);

		/* clear number of breakpoints if inheritance disabled */
		if (!options.opt_data_trigger_follow)
			sdesc->num_code_triggers = 0;
	}
}

static void
pfmon_sdesc_update(pfmon_sdesc_t *sdesc, int type, pfmon_sdesc_t *parent)
{
	unsigned int n;

	/*
	 * the following rules apply for flags inheritance:
	 * fl_monitoring	: inherited
	 * fl_seen_stopsig	: not inherited
	 * fl_detaching		: not inherited
	 * fl_dispatched	: not inherited
	 * fl_attached		: inherited
	 */
	if (parent->fl_attached)
		sdesc->fl_attached = 1;
	if (parent->fl_monitoring)
		sdesc->fl_monitoring = 1;

	if (type == PFMON_SDESC_ATTACH)
		sdesc->fl_attached = 1;

	if (parent->num_code_triggers) {
		n = parent->num_code_triggers;
		memcpy(sdesc->code_triggers, parent->code_triggers, n*sizeof(pfmon_trigger_t));
		sdesc->num_code_triggers = n;
		/* adjustment done in pfmon_sdesc_update_stopped */
	}

	if (parent->num_data_triggers && options.opt_data_trigger_follow) {
		n = parent->num_data_triggers;
		memcpy(sdesc->data_triggers, parent->data_triggers, n*sizeof(pfmon_trigger_t));
		sdesc->num_data_triggers = n;
	}
	/*
 	 * only load symbol map when not initial sdesc
 	 */
	switch(type) {
	case PFMON_SDESC_CLONE:
	case PFMON_SDESC_ATTACH:
		/* share the syms on clone (pthread_create) */
		if (parent->syms)
			sdesc->syms = syms_get(parent);
		break;
	case PFMON_SDESC_FORK:
	case PFMON_SDESC_VFORK:
		/* create new syms on fork, vfork */
		load_sdesc_symbols(sdesc, 0);
		break;
	}

	/*
 	 * if SIGSTOP was received, then we can proceed with
 	 * actions requiring task to be stopped
 	 */
	if (sdesc->fl_seen_stopsig)
		pfmon_sdesc_update_stopped(sdesc);
}



static pfmon_sdesc_t *
pfmon_sdesc_new(int type, pfmon_sdesc_t *parent, pid_t new_tid)
{
	pfmon_sdesc_t *sdesc;
	int ret;

	sdesc = pfmon_sdesc_alloc();

	pfmon_sdesc_set_pid(sdesc, new_tid);
	ret = pfmon_extract_cmdline(sdesc);
	if (ret == -1)
		sdesc->cmd = strdup("UNKNOWN");

	sdesc->type = type;

	sdesc->ctxid  = -1; /* not associated with a context */
	/*
 	 * refcnt used to protect sdesc when shared between
 	 * ptrace and worker thread (sampling mode)
 	 */
	sdesc->refcnt = 1;

	DPRINT(("[%d] SDESC_NEW %s parent=%d pid=%lu tid=%d flags=0x%lx\n", 
		sdesc->tid,
		sdesc->cmd,
		sdesc_type_str[type],
		sdesc->ppid,
		sdesc->pid,
		sdesc->tid,
		sdesc->flags));

	pfmon_sdesc_pid_hash_add(sdesc_pid_hash, sdesc);

	if (type == PFMON_SDESC_ATTACH)
		sdesc->fl_attached = 1;

	if (parent)
		pfmon_sdesc_update(sdesc, type, parent);

	pthread_mutex_lock(&task_info_lock);

	task_info.num_sdesc++;

	if (task_info.num_sdesc > task_info.max_sdesc) 
		task_info.max_sdesc = task_info.num_sdesc;

	pthread_mutex_unlock(&task_info_lock);

	return sdesc;
}

/*
 * return:
 * 	0 : not interested
 * 	1 : interested
 */
static inline int
pfmon_sdesc_interesting(pfmon_sdesc_t *sdesc)
{
	int r = 0;

	if (options.fexec_pattern) {
		/* r = 0 means match */
		r = regexec(&follow_exec_preg, sdesc->cmd, 0, NULL, 0);
		if (options.opt_follow_exec_excl) r = !r;
	}
	return r == 0 ? 1 : 0;
}

static void
pfmon_setup_entry_brkpt(pfmon_sdesc_t *sdesc)
{
	uint64_t addr;

	/*
 	 * don't bother with breakpoints and loading
 	 * of symbols when not required
 	 */
	if (!(options.opt_addr2sym || options.opt_triggers))
		return;
	/*
	 * we arm a breakpoint on entry point to capture
	 * symbol table from /proc. We leverage the existing
	 * trigger-code infrastructure by injecting breakpoint.
	 * Monitoring is not started until we reach that breakpoint
	 * which is set on the first user instruction of the program
	 * we are starting.
	 */
	addr = get_entry_point(sdesc->cmd);
	if (addr) {
		sdesc->code_triggers[0].brk_address = addr;
		sdesc->code_triggers[0].brk_type = PFMON_TRIGGER_ENTRY;
		sdesc->code_triggers[0].trg_attr_repeat = 0;
		sdesc->num_code_triggers = 1;
		/*
 		 * do not issue pfmon_start() in task_pfm_init()
 		 * and defer to pfmon_handle_entry_trigger()
 		 */
		sdesc->fl_has_entry = 1;
	}
}

static void
pfmon_sdesc_exec(pfmon_sdesc_t *sdesc)
{
	int ret;

	/*
 	 * override existing cmdline
 	 */
	ret = pfmon_extract_cmdline(sdesc);
	if (ret == -1)
		sdesc->cmd = strdup("UNKNOWN");

	sdesc->fl_abi32 = program_is_abi32(sdesc->cmd);

	vbprintf("[%d] %s-bit binary\n",
		sdesc->pid,
		sdesc->fl_abi32 ? "32" : "64");
 
	/*
 	 * triggers not inherited across exec
 	 */
	uninstall_code_triggers(sdesc);
	uninstall_data_triggers(sdesc);

	sdesc->num_code_triggers = 0;
	sdesc->num_data_triggers = 0;

	if (sdesc->fl_attached)
		load_sdesc_symbols(sdesc, 1);
	else
		pfmon_setup_entry_brkpt(sdesc);
}

static int
task_worker_send_msg(unsigned int cpu, task_worker_msg_t *msg, int wait)
{
	task_worker_msg_t fake;
	int r;

	r = write(workers[cpu].to_worker[1], msg, sizeof(*msg));
	DPRINT(("sending msg.type=%d to wCPU%u\n", msg->type, cpu));

	/*
	 * dummy response, just used for synchronization
	 */
	if (wait) r = read(workers[cpu].from_worker[0], &fake, sizeof(fake));

	return r;
}


static int
task_create(pfmon_ctx_t *ctx, char **argv)
{
	pfmon_sdesc_t *sdesc;
	pid_t pid = 0;
	int status, ret;

	if ((pid=vfork()) == -1) {
		warning("cannot vfork process\n");
		return -1;
	}

	if (pid == 0) {		 
		/*
		 * The use of ptrace() allows us to actually start monitoring after the exec()
		 * is done, i.e., when the new program is ready to go back to user mode for the
		 * "first time". Using this technique we ensure that the overhead of 
		 * exec'ing is not captured in the results. This * can be important for 
		 * short running programs.
		 */
		ret = ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		if (ret == -1) {
			warning("cannot ptrace self: %s\n", strerror(errno));
			exit(1);
		}
		if (options.opt_cmd_no_verbose) {
			dup2 (open("/dev/null", O_WRONLY), 1);
			dup2 (open("/dev/null", O_WRONLY), 2);
		}	

		execvp(argv[0], argv);

		warning("cannot exec %s: %s\n", argv[0], strerror(errno));

		exit(1);
		/* NOT REACHED */
	}
	/* 
	 * wait for the child to exec 
	 */
	waitpid(pid, &status, WUNTRACED);

	if (options.opt_verbose) {
		char **p = argv;
		vbprintf("[%d] started task: ", pid);
		while (*p) vbprintf("%s ", *p++);
		vbprintf("\n");
	}

	/*
	 * process is stopped at this point
	 */
	if (WIFEXITED(status)) {
		warning("error cannot monitor task %s(%d): exit status %d\n", argv[0], pid, WEXITSTATUS(status));
		return -1;
	}

	sdesc = pfmon_sdesc_new(PFMON_SDESC_VFORK_INIT, NULL, pid);

	if (sdesc == NULL || pfmon_setup_ptrace(pid)) {
		/* get rid of the task, we cannot proceed */
		status = ptrace(PTRACE_KILL, pid, NULL, NULL);
		if (status != 0) warning("cannot kill task %d: %s\n", pid, strerror(errno));
		if (sdesc)
			pfmon_sdesc_free(sdesc);
		return -1;
	}
	pfmon_sdesc_exec(sdesc);
	return task_pfm_init(sdesc, 1, ctx);
}

static void pfmon_setup_dlopen_brkpt(pfmon_sdesc_t *sdesc);

static pfmon_sdesc_t *
task_attach_one(pfmon_ctx_t *ctx, pid_t tid, pfmon_sdesc_t *parent)
{
	pfmon_sdesc_t *sdesc;
	int status, ret;

	sdesc = pfmon_sdesc_new(PFMON_SDESC_ATTACH, parent, tid);
	if (sdesc == NULL)
		return NULL;


	status = ptrace(PTRACE_ATTACH, tid, NULL, NULL);
	if (status == -1) {
		warning("cannot attach to %d: %s\n", tid, strerror(errno));
		goto error;
	}

	ret = waitpid(tid, &status, WUNTRACED|__WALL);
	if (ret < 0) {
		warning("error attaching to %d : %s\n", tid, strerror(errno));
		ptrace(PTRACE_DETACH, tid, NULL, NULL);
		goto error;
	}

	/*
	 * process is stopped at this point
	 */
	if (WIFEXITED(status)) {
		warning("error command already terminated, exit code %d\n", WEXITSTATUS(status));
		goto error;
	}

	if (pfmon_setup_ptrace(tid)) {
		/* cannot proceed, just detach */
		status = ptrace(PTRACE_DETACH, tid, NULL, NULL);
		if (status != 0)
			warning("cannot detach task %d: %s\n", tid, strerror(errno));
		goto error;
	}
	sdesc->fl_abi32 = program_is_abi32(sdesc->cmd);
	vbprintf("[%d] %s-bit binary\n",
		sdesc->tid,
		sdesc->fl_abi32 ? "32" : "64");

	/*
 	 * only load symbol table for first sessions, other just
 	 * point to it (pfmon_sdesc_update)
 	 */
	if (!parent)
		load_sdesc_symbols(sdesc, 1);
 
	if (options.opt_print_syms)
		print_syms(sdesc);
	/*
	 * install dlopen/dlclose breakpoint in sdesc->code_triggers[0]
	 * also correct sdesc->num_code_triggers
	 */
	sdesc->num_code_triggers = 0;
	sdesc->num_data_triggers = 0;
	pfmon_setup_dlopen_brkpt(sdesc);
	setup_trigger_addresses(sdesc);
	pfmon_disable_all_breakpoints(sdesc->tid);

	vbprintf("[%d] attached to %s...\n", tid, sdesc->cmd);

	if (task_pfm_init(sdesc, 1, ctx))
		return NULL;
	return sdesc;
error:
	/* new sdesc freed by caller when scanning the pid hash */
	return NULL;
}

static int
task_attach(pfmon_ctx_t *ctx, char **argv)
{       
	pfmon_sdesc_t *sdesc, *tmp, *parent = NULL;
	DIR *dir;
	struct dirent *d;
	pid_t tid;
	int i, num_threads = 0;
	int first = 1;
	char path[32];

	sprintf(path, "/proc/%d/task", options.attach_tid);

	dir = opendir(path);
	if (!dir)
		fatal_error("task %d does not exist\n", options.attach_tid);

	while((d=readdir(dir))) {
		if (*d->d_name == '.')
			continue;

		tid = atoi(d->d_name);
		if (options.opt_follow_pthread || tid == options.attach_tid) {
			tmp = task_attach_one(ctx, tid, parent);
			if (!tmp)
				goto error;
			if (first) {
				first = 0;
				parent = tmp;
			}
		}
		num_threads++;
	}

	if (num_threads > 1 && !options.opt_follow_pthread)
		warning("[%d] is multi-threaded(%d threads), attached only to thread [%d],"
				" use --follow-pthread to attach to all threads\n",
				options.attach_tid,
				num_threads,
				options.attach_tid);

	closedir(dir);
	return 0;
error:
	for (i=0; i < PFMON_SDESC_PID_HASH_SIZE; i++) {
		sdesc = sdesc_pid_hash[i];
		while(sdesc) {
			tmp = sdesc->next;
vbprintf("REMOVE [%d] next=%p\n", sdesc->tid, tmp);
			pfmon_sdesc_exit(sdesc);
			sdesc = tmp;
		}
	}
	return -1;
}

static void
task_dispatch_sdesc(pfmon_sdesc_t *sdesc)
{
	task_worker_msg_t msg;
	static unsigned int next_worker;

	/* sanity check */
	if (sdesc->fl_dispatched) fatal_error("[%d] already dispatched error\n", sdesc->tid);

	memset(&msg, 0, sizeof(msg));

	msg.type = PFMON_TASK_MSG_ADD_TASK;	
	msg.data = sdesc;

	sdesc->refcnt++;
	/*
	 * override meaning of sdesc->cpu
	 * here it identifies the worker not the cpu
	 */
	sdesc->cpu = next_worker;
	sdesc->fl_dispatched = 1;

	DPRINT(("[%d] dispatched to worker %u\n", sdesc->tid, next_worker));

	task_worker_send_msg(next_worker, &msg, 0);

	/*
	 * basic round-robin allocation
	 * we created as many workers as there are
	 * online cpus
	 * however online cpus may not have contiguous
	 * numbers.
	 */
	next_worker = (next_worker+1) % options.online_cpus;
}

/*
 * return:
 * 	-1 : error
 * 	 0 : ok
 */
static int
task_pfm_init(pfmon_sdesc_t *sdesc, int from_exec, pfmon_ctx_t *ctx)
{
	task_worker_msg_t msg;
	pid_t tid;
	int has_ctxid, was_monitoring;
	int ret, error;
	int activate_brkpoints = 0;

	tid = sdesc->tid;

	/*
	 * we only take the long path if we are coming from exec, otherwise we inherited
	 * from the parent task. 
	 */
	if (from_exec == 0) {
		/*
		 * parent was active, we need to create our context
		 */
		if (sdesc->fl_monitoring) goto init_pfm;
		/*
		 * parent was not active
		 */
		DPRINT(("keep inactive task [%d] monitoring=%d: %s\n", tid, sdesc->fl_monitoring, sdesc->cmd));
		return 0;
	} 
	/*
	 * we are coming for an exec event
	 */
	DPRINT((" in: [%d] ctxid=%d monitoring=%d refcnt=%d: %s\n", 
		tid, sdesc->ctxid, sdesc->fl_monitoring, sdesc->refcnt,sdesc->cmd));

	/*
	 * in case we do not follow exec, we have to stop right here
	 * sdesc->ppid=-1 denotes the first process. In case we do not follow exec (pattern), 
	 * we always monitor the first process until it exec's.
	 */
	if (options.opt_follow_exec == 0) {
		//ret = sdesc->ppid != -1 || sdesc->exec_count ? 0 : 1;
		ret = sdesc->exec_count ? 0 : 1;
	} else {
		ret = pfmon_sdesc_interesting(sdesc);
	}
	if (ret == 0) {
		vbprintf("[%d] not monitoring %s...\n", sdesc->tid, sdesc->cmd);

		/*
		 * if there was a context attached to the session, then clean up
		 * when split-exec is used. Otherwise, we just stop monitoring
		 * but keep the context around
		 */
		if (sdesc->ctxid != -1) {

			vbprintf("[%d] stopping monitoring at exec\n", tid);

			if (options.opt_split_exec) {
				if (sdesc->fl_monitoring) {
					vbprintf("[%d] collecting results at exec\n", tid);
					task_collect_results(sdesc, 0);
				}

				if (sdesc->fl_dispatched) {
					msg.type = PFMON_TASK_MSG_REM_TASK;
					msg.data = sdesc;

					task_worker_send_msg(sdesc->cpu, &msg, 1);

					sdesc->fl_dispatched = 0;
				}
				close(sdesc->ctxid);
				sdesc->ctxid = -1;
			} else {
				/*
				 * only stop monitoring
				 *
				 * code/data triggers are automatically cleared 
				 * by the kernel on exec()
				 */
				pfmon_stop(sdesc->ctxid, &error);
			}
			/*
			 * monitoring is deactivated
			 */
			sdesc->fl_monitoring = 0;

			pthread_mutex_lock(&task_info_lock);
			task_info.num_active_sdesc--;
			pthread_mutex_unlock(&task_info_lock);

		}
		/* 
		 * cannot be done before we save results
		 */
		sdesc->exec_count++;
		return 0;
	}
	if (options.opt_split_exec && sdesc->ctxid != -1 && sdesc->fl_monitoring) {
		vbprintf("[%d] collecting results at exec\n", tid);
		task_collect_results(sdesc, 0);
	}

	sdesc->exec_count++;

	/*
	 * necessarily in follow-exec mode at this point
	 */

init_pfm:

	vbprintf("[%d] monitoring %s...\n", sdesc->tid, sdesc->cmd);

	was_monitoring = sdesc->fl_monitoring;
	has_ctxid      = sdesc->ctxid != -1;

	/*
	 * we want to monitoring this task
	 */
	sdesc->fl_monitoring = 1;


	/* 
	 * We come either on fork or exec. With the former
	 * we need to create a new context whereas for the latter
	 * it already exists (has_ctxid != 0). 
	 */
	if (has_ctxid == 0) {
		/*
		 * on some architectures (i386,x86-64), debug registers
		 * are systematically inherited by children. We undo did
		 * now to avoid getting spurious breakpoints. Should inheritance
		 * be necessary then use the --trigger-code-follow and
		 * --trigger-data-follow options
		 */
#if 0
		if (sdesc->num_code_triggers || sdesc->num_data_triggers) {
			vbprintf("[%d] disabling all %d breakpoints\n", sdesc->tid, sdesc->num_code_triggers);
			ret = pfmon_disable_all_breakpoints(sdesc->tid);	
			if (ret)
				warning("error: could not disable all breakpoints for %d\n", sdesc->tid);
		}
#endif
		DPRINT(("setup perfmon ctx for [%d] monitoring=%d refcnt=%d: %s\n", 
			tid, sdesc->fl_monitoring, sdesc->refcnt, sdesc->cmd));

		ret = task_setup_pfm_context(sdesc, ctx);
		if (ret == -1)
			return -1;

		/*
		 * we may defer actual activation until later
		 *
		 * If it is not attach mode, pfmon_start() is called after
		 * task_handle_entry/start_trigger().
		 */
		if (options.opt_dont_start == 0 && sdesc->fl_has_entry == 0) {
			pfmon_start(sdesc->ctxid, &error);
			vbprintf("[%d] activating monitoring\n", tid);
		} else {
			vbprintf("[%d] monitoring not activated\n", tid);
		}

	} else {
		/*
		 * come here when the context already exists, i.e., exec
		 *
				 */
		/*
		 * in split-exec mode, we need to reset our context
		 * before we proceed further. We also need to reopen
		 * the output file because it was closed in
		 * task_collect_results()
		 */
		if (options.opt_split_exec) {
			task_reset_pfm_context(sdesc);
			if (open_results(sdesc) == -1) return -1;

			/* monitoring is stopped in task_reset_pfm() because of
			 * unload/reload
			 */
			was_monitoring = 0;
		}
		/*
		 * context was not actively monitoring, then we just
		 * need to restart now
		 */
		if (was_monitoring == 0 && options.opt_dont_start == 0) {
			pfmon_start(sdesc->ctxid, &error);
			vbprintf("[%d] restarting monitoring\n", tid);
		}
	}
	/* 
	 * across fork/pthread_create:
	 * 	you need to use --trigger-code-follow or --trigger-data-follow
	 * 	to inherit breakpoints in the new thread/process.
	 *
	 * across exec:
	 * 	breakpoints can never be inherited across exec. However
	 * 	for every newly create binary image, we need to intercept
	 * 	the entry point to snapshot /proc/PID/maps. Thus every
	 * 	new sdesc has one breakpoint, the entry breakpoint. This
	 * 	breakpoint is also used to trigger to dyanmic insertion of
	 * 	the dlopen breakpoint.
	 *
	 * At this point, num_code_trigger = 1, num_data_trigger = 0
	 */
	if (sdesc->num_code_triggers) {
		ret = install_code_triggers(sdesc);
		if (ret) return ret;
		activate_brkpoints = 1;
	}

	if (sdesc->num_data_triggers) {
		ret = install_data_triggers(sdesc);
		if (ret) return ret;
		activate_brkpoints = 1;
	}	

	if (activate_brkpoints)
		pfmon_enable_all_breakpoints(sdesc->tid);

	if (was_monitoring == 0 || has_ctxid == 0) {
		pthread_mutex_lock(&task_info_lock);
		task_info.num_active_sdesc++;
		if (task_info.num_active_sdesc > task_info.max_active_sdesc) 
			task_info.max_active_sdesc = task_info.num_active_sdesc;
		pthread_mutex_unlock(&task_info_lock);
	}

	DPRINT(("out: [%d] fl_monitoring=%d ctxid=%d was_monitoring=%d has_ctxid=%d\n",
			tid,
			sdesc->fl_monitoring,
			sdesc->ctxid,
			was_monitoring,
			has_ctxid));
	/*
	 * pick a worker thread to manage perfmon notifications, if necessary.
	 */
	if (has_ctxid == 0 && options.opt_use_smpl)
		task_dispatch_sdesc(sdesc);

	if (options.opt_show_rusage)
		gettimeofday(&sdesc->tv_start, NULL);

	return 0;
}

static void
task_pfm_exit(pfmon_sdesc_t *sdesc)
{
	DPRINT(("[%d] TASK_PFM_EXIT\n", sdesc->tid));

	task_collect_results(sdesc, 1);

	if (sdesc->ctxid != -1)
		close(sdesc->ctxid);

	if (options.opt_use_smpl) {
		munmap(sdesc->csmpl.smpl_hdr, sdesc->csmpl.map_size);
		/*
 		 * when eager is on, we process samples at the end of
 		 * the session instead of at the end of the pfmon run
 		 * The incurs more overhead but less pressure on system
 		 * resources such as file descriptors and memory
 		 */
		if (options.opt_eager) {
			if(options.opt_addr2sym)
				pfmon_gather_module_symbols(sdesc);
			pfmon_close_smpl_outfile(sdesc);
		}
	}
	pfmon_sdesc_exit(sdesc);
}

static void
pfmon_setup_dlopen_brkpt(pfmon_sdesc_t *sdesc)
{
	unsigned long brk_func;
	int n;

	brk_func = pfmon_get_dlopen_hook(sdesc);
	if (brk_func == 0) {
		vbprintf("[%d] dlopen hook not found (statically linked?)\n", sdesc->tid);
		return;
	}

	vbprintf("[%d] dlopen hook found @0x%lx\n", sdesc->tid, brk_func);

	/*
	 * append dlopen breakpoint to this list user defined breakpoints
	 */
	n = sdesc->num_code_triggers;
	sdesc->code_triggers[n].brk_address = brk_func;
	sdesc->code_triggers[n].brk_type = PFMON_TRIGGER_DLOPEN;
	sdesc->code_triggers[n].trg_attr_repeat = 1;
	sdesc->num_code_triggers++;
}

static int
task_handle_dlopen_trigger(pfmon_sdesc_t *sdesc, pfmon_trigger_t *trg)
{
	/*
	 * breakpoint is invoked twice on dlopen:
	 * - before the library is added (stop_idx = 1)
	 * - after the library is added (stop_idx = 2)
	 * We want to capture the state with the new library,
	 * thus we wait until stop_idx == 2.
	 * stop_idx is otherwise unused for dlopen, so we use
	 * it as a counter
	 *
	 * this optimization avoids calling load_sdesc_symbols()
	 * twice, because the first time would be useless.
	 */
	trg->brk_stop_idx++;
	if (trg->brk_stop_idx == 2) {
		int mode;
		/*
		 * must force symbol load if sampling
		 * and module is printing samples as they come
		 * instead of deferring until the end of the run
		 * in which case symbols are loaded at the end
		 */
		if (options.opt_use_smpl && (options.smpl_mod->flags & PFMON_SMPL_MOD_FL_OUTFILE))
			mode = 1;
		else
			mode = 0;
		load_sdesc_symbols(sdesc, mode);
		trg->brk_stop_idx = 0;
	}
	return 0;
}

static int
task_handle_entry_trigger(pfmon_sdesc_t *sdesc, pfmon_trigger_t *trg)
{
	load_sdesc_symbols(sdesc, 1);

	if (options.opt_print_syms)
		print_syms(sdesc);

	/*
	 * Clear dont_start flag before setup_strigger_addresses().
	 * It may be set again in the following setup_trigger_addresses()
	 * if start-trigger option is specified.
	 */
	options.opt_dont_start = 0;

	/* remove all existing triggers (incl. entry for code) */
	sdesc->num_code_triggers = 0;
	sdesc->num_data_triggers = 0;

	/* setup dlopen and user defined triggers */
	pfmon_disable_all_breakpoints(sdesc->tid);	
	pfmon_setup_dlopen_brkpt(sdesc);
	setup_trigger_addresses(sdesc);


	if (options.opt_dont_start == 0) {
		int error;
		sdesc->fl_has_entry = 0; /* done with entry breakpoint */
		pfmon_start(sdesc->ctxid, &error);
		vbprintf("[%d] activating monitoring\n", sdesc->tid);
	}
	return 0;
}

static int
task_handle_start_trigger(pfmon_sdesc_t *sdesc, pfmon_trigger_t *trg)
{
	pfmon_trigger_t *stop_trg;
	unsigned long rp;
	pid_t tid;
	int error, ret;

	tid = sdesc->tid;
	/*
	 * check if start breakpoint triggers a dynamic return
	 * stop breakpoint
	 */
	if (trg->brk_stop_idx != -1) {
		pfmon_get_return_pointer(tid, &rp);

		/*
		 * get address of (to be completed) stop breakpoint
		 */
		stop_trg = sdesc->code_triggers+trg->brk_stop_idx;
		stop_trg->brk_address = rp;
		stop_trg->trg_attr_func = 1; /* is dyanmic stop */

		ret = pfmon_set_code_breakpoint(tid, stop_trg);
		if (ret) {
			warning("cannot set dynamic stop breakpoint\n");
			return 0;
		}
		vbprintf("[%d] installed dynamic stop code breakpoint(db%u) %d at %p\n", 
			tid, 
			trg->brk_stop_idx,
			stop_trg->brk_idx,
			stop_trg->brk_address);
	}
	pfmon_start(sdesc->ctxid, &error);
	vbprintf("[%d] activating monitoring at trigger start\n", tid);
	return 0;
}

static int
task_handle_stop_trigger(pfmon_sdesc_t *sdesc, pfmon_trigger_t *trg)
{
	pid_t tid;
	int error;

	tid = sdesc->tid;

	pfmon_stop(sdesc->ctxid, &error);
	vbprintf("[%d] stopping monitoring at trigger stop\n", tid);
	return 0;
}

static int (*trigger_actions[])(pfmon_sdesc_t *s, pfmon_trigger_t *t)={
	task_handle_entry_trigger,
	task_handle_start_trigger,
	task_handle_stop_trigger,
	task_handle_dlopen_trigger
};

static int
task_handle_trigger(pfmon_sdesc_t *sdesc)
{
	pfmon_trigger_t *trg;
	unsigned long addr;
	int is_repeat, is_data = 0;
	pfmon_trigger_t orig_trg;
	int type;
	pid_t tid;
	
	tid = sdesc->tid;
	
	/*
	 * used for SW breakpoints
	 */
	if (sdesc->last_code_trigger) {
		DPRINT(("reinstall brk after SW singlestep\n"));
		pfmon_set_code_breakpoint(tid, sdesc->last_code_trigger);
		sdesc->last_code_trigger = NULL;
		return  0;
	}

	pfmon_get_breakpoint_addr(tid, &addr, &is_data);

	trg = find_trigger(sdesc,
			   is_data ? sdesc->data_triggers : sdesc->code_triggers,
			   is_data ? sdesc->num_data_triggers: sdesc->num_code_triggers,
			   addr);

	if (trg == NULL) {
		warning("task [%d] interrupted @%p for no reason\n", tid, addr);
		return 1; /* error and issue PTRACE_CONT */
	}

	is_repeat = trg->trg_attr_repeat;
	type      = trg->brk_type;
	orig_trg  = *trg;

	vbprintf("[%d] reached %-5s %s breakpoint @%p\n", 
		tid, 
		trigger_strs[type],
		is_data ? "data" : "code",
		addr);

	/*
	 * trigger may be modified by the call
	 */
	trigger_actions[type](sdesc, trg);

	/*
	 * check type of original breakpoint, some handler may reuse the
	 * slots, e.g., TRIGGER_ENTRY.
	 */
	if (is_repeat == 0) {
		vbprintf("[%d] clearing %s breakpoint(db%d) @%p\n", 
			tid, 
			is_data? "data" : "code", 
			orig_trg.brk_idx,
			orig_trg.brk_address);

		if (is_data)
			pfmon_clear_data_breakpoint(tid, &orig_trg);
		else
			pfmon_clear_code_breakpoint(tid, &orig_trg);
	}

	/*
 	 * install main triggers, must be done after clearing above
 	 * deferred form pfmon_handle_entry_trigger()
 	 */
	if (type == PFMON_TRIGGER_ENTRY) {
		install_code_triggers(sdesc);
		install_data_triggers(sdesc);
		pfmon_enable_all_breakpoints(tid);
	}

	/*
	 * dynamic stop breakpoint are systemtically cleared
	 */
	if (trg->trg_attr_func) {
		trg->brk_address = 0;
		trg->trg_attr_func = 0;
	}

	vbprintf("[%d] resume after %s breakpoint\n", tid, is_data ? "data" : "code");

	if (is_data)
		pfmon_resume_after_data_breakpoint(tid, &orig_trg);
	else {
		if (options.opt_hw_brk == 0)
			sdesc->last_code_trigger = trg;
		pfmon_resume_after_code_breakpoint(tid, &orig_trg);
	}
	return 0; /* needs pfmon_continue() */
}

/*
 * task must be stopped when calling
 */
static int                                      
task_detach(pfmon_sdesc_t *sdesc)
{
        task_worker_msg_t msg;
        pid_t pid;
        int error;

        pid = sdesc->tid;

        if (sdesc->ctxid != -1) {
                if (sdesc->fl_dispatched) {
                        msg.type = PFMON_TASK_MSG_REM_TASK;
                        msg.data = sdesc;

                        /* wait for ack */
                        task_worker_send_msg(sdesc->cpu, &msg, 1);
                        pfmon_sdesc_exit(sdesc);
                }
                pfmon_unload_context(sdesc->ctxid, &error);
        }

        uninstall_code_triggers(sdesc);
        uninstall_data_triggers(sdesc);

        /* ensure everything is shutdown */
        pfmon_disable_all_breakpoints(sdesc->tid);

        if (sdesc->ctxid != -1)
                task_pfm_exit(sdesc);
        else
                pfmon_sdesc_exit(sdesc);

        pfmon_detach(pid);
        return 0;
}

int tgkill(pid_t tgid, pid_t pid, int sig)
{
	return syscall(__NR_tgkill, tgid, pid, sig);
}

static void
task_force_exit(void)
{
	pfmon_sdesc_t *t;
	unsigned int i;
	long sig;
	int ret;

	for(i=0; i < PFMON_SDESC_PID_HASH_SIZE; i++) {
		t = sdesc_pid_hash[i];
		while (t) {
			if (t->fl_attached) {
				sig = SIGSTOP;
				t->fl_detaching = 1;
			} else {
				sig = SIGKILL;
			}

			ret = tgkill(t->pid, t->tid, sig);
			vbprintf("sending signal %d to [%d]\n",
				sig, t->tid, ret);
			t = t->next;
		}
	}
}

// this points to the last item, the list is built backwards
static pfmon_sdesc_t *sdesc_chain;

static void
pfmon_sdesc_chain_append(pfmon_sdesc_t *sdesc)
{
	sdesc->next = sdesc_chain;
	sdesc_chain = sdesc;
	DPRINT(("Adding sdesc to task chain: %d - %s (previously: %s)\n", sdesc->pid, sdesc->cmd, sdesc->cmd));
}

static void
pfmon_sdesc_chain_free(void)
{
	pfmon_sdesc_t *sdesc, *next;
	
	for(sdesc=sdesc_chain; sdesc; sdesc = next) {
		next = sdesc->next;
		pfmon_sdesc_free(sdesc);
	}
}

static void
pfmon_sdesc_chain_print(void)
{
	pfmon_sdesc_t *sdesc;

	vbprintf("sdesc chain to process:\n");
	for(sdesc = sdesc_chain; sdesc; sdesc = sdesc->next)
		vbprintf("        [%d] (%s)\n", sdesc->tid, sdesc->cmd);
}

static void
pfmon_sdesc_chain_process(void)
{
	pfmon_sdesc_t *sdesc;

	pfmon_sdesc_chain_print();

	for(sdesc = sdesc_chain; sdesc; sdesc = sdesc->next) {
		if(options.opt_addr2sym)
			pfmon_gather_module_symbols(sdesc);
		if (options.opt_use_smpl)
			pfmon_close_smpl_outfile(sdesc);
	}
	pfmon_sdesc_chain_free();
}

static pid_t
task_handle_ptrace_event(pfmon_sdesc_t *sdesc, int event, char *msg, int cleanup, pfmon_ctx_t *ctx)
{
	unsigned long new_pid; /* must be long due to ptrace */
	pfmon_sdesc_t *new_sdesc;
	int r;

  	/* new pid is really new tid */
  	r = ptrace (PTRACE_GETEVENTMSG, sdesc->tid, NULL, (void *) &new_pid);
	if (r)
		return -1;

	if (cleanup)
		return new_pid;

  	vbprintf ("[%d] %s [%ld]\n", sdesc->tid, msg, new_pid);

	/*
	 * check if pid does not already exist due to the fact that the
	 * ptrace event and child sigstop may come in any order.
	 *
	 * If found, then we need to update the sdesc state with parent
	 * info, create the perfmon state and wakeup the task which
	 * remained stop since we got the SIGSTOP notification
	 */
      	new_sdesc = pfmon_sdesc_pid_hash_find(sdesc_pid_hash, new_pid);
	if (!new_sdesc) {
		new_sdesc = pfmon_sdesc_new(event, sdesc, new_pid);
	} else {
		pfmon_sdesc_update(new_sdesc, event, sdesc);
		r = task_pfm_init(new_sdesc, 0 , ctx);
		if (r) {
			time_to_quit = 1;
			quit_reason  = QUIT_ERROR;
		} else {
			vbprintf("[%d] resuming\n", new_sdesc->tid);
			pfmon_continue(new_sdesc->tid, 0);
		}
	}
	return 0;
}

static void
task_start_all(void)
{
        pfmon_sdesc_t *sdesc;
        unsigned int i;

        /* iterate pfmon_continue over all sdescs */
        for (i=0; i < PFMON_SDESC_PID_HASH_SIZE; i++) {
                sdesc = sdesc_pid_hash[i];
                while(sdesc) {
                        /* actually start the task */
                        pfmon_continue(sdesc->tid, 0);
                        sdesc = sdesc->next;
                }
        }
}

static int
task_mainloop(pfmon_ctx_t *ctx, char **argv)
{	
	pfmon_sdesc_t *sdesc;
	time_t start_time;
	unsigned long sig;
	struct rusage rusage;
	struct timeval tv;
	long new_pid; /* must be long */
	pid_t tid = -1;
	int status, event, wait_type, has_follow;
	int r, has_workers, needs_time, cleaning_up = 0;

	has_workers = options.opt_use_smpl    ? 1 : 0;
	needs_time  = options.opt_show_rusage ? 1 : 0;

	/*
 	 * we mask all signals to avoid issues when attaching to multiple
 	 * thread at the same time. Otherwise, there are cleanup problems
 	 * and thread are not necessarily properly detached
 	 */

	if (options.opt_attach) {
		mask_global_signals();
		r = task_attach(ctx, argv);
		unmask_global_signals();
	} else
		r = task_create(ctx, argv);

	if (r)
		return -1;

	time(&start_time);
	vbprintf("measurements started at %s\n", asctime(localtime(&start_time)));

	task_start_all();

	if (options.session_timeout != PFMON_NO_TIMEOUT) {
		alarm(options.session_timeout);
		vbprintf("arming session alarm to %u seconds\n", options.session_timeout);
	}
	has_follow = options.opt_follow_fork || options.opt_follow_vfork || options.opt_follow_pthread;
	/*
 	 * WUNTRACED: PTtrace events
 	 * WNBOHANG : o not block, return -1 instead
 	 * __WALL   : return info about all threads
 	 */
	wait_type = WUNTRACED|WNOHANG|__WALL;

	work_todo = 1;

	for(;work_todo;) {

		unmask_global_signals();

		sem_wait(&master_work_sem);

		mask_global_signals();

		while (work_todo && (tid = wait4(-1, &status, wait_type, &rusage)) > 0) {

			if (needs_time)
				gettimeofday(&tv, NULL);

			sdesc = pfmon_sdesc_pid_hash_find(sdesc_pid_hash, tid);

			DPRINT(("tid=%d errno=%d exited=%d stopped=%d signaled=%d stopsig=%-2d "
					"ppid=%-6d ctxid=%-3d mon=%d att=%d det=%d quit=%d clean=%d cmd: %s\n",
					tid, errno, 
					WIFEXITED(status), 
					WIFSTOPPED(status), 
					WIFSIGNALED(status), 
					WSTOPSIG(status), 
					sdesc ? sdesc->ppid : -1,
					sdesc ? sdesc->ctxid: -1,
					sdesc ? sdesc->fl_monitoring: 0,
					sdesc ? sdesc->fl_attached: 0,
					sdesc ? sdesc->fl_detaching: 0,
					time_to_quit, cleaning_up,
					sdesc ? sdesc->cmd : ""));

			if (WIFEXITED(status) || WIFSIGNALED(status)) {

				vbprintf("[%d] task exited\n", tid);

				if (!sdesc)
					continue;
				if (has_workers)
					pfmon_sdesc_exit(sdesc);
				else
					task_pfm_exit(sdesc);

				if (needs_time)
					show_task_rusage(&sdesc->tv_start, &tv, &rusage);
				continue;
			}

			/* 
			 * task is stopped
			 */
			sig = WSTOPSIG(status);
			if (sig == SIGTRAP) {
				/*
				 * do not propagate the signal, it was for us
				 */
				sig = 0;

				/*
				 * extract event code from status (should be in some macro)
				 */
				event = status >> 16;
				switch(event) {
					case PTRACE_EVENT_FORK:
						new_pid = task_handle_ptrace_event(sdesc, PFMON_SDESC_FORK, "forked", cleaning_up, ctx);
						if (cleaning_up)
							pfmon_detach(new_pid);
						break;
					case PTRACE_EVENT_CLONE:
						new_pid = task_handle_ptrace_event(sdesc, PFMON_SDESC_CLONE, "cloned", cleaning_up, ctx);
						if (cleaning_up)
							pfmon_detach(new_pid);
						break;
					case PTRACE_EVENT_VFORK:
						new_pid = task_handle_ptrace_event(sdesc, PFMON_SDESC_VFORK, "vforked", cleaning_up, ctx);
						if (cleaning_up)
							pfmon_detach(new_pid);
						break;
					case PTRACE_EVENT_EXEC:
						pfmon_sdesc_exec(sdesc);
						vbprintf("[%d] exec %s...\n", sdesc->tid, sdesc->cmd);

						if (cleaning_up)  break;
						r = task_pfm_init(sdesc, 1, ctx);
						if (r) {
							time_to_quit = 1;
							quit_reason  = QUIT_ERROR;
						}
						break;
					case  0:
						if (cleaning_up) break;

						r = task_handle_trigger(sdesc);
						/* we detached the task, no need for PTRACE_CONT */
						if (r == 1) continue;
						/* need a cont */
						break;
					default: 
						DPRINT((">>got unknown event %d\n", event));
						/*
						 * when a task is ptraced' and executes execve:
						 * 	- if PTRACE_O_TRACEEXEC is set, then we get PTRACE_EVENT_EXEC event
						 * 	- if PTRACE_O_TRACEEXEC is not set, then we just receive a SIGTRAP
						 */
						if (options.opt_follow_exec == 1) 
							printf("unknown ptrace event %d\n", event);
				}
			} else if (sig == SIGSTOP) {

				if (!sdesc) { 
					/*
					 * on new task creation 2 events are generated:
					 * - parent gets a PTRACE event
					 * - new task gets a SIGSTOP
					 * There is no guarantee on the order in which these events are received
					 * by pfmon. Thus, we assume that if we get infos about a task we do not
					 * know, then it means, this is for a newly created task. We create the
					 * sdesc but keep the task blocked until we get the PTRACE event.
					 */
					if (has_follow) {
						vbprintf("[%d] out-of-order creation, stopped\n", tid);
						sdesc = pfmon_sdesc_new(PFMON_SDESC_FORK, NULL, tid);
						sdesc->fl_seen_stopsig = 1;
						continue;
					} else {
						warning("unknown task [%d]\n", tid); 
						continue; 
					}
				} 

				/* 
				 * cancel signal, it was for us
				 *
				 * XXX: it that always the case?
				 */
				sig = 0;

				/*
				 * we need to wait until a newly created task reaches the stopped
				 * state to ensure that perfmon will see the task actually stopped
				 * and not just cloned. We do get two events: fork/vfork/clone and
				 * the first STOPPED signal when the task reaches its first 
				 * notification point.
				 */
				if (sdesc->fl_detaching) {
					task_detach(sdesc);
					continue;
				}
				if (sdesc->fl_seen_stopsig == 0 && sdesc->fl_monitoring) {
					sdesc->fl_seen_stopsig = 1;
					/*
 					 * certain actions need to be deferred
 					 * until new task is stoppped
 					 */
					pfmon_sdesc_update_stopped(sdesc);

					r = task_pfm_init(sdesc, 0, ctx);
					if (r) {
						time_to_quit = 1;
						quit_reason  = QUIT_ERROR;
					}
				}
			} else {
				DPRINT(("forward signal %lu to [%d]\n", sig, tid));
			}
			pfmon_continue(tid, sig);
		}
		DPRINT(("tid=%d errno=%d time_to_quit=%d cleaning_up=%d todo=%d active=%lu\n", 
			tid, errno, time_to_quit, cleaning_up, work_todo,task_info.num_active_sdesc));
		/*
		 * we check for interruption only when we are done processing pending ptrace events
		 */
		if (time_to_quit && cleaning_up == 0) {
			pfmon_print_quit_reason(quit_reason);
			task_force_exit();
			cleaning_up  = 1;
			wait_type |= __WNOTHREAD|__WALL;
		}
	}

	if (options.opt_aggr) {
		if(options.opt_addr2sym)
			pfmon_gather_module_symbols(sdesc);
		sdesc_task_aggr.syms = sdesc->syms;
		if (quit_reason != QUIT_ERROR)
			print_results(&sdesc_task_aggr);
		if (options.opt_use_smpl)
			pfmon_close_smpl_outfile(&sdesc_task_aggr);
		pfmon_sdesc_chain_free();
	} else {
		pfmon_sdesc_chain_process();
	}

	vbprintf("created tasks        : %lu\n"
		 "maximum tasks        : %lu\n"
		 "maximum active tasks : %lu\n", 
		task_info.total_sdesc, 
		task_info.max_sdesc,
		task_info.max_active_sdesc);

	return 0;
}

static
void pfmon_thread_arg_destroy(void *data)
{
	if (data) free(data);
}

static void
exit_per_task(int i)
{
	if (gettid() == master_tid) exit(i);

	pthread_exit((void *)((unsigned long)i));
}

static void
task_worker_mainloop(void *data)
{
	task_worker_t *mywork = (task_worker_t *)data;
	int epfd, fd;
	struct epoll_event ev, *events;
	pfmon_sdesc_t *sdesc;
#ifdef __ia64__
	pfm_msg_t msg_old;
#endif
	pfarg_msg_t msg;
	size_t sz;
	task_worker_msg_t pfmon_msg;
	pid_t mytid;
	unsigned int mycpu;
	unsigned int myjobs = 0;
	int i, ret;
	int ctrl_fd;
	int ndesc, msg_type;

	/*
	 * POSIX threads: 
	 * The signal state of the new thread is initialised as follows:
    	 *    - the signal mask is inherited from the creating thread.
         *    - the set of signals pending for the new thread is empty.
	 *
	 * we want to let the master handle the global signals, therefore
	 * we mask them here.
	 */
	mask_global_signals();


	ctrl_fd = mywork->to_worker[0];
	mycpu   = mywork->cpu_id;
	mytid   = gettid();

	pfmon_pin_self(mycpu);

	/*
	 * some NPTL sanity checks
	 */
	if (mytid == master_tid) 
		fatal_error("pfmon is not compiled/linked with the correct pthread library,"
			"the program is linked with NPTL when it should not. Check Makefile.\n");

	events = calloc(mywork->max_fds, sizeof(struct epoll_event));
	if (!events)
		goto error;

	epfd = epoll_create(mywork->max_fds);
	if (epfd == -1)
		goto error1;

	/* register ctrl_fd */
	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN;
	ret = epoll_ctl(epfd, EPOLL_CTL_ADD, ctrl_fd, &ev);
	if (ret == -1)
		goto error;

	
	DPRINT(("worker [%d] on CPU%u ctrl_fd=%d\n", mytid, mycpu, ctrl_fd));
	for(;;) {
		ndesc = epoll_wait(epfd, events, mywork->max_fds, -1);
		if (ndesc == -1) {
			if (errno == EINTR)
				continue;
			fatal_error("epoll error %d\n", errno);
		}

		DPRINT(("worker on CPU%u ndesc=%d ctrl_fd=%d\n", mycpu, ndesc, ctrl_fd));

		for(i=0; i < ndesc; i++) {

			DPRINT(("worker on CPU%u activity on fd=%d\n", mycpu, i));
			sdesc  = events[i].data.ptr;

			if (sdesc) {
				fd = sdesc->ctxid;
#ifdef __ia64__
				if (options.pfm_version == PERFMON_VERSION_20) {
					ret = read(fd, &msg_old, sizeof(msg_old));
					sz = sizeof(msg_old);
					msg_type = msg_old.type;
				} else
#endif
 				{
					ret = read(fd, &msg, sizeof(msg));
					sz = sizeof(msg);
					msg_type = msg.type;
				}

				if (ret != sz) {
					warning("[%d] error reading on %d: ret=%d errno=%s\n",
						mytid,
						fd,
						ret, strerror(errno));
					continue;
				}
				if (msg_type == PFM_MSG_OVFL) {
					pfmon_process_smpl_buf(sdesc, 0);
					continue;
				}

				if (msg_type != PFM_MSG_END) 
					fatal_error("wCPU%u unknown message type %d\n", mycpu, msg_type);

				ret = epoll_ctl(epfd, EPOLL_CTL_DEL, sdesc->ctxid, NULL);

				myjobs--;

				DPRINT(("wCPU%u end_msg ctxid=%d tid=%d\n",
					mycpu,
					sdesc->ctxid,
					sdesc->tid));

				task_pfm_exit(sdesc);

				continue;
			} 

			ret = read(ctrl_fd, &pfmon_msg, sizeof(pfmon_msg));
			if (ret != sizeof(pfmon_msg)) {
				warning("error reading ctrl_fd(%d) on CPU%u: ret=%d errno=%d\n", ctrl_fd, mycpu, ret, errno);
				continue;
			}

			sdesc = (pfmon_sdesc_t *)pfmon_msg.data;

			switch(pfmon_msg.type) {

				case PFMON_TASK_MSG_ADD_TASK:
					myjobs++;
					DPRINT(("wCPU%u managing [tid=%d:fd=%d] jobs=%u\n", mycpu, sdesc->tid, sdesc->ctxid, myjobs));

					/* register ctrl_fd */
					ev.events = EPOLLIN;
					ev.data.ptr = sdesc;
					ret = epoll_ctl(epfd, EPOLL_CTL_ADD, sdesc->ctxid, &ev);
					if (ret == -1)
						goto error_add;

					break;

				case PFMON_TASK_MSG_REM_TASK:
					myjobs--;
					vbprintf("wCPU%u removing [%d:%d]\n", mycpu, sdesc->tid, sdesc->ctxid);

					ret = epoll_ctl(epfd, EPOLL_CTL_DEL, sdesc->ctxid, NULL);

					/*
					 * ack the removal
					 */
					ret = write(workers[mycpu].from_worker[1], &msg, sizeof(msg));
					if (ret != sizeof(msg))
						warning("cannot ack remove task message\n");
					break;

				default:
					warning("wCPU%u unexpected message %d, size=%d\n", mycpu, pfmon_msg.type, ret);
			}
		}
	}
error1:
	fatal_error("CPU%u worker failed to intialize create\n", mycpu);
error:
	fatal_error("CPU%u worker failed to intialize err=%d\n", mycpu, errno);
error_add:
	fatal_error("CPU%u worker failed to add descriptor %d\n", mycpu, sdesc->ctxid);
}

static void
task_create_workers(void)
{
	int i, j;
	int nfiles, max_fds;
	int ret;

	nfiles = sysconf(_SC_OPEN_MAX);
	max_fds = nfiles / options.online_cpus;

	DPRINT(("nfiles=%lu max_fds=%d\n", nfiles, max_fds));

	workers = malloc(options.online_cpus * sizeof(task_worker_t));
	if (!workers)
		fatal_error("cannot allocate worker table\n");

	for (i=0, j=0; j < options.online_cpus; i++) {

		if (pfmon_bitmask_isset(&options.phys_cpu_mask, i) == 0)
			continue;

		workers[j].cpu_id = i;
		workers[j].max_fds = max_fds;

		if (pipe(workers[j].to_worker) == -1 || pipe(workers[j].from_worker) == -1)
			fatal_error("cannot create control channels for worker for CPU%d\n", i);

		ret = pthread_create(&workers[j].thread_id, NULL, (void *(*)(void *))task_worker_mainloop, workers+j);
		if (ret != 0) 
			fatal_error("cannot create worker thread for CPU%u\n", i);

		j++;
	}
}

static int
pfmon_task_init(void)
{
	master_tid = gettid();

	sem_init(&master_work_sem, 0, 0);

	if (options.opt_aggr) {
		pfmon_clone_sets(options.sets, &sdesc_task_aggr);
		if (pfmon_setup_sdesc_aggr_smpl(&sdesc_task_aggr) == -1) return -1;
	}

	if (options.opt_use_smpl) task_create_workers();

	/*
	 * create thread argument key
	 */
	pthread_key_create(&arg_key, pfmon_thread_arg_destroy);

	register_exit_function(exit_per_task);

	setup_global_signals();

	/*
	 * compile regex once and for all
	 */
	if (options.fexec_pattern) {
		if (regcomp(&follow_exec_preg, options.fexec_pattern, REG_ICASE|REG_NOSUB)) {
			warning("error in regular expression for event \"%s\"\n", options.fexec_pattern);
			return -1;
		}
	}
	vbprintf("exec-pattern=%s\n", options.fexec_pattern ? options.fexec_pattern : "*");
	return 0;
}

static void
task_cleanup(void)
{
	register_exit_function(NULL);
}

int
measure_task(pfmon_ctx_t *ctx, char **argv)
{
	int ret;
	time_t end_time;

	ret = pfmon_task_init();
	if (ret) return ret;

	ret = task_mainloop(ctx, argv);
	if (ret == 0) {
		time(&end_time);
		vbprintf("measurements completed at %s\n", asctime(localtime(&end_time)));
	}
	task_cleanup();

	return ret;
}
