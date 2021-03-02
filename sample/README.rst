.. SPDX-License-Identifier: BSD-3-Clause

===============================================
IPCF Shared Memory Sample Application for Linux
===============================================

:Copyright: 2018-2021 NXP

Overview
========
This sample application is a kernel module that demonstrates a ping-pong message
communication with an RTOS application, using the shared memory driver.

The sample app initializes the shared memory driver and sends messages to the
remote app, waiting for a reply after each message is sent. When a reply is
received from remote app, it wakes up and sends another message. The Linux app
is controlled from console via a sysfs file (see Running the Application).

Prerequisites
=============
 - EVB board for supported processors: S32V234, S32G274A and S32R45
 - NXP Automotive Linux BSP

Building the application
========================
This module is built with Yocto from NXP Auto Linux BSP, but can also be built
manually, if needed.

Note: modules are also included in NXP Auto Linux BSP pre-built images that can
      be downloaded from NXP Auto Linux BSP Flexera catalog.

Building with Yocto
-------------------
Follow the steps for building NXP Auto Linux BSP with Yocto:
 - https://source.codeaurora.org/external/autobsps32/auto_yocto_bsp/tree/README?h=alb/master

Note: use image fsl-image-auto with any of the following machines supported for IPCF:
      s32g274aevb, s32r45evb, s32v234evb.

Building manually
-----------------
1. Get NXP Auto Linux kernel and IPCF driver from Code Aurora::

   git clone https://source.codeaurora.org/external/autobsps32/linux/
   git clone https://source.codeaurora.org/external/autobsps32/ipcf/ipc-shm/

2. Export CROSS_COMPILE and ARCH variables and build Linux kernel providing the
   desired config, e.g. for S32G274A or S32R45::

   export CROSS_COMPILE=/<toolchain-path>/aarch64-linux-gnu-
   export ARCH=arm64
   make -C ./linux s32gen1_defconfig

3. Build IPCF driver and sample modules providing kernel source location, e.g.::

   make -C ./ipc-shm/sample KERNELDIR=$PWD/linux modules

.. _run-shm-linux:

Running the application
=======================
1. If sample was built manually, copy ipc-shm-dev.ko and ipc-shm-sample.ko in
   rootfs

2. Boot Linux: see section "How to boot" from Auto Linux BSP user manual.

3. Insert IPCF kernel modules after Linux boot::

    insmod /lib/modules/`uname -r`/extra/ipc-shm-dev.ko
    insmod /lib/modules/`uname -r`/extra/ipc-shm-sample.ko

4. Clear the kernel log::

    dmesg -c > /dev/null

5. Send 10 ping messages to remote OS and display output from kernel log::

    echo 10 > /sys/kernel/ipc-shm-sample/ping
    dmesg -c

6. Repeat previous step with different number of messages

7. Unload the modules::

    rmmod ipc-shm-sample ipc-shm-dev
