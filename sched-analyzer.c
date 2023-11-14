/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2022 Qais Yousef */
#include <bpf/libbpf.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "parse_argp.h"
#include "perfetto_wrapper.h"

#include "sched-analyzer-events.h"
#include "sched-analyzer.skel.h"

#ifdef DEBUG
#define pr_debug(...)	fprintf(__VA_ARGS__)
#else
#define pr_debug(...)
#endif

static volatile bool exiting = false;

static void sig_handler(int sig)
{
	exiting = true;
}

static int handle_rq_pelt_event(void *ctx, void *data, size_t data_sz)
{
	struct rq_pelt_event *e = data;
	static FILE *file = NULL;
	static int file_size = 0;

	if (sa_opts.csv) {
		if (!file) {
			char buffer[128];
			snprintf(buffer, 128, "%s/%s", sa_opts.output_path, "rq_pelt.csv");
			file = fopen(buffer, "w");
			if (!file)
				return 0;
			file_size += fprintf(file, "ts,cpu,type,util,uclamp_min,uclamp_max\n");
		}

		if (file_size < sa_opts.max_size) {
			file_size += fprintf(file, "%llu,%d,%s,%lu,%lu,%lu\n",
					     e->ts, e->cpu, e->type, e->util_avg, e->uclamp_min, e->uclamp_max);
		}
	}

	if (sa_opts.util_avg_cpu && e->util_avg != -1)
		trace_cpu_util_avg(e->ts, e->cpu, e->util_avg);

	if (sa_opts.util_est_cpu && e->util_avg == -1) {
		trace_cpu_util_est_enqueued(e->ts, e->cpu, e->util_est_enqueued);
		trace_cpu_util_est_ewma(e->ts, e->cpu, e->util_est_ewma);
	}

	return 0;
}

static int handle_task_pelt_event(void *ctx, void *data, size_t data_sz)
{
	struct task_pelt_event *e = data;
	static FILE *file = NULL;
	static int file_size = 0;

	if (sa_opts.csv) {
		if (!file) {
			char buffer[128];
			snprintf(buffer, 128, "%s/%s", sa_opts.output_path, "task_pelt.csv");
			file = fopen(buffer, "w");
			if (!file)
				return 0;
			file_size += fprintf(file, "ts,cpu,pid,comm,util,uclamp_min,uclamp_max,running\n");
		}

		if (file_size < sa_opts.max_size) {
			file_size += fprintf(file, "%llu,%d,%d,%s,%lu,%lu,%lu,%d\n",
					     e->ts, e->cpu, e->pid, e->comm, e->util_avg, e->uclamp_min, e->uclamp_max, e->running);
		}
	}

	if (sa_opts.util_avg_task && e->util_avg != -1)
		trace_task_util_avg(e->ts, e->comm, e->pid, e->util_avg);

	if (sa_opts.util_est_task && e->util_avg == -1) {
		trace_task_util_est_enqueued(e->ts, e->comm, e->pid, e->util_est_enqueued);
		trace_task_util_est_ewma(e->ts, e->comm, e->pid, e->util_est_ewma);
	}

	return 0;
}

static int handle_rq_nr_running_event(void *ctx, void *data, size_t data_sz)
{
	struct rq_nr_running_event *e = data;
	static FILE *file = NULL;
	static int file_size = 0;

	if (sa_opts.csv) {
		if (!file) {
			char buffer[128];
			snprintf(buffer, 128, "%s/%s", sa_opts.output_path, "rq_nr_running.csv");
			file = fopen(buffer, "w");
			if (!file)
				return 0;
			file_size += fprintf(file, "ts,cpu,nr_running,change\n");
		}

		if (file_size < sa_opts.max_size) {
			file_size += fprintf(file, "%llu,%d,%d,%d\n",
					     e->ts, e->cpu, e->nr_running, e->change);
		}
	}

	if (sa_opts.cpu_nr_running)
		trace_cpu_nr_running(e->ts, e->cpu, e->nr_running);

	return 0;
}

static int handle_sched_switch_event(void *ctx, void *data, size_t data_sz)
{
	struct sched_switch_event *e = data;
	static FILE *file = NULL;
	static int file_size = 0;

	if (sa_opts.csv) {
		if (!file) {
			char buffer[128];
			snprintf(buffer, 128, "%s/%s", sa_opts.output_path, "sched_switch.csv");
			file = fopen(buffer, "w");
			if (!file)
				return 0;
			file_size += fprintf(file, "ts,cpu,pid,comm,running\n");
		}

		if (file_size < sa_opts.max_size) {
			file_size += fprintf(file, "%llu,%d,%d,%s,%d\n",
					     e->ts, e->cpu, e->pid, e->comm, e->running);
		}
	}

	/* Reset util_avg to 0 for !running */
	if (!e->running && sa_opts.util_avg_task)
		trace_task_util_avg(e->ts, e->comm, e->pid, 0);

	return 0;
}

static int handle_freq_idle_event(void *ctx, void *data, size_t data_sz)
{
	struct freq_idle_event *e = data;
	static FILE *file = NULL;
	static int file_size = 0;

	if (sa_opts.csv) {
		if (!file) {
			char buffer[128];
			snprintf(buffer, 128, "%s/%s", sa_opts.output_path, "freq_idle.csv");
			file = fopen(buffer, "w");
			if (!file)
				return 0;
			file_size += fprintf(file, "ts,cpu,freq,idle_state\n");
		}

		if (file_size < sa_opts.max_size) {
			file_size += fprintf(file, "%llu,%d,%u,%d\n",
					     e->ts, e->cpu, e->frequency, e->idle_state);
		}
	}

	return 0;
}

static int handle_softirq_event(void *ctx, void *data, size_t data_sz)
{
	struct softirq_event *e = data;
	static FILE *file = NULL;
	static int file_size = 0;

	if (sa_opts.csv) {
		if (!file) {
			char buffer[128];
			snprintf(buffer, 128, "%s/%s", sa_opts.output_path, "softirq.csv");
			file = fopen(buffer, "w");
			if (!file)
				return 0;
			file_size += fprintf(file, "ts,cpu,softirq,duration\n");
		}

		if (file_size < sa_opts.max_size) {
			file_size += fprintf(file, "%llu,%d,%s,%lu\n",
					     e->ts, e->cpu, e->softirq, e->duration);
		}
	}

	return 0;
}

#define INIT_EVENT_RB(event)	struct ring_buffer *event##_rb = NULL

#define CREATE_EVENT_RB(event) do {							\
		event##_rb = ring_buffer__new(bpf_map__fd(skel->maps.event##_rb),	\
					      handle_##event##_event, NULL, NULL);	\
		if (!event##_rb) {							\
			err = -1;							\
			fprintf(stderr, "Failed to create " #event " ringbuffer\n");	\
			goto cleanup;							\
		}									\
	} while(0)

#define DESTROY_EVENT_RB(event) do {							\
		ring_buffer__free(event##_rb);						\
	} while(0)

#define POLL_EVENT_RB(event) do {							\
		err = ring_buffer__poll(event##_rb, 1000);				\
		if (err == -EINTR) {							\
			err = 0;							\
			break;								\
		}									\
		if (err < 0) {								\
			fprintf(stderr, "Error polling " #event " ring buffer: %d\n", err); \
			break;								\
		}									\
		pr_debug(stdout, "[" #event "] consumed %d events\n", err);		\
	} while(0)

#define INIT_EVENT_THREAD(event) pthread_t event##_tid

#define CREATE_EVENT_THREAD(event) do {							\
		err = pthread_create(&event##_tid, NULL, event##_thread_fn, NULL);	\
		if (err) {								\
			fprintf(stderr, "Failed to create " #event " thread: %d\n", err); \
			goto cleanup;							\
		}									\
	} while(0)

#define DESTROY_EVENT_THREAD(event) do {						\
		err = pthread_join(event##_tid, NULL);					\
		if (err)								\
			fprintf(stderr, "Failed to destory " #event " thread: %d\n", err); \
	} while(0)

#define EVENT_THREAD_FN(event)								\
	void *event##_thread_fn(void *data)						\
	{										\
		int err;								\
		INIT_EVENT_RB(event);							\
		CREATE_EVENT_RB(event);							\
		while (!exiting) {							\
			POLL_EVENT_RB(event);						\
			usleep(10000);							\
		}									\
	cleanup:									\
		DESTROY_EVENT_RB(event);						\
		return NULL;								\
	}


/*
 * All events require to access this variable to get access to the ringbuffer.
 * Make it available for all event##_thread_fn.
 */
struct sched_analyzer_bpf *skel;

/*
 * Define a pthread function handler for each event
 */
EVENT_THREAD_FN(rq_pelt)
EVENT_THREAD_FN(task_pelt)
EVENT_THREAD_FN(rq_nr_running)
EVENT_THREAD_FN(sched_switch)
EVENT_THREAD_FN(freq_idle)
EVENT_THREAD_FN(softirq)

int main(int argc, char **argv)
{
	INIT_EVENT_THREAD(rq_pelt);
	INIT_EVENT_THREAD(task_pelt);
	INIT_EVENT_THREAD(rq_nr_running);
	INIT_EVENT_THREAD(sched_switch);
	INIT_EVENT_THREAD(freq_idle);
	INIT_EVENT_THREAD(softirq);
	int err;

	err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (err)
		return err;

	if (!sa_opts.output_path)
		sa_opts.output_path = ".";

	init_perfetto();

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	skel = sched_analyzer_bpf__open();
	if (!skel) {
		fprintf(stderr, "Failed to open and load BPF skeleton\n");
		return 1;
	}

	/* Initialize BPF global variables */
	skel->bss->sa_opts = sa_opts;

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

	CREATE_EVENT_THREAD(rq_pelt);
	CREATE_EVENT_THREAD(task_pelt);
	CREATE_EVENT_THREAD(rq_nr_running);
	CREATE_EVENT_THREAD(sched_switch);
	CREATE_EVENT_THREAD(freq_idle);
	CREATE_EVENT_THREAD(softirq);

	printf("Collecting data, CTRL+c to stop\n");

	start_perfetto_trace();

	while (!exiting) {
		sleep(1);
	}

	stop_perfetto_trace();

	if (sa_opts.perfetto)
		printf("\rCollected %s/%s\n", sa_opts.output_path, sa_opts.output);
	else if (sa_opts.csv)
		printf("\rCollected csv files are in %s\n", sa_opts.output_path);

cleanup:
	DESTROY_EVENT_THREAD(rq_pelt);
	DESTROY_EVENT_THREAD(task_pelt);
	DESTROY_EVENT_THREAD(rq_nr_running);
	DESTROY_EVENT_THREAD(sched_switch);
	DESTROY_EVENT_THREAD(freq_idle);
	DESTROY_EVENT_THREAD(softirq);
	sched_analyzer_bpf__destroy(skel);
	return err < 0 ? -err : 0;
}
