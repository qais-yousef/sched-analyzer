#ifdef __cplusplus
#define perf_prefix extern "C"
#else
#define perf_prefix
#endif

perf_prefix void init_perfetto(void);
perf_prefix void start_perfetto_trace(void);
perf_prefix void stop_perfetto_trace(void);
perf_prefix void trace_cpu_pelt(int cpu, int value);
