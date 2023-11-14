/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Qais Yousef */
void init_perfetto(void);
void flush_perfetto(void);
void start_perfetto_trace(void);
void stop_perfetto_trace(void);
void trace_cpu_util_avg(uint64_t ts, int cpu, int value);
void trace_cpu_util_est_enqueued(uint64_t ts, int cpu, int value);
void trace_cpu_util_est_ewma(uint64_t ts, int cpu, int value);
void trace_task_util_avg(uint64_t ts, const char *name, int pid, int value);
void trace_task_util_est_enqueued(uint64_t ts, const char *name, int pid, int value);
void trace_task_util_est_ewma(uint64_t ts, const char *name, int pid, int value);
void trace_cpu_nr_running(uint64_t ts, int cpu, int value);
