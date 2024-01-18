/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Qais Yousef */
struct lb_sd_stats;

void init_perfetto(void);
void flush_perfetto(void);
void start_perfetto_trace(void);
void stop_perfetto_trace(void);
void trace_cpu_util_avg(uint64_t ts, int cpu, int value);
void trace_cpu_uclamped_avg(uint64_t ts, int cpu, int value);
void trace_cpu_util_est_enqueued(uint64_t ts, int cpu, int value);
void trace_cpu_util_avg_rt(uint64_t ts, int cpu, int value);
void trace_cpu_util_avg_dl(uint64_t ts, int cpu, int value);
void trace_cpu_util_avg_irq(uint64_t ts, int cpu, int value);
void trace_cpu_util_avg_thermal(uint64_t ts, int cpu, int value);
void trace_task_util_avg(uint64_t ts, const char *name, int pid, int value);
void trace_task_uclamped_avg(uint64_t ts, const char *name, int pid, int value);
void trace_task_util_est_enqueued(uint64_t ts, const char *name, int pid, int value);
void trace_task_util_est_ewma(uint64_t ts, const char *name, int pid, int value);
void trace_cpu_nr_running(uint64_t ts, int cpu, int value);
void trace_lb_entry(uint64_t ts, int this_cpu, int lb_cpu, char *phase);
void trace_lb_exit(uint64_t ts, int this_cpu, int lb_cpu);
void trace_lb_sd_stats(uint64_t ts, struct lb_sd_stats *sd_stats);
void trace_lb_misfit(uint64_t ts, int cpu, unsigned long misfit_task_load);
