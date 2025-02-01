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

You can also create a systemd unit file. Ensure perfetto binaries are in a PATH
accessible by root.

```
cat << EOF | sudo tee /usr/lib/systemd/system//perfetto_traced.service
[Unit]
Description=Perfetto traced

[Service]
ExecStart=tracebox traced

[Install]
WantedBy=default.target

EOF

cat << EOF | sudo tee /usr/lib/systemd/system//perfetto_traced_probes.service
[Unit]
Description=Perfetto traced_probes

[Service]
ExecStart=tracebox traced_probes

[Install]
WantedBy=default.target

EOF

systemctl daemon-reload
systemctl enable perfetto_traced.service
systemctl enable perfetto_traced_probes.service
systemctl start perfetto_traced.service
systemctl start perfetto_traced_probes.service
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

### Examples

#### Collect PELT data filtering for tasks with specific name

```
sudo ./sched-analyzer --util_avg --util_est --comm firefox
```

The filtering is actually globbing for a task which contains the string. It
doens't look for exact match.

![perfetto-screenshot](screenshots/sched-analyzer-screenshot-pelt-filtered.png?raw=true)

#### Collect when an IPI happen with info about who triggered it

```
sudo ./sched-analyzer --ipi
```

When clicking on the IPI slice in perfetto, you'd be able to see extra info
about who send the IPI from the new trace events.

0x1 is for the callsite function.
0x2 is for the callback function.

![perfetto-screenshot](screenshots/sched-analyzer-screenshot-ipi.png?raw=true)

## sched-analyzer-pp

Post process the produced sched-analyzer.perfetto-trace to detect potential
problems or summarizes some metrics.

See [REAMDE.md](sched-analyzer-pp) for more details on setup and usage.

### Example


```
sched-analyzer-pp --freq --freq-residency --idle-residency --util-avg CPU0 --util-avg CPU4 sched-analyzer.perfetto-trace

                                             CPU0 Frequency                                         
    ┌──────────────────────────────────────────────────────────────────────────────────────────────┐
2.06┤                       ▐▌                           ▐▀▌                                       │
1.70┤                       ▐▙                           ▐ ▌                                       │
1.33┤                       ▐█▖   ▖                      ▟ ▌                                       │
    │                        █▌   ▌                      ▌ ▌                                       │
0.97┤                        ▜█   █                      ▌ ▌   ▐                                   │
0.60┤                        ▐█▄▄▄█▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▌ ▙▄▄▄▟▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄│
    └┬─────────┬──────────┬─────────┬─────────┬──────────┬─────────┬─────────┬──────────┬─────────┬┘
   0.00      0.36       0.72      1.07      1.43       1.79      2.15      2.51       2.87     3.22 
                                             CPU4 Frequency                                         
    ┌──────────────────────────────────────────────────────────────────────────────────────────────┐
3.20┤                       ▝▌                              ▟▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀│
2.55┤                        ▌                           ▗▄▐▘                                      │
1.90┤                        ▌    ▌                      ▐▐▛                                       │
    │                        ▌    ▙▖                     ▐▐▌                                       │
1.25┤                        ▌    █▌                     ▐▐▌                                       │
0.60┤                        ▜▄▄▄▄█▙▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▟▐▌                                       │
    └┬─────────┬──────────┬─────────┬─────────┬──────────┬─────────┬─────────┬──────────┬─────────┬┘
   0.00      0.36       0.72      1.07      1.43       1.79      2.15      2.51       2.87     3.22 

──────────────────────────────────── CPU0 Frequency residency % ────────────────────────────────────
0.6   ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇ 94.35000000000001
0.972 ▇ 1.47
1.332 ▇ 1.31
1.704 ▇ 0.68
2.064 ▇▇ 2.19

──────────────────────────────────── CPU4 Frequency residency % ────────────────────────────────────
0.6   ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇ 40.18
0.828 ▇ 0.44
1.5   ▇▇ 1.1400000000000001
1.956 ▇ 0.42
2.184 ▇ 0.63
2.388 ▇ 0.42
2.592 ▇ 0.8200000000000001
2.772 ▇ 0.42
2.988 ▇ 0.42
3.096 ▇ 0.42
3.204 ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇ 54.72

─────────────────────────────────────── CPU Idle Residency % ───────────────────────────────────────
  ▇▇▇▇▇▇ 6.67
0 ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇ 56.21
  ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇ 37.12

  ▇ 1.1
1 ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇ 68.69
  ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇ 30.21

   0.32
2 ▇▇▇▇▇▇▇▇▇▇ 10.93
  ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇ 88.76

  ▇ 1.1
3 ▇▇▇▇▇▇▇▇▇ 9.82
  ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇ 89.08

   0.05
4  0.08
  ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇ 99.87

   0.01
5 ▇ 1.19
  ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇ 98.8

   0.01
6 ▇ 1.34
  ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇ 98.65

   0.01
7 ▇ 1.31
  ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇ 98.68
───────────────────────────────────── ▇▇▇ -1.0 ▇▇▇ 0.0 ▇▇▇ 1.0 ─────────────────────────────────────
                                              CPU0 util_avg                                         
     ┌─────────────────────────────────────────────────────────────────────────────────────────────┐
426.0┤                                                    ▗█                                       │
319.5┤                                                    ▐▐                                       │
213.0┤                                                    ▟▐                                       │
     │ ▐▖                    ▙                            ▌▐                                       │
106.5┤ ▐▜▖                   ▛▙                          ▟▘▐                                       │
  0.0┤▄▟ ▀▜▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▌▝▀▙▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▟▘ ▐▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄│
     └┬─────────┬─────────┬──────────┬─────────┬─────────┬─────────┬──────────┬─────────┬─────────┬┘
    0.00      0.36      0.72       1.07      1.43      1.79      2.15       2.51      2.87     3.22 
                                              CPU4 util_avg                                         
    ┌──────────────────────────────────────────────────────────────────────────────────────────────┐
1024┤                                                        ▄▄▛▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▌│
 768┤▙                                                      ▟▘                                    ▌│
 512┤▐▖                                                    ▟▘                                     ▙│
    │ ▙                                                    ▌                                       │
 256┤ ▝▙▄                                                  ▌                                       │
   0┤   ▝▜▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▌                                       │
    └┬─────────┬──────────┬─────────┬─────────┬──────────┬─────────┬─────────┬──────────┬─────────┬┘
   0.00      0.36       0.72      1.07      1.43       1.79      2.15      2.51       2.87     3.22
```
