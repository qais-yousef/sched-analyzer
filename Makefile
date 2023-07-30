CLANG ?= clang
STRIP ?= llvm-strip
BPFTOOL ?= bpftool

LIBBPF_SRC ?= $(abspath libbpf/src)
PERFETTO_SRC ?= $(abspath perfetto/sdk)

ARCH ?= arm64
CFLAGS := -g -O2 -Wall
CFLAGS_BPF := $(CFLAGS) -target bpf -D__TARGET_ARCH_$(ARCH)
LDFLAGS := -lelf -lz -lpthread

SCHED_ANALYZER := sched-analyzer

VMLINUX_H := vmlinux.h
VMLINUX ?= /sys/kernel/btf/vmlinux

LIBBPF_DIR := $(abspath bpf)
LIBBPF_OBJ := $(LIBBPF_DIR)/usr/lib64/libbpf.a
LIBBPF_INCLUDE := -I$(abspath bpf/usr/include)

PERFETTO_OBJ := libperfetto.a
PERFETTO_INCLUDE := -I$(abspath $(PERFETTO_SRC))

SRC := sched-analyzer.c
OBJS :=$(subst .c,.o,$(SRC))

SRC_BPF := $(subst .c,.bpf.c,$(SRC))
OBJS_BPF := $(subst .bpf.c,.bpf.o,$(SRC_BPF))
SKEL_BPF := $(subst .bpf.c,.skel.h,$(SRC_BPF))

SRC_PERFETTO := $(PERFETTO_SRC)/perfetto.cc
OBJS_PERFETETO := $(subst .cc,.o,$(notdir $(SRC_PERFETTO)))

SRC_PERFETTO_WRAPPER := perfetto_wrapper.cc
OBJS_PERFETETO_WRAPPER := $(subst .cc,.o,$(notdir $(SRC_PERFETTO_WRAPPER)))

ifneq ($(STATIC),)
	LDFLAGS := $(LDFLAGS) -static
endif

ifneq ($(DEBUG),)
	CFLAGS := $(CFLAGS) -DDEBUG
	CFLAGS_BPF := $(CFLAGS_BPF) -DDEBUG
endif

all: $(SCHED_ANALYZER)

$(OBJS_PERFETETO): $(SRC_PERFETTO)
	git submodule init
	git submodule update
	$(CXX) $(CFLAGS) -c $^ -std=c++17 -o $@

$(OBJS_PERFETETO_WRAPPER): $(SRC_PERFETTO_WRAPPER)
	$(CXX) $(PERFETTO_INCLUDE) $(CFLAGS) -c $^ -std=c++17 -o $@

$(PERFETTO_OBJ): $(OBJS_PERFETETO) $(OBJS_PERFETETO_WRAPPER)
	$(AR) crf $@ $^

$(VMLINUX_H):
	$(BPFTOOL) btf dump file $(VMLINUX) format c > $@

$(LIBBPF_OBJ):
	git submodule init
	git submodule update
	$(MAKE) -C $(LIBBPF_SRC) BUILD_STATIC_ONLY=1 DESTDIR=$(LIBBPF_DIR) install

%.bpf.o: %.bpf.c $(VMLINUX_H) $(LIBBPF_OBJ)
	$(CLANG) $(CFLAGS_BPF) $(LIBBPF_INCLUDE) $(PERFETTO_INCLUDE) -c $< -o $@
	$(STRIP) -g $@

%.skel.h: %.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

%.o: %.c
	$(CC) $(CFLAGS) $(LIBBPF_INCLUDE) $(PERFETTO_INCLUDE) -c $< -o $@

$(OBJS): $(OBJS_BPF) $(SKEL_BPF) $(PERFETTO_OBJ)

$(SCHED_ANALYZER): $(OBJS)
	$(CXX) $(CFLAGS) $(LIBBPF_INCLUDE) $(PERFETTO_INCLUDE) $(filter %.o,$^) $(LIBBPF_OBJ) $(PERFETTO_OBJ) $(LDFLAGS) -o $@

release:
	make RELEASE=1

static:
	make STATIC=1

debug:
	make DEBUG=1

clean:
	$(MAKE) -C $(LIBBPF_SRC) clean
	rm -rf $(SCHED_ANALYZER) $(VMLINUX_H) *.o *.skel.h bpf *.a
