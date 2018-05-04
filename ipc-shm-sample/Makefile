# SPDX-License-Identifier:	BSD-3-Clause
#
# Copyright (C) 2018 NXP Semiconductors
#

obj-m := ipc-shm-sample.o
PWD := $(shell pwd)
IPC_SHM_DEV_ROOT ?= $(PWD)/../ipc-shm-dev

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) KBUILD_EXTRA_SYMBOLS=${IPC_SHM_DEV_ROOT}/Module.symvers $@

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $@

.PHONY: modules clean
