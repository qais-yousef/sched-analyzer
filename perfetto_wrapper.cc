#include <condition_variable>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <perfetto.h>

PERFETTO_DEFINE_CATEGORIES(
	perfetto::Category("pelt-cpu").SetDescription("Track PELT at CPU level"),
	perfetto::Category("pelt-task").SetDescription("Track PELT at task level")
);

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

/*
 * Copied from perfetto/example/sdk/example_system_wide.cc
 *
 * Following Copyright and License apply to this observer class.
 *
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 */
class Observer : public perfetto::TrackEventSessionObserver {

	public:

	Observer() { perfetto::TrackEvent::AddSessionObserver(this); }
	~Observer() override { perfetto::TrackEvent::RemoveSessionObserver(this); }

	void OnStart(const perfetto::DataSourceBase::StartArgs&) override {
		std::unique_lock<std::mutex> lock(mutex);
		cv.notify_one();
	}

	void WaitForTracingStart() {
		std::unique_lock<std::mutex> lock(mutex);
		cv.wait(lock, [] { return perfetto::TrackEvent::IsEnabled(); });
	}

	std::mutex mutex;
	std::condition_variable cv;
};

extern "C" void init_perfetto(void)
{
	perfetto::TracingInitArgs args;

	// The backends determine where trace events are recorded. You may select one
	// or more of:

	// 1) The in-process backend only records within the app itself.
	args.backends |= perfetto::kInProcessBackend;

	// 2) The system backend writes events into a system Perfetto daemon,
	//    allowing merging app and system events (e.g., ftrace) on the same
	//    timeline. Requires the Perfetto `traced` daemon to be running (e.g.,
	//    on Android Pie and newer).
	/* args.backends |= perfetto::kSystemBackend; */

	perfetto::Tracing::Initialize(args);
	perfetto::TrackEvent::Register();
}

extern "C" void wait_for_perfetto(void)
{
	Observer observer;
	observer.WaitForTracingStart();
}

extern "C" void flush_perfetto(void)
{
	perfetto::TrackEvent::Flush();
}

static std::unique_ptr<perfetto::TracingSession> tracing_session;
static int fd;

extern "C" void start_perfetto_trace(void)
{
	perfetto::protos::gen::TrackEventConfig track_event_cfg;
	track_event_cfg.add_enabled_categories("sched-analyzer");

	perfetto::TraceConfig cfg;
	cfg.add_buffers()->set_size_kb(1024*100);  // Record up to 100 MiB.
	auto* ds_cfg = cfg.add_data_sources()->mutable_config();
	ds_cfg->set_name("track_event");
	ds_cfg->set_track_event_config_raw(track_event_cfg.SerializeAsString());

	fd = open("sched-analyzer.perfetto-trace", O_RDWR | O_CREAT | O_TRUNC, 0644);

	tracing_session = perfetto::Tracing::NewTrace();
	tracing_session->Setup(cfg, fd);
	tracing_session->StartBlocking();
}

extern "C" void stop_perfetto_trace(void)
{
	tracing_session->StopBlocking();
	close(fd);
}

extern "C" void trace_cpu_util_avg(int cpu, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "CPU%d util_avg", cpu);

	TRACE_COUNTER("pelt-cpu", track_name, value);
}

extern "C" void trace_task_util_avg(const char *name, int pid, int value)
{
	char track_name[32];
	snprintf(track_name, sizeof(track_name), "%s-%d util_avg", name, pid);

	TRACE_COUNTER("pelt-task", track_name, value);
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
