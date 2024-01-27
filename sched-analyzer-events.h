/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2022 Qais Yousef */
#ifndef __SCHED_ANALYZER_EVENTS_H__
#define __SCHED_ANALYZER_EVENTS_H__

#define TASK_COMM_LEN	16
#define PELT_TYPE_LEN	4

static inline void copy_str(char *dst, char *src, unsigned int len)
{
	unsigned int i;
	for (i = 0; i < len; i++)
		dst[i] = src[i];
}

enum pelt_type {
	PELT_TYPE_CFS,
	PELT_TYPE_RT,
	PELT_TYPE_DL,
	PELT_TYPE_IRQ,
	PELT_TYPE_THERMAL,
};

struct rq_pelt_event {
	unsigned long long ts;
	int cpu;
	enum pelt_type type;
	unsigned long load_avg;
	unsigned long runnable_avg;
	unsigned long util_avg;
	unsigned long util_est_enqueued;
	unsigned long util_est_ewma;
	unsigned long uclamp_min;
	unsigned long uclamp_max;
};

struct task_pelt_event {
	unsigned long long ts;
	int cpu;
	pid_t pid;
	char comm[TASK_COMM_LEN];
	unsigned long load_avg;
	unsigned long runnable_avg;
	unsigned long util_avg;
	unsigned long util_est_enqueued;
	unsigned long util_est_ewma;
	unsigned long uclamp_min;
	unsigned long uclamp_max;
	int running;
};


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

enum lb_phases {
	LB_NOHZ_IDLE_BALANCE,
	LB_RUN_REBALANCE_DOMAINS,
	LB_REBALANCE_DOMAINS,
	LB_BALANCE_FAIR,
	LB_PICK_NEXT_TASK_FAIR,
	LB_NEWIDLE_BALANCE,
	LB_LOAD_BALANCE,
};

#define MAX_SD_LEVELS		10

struct lb_sd_stats {
	int cpu;
	int level[MAX_SD_LEVELS];
	unsigned int balance_interval[MAX_SD_LEVELS];
};

struct lb_event {
	unsigned long long ts;
	int this_cpu;
	int lb_cpu;
	enum lb_phases phase;
	bool entry;
	unsigned long misfit_task_load;
	struct lb_sd_stats sd_stats;
};

struct ipi_event {
	unsigned long long ts;
	int cpu;
	void *callsite;
	void *callback;
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
