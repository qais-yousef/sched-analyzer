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
* Track cpu_idle and cpu_idle_miss events
* Track load balance entry/exit and some related info (Experimental)
* Track IPI related info (Experimental)
* Collect hard and soft irq entry/exit data (perfetto builtin functionality)
* Filter tasks per pid or comm

## Planned work

* Trace wake up path and why a task placement decision was made
* Better tracing of load balancer to understand when it kicks and what it
  performs when it runs
* Trace schedutil and its decision to select a frequency and when it's rate
  limited
* Trace TEO idle governor and its decision making process
* Add more python post processing tools to summarize task placement histogram
  for a sepcifc task(s) and residency of various PELT signals
* Add more python post processing tools to summarize softirq residencies and CPU
  histogram
* Track PELT at cgroup level (cfs_rq)


# Requirements

```
sudo apt install linux-tools-$(uname -r) git clang llvm libelf1 zlib1g libelf-dev bpftool
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
sudo tracebox traced_probes --background

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

## sched-analyzer-pp

Post process the produced sched-analyzer.perfetto-trace to detect potential
problems or summarizes some metrics.

### Requirements

```
pip install -r ./sched-analyzer-pp/requirements.txt
```

#### Setup autocomplete

```
activate-global-python-argcomplete --user
complete -r sched-analyzer-pp
```

You might need to add the following to your ~/.bash_completion to get it to
work:

```
for file in ~/.bash_completion.d/*
do
	. $file
done
```

### Available analysis

See `./sched-analyzer-pp/sched-analyzer-pp --help` for list of all available
options

* Plot frequency selection and residency on TUI
* Plot idle residencies on TUI
* Save produced TUI output as png image
* Save raw data as csv

### Example


```
./sched-analyzer-pp/sched-analyzer-pp --freq --freq-residency --idle-residency sched-analyzer.perfetto-trace

                                             CPU0 Frequency                                         
    ┌──────────────────────────────────────────────────────────────────────────────────────────────┐
2.06┤                                                                             ▗▄▄▄▟▗▌▟▟▟▟▗▌▟ ▗▌│
1.82┤                                                                    ▗▄▄▄▄▀▀▀▀▘   ▐▐▌████▐▌█ ▐▌│
1.58┤                                ▗                          ▗▄▄▄▄▀▀▀▀▘            ▐▐▌████▐▌█ ▌▌│
1.09┤                       ▄▄▄▄▄▀▀▀▀▜                  ▄▄▄▄▞▀▀▀▘                     ▐▌█▐███▌█ ▌▌▌│
0.84┤  ▟          ▄▄▄▄▄▀▀▀▀▀         ▐         ▄▄▄▄▞▀▀▀▀                              ▐▌█▐███▌█ █ ▌│
0.60┤▄▀▝▄▄▄▄▄▀▀▀▀▀                   ▝▄▄▄▄▞▀▀▀▀                                       ▝▌▜▝▛▛▛▌▜ ▜ ▚│
    └┬──────────────────────┬───────────────────────┬──────────────────────┬──────────────────────┬┘
   1.79                   2.28                    2.78                   3.27                  3.76 
                                             CPU4 Frequency                                         
    ┌──────────────────────────────────────────────────────────────────────────────────────────────┐
3.20┤                                                                                             ▞│
2.77┤▖                                                                                            ▌│
2.34┤▌                                          ▗                                                ▗▘│
1.47┤▌                            ▗▄▄▄▄▄▄▞▀▀▀▀▀▀▀▖     ▗▄▌                                       ▐ │
1.03┤▌              ▄▄▄▄▄▄▄▀▀▀▀▀▀▀▘              ▌  ▗▄▀▘ ▚                          ▗▄▄▄▄▄▄▄▄▄▄▄▄▞ │
0.60┤▚▄▄▄▄▄▄▄▀▀▀▀▀▀▀                             ▚▄▀▘    ▝▄▄▄▄▄▄▄▄▄▄▄▄▄▞▀▀▀▀▀▀▀▀▀▀▀▀▘              │
    └┬──────────────────────┬───────────────────────┬──────────────────────┬──────────────────────┬┘
   0.00                   0.45                    0.90                   1.34                  1.79 
                                       CPU0 Frequency residency %                                   
    ┌──────────────────────────────────────────────────────────────────────────────────────────────┐
93.3┤███████                                                                                       │
77.7┤███████                                                                                       │
62.2┤███████                                                                                       │
31.1┤███████                                                                                       │
15.5┤███████                                                                                       │
 0.0┤███████               ███████               ██████                                     ███████│
    └───┬─────────────────────┬─────────────────────┬──────────────────────────────────────────┬───┘
       0.6                  0.972                 1.332                                     2.064   
                                       CPU4 Frequency residency %                                   
    ┌──────────────────────────────────────────────────────────────────────────────────────────────┐
95.7┤████                                                                                          │
79.7┤████                                                                                          │
63.8┤████                                                                                          │
31.9┤████                                                                                          │
15.9┤████                                                                                          │
 0.0┤████                    ███    ████    ████    ████    ████   ████                            │
    └─┬───────────────────────┬───────┬──────┬───────┬───────┬──────┬────────────────────────────┬─┘
     0.6                    1.284    1.5   1.728   1.956   2.184  2.388                      3.204  
       CPU0 Idle residency %         CPU1 Idle residency %         CPU2 Idle residency %         CPU3 Idle residency %  
    ┌────────────────────────┐    ┌────────────────────────┐    ┌────────────────────────┐    ┌────────────────────────┐
70.1┤                     ███│82.2┤                     ███│60.8┤                     ███│86.3┤                     ███│
58.4┤                     ███│68.5┤                     ███│50.6┤                     ███│71.9┤                     ███│
46.7┤                     ███│54.8┤                     ███│40.5┤          ████       ███│57.5┤                     ███│
23.4┤          ████       ███│27.4┤                     ███│20.3┤          ████       ███│28.8┤                     ███│
11.7┤          ████       ███│13.7┤          ████       ███│10.1┤          ████       ███│14.4┤          ████       ███│
 0.0┤███       ████       ███│ 0.0┤███       ████       ███│ 0.0┤███       ████       ███│ 0.0┤███       ████       ███│
    └─┬──────────┬─────────┬─┘    └─┬──────────┬─────────┬─┘    └─┬──────────┬─────────┬─┘    └─┬──────────┬─────────┬─┘
    -1.0        0.0      1.0      -1.0        0.0      1.0      -1.0        0.0      1.0      -1.0        0.0      1.0  
       CPU4 Idle residency %         CPU5 Idle residency %         CPU6 Idle residency %         CPU7 Idle residency %  
    ┌────────────────────────┐    ┌────────────────────────┐    ┌────────────────────────┐    ┌────────────────────────┐
56.1┤                     ███│79.3┤                     ███│72.2┤                     ███│97.0┤                     ███│
46.7┤          ████       ███│66.1┤                     ███│60.1┤                     ███│80.8┤                     ███│
37.4┤          ████       ███│52.9┤                     ███│48.1┤                     ███│64.6┤                     ███│
18.7┤          ████       ███│26.4┤                     ███│24.1┤          ████       ███│32.3┤                     ███│
 9.3┤          ████       ███│13.2┤          ████       ███│12.0┤          ████       ███│16.2┤                     ███│
 0.0┤███       ████       ███│ 0.0┤███       ████       ███│ 0.0┤███       ████       ███│ 0.0┤███       ████       ███│
    └─┬──────────┬─────────┬─┘    └─┬──────────┬─────────┬─┘    └─┬──────────┬─────────┬─┘    └─┬──────────┬─────────┬─┘
    -1.0        0.0      1.0      -1.0        0.0      1.0      -1.0        0.0      1.0      -1.0        0.0      1.0 
```
