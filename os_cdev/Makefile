# SPDX-License-Identifier:	BSD-3-Clause
#
# Copyright 2023 NXP
#
MAKEFLAGS += --warn-undefined-variables
EXTRA_CFLAGS ?=

ifeq ($(CROSS_COMPILE),)
$(error CROSS_COMPILE is not set!)
endif

# IPC_CDEV_MODULE_DIR needed for automatically inserting kernel module
IPC_CDEV_MODULE_DIR ?=
ifeq ($(IPC_CDEV_MODULE_DIR),)
IPC_CDEV_MODULE_DIR := .
$(info IPC_CDEV_MODULE_DIR is not set! using default value)
endif
$(info IPC_CDEV_MODULE_DIR = $(IPC_CDEV_MODULE_DIR))

CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar
RM := rm -f

includes := -I./.. -I./../hw -I./../os_cdev -I./../os_kernel

CFLAGS += -Wall -g $(includes) #-DDEBUG
CFLAGS += $(EXTRA_CFLAGS)

lib_name = ../libipc-shm.a

ipc_cdev_name = ipc-shm-cdev
CFLAGS += -DIPC_ISR_MODULE_PATH=\"$(IPC_CDEV_MODULE_DIR)/$(ipc_cdev_name).ko\"
CFLAGS += -DIPC_ISR_MODULE_NAME=\"$(shell echo $(ipc_cdev_name) | tr '-' '_')\"

# object file list
objs = ../ipc-shm.o ../ipc-queue.o ipc-os.o

%.o: %.c
	@echo 'Building lib file: $<'
	$(CC) -c $(CFLAGS) -o $@ $<
	@echo ' '

$(lib_name): $(objs)
	@echo 'Building target: $@'
	$(AR) rcs $(lib_name) $(objs)
	@echo ' '

clean:
	$(RM) $(objs) $(lib_name)

.PHONY: clean
