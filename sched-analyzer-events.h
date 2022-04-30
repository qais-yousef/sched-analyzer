/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2022 Qais Yousef */
#ifndef __SCHED_ANALYZER_EVENTS_H__
#define __SCHED_ANALYZER_EVENTS_H__

#define TASK_COMM_LEN	16
#define PELT_TYPE_LEN	4

struct rq_pelt_event {
	unsigned long long ts;
	int cpu;
	char type[PELT_TYPE_LEN];
	unsigned long util_avg;
	unsigned long uclamp_min;
	unsigned long uclamp_max;
};

struct task_pelt_event {
	unsigned long long ts;
	int cpu;
	pid_t pid;
	char comm[TASK_COMM_LEN];
	unsigned long util_avg;
	unsigned long uclamp_min;
	unsigned long uclamp_max;
	int running;
};


char type_cfs[PELT_TYPE_LEN] = "cfs";
char type_rt[PELT_TYPE_LEN] = "rt";
char type_dl[PELT_TYPE_LEN] = "dl";

static inline void copy_pelt_type(char *dst, char *src)
{
	int i;
	for (i = 0; i < PELT_TYPE_LEN; i++)
		dst[i] = src[i];
}

struct rq_nr_running_event {
	unsigned long long ts;
	int cpu;
	int nr_running;
	int change;
};

struct sched_switch_event {
	unsigned long long ts;
	int cpu;
	pid_t pid;
	char comm[TASK_COMM_LEN];
	int running;
};

struct freq_idle_event {
	unsigned long long ts;
	int cpu;
	unsigned int frequency;
	int idle_state;
};

#endif /* __SCHED_ANALYZER_EVENTS_H__ */
