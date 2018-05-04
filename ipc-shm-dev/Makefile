# SPDX-License-Identifier:	BSD-3-Clause
#
# Copyright (C) 2018 NXP Semiconductors
#

obj-m := ipc-shm.o
PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $@

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $@

.PHONY: modules clean
