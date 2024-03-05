/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Qais Yousef */
#include <condition_variable>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <perfetto.h>

#include "parse_argp.h"
#include "sched-analyzer-events.h"

PERFETTO_DEFINE_CATEGORIES(
	perfetto::Category("pelt-cpu").SetDescription("Track PELT at CPU level"),
	perfetto::Category("pelt-task").SetDescription("Track PELT at task level"),
	perfetto::Category("nr-running-cpu").SetDescription("Track number of tasks running on each CPU"),
	perfetto::Category("cpu-idle").SetDescription("Track cpu idle info for each CPU"),
	perfetto::Category("load-balance").SetDescription("Track load balance internals"),
	perfetto::Category("ipi").SetDescription("Track inter-processor interrupts"),
);

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

enum sched_analyzer_track_ids {
	SA_TRACK_ID_CPU_IDLE_MISS = 1,		/* must start from none 0 */
	SA_TRACK_ID_LOAD_BALANCE,
	SA_TRACK_ID_IPI,
};

#define TRACK_SPACING		1000
#define TRACK_ID(ID)		(SA_TRACK_ID_##ID * TRACK_SPACING)

#define FAKE_DURATION		10000  /* 10us */


extern "C" void init_perfetto(void)
{
	const char *android_traced_prodcuer = "/dev/socket/traced_producer";
	const char *android_traced_consumer = "/dev/socket/traced_consumer";

	if (!access(android_traced_prodcuer, F_OK))
		setenv("PERFETTO_PRODUCER_SOCK_NAME", "/dev/socket/traced_producer", 0);
	if (!access(android_traced_consumer, F_OK))
		setenv("PERFETTO_CONSUMER_SOCK_NAME", "/dev/socket/traced_consumer", 0);

	perfetto::TracingInitArgs args;

	// The backends determine where trace events are recorded. You may select one
	// or more of:

	// 1) The in-process backend only records within the app itself.
	if (sa_opts.app)
		args.backends |= perfetto::kInProcessBackend;

	// 2) The system backend writes events into a system Perfetto daemon,
	//    allowing merging app and system events (e.g., ftrace) on the same
	//    timeline. Requires the Perfetto `traced` daemon to be running (e.g.,
	//    on Android Pie and newer).
	if (sa_opts.system)
		args.backends |= perfetto::kSystemBackend;

	args.shmem_size_hint_kb = 1024*100; // 100 MiB

	perfetto::Tracing::Initialize(args);
	perfetto::TrackEvent::Register();
}

extern "C" void flush_perfetto(void)
{
	perfetto::TrackEvent::Flush();
}

static std::unique_ptr<perfetto::TracingSession> tracing_session;
static int fd;

extern "C" void start_perfetto_trace(void)
{
	char buffer[256];

	perfetto::TraceConfig cfg;
	perfetto::TraceConfig::BufferConfig* buf;
	buf = cfg.add_buffers();
	buf->set_size_kb(1024*100);  // Record up to 100 MiB.
	buf->set_fill_policy(perfetto::TraceConfig::BufferConfig::RING_BUFFER);
	buf = cfg.add_buffers();
	buf->set_size_kb(1024*100);  // Record up to 100 MiB.
	buf->set_fill_policy(perfetto::TraceConfig::BufferConfig::RING_BUFFER);
	cfg.set_duration_ms(3600000);
	cfg.set_max_file_size_bytes(sa_opts.max_size);
	cfg.set_unique_session_name("sched-analyzer");
	cfg.set_write_into_file(true);
	cfg.set_file_write_period_ms(1000);
	cfg.set_flush_period_ms(30000);
	cfg.set_enable_extra_guardrails(false);
	cfg.set_notify_traceur(true);
	cfg.mutable_incremental_state_config()->set_clear_period_ms(15000);

	/* Track Events Data Source */
	perfetto::protos::gen::TrackEventConfig track_event_cfg;
	track_event_cfg.add_enabled_categories("sched-analyzer");

	auto *te_ds_cfg = cfg.add_data_sources()->mutable_config();
	te_ds_cfg->set_name("track_event");
	te_ds_cfg->set_track_event_config_raw(track_event_cfg.SerializeAsString());

	/* Ftrace Data Source */
	perfetto::protos::gen::FtraceConfig ftrace_cfg;
	ftrace_cfg.add_ftrace_events("sched/sched_switch");
	ftrace_cfg.add_ftrace_events("sched/sched_wakeup_new");
	ftrace_cfg.add_ftrace_events("sched/sched_waking");
	ftrace_cfg.add_ftrace_events("sched/sched_process_exit");
	ftrace_cfg.add_ftrace_events("sched/sched_process_free");
	ftrace_cfg.add_ftrace_events("power/suspend_resume");
	ftrace_cfg.add_ftrace_events("power/cpu_frequency");
	ftrace_cfg.add_ftrace_events("power/cpu_idle");
	ftrace_cfg.add_ftrace_events("task/task_newtask");
	ftrace_cfg.add_ftrace_events("task/task_rename");
	ftrace_cfg.add_ftrace_events("ftrace/print");
	if (sa_opts.irq)
		ftrace_cfg.add_atrace_categories("irq");

	for (unsigned int i = 0; i < sa_opts.num_ftrace_event; i++)
		ftrace_cfg.add_ftrace_events(sa_opts.ftrace_event[i]);

	for (unsigned int i = 0; i < sa_opts.num_atrace_cat; i++)
		ftrace_cfg.add_atrace_categories(sa_opts.atrace_cat[i]);

	ftrace_cfg.set_drain_period_ms(1000);

	auto *ft_ds_cfg = cfg.add_data_sources()->mutable_config();
	ft_ds_cfg->set_name("linux.ftrace");
	ft_ds_cfg->set_ftrace_config_raw(ftrace_cfg.SerializeAsString());

	/* Process Stats Data Source */
	perfetto::protos::gen::ProcessStatsConfig ps_cfg;
	ps_cfg.set_proc_stats_poll_ms(10000);
	ps_cfg.set_record_thread_names(true);

	auto *ps_ds_cfg = cfg.add_data_sources()->mutable_config();
	ps_ds_cfg->set_name("linux.process_stats");
	ps_ds_cfg->set_process_stats_config_raw(ps_cfg.SerializeAsString());

	/* On Android traces can be saved on specific path only */
	const char *android_traces_path = "/data/misc/perfetto-traces";
	DIR *dir = opendir(android_traces_path);
	if (!sa_opts.output_path) {
		sa_opts.output_path = ".";
		if (dir) {
			sa_opts.output_path = android_traces_path;
			closedir(dir);
		}
	}

	snprintf(buffer, 256, "%s/%s", sa_opts.output_path, sa_opts.output);
	fd = open(buffer, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		snprintf(buffer, 256, "Failed to create %s/%s", sa_opts.output_path, sa_opts.output);
		perror(buffer);
		return;
	}

	tracing_session = perfetto::Tracing::NewTrace();
	tracing_session->Setup(cfg, fd);
	tracing_session->StartBlocking();
}

extern "C" void stop_perfetto_trace(void)
{
	if (fd < 0)
		return;

	tracing_session->StopBlocking();
	close(fd);
}

extern "C" void trace_cpu_load_avg(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d load_avg", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, ts, value);
}

extern "C" void trace_cpu_runnable_avg(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d runnable_avg", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, ts, value);
}

extern "C" void trace_cpu_util_avg(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d util_avg", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, ts, value);
}

extern "C" void trace_cpu_uclamped_avg(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d uclamped_avg", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, ts, value);
}

extern "C" void trace_cpu_util_est_enqueued(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d util_est.enqueued", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, ts, value);
}

extern "C" void trace_cpu_util_avg_rt(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d util_avg_rt", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, ts, value);
}

extern "C" void trace_cpu_util_avg_dl(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d util_avg_rt", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, ts, value);
}

extern "C" void trace_cpu_util_avg_irq(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d util_avg_rt", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, ts, value);
}

extern "C" void trace_cpu_util_avg_thermal(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d util_avg_thermal", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, ts, value);
}

extern "C" void trace_task_load_avg(uint64_t ts, const char *name, int pid, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "%s-%d load_avg", name, pid);

	TRACE_COUNTER("pelt-task", track_name, ts, value);
}

extern "C" void trace_task_runnable_avg(uint64_t ts, const char *name, int pid, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "%s-%d runnable_avg", name, pid);

	TRACE_COUNTER("pelt-task", track_name, ts, value);
}

extern "C" void trace_task_util_avg(uint64_t ts, const char *name, int pid, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "%s-%d util_avg", name, pid);

	TRACE_COUNTER("pelt-task", track_name, ts, value);
}

extern "C" void trace_task_uclamped_avg(uint64_t ts, const char *name, int pid, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "%s-%d uclamped_avg", name, pid);

	TRACE_COUNTER("pelt-task", track_name, ts, value);
}

extern "C" void trace_task_util_est_enqueued(uint64_t ts, const char *name, int pid, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "%s-%d util_est.enqueued", name, pid);

	TRACE_COUNTER("pelt-task", track_name, ts, value);
}

extern "C" void trace_task_util_est_ewma(uint64_t ts, const char *name, int pid, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "%s-%d util_est.ewma", name, pid);

	TRACE_COUNTER("pelt-task", track_name, ts, value);
}

extern "C" void trace_cpu_nr_running(uint64_t ts, int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d nr_running", cpu);

	TRACE_COUNTER("nr-running-cpu", track_name, ts, value);
}

extern "C" void trace_cpu_idle(uint64_t ts, int cpu, int state)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d idle_state", cpu);

	TRACE_COUNTER("cpu-idle", track_name, ts, state);
}

extern "C" void trace_cpu_idle_miss(uint64_t ts, int cpu, int state, int miss)
{
	TRACE_EVENT("cpu-idle", "cpu_idle_miss",
		    perfetto::Track(TRACK_ID(CPU_IDLE_MISS) + cpu), ts,
		    "CPU", cpu,
		    "STATE", state, "MISS", miss < 0 ? "below" : "above");

	TRACE_EVENT_END("cpu-idle",
			perfetto::Track(TRACK_ID(CPU_IDLE_MISS) + cpu),
			ts + FAKE_DURATION);
}

extern "C" void trace_lb_entry(uint64_t ts, int this_cpu, int lb_cpu, char *phase)
{
	TRACE_EVENT_BEGIN("load-balance", phase,
			  perfetto::Track(TRACK_ID(LOAD_BALANCE) + this_cpu),
			  ts, "CPU", lb_cpu);
}

extern "C" void trace_lb_exit(uint64_t ts, int this_cpu, int lb_cpu)
{
	TRACE_EVENT_END("load-balance",
			perfetto::Track(TRACK_ID(LOAD_BALANCE) + this_cpu), ts);
}

extern "C" void trace_lb_sd_stats(uint64_t ts, struct lb_sd_stats *sd_stats)
{
	char track_name[64];
	int i;

	for (i = 0; i < MAX_SD_LEVELS; i++) {
		if (sd_stats->level[i] == -1 && !sd_stats->balance_interval[i])
			break;

		snprintf(track_name, sizeof(track_name), "CPU%d.level%d.balance_interval",
			 sd_stats->cpu, sd_stats->level[i]);

		TRACE_COUNTER("load-balance", track_name, ts, sd_stats->balance_interval[i]);
	}
}

extern "C" void trace_lb_overloaded(uint64_t ts, unsigned int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "rd.overloaded");

	TRACE_COUNTER("load-balance", track_name, ts, value);
}

extern "C" void trace_lb_overutilized(uint64_t ts, unsigned int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "rd.overutilized");

	TRACE_COUNTER("load-balance", track_name, ts, value);
}

extern "C" void trace_lb_misfit(uint64_t ts, int cpu, unsigned long misfit_task_load)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d misfit_task_load", cpu);

	TRACE_COUNTER("load-balance", track_name, ts, misfit_task_load);
}

extern "C" void trace_ipi_send_cpu(uint64_t ts, int from_cpu, int target_cpu,
				   char *callsite, void *callsitep,
				   char *callback, void *callbackp)
{
	TRACE_EVENT("ipi", "ipi_send_cpu",
		    perfetto::Track(TRACK_ID(IPI) + from_cpu), ts,
		    "FROM_CPU", from_cpu,
		    "TARGET_CPU", target_cpu,
		    callsite ? callsite : "CALLSITE",
		    callsite ? (void *)1 : callsitep,
		    callback ? callback : "CALLBACK",
		    callback ? (void *)2 : callbackp);

	TRACE_EVENT_END("ipi", perfetto::Track(TRACK_ID(IPI) + from_cpu),
			ts + FAKE_DURATION);
}

#if 0
extern "C" int main(int argc, char **argv)
{
	init_perfetto();
	/* wait_for_perfetto(); */

	start_perfetto_trace();

	for (int i = 0; i < 2000; i++) {
		trace_cpu_pelt(0, i % 300);
		usleep(1000);
	}

	stop_perfetto_trace();

	/* flush_perfetto(); */

	return 0;
}
#endif
