# sched-analyzer

BPF CO-RE based sched-analyzer

# Requirements

```
sudo apt install linux-tools-$(uname -r) git clang llvm libelf1 zlib1g

pip install -r requirements.txt
```

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
