/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SCHED_ANALYZER_EVENTS_H__
#define __SCHED_ANALYZER_EVENTS_H__

#define TASK_COMM_LEN	16

struct uclamp_rq_event {
	int cpu;
	unsigned long util_avg;
	unsigned long uclamp_min;
	unsigned long uclamp_max;
};

struct uclamp_task_event {
	char comm[TASK_COMM_LEN];
	unsigned long util_avg;
	unsigned long uclamp_min;
	unsigned long uclamp_max;
};

#endif /* __SCHED_ANALYZER_EVENTS_H__ */
