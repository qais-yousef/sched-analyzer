/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2022 Qais Yousef */
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

#include "sched-analyzer-events.h"

char LICENSE[] SEC("license") = "GPL";

//#define DEBUG
#ifndef DEBUG
#undef bpf_printk
#define bpf_printk(...)
#endif

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 8192);
	__type(key, pid_t);
	__type(value, int);
} sched_switch SEC(".maps");

/*
 * We define multiple ring buffers, one per event.
 */
struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256 * 1024);
} rq_pelt_rb SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256 * 1024);
} task_pelt_rb SEC(".maps");

struct {
       __uint(type, BPF_MAP_TYPE_RINGBUF);
       __uint(max_entries, 256 * 1024);
} rq_nr_running_rb SEC(".maps");

struct {
       __uint(type, BPF_MAP_TYPE_RINGBUF);
       __uint(max_entries, 256 * 1024);
} sched_switch_rb SEC(".maps");

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
	if (entity_is_task(se)) {
		struct task_struct *p = container_of(se, struct task_struct, se);
		unsigned long uclamp_min, uclamp_max, util_avg;
		struct task_pelt_event *e;
		char comm[TASK_COMM_LEN];
		int *running, cpu;
		pid_t pid;

		cpu = BPF_CORE_READ(p, cpu);
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
	int cpu = BPF_CORE_READ(prev, cpu);
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
