/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2022 Qais Yousef */
#include <bpf/libbpf.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "sched-analyzer-events.h"
#include "sched-analyzer.skel.h"

static volatile bool exiting = false;

static void sig_handler(int sig)
{
	exiting = true;
}

static int handle_rq_pelt_event(void *ctx, void *data, size_t data_sz)
{
	struct rq_pelt_event *e = data;
	static FILE *file = NULL;

	if (!file) {
		file = fopen("/tmp/rq_pelt.csv", "w");
		if (!file)
			return 0;
		fprintf(file, "ts,cpu,type,util,uclamp_min,uclamp_max\n");
	}

	fprintf(file, "%llu,%d,%s,%lu,%lu,%lu\n",
		e->ts,e->cpu, e->type, e->util_avg, e->uclamp_min, e->uclamp_max);

	return 0;
}

static int handle_task_pelt_event(void *ctx, void *data, size_t data_sz)
{
	struct task_pelt_event *e = data;
	static FILE *file = NULL;

	if (!file) {
		file = fopen("/tmp/task_pelt.csv", "w");
		if (!file)
			return 0;
		fprintf(file, "ts,cpu,pid,comm,util,uclamp_min,uclamp_max,running\n");
	}

	fprintf(file, "%llu,%d,%d,%s,%lu,%lu,%lu,%d\n",
		e->ts, e->cpu, e->pid, e->comm, e->util_avg, e->uclamp_min, e->uclamp_max, e->running);

	return 0;
}

static int handle_rq_nr_running_event(void *ctx, void *data, size_t data_sz)
{
	struct rq_nr_running_event *e = data;
	static FILE *file = NULL;

	if (!file) {
		file = fopen("/tmp/rq_nr_running.csv", "w");
		if (!file)
			return 0;
		fprintf(file, "ts,cpu,nr_running,change\n");
	}

	fprintf(file, "%llu,%d,%d,%d\n",
		e->ts,e->cpu, e->nr_running, e->change);

	return 0;
}

static int handle_sched_switch_event(void *ctx, void *data, size_t data_sz)
{
	struct sched_switch_event *e = data;
	static FILE *file = NULL;

	if (!file) {
		file = fopen("/tmp/sched_switch.csv", "w");
		if (!file)
			return 0;
		fprintf(file, "ts,cpu,pid,comm,running\n");
	}

	fprintf(file, "%llu,%d,%d,%s,%d\n",
		e->ts, e->cpu, e->pid, e->comm, e->running);

	return 0;
}

static int handle_freq_idle_event(void *ctx, void *data, size_t data_sz)
{
	struct freq_idle_event *e = data;
	static FILE *file = NULL;

	if (!file) {
		file = fopen("/tmp/freq_idle.csv", "w");
		if (!file)
			return 0;
		fprintf(file, "ts,cpu,freq,idle_state\n");
	}

	fprintf(file, "%llu,%d,%u,%d\n",
		e->ts, e->cpu, e->frequency, e->idle_state);

	return 0;
}

static int handle_softirq_event(void *ctx, void *data, size_t data_sz)
{
	struct softirq_event *e = data;
	static FILE *file = NULL;

	if (!file) {
		file = fopen("/tmp/softirq.csv", "w");
		if (!file)
			return 0;
		fprintf(file, "ts,cpu,softirq,duration\n");
	}

	fprintf(file, "%llu,%d,%s,%lu\n",
		e->ts, e->cpu, e->softirq, e->duration);

	return 0;
}

int main(int argc, char **argv)
{
	struct ring_buffer *rq_pelt_rb = NULL;
	struct ring_buffer *task_pelt_rb = NULL;
	struct ring_buffer *rq_nr_running_rb = NULL;
	struct ring_buffer *sched_switch_rb = NULL;
	struct ring_buffer *freq_idle_rb = NULL;
	struct ring_buffer *softirq_rb = NULL;
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

	rq_pelt_rb = ring_buffer__new(bpf_map__fd(skel->maps.rq_pelt_rb),
				      handle_rq_pelt_event, NULL, NULL);
	if (!rq_pelt_rb) {
		err = -1;
		fprintf(stderr, "Failed to create rq_pelt ringbuffer\n");
		goto cleanup;
	}

	task_pelt_rb = ring_buffer__new(bpf_map__fd(skel->maps.task_pelt_rb),
					handle_task_pelt_event, NULL, NULL);
	if (!task_pelt_rb) {
		err = -1;
		fprintf(stderr, "Failed to create task_pelt ringbuffer\n");
		goto cleanup;
	}

	rq_nr_running_rb = ring_buffer__new(bpf_map__fd(skel->maps.rq_nr_running_rb),
					    handle_rq_nr_running_event, NULL, NULL);
	if (!rq_nr_running_rb) {
		err = -1;
		fprintf(stderr, "Failed to create rq_nr_running ringbuffer\n");
		goto cleanup;
	}

	sched_switch_rb = ring_buffer__new(bpf_map__fd(skel->maps.sched_switch_rb),
					   handle_sched_switch_event, NULL, NULL);
	if (!sched_switch_rb) {
		err = -1;
		fprintf(stderr, "Failed to create sched_switch_rb ringbuffer\n");
		goto cleanup;
	}

	freq_idle_rb = ring_buffer__new(bpf_map__fd(skel->maps.freq_idle_rb),
					handle_freq_idle_event, NULL, NULL);
	if (!freq_idle_rb) {
		err = -1;
		fprintf(stderr, "Failed to create freq_idle_rb ringbuffer\n");
		goto cleanup;
	}

	softirq_rb = ring_buffer__new(bpf_map__fd(skel->maps.softirq_rb),
				      handle_softirq_event, NULL, NULL);
	if (!softirq_rb) {
		err = -1;
		fprintf(stderr, "Failed to create softirq_rb ringbuffer\n");
		goto cleanup;
	}

	while (!exiting) {
		err = ring_buffer__poll(rq_pelt_rb, 1000);
		if (err == -EINTR) {
			err = 0;
			break;
		}
		if (err < 0) {
			fprintf(stderr, "Error polling rq_pelt ring buffer: %d\n", err);
			break;
		}

		err = ring_buffer__poll(task_pelt_rb, 1000);
		if (err == -EINTR) {
			err = 0;
			break;
		}
		if (err < 0) {
			fprintf(stderr, "Error polling task_pelt ring buffer: %d\n", err);
			break;
		}

		err = ring_buffer__poll(rq_nr_running_rb, 1000);
		if (err == -EINTR) {
			err = 0;
			break;
		}
		if (err < 0) {
			fprintf(stderr, "Error polling rq_nr_running ring buffer: %d\n", err);
			break;
		}

		err = ring_buffer__poll(sched_switch_rb, 1000);
		if (err == -EINTR) {
			err = 0;
			break;
		}
		if (err < 0) {
			fprintf(stderr, "Error polling sched_switch_rb ring buffer: %d\n", err);
			break;
		}

		err = ring_buffer__poll(freq_idle_rb, 1000);
		if (err == -EINTR) {
			err = 0;
			break;
		}
		if (err < 0) {
			fprintf(stderr, "Error polling freq_idle_rb ring buffer: %d\n", err);
			break;
		}

		err = ring_buffer__poll(softirq_rb, 1000);
		if (err == -EINTR) {
			err = 0;
			break;
		}
		if (err < 0) {
			fprintf(stderr, "Error polling softirq_rb ring buffer: %d\n", err);
			break;
		}
	}

cleanup:
	ring_buffer__free(rq_pelt_rb);
	ring_buffer__free(task_pelt_rb);
	ring_buffer__free(rq_nr_running_rb);
	ring_buffer__free(sched_switch_rb);
	ring_buffer__free(freq_idle_rb);
	ring_buffer__free(softirq_rb);
	sched_analyzer_bpf__destroy(skel);
	return err < 0 ? -err : 0;
}
