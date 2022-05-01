/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2022 Qais Yousef */
#ifndef __SCHED_ANALYZER_EVENTS_H__
#define __SCHED_ANALYZER_EVENTS_H__

#define TASK_COMM_LEN	16
#define PELT_TYPE_LEN	4

static inline void copy_str(char *dst, char *src, unsigned int len)
{
	int i;
	for (i = 0; i < len; i++)
		dst[i] = src[i];
}

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


#ifdef __VMLINUX_H__
char type_cfs[PELT_TYPE_LEN] = "cfs";
char type_rt[PELT_TYPE_LEN] = "rt";
char type_dl[PELT_TYPE_LEN] = "dl";

static inline void copy_pelt_type(char *dst, char *src)
{
	copy_str(dst, src, PELT_TYPE_LEN);
}
#endif /* __VMLINUX_H__ */

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

struct softirq_event {
	unsigned long long ts;
	int cpu;
	char softirq[TASK_COMM_LEN];
	unsigned long duration;
};


#ifdef __VMLINUX_H__
char hi_softirq[TASK_COMM_LEN] = "hi";
char timer_softirq[TASK_COMM_LEN] = "timer";
char net_tx_softirq[TASK_COMM_LEN] = "net_tx";
char net_rx_softirq[TASK_COMM_LEN] = "net_rx";
char block_softirq[TASK_COMM_LEN] = "block";
char irq_poll_softirq[TASK_COMM_LEN] = "irq_poll";
char tasklet_softirq[TASK_COMM_LEN] = "tasklet";
char sched_softirq[TASK_COMM_LEN] = "sched";
char hrtimer_softirq[TASK_COMM_LEN] = "hrtimer";
char rcu_softirq[TASK_COMM_LEN] = "rcu";
char unknown_softirq[TASK_COMM_LEN] = "unknown";

static inline void copy_softirq(char *dst, unsigned int vec_nr)
{
	switch (vec_nr) {
	case HI_SOFTIRQ:
		copy_str(dst, hi_softirq, TASK_COMM_LEN);
		break;
	case TIMER_SOFTIRQ:
		copy_str(dst, timer_softirq, TASK_COMM_LEN);
		break;
	case NET_TX_SOFTIRQ:
		copy_str(dst, net_tx_softirq, TASK_COMM_LEN);
		break;
	case NET_RX_SOFTIRQ:
		copy_str(dst, net_rx_softirq, TASK_COMM_LEN);
		break;
	case BLOCK_SOFTIRQ:
		copy_str(dst, block_softirq, TASK_COMM_LEN);
		break;
	case IRQ_POLL_SOFTIRQ:
		copy_str(dst, irq_poll_softirq, TASK_COMM_LEN);
		break;
	case TASKLET_SOFTIRQ:
		copy_str(dst, tasklet_softirq, TASK_COMM_LEN);
		break;
	case SCHED_SOFTIRQ:
		copy_str(dst, sched_softirq, TASK_COMM_LEN);
		break;
	case HRTIMER_SOFTIRQ:
		copy_str(dst, hrtimer_softirq, TASK_COMM_LEN);
		break;
	case RCU_SOFTIRQ:
		copy_str(dst, rcu_softirq, TASK_COMM_LEN);
		break;
	default:
		copy_str(dst, unknown_softirq, TASK_COMM_LEN);
		break;
	}
}
#endif /* __VMLINUX_H__ */

#endif /* __SCHED_ANALYZER_EVENTS_H__ */
