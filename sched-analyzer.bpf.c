/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2022 Qais Yousef */
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

#include "parse_argp.h"
#include "sched-analyzer-events.h"

/*
 * Global variables shared with userspace counterpart.
 */
struct sa_opts sa_opts;

char LICENSE[] SEC("license") = "GPL";

//#define DEBUG
#ifndef DEBUG
#undef bpf_printk
#define bpf_printk(...)
#endif

/*
 * Compatibility defs - for when kernel changes struct fields.
 */
struct task_struct__old {
	int cpu;
} __attribute__((preserve_access_index));

#define RB_SIZE		(256 * 1024)

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 8192);
	__type(key, pid_t);
	__type(value, int);
} sched_switch SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 8192);
	__type(key, int);
	__type(value, u64);
} softirq_entry SEC(".maps");

/*
 * We define multiple ring buffers, one per event.
 */
struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, RB_SIZE);
} rq_pelt_rb SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, RB_SIZE);
} task_pelt_rb SEC(".maps");

struct {
       __uint(type, BPF_MAP_TYPE_RINGBUF);
       __uint(max_entries, RB_SIZE);
} rq_nr_running_rb SEC(".maps");

struct {
       __uint(type, BPF_MAP_TYPE_RINGBUF);
       __uint(max_entries, RB_SIZE);
} sched_switch_rb SEC(".maps");

struct {
       __uint(type, BPF_MAP_TYPE_RINGBUF);
       __uint(max_entries, RB_SIZE);
} freq_idle_rb SEC(".maps");

struct {
       __uint(type, BPF_MAP_TYPE_RINGBUF);
       __uint(max_entries, RB_SIZE);
} softirq_rb SEC(".maps");

static inline bool entity_is_task(struct sched_entity *se)
{
	if (bpf_core_field_exists(se->my_q))
		return !BPF_CORE_READ(se, my_q);
	else
		return true;
}

static inline struct rq *rq_of(struct cfs_rq *cfs_rq)
{
	if (bpf_core_field_exists(cfs_rq->rq))
		return BPF_CORE_READ(cfs_rq, rq);
	else
		return container_of(cfs_rq, struct rq, cfs);
}

static inline bool cfs_rq_is_root(struct cfs_rq *cfs_rq)
{
	struct rq *rq = rq_of(cfs_rq);

	if (rq)
		return &rq->cfs == cfs_rq;
	else
		return false;
}

SEC("raw_tp/pelt_se_tp")
int BPF_PROG(handle_pelt_se, struct sched_entity *se)
{
	if (!sa_opts.util_avg_task)
		return 0;

	if (entity_is_task(se)) {
		struct task_struct *p = container_of(se, struct task_struct, se);
		unsigned long uclamp_min, uclamp_max, util_avg;
		struct task_pelt_event *e;
		char comm[TASK_COMM_LEN];
		int *running, cpu;
		pid_t pid;

		if (bpf_core_field_exists(p->wake_cpu)) {
			cpu = BPF_CORE_READ(p, wake_cpu);
		} else {
			struct task_struct__old *p_old = (void *)p;
			cpu = BPF_CORE_READ(p_old, cpu);
		}
		pid = BPF_CORE_READ(p, pid);
		BPF_CORE_READ_STR_INTO(&comm, p, comm);

		running = bpf_map_lookup_elem(&sched_switch, &pid);

		uclamp_min = BPF_CORE_READ_BITFIELD_PROBED(p, uclamp_req[UCLAMP_MIN].value);
		uclamp_max = BPF_CORE_READ_BITFIELD_PROBED(p, uclamp_req[UCLAMP_MAX].value);
		bpf_printk("[%s] Req: uclamp_min = %lu uclamp_max = %lu",
			   comm, uclamp_min, uclamp_max);

		uclamp_min = BPF_CORE_READ_BITFIELD_PROBED(p, uclamp[UCLAMP_MIN].value);
		uclamp_max = BPF_CORE_READ_BITFIELD_PROBED(p, uclamp[UCLAMP_MAX].value);
		bpf_printk("[%s] Eff: uclamp_min = %lu uclamp_max = %lu",
			   comm, uclamp_min, uclamp_max);

		util_avg = BPF_CORE_READ(se, avg.util_avg);

		e = bpf_ringbuf_reserve(&task_pelt_rb, sizeof(*e), 0);
		if (e) {
			e->ts = bpf_ktime_get_ns();
			e->cpu = cpu;
			e->pid = pid;
			BPF_CORE_READ_STR_INTO(&e->comm, p, comm);
			e->util_avg = util_avg;
			e->uclamp_min = uclamp_min;
			e->uclamp_max = uclamp_max;
			if (running)
				e->running = 1;
			else
				e->running = 0;
			bpf_ringbuf_submit(e, 0);
		}
	}

	return 0;
}

SEC("raw_tp/pelt_cfs_tp")
int BPF_PROG(handle_pelt_cfs, struct cfs_rq *cfs_rq)
{
	if (!sa_opts.util_avg_cpu)
		return 0;

	if (cfs_rq_is_root(cfs_rq)) {
		struct rq *rq = rq_of(cfs_rq);
		int cpu = BPF_CORE_READ(rq, cpu);
		struct rq_pelt_event *e;

		unsigned long uclamp_min = BPF_CORE_READ(rq, uclamp[UCLAMP_MIN].value);
		unsigned long uclamp_max = BPF_CORE_READ(rq, uclamp[UCLAMP_MAX].value);
		unsigned long util_avg = BPF_CORE_READ(cfs_rq, avg.util_avg);

		bpf_printk("cfs: [CPU%d] uclamp_min = %lu uclamp_max = %lu",
			   cpu, uclamp_min, uclamp_max);

		e = bpf_ringbuf_reserve(&rq_pelt_rb, sizeof(*e), 0);
		if (e) {
			e->ts = bpf_ktime_get_ns();
			e->cpu = cpu;
			copy_pelt_type(e->type, type_cfs);
			e->util_avg = util_avg;
			e->uclamp_min = uclamp_min;
			e->uclamp_max = uclamp_max;
			bpf_ringbuf_submit(e, 0);
		}
	}

	return 0;
}

SEC("raw_tp/pelt_rt_tp")
int BPF_PROG(handle_pelt_rt, struct rq *rq)
{
	if (!sa_opts.util_avg_rt)
		return 0;

	int cpu = BPF_CORE_READ(rq, cpu);
	struct rq_pelt_event *e;

	if (!bpf_core_field_exists(rq->avg_rt))
		return 0;

	unsigned long uclamp_min = BPF_CORE_READ(rq, uclamp[UCLAMP_MIN].value);
	unsigned long uclamp_max = BPF_CORE_READ(rq, uclamp[UCLAMP_MAX].value);
	unsigned long util_avg = BPF_CORE_READ(rq, avg_rt.util_avg);

	bpf_printk("rt: [CPU%d] uclamp_min = %lu uclamp_max = %lu",
		   cpu, uclamp_min, uclamp_max);

	e = bpf_ringbuf_reserve(&rq_pelt_rb, sizeof(*e), 0);
	if (e) {
		e->ts = bpf_ktime_get_ns();
		e->cpu = cpu;
		copy_pelt_type(e->type, type_rt);
		e->util_avg = util_avg;
		e->uclamp_min = uclamp_min;
		e->uclamp_max = uclamp_max;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("raw_tp/pelt_dl_tp")
int BPF_PROG(handle_pelt_dl, struct rq *rq)
{
	if (!sa_opts.util_avg_dl)
		return 0;

	int cpu = BPF_CORE_READ(rq, cpu);
	struct rq_pelt_event *e;

	if (!bpf_core_field_exists(rq->avg_dl))
		return 0;

	unsigned long uclamp_min = BPF_CORE_READ(rq, uclamp[UCLAMP_MIN].value);
	unsigned long uclamp_max = BPF_CORE_READ(rq, uclamp[UCLAMP_MAX].value);
	unsigned long util_avg = BPF_CORE_READ(rq, avg_dl.util_avg);

	bpf_printk("dl: [CPU%d] uclamp_min = %lu uclamp_max = %lu",
		   cpu, uclamp_min, uclamp_max);

	e = bpf_ringbuf_reserve(&rq_pelt_rb, sizeof(*e), 0);
	if (e) {
		e->ts = bpf_ktime_get_ns();
		e->cpu = cpu;
		copy_pelt_type(e->type, type_dl);
		e->util_avg = util_avg;
		e->uclamp_min = uclamp_min;
		e->uclamp_max = uclamp_max;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("raw_tp/sched_update_nr_running_tp")
int BPF_PROG(handle_sched_update_nr_running, struct rq *rq, int change)
{
	if (!sa_opts.cpu_nr_running)
		return 0;

	int cpu = BPF_CORE_READ(rq, cpu);
	struct rq_nr_running_event *e;

	int nr_running = BPF_CORE_READ(rq, nr_running);

	bpf_printk("[CPU%d] nr_running = %d change = %d",
		  cpu, nr_running, change);

	e = bpf_ringbuf_reserve(&rq_nr_running_rb, sizeof(*e), 0);
	if (e) {
	       e->ts = bpf_ktime_get_ns();
	       e->cpu = cpu;
	       e->nr_running = nr_running;
	       e->change = change;
	       bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("raw_tp/sched_switch")
int BPF_PROG(handle_sched_switch, bool preempt,
	     struct task_struct *prev, struct task_struct *next)
{
	if (sa_opts.perfetto && !sa_opts.util_avg_task)
		return 0;
	else if (sa_opts.csv && !sa_opts.sched_switch)
		return 0;

	int cpu;
	if (bpf_core_field_exists(prev->wake_cpu)) {
		cpu = BPF_CORE_READ(prev, wake_cpu);
	} else {
		struct task_struct__old *prev_old = (void*)prev;
		cpu = BPF_CORE_READ(prev_old, cpu);
	}
	struct sched_switch_event *e;
	char comm[TASK_COMM_LEN];
	int running = 1;
	pid_t pid;

	pid = BPF_CORE_READ(prev, pid);
	bpf_map_delete_elem(&sched_switch, &pid);

	BPF_CORE_READ_STR_INTO(&comm, prev, comm);
	bpf_printk("[CPU%d] comm = %s running = %d",
		   cpu, comm, 0);

	pid = BPF_CORE_READ(next, pid);
	bpf_map_update_elem(&sched_switch, &pid, &running, BPF_ANY);

	BPF_CORE_READ_STR_INTO(&comm, next, comm);
	bpf_printk("[CPU%d] comm = %s running = %d",
		   cpu, comm, 1);

	e = bpf_ringbuf_reserve(&sched_switch_rb, sizeof(*e), 0);
	if (e) {
		e->ts = bpf_ktime_get_ns();
		e->cpu = cpu;
		e->pid = BPF_CORE_READ(prev, pid);
		BPF_CORE_READ_STR_INTO(&e->comm, prev, comm);
		e->running = 0;
		bpf_ringbuf_submit(e, 0);
	}

	e = bpf_ringbuf_reserve(&sched_switch_rb, sizeof(*e), 0);
	if (e) {
		e->ts = bpf_ktime_get_ns();
		e->cpu = cpu;
		e->pid = BPF_CORE_READ(next, pid);
		BPF_CORE_READ_STR_INTO(&e->comm, next, comm);
		e->running = 1;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("raw_tp/cpu_frequency")
int BPF_PROG(handle_cpu_frequency, unsigned int frequency, unsigned int cpu)
{
	if (!sa_opts.csv || !sa_opts.cpu_freq)
		return 0;

	struct freq_idle_event *e;
	int idle_state = -1;

	bpf_printk("[CPU%d] freq = %u idle_state = %u",
		   cpu, frequency, idle_state);

	e = bpf_ringbuf_reserve(&freq_idle_rb, sizeof(*e), 0);
	if (e) {
		e->ts = bpf_ktime_get_ns();
		e->cpu = cpu;
		e->frequency = frequency;
		e->idle_state = idle_state;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("raw_tp/cpu_idle")
int BPF_PROG(handle_cpu_idle, unsigned int state, unsigned int cpu)
{
	if (!sa_opts.csv || !sa_opts.cpu_idle)
		return 0;

	int idle_state = (int)state;
	unsigned int frequency = 0;
	struct freq_idle_event *e;

	bpf_printk("[CPU%d] freq = %u idle_state = %u",
		   cpu, frequency, idle_state);

	e = bpf_ringbuf_reserve(&freq_idle_rb, sizeof(*e), 0);
	if (e) {
		e->ts = bpf_ktime_get_ns();
		e->cpu = cpu;
		e->frequency = frequency;
		e->idle_state = idle_state;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("raw_tp/softirq_entry")
int BPF_PROG(handle_softirq_entry, unsigned int vec_nr)
{
	if (!sa_opts.csv || !sa_opts.softirq)
		return 0;

	int cpu = bpf_get_smp_processor_id();
	u64 ts = bpf_ktime_get_ns();
	bpf_map_update_elem(&softirq_entry, &cpu, &ts, BPF_ANY);

	return 0;
}

SEC("raw_tp/softirq_exit")
int BPF_PROG(handle_softirq_exit, unsigned int vec_nr)
{
	if (!sa_opts.csv || !sa_opts.softirq)
		return 0;

	int cpu = bpf_get_smp_processor_id();
	u64 exit_ts = bpf_ktime_get_ns();
	struct softirq_event *e;
	u64 entry_ts, *ts;

	ts = bpf_map_lookup_elem(&softirq_entry, &cpu);
	if (!ts)
		return 0;
	bpf_map_delete_elem(&softirq_entry, &cpu);

	entry_ts = *ts;

	e = bpf_ringbuf_reserve(&softirq_rb, sizeof(*e), 0);
	if (e) {
		e->ts = entry_ts;
		e->cpu = cpu;
		copy_softirq(e->softirq, vec_nr);
		e->duration = exit_ts - entry_ts;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}
