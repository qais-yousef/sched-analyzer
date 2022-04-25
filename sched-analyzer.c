/* SPDX-License-Identifier: GPL-2.0 */
#include <bpf/libbpf.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>

#include "sched-analyzer-events.h"
#include "sched-analyzer.skel.h"

static volatile bool exiting = false;

static void sig_handler(int sig)
{
	exiting = true;
}

static int handle_uclamp_rq_event(void *ctx, void *data, size_t data_sz)
{
	struct uclamp_rq_event *e = data;
	static FILE *file = NULL;

	if (!file) {
		file = fopen("/tmp/uclamp_rq_event.csv", "w");
		if (!file)
			return 0;
		fprintf(file, "ts,cpu,util,uclamp_min,uclamp_max\n");
	}

	fprintf(file, "%llu,%d,%lu,%lu,%lu\n",
		e->ts,e->cpu, e->util_avg, e->uclamp_min, e->uclamp_max);

	return 0;
}

static int handle_uclamp_task_event(void *ctx, void *data, size_t data_sz)
{
	struct uclamp_task_event *e = data;
	static FILE *file = NULL;

	if (!file) {
		file = fopen("/tmp/uclamp_task_event.csv", "w");
		if (!file)
			return 0;
		fprintf(file, "ts,comm,util,uclamp_min,uclamp_max\n");
	}

	fprintf(file, "%llu,%s,%lu,%lu,%lu\n",
		e->ts,e->comm, e->util_avg, e->uclamp_min, e->uclamp_max);

	return 0;
}

int main(int argc, char **argv)
{
	struct ring_buffer *uclamp_task_rb = NULL;
	struct ring_buffer *uclamp_rq_rb = NULL;
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

	uclamp_rq_rb = ring_buffer__new(bpf_map__fd(skel->maps.uclamp_rq_rb),
					handle_uclamp_rq_event, NULL, NULL);
	if (!uclamp_rq_rb) {
		err = -1;
		fprintf(stderr, "Failed to create uclamp_rq ringbuffer\n");
		goto cleanup;
	}

	uclamp_task_rb = ring_buffer__new(bpf_map__fd(skel->maps.uclamp_task_rb),
					  handle_uclamp_task_event, NULL, NULL);
	if (!uclamp_task_rb) {
		err = -1;
		fprintf(stderr, "Failed to create uclamp_task ringbuffer\n");
		goto cleanup;
	}

	while (!exiting) {
		err = ring_buffer__poll(uclamp_rq_rb, 1000);
		if (err == -EINTR) {
			err = 0;
			break;
		}
		if (err < 0) {
			fprintf(stderr, "Error polling uclamp rq ring buffer: %d\n", err);
			break;
		}

		err = ring_buffer__poll(uclamp_task_rb, 1000);
		if (err == -EINTR) {
			err = 0;
			break;
		}
		if (err < 0) {
			fprintf(stderr, "Error polling uclamp task ring buffer: %d\n", err);
			break;
		}
	}

cleanup:
	ring_buffer__free(uclamp_rq_rb);
	ring_buffer__free(uclamp_task_rb);
	sched_analyzer_bpf__destroy(skel);
	return err < 0 ? -err : 0;
}
