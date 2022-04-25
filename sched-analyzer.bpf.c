/* SPDX-License-Identifier: GPL-2.0 */
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

/*
 * We define multiple ring buffers, one per event.
 */
struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256 * 1024);
} uclamp_rq_rb SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256 * 1024);
} uclamp_task_rb SEC(".maps");

static inline bool entity_is_task(struct sched_entity *se)
{
	if (bpf_core_field_exists(se->my_q))
		return !BPF_CORE_READ(se, my_q);
	else
		return true;
}

SEC("raw_tp/pelt_se_tp")
int BPF_PROG(handle_pelt_se, struct sched_entity *se)
{

	if (entity_is_task(se)) {
		struct task_struct *p = container_of(se, struct task_struct, se);
		unsigned long uclamp_min, uclamp_max, util_avg;
		struct uclamp_task_event *e;
		char comm[TASK_COMM_LEN];

		BPF_CORE_READ_STR_INTO(&comm, p, comm);

		uclamp_min = BPF_CORE_READ_BITFIELD_PROBED(p, uclamp_req[UCLAMP_MIN].value);
		uclamp_max = BPF_CORE_READ_BITFIELD_PROBED(p, uclamp_req[UCLAMP_MAX].value);
		bpf_printk("[%s] Req: uclamp_min = %lu uclamp_max = %lu",
			   comm, uclamp_min, uclamp_max);

		uclamp_min = BPF_CORE_READ_BITFIELD_PROBED(p, uclamp[UCLAMP_MIN].value);
		uclamp_max = BPF_CORE_READ_BITFIELD_PROBED(p, uclamp[UCLAMP_MAX].value);
		bpf_printk("[%s] Eff: uclamp_min = %lu uclamp_max = %lu",
			   comm, uclamp_min, uclamp_max);

		util_avg = BPF_CORE_READ(se, avg.util_avg);

		e = bpf_ringbuf_reserve(&uclamp_task_rb, sizeof(*e), 0);
		if (e) {
			BPF_CORE_READ_STR_INTO(&e->comm, p, comm);
			e->util_avg = util_avg;
			e->uclamp_min = uclamp_min;
			e->uclamp_max = uclamp_max;
			bpf_ringbuf_submit(e, 0);
		}
	} else {
		struct rq *rq = BPF_CORE_READ(se, cfs_rq, rq);
		int cpu = BPF_CORE_READ(rq, cpu);
		struct uclamp_rq_event *e;

		unsigned long uclamp_min = BPF_CORE_READ(rq, uclamp[UCLAMP_MIN].value);
		unsigned long uclamp_max = BPF_CORE_READ(rq, uclamp[UCLAMP_MAX].value);
		unsigned long util_avg = BPF_CORE_READ(se, avg.util_avg);

		bpf_printk("[CPU%d] uclamp_min = %lu uclamp_max = %lu",
			   cpu, uclamp_min, uclamp_max);

		e = bpf_ringbuf_reserve(&uclamp_rq_rb, sizeof(*e), 0);
		if (e) {
			e->cpu = cpu;
			e->util_avg = util_avg;
			e->uclamp_min = uclamp_min;
			e->uclamp_max = uclamp_max;
			bpf_ringbuf_submit(e, 0);
		}
	}

	return 0;
}
