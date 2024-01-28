SHELL=/bin/bash
CROSS_COMPILE ?=
CLANG ?= clang
STRIP ?= llvm-strip
BPFTOOL ?= bpftool

VERSION=$(shell git describe --tags)

include cross_compile.mk

LIBBPF_SRC ?= $(abspath libbpf/src)
PERFETTO_SRC ?= $(abspath perfetto/sdk)

CFLAGS := -g -O2 -Wall -DSA_VERSION=$(VERSION)
CFLAGS_BPF := $(CFLAGS) -target bpf -D__TARGET_ARCH_$(ARCH) -D__SA_BPF_BUILD
LDFLAGS := -lelf -lz -lpthread

SCHED_ANALYZER := sched-analyzer

VMLINUX_H := vmlinux.h
VMLINUX ?= /sys/kernel/btf/vmlinux

LIBBPF_DIR := $(abspath bpf)
LIBBPF_OBJ := $(LIBBPF_DIR)/usr/lib64/libbpf.a
LIBBPF_INCLUDE := -I$(abspath bpf/usr/include)

PERFETTO_DIR := $(abspath perfetto_sdk)
PERFETTO_OBJ := $(PERFETTO_DIR)/libperfetto.a
PERFETTO_INCLUDE := -I$(abspath $(PERFETTO_SRC))

SRC := sched-analyzer.c parse_argp.c parse_kallsyms.c
OBJS :=$(subst .c,.o,$(SRC))

SRC_BPF := $(wildcard *.bpf.c)
OBJS_BPF := $(subst .bpf.c,.bpf.o,$(SRC_BPF))
SKEL_BPF := $(subst .bpf.c,.skel.h,$(SRC_BPF))

SRC_PERFETTO := $(PERFETTO_SRC)/perfetto.cc
OBJS_PERFETETO := $(PERFETTO_DIR)/$(subst .cc,.o,$(notdir $(SRC_PERFETTO)))

SRC_PERFETTO_WRAPPER := perfetto_wrapper.cc
OBJS_PERFETETO_WRAPPER := $(subst .cc,.o,$(notdir $(SRC_PERFETTO_WRAPPER)))

INCLUDES := $(LIBBPF_INCLUDE) $(PERFETTO_INCLUDE)
LDFLAGS := $(LIBBPF_OBJ) $(PERFETTO_OBJ) $(LDFLAGS)

ifneq ($(STATIC),)
	LDFLAGS := $(LDFLAGS) $(shell [ $$(find /usr/lib -name libzstd.a | grep .) ] && echo -lzstd)
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
	mkdir -p $(PERFETTO_DIR)
	$(CXX) $(CFLAGS) -c $^ -std=c++17 -o $@

$(OBJS_PERFETETO_WRAPPER): $(SRC_PERFETTO_WRAPPER)
	$(CXX) $(PERFETTO_INCLUDE) $(CFLAGS) -c $^ -std=c++17 -o $@

$(PERFETTO_OBJ): $(OBJS_PERFETETO) $(OBJS_PERFETETO_WRAPPER)
	$(AR) crf $@ $^

$(VMLINUX_H):
	($(BPFTOOL) btf dump file $(VMLINUX) format c > $@) || (rm $@ && exit 1)

$(LIBBPF_OBJ):
	git submodule init
	git submodule update
	$(MAKE) -C $(LIBBPF_SRC) CC=$(CC) BUILD_STATIC_ONLY=1 DESTDIR=$(LIBBPF_DIR) install

%.bpf.o: %.bpf.c $(VMLINUX_H) $(LIBBPF_OBJ)
	$(CLANG) $(CFLAGS_BPF) $(INCLUDES) -c $< -o $@
	$(STRIP) -g $@

%.skel.h: %.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJS): $(OBJS_BPF) $(SKEL_BPF) $(PERFETTO_OBJ)

$(SCHED_ANALYZER): $(OBJS)
	$(CXX) $(CFLAGS) $(INCLUDES) $(filter %.o,$^) $(LDFLAGS) -o $@

package: $(SCHED_ANALYZER)
	tar cfz $(SCHED_ANALYZER)-$(ARCH)-$(VERSION)$(shell [ "$(STATIC)x" != "x" ] && echo "-static").tar.gz $(SCHED_ANALYZER)

release:
	[ "$(shell ls | grep $(SCHED_ANALYZER).*.tar.gz)x" == "x" ] || (echo "Release file found, clean then try again" && exit 1)
	$(MAKE) clobber
	$(MAKE) ARCH=x86 package
	$(MAKE) clean
	$(MAKE) ARCH=x86 STATIC=1 package
	$(MAKE) clobber
	$(MAKE) ARCH=arm64 package
	$(MAKE) clean
	$(MAKE) ARCH=arm64 STATIC=1 package

static:
	$(MAKE) STATIC=1

debug:
	$(MAKE) DEBUG=1

clean:
	rm -rf $(SCHED_ANALYZER) *.o *.skel.h

clobber: clean
	$(MAKE) -C $(LIBBPF_SRC) clean
	rm -rf $(LIBBPF_DIR) $(PERFETTO_DIR)

help:
	@echo "Following build targets are available:"
	@echo ""
	@echo "	static:		Create statically linked binary"
	@echo "	debug:		Create a debug build which contains verbose debug prints"
	@echo "	clean:		Clean sched-analyzer, but not dependent libraries"
	@echo "	clobber:	Clean everything"
	@echo ""
	@echo "Cross compile:"
	@echo ""
	@echo "	make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-"
	@echo ""
	@echo "	You can only specifiy the ARCH and we will try to guess the correct gcc CROSS_COMPILE to use"
