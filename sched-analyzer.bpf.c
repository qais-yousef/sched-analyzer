#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "GPL";

SEC("raw_tp/pelt_se_tp")
int BPF_PROG(handle_pelt_se, struct sched_entity *se)
{
	int cpu = BPF_CORE_READ(se, cfs_rq, rq, cpu);

	bpf_printk("[%d] Hello world!", cpu);

	return 0;
}
