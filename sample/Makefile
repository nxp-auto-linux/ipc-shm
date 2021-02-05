# SPDX-License-Identifier:	BSD-3-Clause
#
# Copyright 2018,2021 NXP
#

MODULE_NAME := ipc-shm-sample

ifneq ($(KERNELRELEASE),)
# kbuild part of makefile

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-y := sample.o

polling ?= no
ifeq ($(polling),yes)
ccflags-y += -DPOLLING
endif

# Add here cc flags (e.g. header lookup paths, defines, etc)
ccflags-y +=

else
# normal part of makefile

ARCH ?= arm64
PWD := $(shell pwd)
IPC_SHM_DEV_PATH ?= $(PWD)/..

# additional module symbols dependency
EXTRA_SYMBOLS=${IPC_SHM_DEV_PATH}/Module.symvers

# The following parameters must be passed from the caller,
# e.g. build system (Yocto), command line:
# KERNELDIR    : Linux kernel source code location
# INSTALL_DIR  : location of the module installation
# CROSS_COMPILE: cross compiler path and prefix
# Optional parameters:
# polling      : set yo 'yes' to disable tx interrupt if
#                remote sample application use polling

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
