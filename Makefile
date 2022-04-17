CLANG ?= clang
STRIP ?= llvm-strip

LIBBPF_SRC ?= libbpf/src

ARCH := arm64
CFLAGS := -g -O2 -Wall
CFLAGS_BPF := $(CFLAGS) -target bpf -D__TARGET_ARCH_$(ARCH)
LDFLAGS := -lelf -lz

SCHED_ANALYZER := sched-analyzer

VMLINUX_H := vmlinux.h
VMLINUX ?= /sys/kernel/btf/vmlinux

SRC_BPF := bpf-sched-analyzer.bpf.c
OBJS_BPF := $(subst .bpf.c,.bpf.o,$(SRC_BPF))
SKEL_BPF := $(subst .bpf.c,.skel.h,$(SRC_BPF))

all: $(SCHED_ANALYZER)

$(VMLINUX_H):
	bpftool btf dump file $(VMLINUX) format c > $@

%.bpf.o: %.bpf.c $(VMLINUX_H)
	$(CLANG) $(CFLAGS_BPF) $< -o $@
	$(STRIP) -g $@

%.skel.h: %.bpf.o
	bpftool gen skeleton $< > $@

$(SCHED_ANALYZER): $(OBJS_BPF) $(SKEL_BPF)
	echo $(OBJS_BPF)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -f $(SCHED_ANALYZER) $(VMLINUX_H) *.o
