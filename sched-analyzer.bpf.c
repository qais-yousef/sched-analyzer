/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2022 Qais Yousef */
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

#include "parse_argp.h"
#include "sched-analyzer-events.h"

#define UTIL_AVG_UNCHANGED              0x80000000

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

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 8192);
	__type(key, int);
	__type(value, int);
} lb_map SEC(".maps");

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

struct {
       __uint(type, BPF_MAP_TYPE_RINGBUF);
       __uint(max_entries, RB_SIZE);
} lb_rb SEC(".maps");

struct {
       __uint(type, BPF_MAP_TYPE_RINGBUF);
       __uint(max_entries, RB_SIZE);
} ipi_rb SEC(".maps");

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
		unsigned long uclamp_min, uclamp_max;
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

		uclamp_min = -1;
		uclamp_max = -1;

		if (bpf_core_field_exists(p->uclamp_req[UCLAMP_MIN].value))
			uclamp_min = BPF_CORE_READ_BITFIELD_PROBED(p, uclamp_req[UCLAMP_MIN].value);
		if (bpf_core_field_exists(p->uclamp_req[UCLAMP_MAX].value))
			uclamp_max = BPF_CORE_READ_BITFIELD_PROBED(p, uclamp_req[UCLAMP_MAX].value);

		bpf_printk("[%s] Req: uclamp_min = %lu uclamp_max = %lu",
			   comm, uclamp_min, uclamp_max);

		if (bpf_core_field_exists(p->uclamp[UCLAMP_MIN].value)) {
			bool active = BPF_CORE_READ_BITFIELD_PROBED(p, uclamp[UCLAMP_MIN].active);
			if (active)
				uclamp_min = BPF_CORE_READ_BITFIELD_PROBED(p, uclamp[UCLAMP_MIN].value);
		}
		if (bpf_core_field_exists(p->uclamp[UCLAMP_MAX].value)) {
			bool active = BPF_CORE_READ_BITFIELD_PROBED(p, uclamp[UCLAMP_MAX].active);
			if (active)
				uclamp_max = BPF_CORE_READ_BITFIELD_PROBED(p, uclamp[UCLAMP_MAX].value);
		}

		bpf_printk("[%s] Eff: uclamp_min = %lu uclamp_max = %lu",
			   comm, uclamp_min, uclamp_max);

		e = bpf_ringbuf_reserve(&task_pelt_rb, sizeof(*e), 0);
		if (e) {
			e->ts = bpf_ktime_get_ns();
			e->cpu = cpu;
			e->pid = pid;
			BPF_CORE_READ_STR_INTO(&e->comm, p, comm);
			e->load_avg = BPF_CORE_READ(se, avg.load_avg);
			e->runnable_avg = BPF_CORE_READ(se, avg.runnable_avg);
			e->util_avg = BPF_CORE_READ(se, avg.util_avg);
			e->util_est_enqueued = -1;
			e->util_est_ewma = -1;
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

SEC("raw_tp/sched_util_est_se_tp")
int BPF_PROG(handle_util_est_se, struct sched_entity *se)
{
	if (entity_is_task(se)) {
		struct task_struct *p = container_of(se, struct task_struct, se);
		unsigned long util_est_enqueued, util_est_ewma;
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

		util_est_enqueued = BPF_CORE_READ(se, avg.util_est.enqueued);
		util_est_ewma = BPF_CORE_READ(se, avg.util_est.ewma);

		e = bpf_ringbuf_reserve(&task_pelt_rb, sizeof(*e), 0);
		if (e) {
			e->ts = bpf_ktime_get_ns();
			e->cpu = cpu;
			e->pid = pid;
			BPF_CORE_READ_STR_INTO(&e->comm, p, comm);
			e->load_avg = -1;
			e->runnable_avg = -1;
			e->util_avg = -1;
			e->util_est_enqueued = util_est_enqueued & ~UTIL_AVG_UNCHANGED;
			e->util_est_ewma = util_est_ewma;
			e->uclamp_min = -1;
			e->uclamp_max = -1;
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

		unsigned long uclamp_min = -1;
		unsigned long uclamp_max = -1;

		if (bpf_core_field_exists(rq->uclamp[UCLAMP_MIN].value))
			uclamp_min = BPF_CORE_READ(rq, uclamp[UCLAMP_MIN].value);
		if (bpf_core_field_exists(rq->uclamp[UCLAMP_MAX].value))
			uclamp_max = BPF_CORE_READ(rq, uclamp[UCLAMP_MAX].value);

		bpf_printk("cfs: [CPU%d] uclamp_min = %lu uclamp_max = %lu",
			   cpu, uclamp_min, uclamp_max);

		e = bpf_ringbuf_reserve(&rq_pelt_rb, sizeof(*e), 0);
		if (e) {
			e->ts = bpf_ktime_get_ns();
			e->cpu = cpu;
			e->type = PELT_TYPE_CFS;
			e->load_avg = BPF_CORE_READ(cfs_rq, avg.load_avg);
			e->runnable_avg = BPF_CORE_READ(cfs_rq, avg.runnable_avg);
			e->util_avg = BPF_CORE_READ(cfs_rq, avg.util_avg);
			e->util_est_enqueued = -1;
			e->util_est_ewma = -1;
			e->uclamp_min = uclamp_min;
			e->uclamp_max = uclamp_max;
			bpf_ringbuf_submit(e, 0);
		}
	}

	return 0;
}

SEC("raw_tp/sched_util_est_cfs_tp")
int BPF_PROG(handle_util_est_cfs, struct cfs_rq *cfs_rq)
{
	if (cfs_rq_is_root(cfs_rq)) {
		struct rq *rq = rq_of(cfs_rq);
		int cpu = BPF_CORE_READ(rq, cpu);
		struct rq_pelt_event *e;

		unsigned long util_est_enqueued = BPF_CORE_READ(cfs_rq, avg.util_est.enqueued);
		unsigned long util_est_ewma = BPF_CORE_READ(cfs_rq, avg.util_est.ewma);

		bpf_printk("cfs: [CPU%d] util_est.enqueued = %lu util_est.ewma = %lu",
			   cpu, util_est_enqueued, util_est_ewma);

		e = bpf_ringbuf_reserve(&rq_pelt_rb, sizeof(*e), 0);
		if (e) {
			e->ts = bpf_ktime_get_ns();
			e->cpu = cpu;
			e->load_avg = -1;
			e->runnable_avg = -1;
			e->util_avg = -1;
			e->util_est_enqueued = util_est_enqueued & ~UTIL_AVG_UNCHANGED;
			e->util_est_ewma = util_est_ewma;
			e->uclamp_min = -1;
			e->uclamp_max = -1;
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

	unsigned long util_avg = BPF_CORE_READ(rq, avg_rt.util_avg);

	e = bpf_ringbuf_reserve(&rq_pelt_rb, sizeof(*e), 0);
	if (e) {
		e->ts = bpf_ktime_get_ns();
		e->cpu = cpu;
		e->type = PELT_TYPE_RT;
		e->load_avg = -1;
		e->runnable_avg = -1;
		e->util_avg = util_avg;
		e->util_est_enqueued = -1;
		e->util_est_ewma = -1;
		e->uclamp_min = -1;
		e->uclamp_max = -1;
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

	unsigned long util_avg = BPF_CORE_READ(rq, avg_dl.util_avg);

	e = bpf_ringbuf_reserve(&rq_pelt_rb, sizeof(*e), 0);
	if (e) {
		e->ts = bpf_ktime_get_ns();
		e->cpu = cpu;
		e->type = PELT_TYPE_DL;
		e->load_avg = -1;
		e->runnable_avg = -1;
		e->util_avg = util_avg;
		e->util_est_enqueued = -1;
		e->util_est_ewma = -1;
		e->uclamp_min = -1;
		e->uclamp_max = -1;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("raw_tp/pelt_irq_tp")
int BPF_PROG(handle_pelt_irq, struct rq *rq)
{
	int cpu = BPF_CORE_READ(rq, cpu);
	struct rq_pelt_event *e;

	if (!bpf_core_field_exists(rq->avg_irq))
		return 0;

	unsigned long util_avg = BPF_CORE_READ(rq, avg_irq.util_avg);

	e = bpf_ringbuf_reserve(&rq_pelt_rb, sizeof(*e), 0);
	if (e) {
		e->ts = bpf_ktime_get_ns();
		e->cpu = cpu;
		e->type = PELT_TYPE_IRQ;
		e->load_avg = -1;
		e->runnable_avg = -1;
		e->util_avg = util_avg;
		e->util_est_enqueued = -1;
		e->util_est_ewma = -1;
		e->uclamp_min = -1;
		e->uclamp_max = -1;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("raw_tp/pelt_thermal_tp")
int BPF_PROG(handle_pelt_thermal, struct rq *rq)
{
	int cpu = BPF_CORE_READ(rq, cpu);
	struct rq_pelt_event *e;

	if (!bpf_core_field_exists(rq->avg_thermal))
		return 0;

	unsigned long util_avg = BPF_CORE_READ(rq, avg_thermal.util_avg);

	e = bpf_ringbuf_reserve(&rq_pelt_rb, sizeof(*e), 0);
	if (e) {
		e->ts = bpf_ktime_get_ns();
		e->cpu = cpu;
		e->type = PELT_TYPE_THERMAL;
		e->load_avg = -1;
		e->runnable_avg = -1;
		e->util_avg = util_avg;
		e->util_est_enqueued = -1;
		e->util_est_ewma = -1;
		e->uclamp_min = -1;
		e->uclamp_max = -1;
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

SEC("raw_tp/sched_process_free")
int BPF_PROG(handle_sched_process_free, struct task_struct *p)
{
	struct task_pelt_event *e;
	char comm[TASK_COMM_LEN];
	pid_t pid;
	int cpu;

	if (bpf_core_field_exists(p->wake_cpu)) {
		cpu = BPF_CORE_READ(p, wake_cpu);
	} else {
		struct task_struct__old *p_old = (void *)p;
		cpu = BPF_CORE_READ(p_old, cpu);
	}
	pid = BPF_CORE_READ(p, pid);
	BPF_CORE_READ_STR_INTO(&comm, p, comm);

	e = bpf_ringbuf_reserve(&task_pelt_rb, sizeof(*e), 0);
	if (e) {
		e->ts = bpf_ktime_get_ns();
		e->cpu = cpu;
		e->pid = pid;
		BPF_CORE_READ_STR_INTO(&e->comm, p, comm);
		e->load_avg = 0;
		e->runnable_avg = 0;
		e->util_avg = -0;
		e->util_est_enqueued = 0;
		e->util_est_ewma = 0;
		e->uclamp_min = 0;
		e->uclamp_max = 0;
		e->running = 0;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("raw_tp/cpu_frequency")
int BPF_PROG(handle_cpu_frequency, unsigned int frequency, unsigned int cpu)
{
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
		e->idle_miss = 0;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("raw_tp/cpu_idle")
int BPF_PROG(handle_cpu_idle, unsigned int state, unsigned int cpu)
{
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
		e->idle_miss = 0;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("raw_tp/cpu_idle_miss")
int BPF_PROG(handle_cpu_idle_miss, unsigned int cpu,
	     unsigned int state, bool below)
{
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
		e->idle_miss = below ? -1 : 1;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("raw_tp/softirq_entry")
int BPF_PROG(handle_softirq_entry, unsigned int vec_nr)
{
	int cpu = bpf_get_smp_processor_id();
	u64 ts = bpf_ktime_get_ns();
	bpf_map_update_elem(&softirq_entry, &cpu, &ts, BPF_ANY);

	return 0;
}

SEC("raw_tp/softirq_exit")
int BPF_PROG(handle_softirq_exit, unsigned int vec_nr)
{
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

SEC("kprobe/_nohz_idle_balance.isra.0")
int BPF_PROG(handle_nohz_idle_balance_entry, struct rq *rq)
{
	int this_cpu = bpf_get_smp_processor_id();
	int lb_cpu = BPF_CORE_READ(rq, cpu);
	u64 ts = bpf_ktime_get_ns();
	struct lb_event *e;

	int key = LB_NOHZ_IDLE_BALANCE << 16 | this_cpu;
	bpf_map_update_elem(&lb_map, &key, &lb_cpu, BPF_ANY);

	e = bpf_ringbuf_reserve(&lb_rb, sizeof(*e), 0);
	if (e) {
		e->ts = ts;
		e->this_cpu = this_cpu;
		e->lb_cpu = lb_cpu;
		e->phase = LB_NOHZ_IDLE_BALANCE;
		e->entry = true;
		e->misfit_task_load = -1;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("kretprobe/_nohz_idle_balance.isra.0")
int BPF_PROG(handle_nohz_idle_balance_exit)
{
	int this_cpu = bpf_get_smp_processor_id();
	u64 ts = bpf_ktime_get_ns();
	struct lb_event *e;

	int key = LB_NOHZ_IDLE_BALANCE << 16 | this_cpu;
	int *lb_cpu = bpf_map_lookup_elem(&lb_map, &key);
	if (!lb_cpu)
		return 0;
	bpf_map_delete_elem(&lb_map, &key);

	e = bpf_ringbuf_reserve(&lb_rb, sizeof(*e), 0);
	if (e) {
		e->ts = ts;
		e->this_cpu = this_cpu;
		e->lb_cpu = *lb_cpu;
		e->phase = LB_NOHZ_IDLE_BALANCE;
		e->entry = false;
		e->misfit_task_load = -1;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("kprobe/run_rebalance_domains")
int BPF_PROG(handle_run_rebalance_domains_entry)
{
	int this_cpu = bpf_get_smp_processor_id();
	u64 ts = bpf_ktime_get_ns();
	struct lb_event *e;

	e = bpf_ringbuf_reserve(&lb_rb, sizeof(*e), 0);
	if (e) {
		e->ts = ts;
		e->this_cpu = this_cpu;
		e->lb_cpu = this_cpu;
		e->phase = LB_RUN_REBALANCE_DOMAINS;
		e->entry = true;
		e->misfit_task_load = -1;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("kretprobe/run_rebalance_domains")
int BPF_PROG(handle_run_rebalance_domains_exit)
{
	int this_cpu = bpf_get_smp_processor_id();
	u64 ts = bpf_ktime_get_ns();
	struct lb_event *e;

	e = bpf_ringbuf_reserve(&lb_rb, sizeof(*e), 0);
	if (e) {
		e->ts = ts;
		e->this_cpu = this_cpu;
		e->lb_cpu = this_cpu;
		e->phase = LB_RUN_REBALANCE_DOMAINS;
		e->entry = false;
		e->misfit_task_load = -1;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

static void gen_sched_domain_stats(struct rq *rq, enum cpu_idle_type idle,
				   struct lb_sd_stats *sd_stats)
{
	struct sched_domain *sd = BPF_CORE_READ(rq, sd);
	int cpu = BPF_CORE_READ(rq, cpu);
	int i = 0;

	bool sched_idle = BPF_CORE_READ(rq, nr_running) == BPF_CORE_READ(rq, cfs.idle_h_nr_running);
	sched_idle = sched_idle && BPF_CORE_READ(rq, nr_running);
	bool busy = idle != CPU_IDLE && !sched_idle;

	sd_stats->cpu = cpu;

	while (sd && i < MAX_SD_LEVELS) {
		unsigned int interval = BPF_CORE_READ(sd, balance_interval);
		if (busy)
			interval *= BPF_CORE_READ(sd, busy_factor);

		sd_stats->level[i] = BPF_CORE_READ(sd, level);
		sd_stats->balance_interval[i] = interval;

		sd = BPF_CORE_READ(sd, parent);
		i++;
	}

	sd_stats->level[i] = -1;
	sd_stats->balance_interval[i] = 0;
}

SEC("kprobe/rebalance_domains")
int BPF_PROG(handle_rebalance_domains_entry, struct rq *rq, enum cpu_idle_type idle)
{
	int this_cpu = bpf_get_smp_processor_id();
	int lb_cpu = BPF_CORE_READ(rq, cpu);
	u64 ts = bpf_ktime_get_ns();
	struct lb_event *e;

	int key = LB_REBALANCE_DOMAINS << 16 | this_cpu;
	bpf_map_update_elem(&lb_map, &key, &lb_cpu, BPF_ANY);

	e = bpf_ringbuf_reserve(&lb_rb, sizeof(*e), 0);
	if (e) {
		e->ts = ts;
		e->this_cpu = this_cpu;
		e->lb_cpu = lb_cpu;
		e->phase = LB_REBALANCE_DOMAINS;
		e->entry = true;
		e->misfit_task_load = BPF_CORE_READ(rq, misfit_task_load);
		gen_sched_domain_stats(rq, idle, &e->sd_stats);
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("kretprobe/rebalance_domains")
int BPF_PROG(handle_rebalance_domains_exit)
{
	int this_cpu = bpf_get_smp_processor_id();
	u64 ts = bpf_ktime_get_ns();
	struct lb_event *e;

	int key = LB_REBALANCE_DOMAINS << 16 | this_cpu;
	int *lb_cpu = bpf_map_lookup_elem(&lb_map, &key);
	if (!lb_cpu)
		return 0;
	bpf_map_delete_elem(&lb_map, &key);

	e = bpf_ringbuf_reserve(&lb_rb, sizeof(*e), 0);
	if (e) {
		e->ts = ts;
		e->this_cpu = this_cpu;
		e->lb_cpu = *lb_cpu;
		e->phase = LB_REBALANCE_DOMAINS;
		e->entry = false;
		e->misfit_task_load = -1;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("kprobe/balance_fair")
int BPF_PROG(handle_balance_fair_entry, struct rq *rq)
{
	int this_cpu = bpf_get_smp_processor_id();
	int lb_cpu = BPF_CORE_READ(rq, cpu);
	u64 ts = bpf_ktime_get_ns();
	struct lb_event *e;

	int key = LB_BALANCE_FAIR << 16 | this_cpu;
	bpf_map_update_elem(&lb_map, &key, &lb_cpu, BPF_ANY);

	e = bpf_ringbuf_reserve(&lb_rb, sizeof(*e), 0);
	if (e) {
		e->ts = ts;
		e->this_cpu = this_cpu;
		e->lb_cpu = lb_cpu;
		e->phase = LB_BALANCE_FAIR;
		e->entry = true;
		e->misfit_task_load = BPF_CORE_READ(rq, misfit_task_load);
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("kretprobe/balance_fair")
int BPF_PROG(handle_balance_fair_exit)
{
	int this_cpu = bpf_get_smp_processor_id();
	u64 ts = bpf_ktime_get_ns();
	struct lb_event *e;

	int key = LB_BALANCE_FAIR << 16 | this_cpu;
	int *lb_cpu = bpf_map_lookup_elem(&lb_map, &key);
	if (!lb_cpu)
		return 0;
	bpf_map_delete_elem(&lb_map, &key);

	e = bpf_ringbuf_reserve(&lb_rb, sizeof(*e), 0);
	if (e) {
		e->ts = ts;
		e->this_cpu = this_cpu;
		e->lb_cpu = *lb_cpu;
		e->phase = LB_BALANCE_FAIR;
		e->entry = false;
		e->misfit_task_load = -1;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("kprobe/pick_next_task_fair")
int BPF_PROG(handle_pick_next_task_fair_entry, struct rq *rq)
{
	int this_cpu = bpf_get_smp_processor_id();
	int lb_cpu = BPF_CORE_READ(rq, cpu);
	u64 ts = bpf_ktime_get_ns();
	struct lb_event *e;

	int key = LB_PICK_NEXT_TASK_FAIR << 16 | this_cpu;
	bpf_map_update_elem(&lb_map, &key, &lb_cpu, BPF_ANY);

	e = bpf_ringbuf_reserve(&lb_rb, sizeof(*e), 0);
	if (e) {
		e->ts = ts;
		e->this_cpu = this_cpu;
		e->lb_cpu = lb_cpu;
		e->phase = LB_PICK_NEXT_TASK_FAIR;
		e->entry = true;
		e->misfit_task_load = BPF_CORE_READ(rq, misfit_task_load);
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("kretprobe/pick_next_task_fair")
int BPF_PROG(handle_pick_next_task_fair_exit)
{
	int this_cpu = bpf_get_smp_processor_id();
	u64 ts = bpf_ktime_get_ns();
	struct lb_event *e;

	int key = LB_PICK_NEXT_TASK_FAIR << 16 | this_cpu;
	int *lb_cpu = bpf_map_lookup_elem(&lb_map, &key);
	if (!lb_cpu)
		return 0;
	bpf_map_delete_elem(&lb_map, &key);

	e = bpf_ringbuf_reserve(&lb_rb, sizeof(*e), 0);
	if (e) {
		e->ts = ts;
		e->this_cpu = this_cpu;
		e->lb_cpu = *lb_cpu;
		e->phase = LB_PICK_NEXT_TASK_FAIR;
		e->entry = false;
		e->misfit_task_load = -1;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("kprobe/newidle_balance")
int BPF_PROG(handle_newidle_balance_entry, struct rq *rq)
{
	int this_cpu = bpf_get_smp_processor_id();
	int lb_cpu = BPF_CORE_READ(rq, cpu);
	u64 ts = bpf_ktime_get_ns();
	struct lb_event *e;

	int key = LB_NEWIDLE_BALANCE << 16 | this_cpu;
	bpf_map_update_elem(&lb_map, &key, &lb_cpu, BPF_ANY);

	e = bpf_ringbuf_reserve(&lb_rb, sizeof(*e), 0);
	if (e) {
		e->ts = ts;
		e->this_cpu = this_cpu;
		e->lb_cpu = lb_cpu;
		e->phase = LB_NEWIDLE_BALANCE;
		e->entry = true;
		e->misfit_task_load = BPF_CORE_READ(rq, misfit_task_load);
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("kretprobe/newidle_balance")
int BPF_PROG(handle_newidle_balance_exit)
{
	int this_cpu = bpf_get_smp_processor_id();
	u64 ts = bpf_ktime_get_ns();
	struct lb_event *e;

	int key = LB_NEWIDLE_BALANCE << 16 | this_cpu;
	int *lb_cpu = bpf_map_lookup_elem(&lb_map, &key);
	if (!lb_cpu)
		return 0;
	bpf_map_delete_elem(&lb_map, &key);

	e = bpf_ringbuf_reserve(&lb_rb, sizeof(*e), 0);
	if (e) {
		e->ts = ts;
		e->this_cpu = this_cpu;
		e->lb_cpu = *lb_cpu;
		e->phase = LB_NEWIDLE_BALANCE;
		e->entry = false;
		e->misfit_task_load = -1;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("kprobe/load_balance")
int BPF_PROG(handle_load_balance_entry, int lb_cpu, struct rq *lb_rq,
	     struct sched_domain *sd, enum cpu_idle_type idle)
{
	int this_cpu = bpf_get_smp_processor_id();
	u64 ts = bpf_ktime_get_ns();
	struct lb_event *e;

	int key = LB_LOAD_BALANCE << 16 | this_cpu;
	bpf_map_update_elem(&lb_map, &key, &lb_cpu, BPF_ANY);

	e = bpf_ringbuf_reserve(&lb_rb, sizeof(*e), 0);
	if (e) {
		e->ts = ts;
		e->this_cpu = this_cpu;
		e->lb_cpu = lb_cpu;
		e->phase = LB_LOAD_BALANCE;
		e->entry = true;
		e->misfit_task_load = BPF_CORE_READ(lb_rq, misfit_task_load);
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("kretprobe/load_balance")
int BPF_PROG(handle_load_balance_exit)
{
	int this_cpu = bpf_get_smp_processor_id();
	u64 ts = bpf_ktime_get_ns();
	struct lb_event *e;

	int key = LB_LOAD_BALANCE << 16 | this_cpu;
	int *lb_cpu = bpf_map_lookup_elem(&lb_map, &key);
	if (!lb_cpu)
		return 0;
	bpf_map_delete_elem(&lb_map, &key);

	e = bpf_ringbuf_reserve(&lb_rb, sizeof(*e), 0);
	if (e) {
		e->ts = ts;
		e->this_cpu = this_cpu;
		e->lb_cpu = *lb_cpu;
		e->phase = LB_LOAD_BALANCE;
		e->entry = false;
		e->misfit_task_load = -1;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}

SEC("raw_tp/ipi_send_cpu")
int BPF_PROG(handle_ipi_send_cpu, int cpu, void *callsite, void *callback)
{
	u64 ts = bpf_ktime_get_ns();
	struct ipi_event *e;

	e = bpf_ringbuf_reserve(&ipi_rb, sizeof(*e), 0);
	if (e) {
		e->ts = ts;
		e->cpu = cpu;
		e->callsite = callsite;
		e->callback = callback;
		bpf_ringbuf_submit(e, 0);
	}

	return 0;
}
