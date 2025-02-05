# SPDX-License-Identifier:	BSD-3-Clause
#
# Copyright 2023,2025 NXP
#

# The following variables must be defined by caller:
#  CROSS_COMPILE    : cross compiler path and prefix
#  KERNELDIR        : Linux kernel source code location
#  PLATFORM_FLAVOR  : platform to build for

MAKEFLAGS += --warn-undefined-variables
EXTRA_CFLAGS ?=
EXTRA_LDFLAGS ?=
.DEFAULT_GOAL := all
PLATFORM_FLAVOR ?= s32g2

ifeq ($(CROSS_COMPILE),)
$(error CROSS_COMPILE is not set!)
endif

PLATFORM_FLAVOR_LIST := s32g2 s32g3 s32r45 s32v234 s32n7
ifeq ($(filter-out $(PLATFORM_FLAVOR),$(PLATFORM_FLAVOR_LIST)),$(PLATFORM_FLAVOR_LIST))
    $(error Set PLATFORM_FLAVOR variable to a supported platform: '$(PLATFORM_FLAVOR_LIST)')
endif

ifeq ($(PLATFORM_FLAVOR),s32g2)
CFLAGS += -DPLATFORM_FLAVOR_s32g2
else ifeq ($(PLATFORM_FLAVOR),s32r45)
CFLAGS += -DPLATFORM_FLAVOR_s32r45
else ifeq ($(PLATFORM_FLAVOR),s32g3)
CFLAGS += -DPLATFORM_FLAVOR_s32g3
else ifeq ($(PLATFORM_FLAVOR),s32n7)
CFLAGS += -DPLATFORM_FLAVOR_s32n7
endif

CC := $(CROSS_COMPILE)gcc
RM := rm -rf

elf_name := ipc-shm-sample_uio.elf
libipc_dir ?= $(shell pwd)/../os_uio

CFLAGS += -Wall -g -I$(libipc_dir)/.. -DCONFIG_SOC_$(PLATFORM)
# For debug
# CFLAGS += -DDEBUG
CFLAGS += $(EXTRA_CFLAGS)
LDFLAGS += -L$(libipc_dir)/.. -lipc-shm -lpthread -lrt
LDFLAGS += $(EXTRA_LDFLAGS)

# object file list
objs = sample.o
objs += ipcf_Ip_Cfg_$(PLATFORM_FLAVOR).o

%.o: %.c
	@echo 'Building app file: $<'
	$(CC) -c $(CFLAGS) -o $@ $<
	@echo ' '

$(elf_name): $(objs) libipc
	@echo 'Building target: $@'
	$(CC) -o $(elf_name) $(objs) $(LDFLAGS)
	@echo 'Finished building target: $@'
	@echo ' '

libipc:
	$(MAKE) -C $(libipc_dir) PLATFORM_FLAVOR=$(PLATFORM_FLAVOR)

all: $(elf_name)

clean:
	$(MAKE) -C $(libipc_dir) $@
	$(RM) $(objs) $(elf_name)
	@echo ' '

.PHONY: all clean libipc
