# SPDX-License-Identifier:	BSD-3-Clause
#
# Copyright (C) 2018 NXP Semiconductors
#

PWD := $(shell pwd)

ifneq ($(KERNELRELEASE),)
# kbuild part of makefile
obj-m := ipc-shm.o
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
