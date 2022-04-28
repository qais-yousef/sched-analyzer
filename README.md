# sched-analyzer

BPF CO-RE based sched-analyzer

*This is still WIP*

This is a personal pet project and not affiliated with any organization.

## Goal

The BPF backend, `sched-analyzer`, will collect data and dump them into csv
files for post processing by any front end, or by custom user scripts that is
interested in performing extra processing on these data.

The python frontend, `sched-top`, examines these csv files on regular intervals
and depicts these info on the terminal.

# Requirements

```
sudo apt install linux-tools-$(uname -r) git clang llvm libelf1 zlib1g

pip install -r requirements.txt
```

You need a kernel compiled with BTF to Compile and Run.

Required kernel config to get BTF:

- CONFIG_DEBUG_INFO_BTF=y

# Build

```
make
```

# Run

```
./sched-top
```

To load the BPF program, you'll be asked for sudo password.

![sched-top-screenshot](screenshots/sched-top-screenshot.png?raw=true "sched-top")
