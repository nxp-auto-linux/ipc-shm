# SPDX-License-Identifier:	BSD-3-Clause
#
# Copyright 2021-2023 NXP
#

MODULE_NAME := ipc-shm-sample_multi_instance
PLATFORM_FLAVOR ?= UNDEFINED

ifneq ($(KERNELRELEASE),)
# kbuild part of makefile

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-y := sample_multi_instance.o

ifeq ($(PLATFORM_FLAVOR),UNDEFINED)
$(error "PLATFORM_FLAVOR is UNDEFINED")
else ifeq ($(CONFIG_SOC_S32V234),y)
PLATFORM_HW := s32v234
else ifeq ($(PLATFORM_FLAVOR),s32g3)
PLATFORM_HW := s32g3xx
else
PLATFORM_HW := s32gen1
endif

$(MODULE_NAME)-y += ipcf_Ip_Cfg_$(PLATFORM_FLAVOR).o
ccflags-y += -DPLATFORM_FLAVOR_$(PLATFORM_FLAVOR) \
             -I$(PWD)/.. \
             -I$(PWD)/../hw \
             -I$(PWD)/../hw/$(PLATFORM_HW)\
             -I$(PWD)/../os

else
# normal part of makefile

ARCH ?= arm64
PWD := $(shell pwd)
IPC_SHM_DEV_PATH ?= $(PWD)/..

# Add here cc flags (e.g. header lookup paths, defines, etc)
ccflags-y +=

# additional module symbols dependency
EXTRA_SYMBOLS=${IPC_SHM_DEV_PATH}/Module.symvers

# The following parameters must be passed from the caller,
# e.g. build system (Yocto), command line:
# KERNELDIR    : Linux kernel source code location
# INSTALL_DIR  : location of the module installation
# CROSS_COMPILE: cross compiler path and prefix
# PLATFORM_FLAVOR: build for s32g2, s32r45 or s32g3

modules:
	$(MAKE) -C $(IPC_SHM_DEV_PATH) KERNELDIR=$(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) $@
	$(MAKE) -C $(KERNELDIR) M=$(PWD) KBUILD_EXTRA_SYMBOLS=$(EXTRA_SYMBOLS) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) $@

modules_install: modules
	$(MAKE) -C $(IPC_SHM_DEV_PATH) KERNELDIR=$(KERNELDIR) INSTALL_MOD_PATH="$(INSTALL_DIR)" $@
	$(MAKE) -C $(KERNELDIR) M=$(PWD) INSTALL_MOD_PATH="$(INSTALL_DIR)" $@

clean:
	$(MAKE) -C $(IPC_SHM_DEV_PATH) KERNELDIR=$(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) $@
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) $@

.PHONY: clean
endif
