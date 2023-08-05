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

ARCH ?= $(call guess_arch)

# If no CROSS_COMPILE was given and target ARCH is different from machinen's
# ARCH, guess CROSS_COMPILE for the user
ifeq ($(CROSS_COMPILE),)
ifneq ($(ARCH), $(call guess_arch))
	CROSS_COMPILE := $(call guess_cross_compile, $(ARCH))
	CC := $(CROSS_COMPILE)gcc
	CXX := $(CROSS_COMPILE)$(CXX)
	AR := $(CROSS_COMPILE)$(AR)
endif
endif
