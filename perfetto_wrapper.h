#ifdef __cplusplus
#define perf_prefix extern "C"
#else
#define perf_prefix
#endif

perf_prefix void init_perfetto(void);
perf_prefix void trace_event(const char *cat, const char *name, ...);
