guess_arch := $(shell uname -m			| \
		sed 's/x86_64/x86/'		| \
		sed 's/aarch64/arm64/'		| \
		sed 's/ppc64le/powerpc/'	| \
		sed 's/mips.*/mips/'		  \
		)

guess_cross_compile = $(shell echo $(1)			| \
		sed 's/x86/x86_64-linux-gnu-/'	| \
		sed 's/arm64/aarch64-linux-gnu-/'	| \
		sed 's/powerpc/powerpc64le-linux-gnu-/'	| \
		sed 's/mips/mips-linux-gnu-/'		  \
		)

ifneq ($(ANDROID),)
	ARCH := arm64
	CROSS_COMPILE := $(ANDROID_TOOLCHAIN)
	CC := $(CROSS_COMPILE)clang
	CXX := $(CROSS_COMPILE)clang++
	AR := llvm-ar
else # ANDROID

ARCH ?= $(call guess_arch)

# If no CROSS_COMPILE was given and target ARCH is different from machinen's
# ARCH, guess CROSS_COMPILE for the user
ifeq ($(CROSS_COMPILE),)
ifneq ($(ARCH), $(call guess_arch))
	CROSS_COMPILE := $(call guess_cross_compile, $(ARCH))
endif
endif

ifneq ($(CROSS_COMPILE),)
	CC := $(CROSS_COMPILE)gcc
	CXX := $(CROSS_COMPILE)$(CXX)
	AR := $(CROSS_COMPILE)$(AR)
endif

endif # ANDROID
