#include <perfetto.h>

PERFETTO_DEFINE_CATEGORIES(
	perfetto::Category("test").SetDescription("Test event 2")
);

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

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
	args.backends |= perfetto::kSystemBackend;

	perfetto::Tracing::Initialize(args);
	perfetto::TrackEvent::Register();
}

extern "C" void trace_event(const char *cat, const char *name, ...)
{
	TRACE_EVENT("test", "fixed");
}
