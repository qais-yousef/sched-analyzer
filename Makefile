CC ?= clang
STRIP ?= llvm-strip

LIBBPF_SRC ?= libbpf/src

CFLAGS := -g -Wall
LDFLAGS := -lelf -lz

SCHED_ANALYZER := sched-analyzer

VMLINUX_H := vmlinux.h
VMLINUX ?= /sys/kernel/btf/vmlinux

all: $(SCHED_ANALYZER)

$(VMLINUX_H):
	bpftool btf dump file $(VMLINUX) format c > $@

$(SCHED_ANALYZER): $(wildcard *.c) $(VMLINUX_H)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -f *.o
	rm -f $(SCHED_ANALYZER) $(VMLINUX_H)
