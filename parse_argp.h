/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Qais Yousef */
#ifndef __PARSE_ARGS_H__
#define __PARSE_ARGS_H__
#ifndef __SA_BPF_BUILD
#include <argp.h>
#include <stdbool.h>
#endif

#define TASK_COMM_LEN		16
#define MAX_FILTERS_NUM		128

struct sa_opts {
	/* perfetto opts */
	bool system;
	bool app;
	/* controls */
	char *output;
	const char *output_path;
	long max_size;
	unsigned int num_ftrace_event;
	unsigned int num_atrace_cat;
	char *ftrace_event[MAX_FILTERS_NUM];
	char *atrace_cat[MAX_FILTERS_NUM];
	/* events */
	bool load_avg_cpu;
	bool runnable_avg_cpu;
	bool util_avg_cpu;
	bool load_avg_task;
	bool runnable_avg_task;
	bool util_avg_task;
	bool util_avg_rt;
	bool util_avg_dl;
	bool util_avg_irq;
	bool load_avg_thermal;
	bool util_est_cpu;
	bool util_est_task;
	bool cpu_nr_running;
	bool cpu_freq;
	bool cpu_idle;
	bool softirq;
	bool sched_switch;
	bool load_balance;
	bool ipi;
	bool irq;
	/* filters */
	unsigned int num_pids;
	unsigned int num_comms;
	pid_t pid[MAX_FILTERS_NUM];
	char comm[MAX_FILTERS_NUM][TASK_COMM_LEN];
};

extern struct sa_opts sa_opts;
extern const struct argp argp;

#endif /* __PARSE_ARGS_H__ */
