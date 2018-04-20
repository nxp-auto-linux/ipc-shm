# SPDX-License-Identifier:	BSD-3-Clause
#
# Copyright (C) 2018 NXP Semiconductors
#

obj-m := ipc-shm.o
PWD := $(shell pwd)

kbuild:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

.PHONY: kbuild clean
