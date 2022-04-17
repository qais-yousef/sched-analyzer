#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "GPL";

SEC("tp/sched/sched_pelt_se")
int handle_pelt_se(struct trace_event_raw_sched_pelt_se *ctx)
{
	bpf_trace_printk("Hello world!");

	return 0;
}
