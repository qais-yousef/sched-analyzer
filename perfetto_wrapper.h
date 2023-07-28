/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Qais Yousef */
void init_perfetto(void);
void wait_for_perfetto(void);
void flush_perfetto(void);
void start_perfetto_trace(void);
void stop_perfetto_trace(void);
void trace_cpu_util_avg(int cpu, int value);
void trace_task_util_avg(const char *name, int pid, int value);
void trace_cpu_nr_running(int cpu, int value);
