# sched-analyzer

BPF CO-RE based sched-analyzer

This is a personal pet project and not affiliated with any employer
or organization.

sched-analyzer connects to various points in the kernel to extract internal
info and produce perfetto trace with additional collected events/tracks.
Perfetto has integration with python for usage with libraries like pandas for
more sophisticated post-processing option.

Each event recorded by sched-analyzer is processed in its own thread to ensure
each BPF ringbuffer is emptied in parallel and reduce the chance of overflowing
any of them and potentially lose data.

Since we peek inside kernel internals which are not ABI, there's no guarantee
this will work on every kernel. Or won't silently fail if for instance some
arguments to the one of the tracepoints we attach to changes.

## Goal

The BPF backend, `sched-analyzer`, will collect data and generate perfetto
events for analysis and optionally additional post processing via python.

## Data collected

* load_avg and runnable_avg of FAIR at runqueue level
* util_avg of FAIR, RT, DL, IRQ and thermal pressure at runqueue level
* load_avg, runnable_avg and util_avg of tasks running
* uclamped util_avg of CPUs and tasks: clamp(util_avg, uclamp_min, uclamp_max)
* util_est at runqueue level and of tasks
* Number of tasks running for every runqueue
* Track load balance entry/exit and some related info (Experimental)
* Filter tasks per pid or comm.


# Requirements

```
sudo apt install linux-tools-$(uname -r) git clang llvm libelf1 zlib1g libelf-dev
```

Download latest release of perfetto from [github](https://github.com/google/perfetto/releases/)

### BTF

You need a kernel compiled with BTF to Compile and Run.

Required kernel config to get BTF:

- CONFIG_DEBUG_INFO_BTF=y

# Build

```
make

make help // for generic help and how to static build and cross compile
```

g++-9 and g++-10 fail to create a working static build - see this [issue](https://github.com/google/perfetto/issues/549).

# Usage

## sched-analyzer

### perfetto mode

First make sure perfetto is [downloaded](https://github.com/google/perfetto/releases/) and in your PATH.
You need to run the following commands once after every reboot:

```
sudo tracebox traced --background
sudo tracebox traced_porbes --background

```

To collect data run:

```
sudo ./sched-analyzer --cpu_nr_running --util_avg
```

Press `CTRL+c` to stop. `sched-analyzer.perfetto-trace` will be in PWD that you
can open in [ui.perfetto.dev](https://ui.perfetto.dev)

The captured data are under sched-analyzer tab in perfetto, expand it to see
them

![perfetto-screenshot](screenshots/sched-analyzer-screenshot.png?raw=true)

### csv mode

csv mode was deprecated and is no longer supported on main branch. You can
still find it in [csv-mode branch](https://github.com/qais-yousef/sched-analyzer/tree/csv-mode).
