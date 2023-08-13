# sched-analyzer

BPF CO-RE based sched-analyzer

This is a personal pet project and not affiliated with any employer
or organization.

sched-analyzer connects to various points in the kernel to extract internal
info and produce either perfetto events (default) or store info in csv file.
Both options enable easy integration with python for usage with libraries like
pandas for more sophisticated post-processing option.

Each event recorded by sched-analyzer is processed in its own thread to ensure
each BPF ringbuffer is emptied in parallel and reduce the chance of overflowing
any of them and potentially lose data.

Since we peek inside kernel internals which are not ABI, there's no guarantee
this will work on every kernel. Or won't silently fail if for instance some
arguments to the one of the tracepoints we attach to changes.

## Goal

The BPF backend, `sched-analyzer`, will collect data and generate perfetto
events or dump them into csv files for post processing by any front end.

Both options can easily be hooked into via python for further post processing.

The python frontend, `sched-top`, examines the csv files on regular intervals
and depicts these info on the terminal. It is the not the most efficient
way to do this, but the simplest to start with. Perfetto is advised for viewing
and post processing going forward.

You can write your own custom post processing scripts to parse csv or
perfetto-trace.

## Data collected

### util_avg of CFS, RT and DL at runqueue level

You can find the data in `/tmp/rq_pelt.csv`

### util_avg of tasks running

You can find the data in `/tmp/task_pelt.csv`

### Number of tasks running for every runqueue

You can find the data in `/tmp/rq_nr_running.csv`

### Frequency and idle states

You can find the data in `/tmp/freq_idle.csv`

### Softirq entry and duration

You can find the data in `/tmp/softirq.csv`


# Requirements

```
sudo apt install linux-tools-$(uname -r) git clang llvm libelf1 zlib1g

pip install -r requirements.txt
```

Download latest release of perfetto from [github](https://github.com/google/perfetto/releases/)

### Setup autocomplete for sched_top options

```
activate-global-python-argcomplete --user
complete -r sched-top
```
You might need to add the following to your ~/.bash_completion to get it to
work:

```
for file in ~/.bash_completion.d/*
do
	. $file
done
```

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

First make sure perfetto is downloaded and in your PATH. You need to run the
following commands once:

```
sudo tracebox traced --background
sudo tracebox traced_porbes --background

```

To collect data run:

```
sudo ./sched-analyzer
```

Press `CTRL+c` to stop. `sched-analyzer.perfetto-trace` will be in PWD that you
can open in [ui.perfetto.dev](https://ui.perfetto.dev)

![perfetto-screenshot](screenshots/sched-analyzer-perfetto.jpeg?raw=true)

### csv mode

```
sudo ./sched-analyzer --csv
```

Press `CTRL+c` to stop. Or `CTRL+z` followed by `bg` to keep running in
background - `sudo pkill -9 sched-analyzer` to force kill it when done.

### Warnings

Don't run more than one instance in csv mode, unless you specify different
--output_path.

On perfetto mode you'll end up with output generated for the first session that
got started, but output of all following sessions will be included on that
file.

### More options

```
./sched-analyzer --help
```

## sched-top

While `sched-analyzer --csv` is running

```
./sched-top
```

By default we show the util_avg of the top 10 busiest tasks every 5 seconds.

![sched-top-screenshot](screenshots/sched-top-task.png?raw=true "sched-top")

```
./sched-top --rq 2 10
```

Display the util_avg of CFS, RT and DL every 2 seconds, showing the last 10
seconds of information at a time.

![sched-top-screenshot](screenshots/sched-top-rq.png?raw=true "sched-top --rq 2 20")

```
./sched-top --rq_hist 2 10
```

Same as above but shows a histogram.

![sched-top-screenshot](screenshots/sched-top-rq-hist.png?raw=true "sched-top --rq_hist 2 20")

```
./sched-top --nr_running
```

Show nr_running for each runqueue every 5 seconds, displaying the last 20
seconds at a time.

![sched-top-screenshot](screenshots/sched-top-nr-running.png?raw=true "sched-top --nr_running")

```
./sched-top --nr_running_hist
```

Same as above but shows a histogram.

![sched-top-screenshot](screenshots/sched-top-nr-running-hist.png?raw=true "sched-top --nr_running_hist")

```
./sched-top --freq_residency 10 60 --theme dark
```

Show percentage of time spent in each frequency over the last 60 seconds
refreshing every 10 seconds. Use dark color theme.

![sched-top-screenshot](screenshots/sched-top-freq-residency.png?raw=true "sched-top --freq_residency 10 60 --theme dark")

```
./sched-top --idle_residency 1 5 --theme dark
```

Show percentage of time spent in each idle state over the last 5 seconds
refreshing every 1 second. Use dark color theme.

![sched-top-screenshot](screenshots/sched-top-idle-residency.png?raw=true "sched-top --idle_residency 1 5 --theme dark")

```
./sched-top -h
```

For list of all available options.
