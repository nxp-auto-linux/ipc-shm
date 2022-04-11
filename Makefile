# SPDX-License-Identifier:	BSD-3-Clause
#
# Copyright 2018-2022 NXP
#

MODULE_NAME := ipc-shm-dev
UIO_MODULE_NAME := ipc-shm-uio
PLATFORM_FLAVOR ?= s32g2

ifneq ($(KERNELRELEASE),)
# kbuild part of makefile

obj-m := $(MODULE_NAME).o $(UIO_MODULE_NAME).o

$(MODULE_NAME)-y := ipc-shm.o ipc-queue.o os/ipc-os.o

$(UIO_MODULE_NAME)-y := os/ipc-uio.o

ifeq ($(CONFIG_SOC_S32V234),y)
$(MODULE_NAME)-y += hw/ipc-hw-s32v234.o
$(UIO_MODULE_NAME)-y += hw/ipc-hw-s32v234.o
else
ifeq ($(PLATFORM_FLAVOR),s32g3)
$(MODULE_NAME)-y += hw/ipc-hw-s32g3xx.o
$(UIO_MODULE_NAME)-y += hw/ipc-hw-s32g3xx.o
else
$(MODULE_NAME)-y += hw/ipc-hw-s32gen1.o
$(UIO_MODULE_NAME)-y += hw/ipc-hw-s32gen1.o
endif
endif

# Add here cc flags (e.g. header lookup paths, defines, etc)
ccflags-y += -I$(src) -I$(src)/hw -I$(src)/os

else
# normal part of makefile

ARCH ?= arm64
PWD := $(shell pwd)

# The following parameters must be passed from the caller,
# e.g. build system (Yocto), command line:
# KERNELDIR    : Linux kernel source code location
# INSTALL_DIR  : location of the module installation
# CROSS_COMPILE: the path and prefix of the used cross compiler
# PLATFORM_FLAVOR: build for s32g2, s32r45 or s32g3

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) $@

modules_install: modules
	$(MAKE) -C $(KERNELDIR) M=$(PWD) INSTALL_MOD_PATH=$(INSTALL_DIR) $@

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) $@

.PHONY: modules clean

endif
