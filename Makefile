SHELL=/bin/bash
CROSS_COMPILE ?=
CLANG ?= clang
STRIP ?= llvm-strip
BPFTOOL ?= bpftool

ANDROID_NDK_URL ?= https://dl.google.com/android/repository
ANDROID_NDK_ZIP ?= android-ndk-r26-beta1-linux.zip
ANDROID_NDK_DIR ?= $(shell pwd)/ndk
ANDROID_NDK_VER ?= 33
ANDROID_TOOLCHAIN_PATH ?= toolchains/llvm/prebuilt/linux-x86_64/bin
ANDROID_TOOLCHAIN_PREFIX ?= aarch64-linux-android$(ANDROID_NDK_VER)-
ANDROID_SYSROOT_PATH ?= $(ANDROID_TOOLCHAIN_PATH)/../sysroot

ANDROID_NDK_PATH := $(ANDROID_NDK_DIR)/$(subst -linux.zip,,$(ANDROID_NDK_ZIP))
ANDROID_TOOLCHAIN := $(ANDROID_NDK_PATH)/$(ANDROID_TOOLCHAIN_PATH)/$(ANDROID_TOOLCHAIN_PREFIX)
ANDROID_SYSROOT := $(ANDROID_NDK_PATH)/$(ANDROID_SYSROOT_PATH)

$(info $(ANDROID_SYSROOT))

include cross_compile.mk

LIBBPF_SRC ?= $(abspath libbpf/src)
PERFETTO_SRC ?= $(abspath perfetto/sdk)

CFLAGS := -g -O2 -Wall
CFLAGS_BPF := $(CFLAGS) -target bpf -D__TARGET_ARCH_$(ARCH) -D__SA_BPF_BUILD
LDFLAGS := -lelf -lz -lpthread

SCHED_ANALYZER := sched-analyzer

VMLINUX_H := vmlinux.h
VMLINUX ?= /sys/kernel/btf/vmlinux

LIBBPF_DIR := $(abspath bpf)
LIBBPF_OBJ := $(LIBBPF_DIR)/usr/lib64/libbpf.a
LIBBPF_INCLUDE := -I$(abspath bpf/usr/include)

PERFETTO_OBJ := libperfetto.a
PERFETTO_INCLUDE := -I$(abspath $(PERFETTO_SRC))

SRC := sched-analyzer.c parse_argp.c
OBJS :=$(subst .c,.o,$(SRC))

SRC_BPF := $(wildcard *.bpf.c)
OBJS_BPF := $(subst .bpf.c,.bpf.o,$(SRC_BPF))
SKEL_BPF := $(subst .bpf.c,.skel.h,$(SRC_BPF))

SRC_PERFETTO := $(PERFETTO_SRC)/perfetto.cc
OBJS_PERFETETO := $(subst .cc,.o,$(notdir $(SRC_PERFETTO)))

SRC_PERFETTO_WRAPPER := perfetto_wrapper.cc
OBJS_PERFETETO_WRAPPER := $(subst .cc,.o,$(notdir $(SRC_PERFETTO_WRAPPER)))

INCLUDES := $(LIBBPF_INCLUDE) $(PERFETTO_INCLUDE)
LDFLAGS := $(LIBBPF_OBJ) $(PERFETTO_OBJ) $(LDFLAGS)

ifneq ($(ANDROID),)
	INCLUDES := -I$(ANDROID_SYSROOT)/usr/include -I$(ANDROID_SYSROOT)/usr/include/aarch64-linux-android $(INCLUDES)
	EXTRA_LDFLAGS := --sysroot=$(ANDROID_SYSROOT) -B $(ANDROID_TOOLCHAIN_PATH) -L$(ANDROID_SYSROOT)/usr/lib/aarch64-linux-android/33/
	LDFLAGS := $(EXTRA_LDFLAGS) $(LDFLAGS)
endif

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
	$(BPFTOOL) btf dump file $(VMLINUX) format c > $@ || rm $@; exit 1

$(LIBBPF_OBJ):
	git submodule init
	git submodule update
	$(MAKE) -C $(LIBBPF_SRC) CC=$(CC) EXTRA_LDFLAGS="$(EXTRA_LDFLAGS)" EXTRA_CFLAGS="$(INCLUDES)" BUILD_STATIC_ONLY=1 DESTDIR=$(LIBBPF_DIR) install

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

android:
	if [ ! -e $(ANDROID_NDK_PATH) ]; then				\
		if [ ! -e $(ANDROID_NDK_DIR)/$(ANDROID_NDK_ZIP) ]; then	\
			mkdir -p $(ANDROID_NDK_DIR);			\
			cd $(ANDROID_NDK_DIR);				\
			wget $(ANDROID_NDK_URL)/$(ANDROID_NDK_ZIP);	\
			cd -;						\
		fi;							\
		cd $(ANDROID_NDK_DIR);					\
		unzip $(ANDROID_NDK_ZIP);				\
		cd -;							\
	fi
	make ANDROID=1 $(filter-out android, $(MAKECMDGOALS))

release:
	make RELEASE=1 $(filter-out release, $(MAKECMDGOALS))

static:
	make STATIC=1 $(filter-out static, $(MAKECMDGOALS))

debug:
	make DEBUG=1 $(filter-out debug, $(MAKECMDGOALS))

clean:
	$(MAKE) -C $(LIBBPF_SRC) clean
	rm -rf $(SCHED_ANALYZER) $(VMLINUX_H) *.o *.skel.h bpf *.a

help:
	@echo "Following build targets are available:"
	@echo ""
	@echo "\tstatic:\tCreate statically linked binary"
	@echo "\tdebug:\tCreate a debug build which contains verbose debug prints"
	@echo "\tclean:\tCreate a debug build which contains verbose debug prints"
	@echo ""
	@echo "Cross compile:"
	@echo ""
	@echo "\tmake ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-"
	@echo ""
	@echo "\tYou can only specifiy the ARCH and we will try to guess the correct gcc CROSS_COMPILE to use"
