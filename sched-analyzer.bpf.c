#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "GPL";

SEC("raw_tp/pelt_se_tp")
int BPF_PROG(handle_pelt_se, struct sched_entity *se)
{
	struct rq *rq = BPF_CORE_READ(se, cfs_rq, rq);
	int cpu = BPF_CORE_READ(rq, cpu);
	unsigned long uclamp_min = BPF_CORE_READ(rq, uclamp[UCLAMP_MIN].value);
	unsigned long uclamp_max = BPF_CORE_READ(rq, uclamp[UCLAMP_MAX].value);

	bpf_printk("[%d] uclamp_min = %lu uclamp_max = %lu",
		   cpu, uclamp_min, uclamp_max);

	return 0;
}
