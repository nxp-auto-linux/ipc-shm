# SPDX-License-Identifier:	BSD-3-Clause
#
# Copyright (C) 2018 NXP Semiconductors
#

PWD := $(shell pwd)

ifneq ($(KERNELRELEASE),)
# kbuild part of makefile
obj-m := ipc-shm-dev.o
ipc-shm-dev-y := ipc-mod.o ipc-shm.o ipc-fifo.o

ifeq ($(CONFIG_SOC_S32GEN1),y)
ipc-shm-dev-y += ipc-hw-s32gen1.o
endif

ifeq ($(CONFIG_SOC_S32V234),y)
ipc-shm-dev-y += ipc-hw-s32v234.o
endif

# Add here cc flags (e.g. header lookup paths, defines, etc)
ccflags-y +=

else
ARCH ?= arm64

# The following parameters must be passed from the caller,
# e.g. build system (Yocto), command line:
# KERNELDIR    : Linux kernel source code location
# INSTALL_DIR  : location of the module installation
# CROSS_COMPILE: the path and prefix of the used cross compiler

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) $@

modules_install: modules
	$(MAKE) -C $(KERNELDIR) M=$(PWD) INSTALL_MOD_PATH=$(INSTALL_DIR) $@

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) $@

.PHONY: modules clean

endif
