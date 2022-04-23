CLANG ?= clang
STRIP ?= llvm-strip

LIBBPF_SRC ?= $(abspath libbpf/src)

ARCH ?= arm64
CFLAGS := -g -O2 -Wall
CFLAGS_BPF := $(CFLAGS) -target bpf -D__TARGET_ARCH_$(ARCH)
LDFLAGS := -lelf -lz -static

SCHED_ANALYZER := sched-analyzer

VMLINUX_H := vmlinux.h
VMLINUX ?= /sys/kernel/btf/vmlinux

LIBBPF_DIR := $(abspath bpf)
LIBBPF_OBJ := $(LIBBPF_DIR)/usr/lib64/libbpf.a
LIBBPF_INCLUDE := -I$(abspath bpf/usr/include)

SRC := sched-analyzer.c
OBJS :=$(subst .c,.o,$(SRC))

SRC_BPF := $(subst .c,.bpf.c,$(SRC))
OBJS_BPF := $(subst .bpf.c,.bpf.o,$(SRC_BPF))
SKEL_BPF := $(subst .bpf.c,.skel.h,$(SRC_BPF))

all: $(SCHED_ANALYZER)

$(VMLINUX_H):
	bpftool btf dump file $(VMLINUX) format c > $@

$(LIBBPF_OBJ):
	$(MAKE) -C $(LIBBPF_SRC) BUILD_STATIC_ONLY=1 DESTDIR=$(LIBBPF_DIR) install

%.bpf.o: %.bpf.c $(VMLINUX_H) $(LIBBPF_OBJ)
	$(CLANG) $(CFLAGS_BPF) $(LIBBPF_INCLUDE) -c $< -o $@
	$(STRIP) -g $@

%.skel.h: %.bpf.o
	bpftool gen skeleton $< > $@

%.o: %.c
	$(CC) $(CFLAGS) $(LIBBPF_INCLUDE) -c $< -o $@

$(OBJS): $(OBJS_BPF) $(SKEL_BPF)

$(SCHED_ANALYZER): $(OBJS)
	$(CC) $(CFLAGS) $(LIBBPF_INCLUDE) $(filter %.o,$^) $(LIBBPF_OBJ) $(LDFLAGS) -o $@

clean:
	$(MAKE) -C $(LIBBPF_SRC) clean
	rm -rf $(SCHED_ANALYZER) $(VMLINUX_H) *.o *.skel.h bpf
