/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Qais Yousef */
#include "parse_argp.h"

const char *argp_program_version = "sched-analyzer 0.1";
const char *argp_program_bug_address = "<qyousef@layalina.io>";

static char doc[] =
"Extract scheduer data using BPF and emit them into csv file or perfetto events";

struct sa_opts sa_opts = {
	/* modes */
	.perfetto = true,
	.csv = false,
	/* events */
	.util_avg_cpu = false,
	.util_avg_task = false,
	.util_avg_irq = false,
	.util_avg_rt = false,
	.util_avg_dl = false,
	.util_est_cpu = false,
	.util_est_task = false,
	.util_est_irq = false,
	.util_est_rt = false,
	.util_est_dl = false,
	.cpu_nr_running = false,
	.cpu_freq = false,
	.cpu_idle = false,
	.soft_irq = false,
	.sched_switch = false,
};

#define OPT_UTIL_AVG			-1
#define OPT_UTIL_AVG_CPU		-2
#define OPT_UTIL_AVG_TASK		-3
#define OPT_UTIL_AVG_IRQ		-4
#define OPT_UTIL_AVG_RT			-5
#define OPT_UTIL_AVG_DL			-6
#define OPT_UTIL_EST			-7
#define OPT_UTIL_EST_CPU		-8
#define OPT_UTIL_EST_TASK		-9
#define OPT_CPU_NR_RUNNING		-10
#define OPT_CPU_FREQ			-11
#define OPT_CPU_IDLE			-12
#define OPT_SOFT_IRQ			-13
#define OPT_SCHED_SWITCH		-14

static const struct argp_option options[] = {
	{ "perfetto", 'p', 0, 0, "Emit perfetto events and collect a trace. Requires traced and traced_probes to be running. (default)" },
	{ "csv", 'c', 0, 0, "Produce CSV files of collected data." },
	{ "util_avg", OPT_UTIL_AVG, 0, 0, "Collect util_avg for CPU, tasks, irq, dl and rt." },
	{ "util_avg_cpu", OPT_UTIL_AVG_CPU, 0, 0, "Collect util_avg for CPU." },
	{ "util_avg_task", OPT_UTIL_AVG_TASK, 0, 0, "Collect util_avg for tasks." },
	{ "util_avg_irq", OPT_UTIL_AVG_IRQ, 0, 0, "Collect util_avg for irq." },
	{ "util_avg_rt", OPT_UTIL_AVG_RT, 0, 0, "Collect util_avg for rt." },
	{ "util_avg_dl", OPT_UTIL_AVG_DL, 0, 0, "Collect util_avg for dl." },
	{ "util_est", OPT_UTIL_EST, 0, 0, "Collect util_est for CPU and tasks." },
	{ "util_est_cpu", OPT_UTIL_EST_CPU, 0, 0, "Collect util_est for CPU." },
	{ "util_est_task", OPT_UTIL_EST_TASK, 0, 0, "Collect util_est for tasks." },
	{ "cpu_nr_running", OPT_CPU_NR_RUNNING, 0, 0, "Collect nr_running tasks for each CPU." },
	{ "cpu_freq", OPT_CPU_FREQ, 0, 0, "Collect cpu frequency changes, csv mode only." },
	{ "cpu_idle", OPT_CPU_IDLE, 0, 0, "Collect cpu idle changes, csv mode only." },
	{ "soft_irq", OPT_SOFT_IRQ, 0, 0, "Collect soft irq duration, csv mode only." },
	{ "sched_switch", OPT_SCHED_SWITCH, 0, 0, "Collect sched_switch events, csv mode only." },
	{ 0 },
};

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
	switch (key) {
	case 'p':
		sa_opts.perfetto = true;
		sa_opts.csv = false;
		break;
	case 'c':
		sa_opts.perfetto = false;
		sa_opts.csv = true;
		break;
	case OPT_UTIL_AVG:
		sa_opts.util_avg_cpu = true;
		sa_opts.util_avg_task = true;
		sa_opts.util_avg_irq = true;
		sa_opts.util_avg_rt = true;
		sa_opts.util_avg_dl = true;
		break;
	case OPT_UTIL_AVG_CPU:
		sa_opts.util_avg_cpu = true;
		break;
	case OPT_UTIL_AVG_TASK:
		sa_opts.util_avg_task = true;
		break;
	case OPT_UTIL_AVG_IRQ:
		sa_opts.util_avg_irq = true;
		break;
	case OPT_UTIL_AVG_RT:
		sa_opts.util_avg_rt = true;
		break;
	case OPT_UTIL_AVG_DL:
		sa_opts.util_avg_dl = true;
		break;
	case OPT_UTIL_EST:
		sa_opts.util_est_cpu = true;
		sa_opts.util_est_task = true;
		break;
	case OPT_UTIL_EST_CPU:
		sa_opts.util_est_cpu = true;
		break;
	case OPT_UTIL_EST_TASK:
		sa_opts.util_est_task = true;
		break;
	case OPT_CPU_NR_RUNNING:
		sa_opts.cpu_nr_running = true;
		break;
	case OPT_CPU_FREQ:
		sa_opts.cpu_freq = true;
		break;
	case OPT_CPU_IDLE:
		sa_opts.cpu_idle = true;
		break;
	case OPT_SOFT_IRQ:
		sa_opts.soft_irq = true;
		break;
	case OPT_SCHED_SWITCH:
		sa_opts.sched_switch = true;
		break;
	case ARGP_KEY_ARG:
		argp_usage(state);
		break;
	case ARGP_KEY_END:
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

const struct argp argp = {
	.options = options,
	.parser = parse_arg,
	.doc = doc,
};
