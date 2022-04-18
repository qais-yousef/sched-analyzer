#include <bpf/libbpf.h>
#include <signal.h>
#include <stdio.h>

#include "sched-analyzer.skel.h"

static volatile bool exiting = false;

static void sig_handler(int sig)
{
	exiting = true;
}

int main(int argc, char **argv)
{
	struct sched_analyzer_bpf *skel;
	int err;

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	skel = sched_analyzer_bpf__open();
	if (!skel) {
		fprintf(stderr, "Failed to open and load BPF skeleton\n");
		return 1;
	}

	err = sched_analyzer_bpf__load(skel);
	if (err) {
		fprintf(stderr, "Failed to load and verify BPF skeleton\n");
		goto cleanup;
	}

	err = sched_analyzer_bpf__attach(skel);
	if (err) {
		fprintf(stderr, "Failed to attach BPF skeleton\n");
		goto cleanup;
	}

	while (!exiting);

cleanup:
	sched_analyzer_bpf__destroy(skel);
	return err < 0 ? -err : 0;
}
