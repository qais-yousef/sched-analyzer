/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Qais Yousef */
#ifndef __PARSE_ARGS_H__
#define __PARSE_ARGS_H__
#ifndef __SA_BPF_BUILD
#include <argp.h>
#include <stdbool.h>
#endif

struct sa_opts {
	/* perfetto opts */
	bool system;
	bool app;
	/* controls */
	char *output;
	const char *output_path;
	long max_size;
	/* events */
	bool util_avg_cpu;
	bool util_avg_task;
	bool util_avg_rt;
	bool util_avg_dl;
	bool util_avg_irq;
	bool util_avg_thermal;
	bool util_est_cpu;
	bool util_est_task;
	bool cpu_nr_running;
	bool cpu_freq;
	bool cpu_idle;
	bool softirq;
	bool sched_switch;
};

extern struct sa_opts sa_opts;
extern const struct argp argp;

#endif /* __PARSE_ARGS_H__ */
