.. SPDX-License-Identifier: BSD-3-Clause

===============================================
IPCF Shared Memory Sample Application for Linux
===============================================

:Copyright: 2018-2019 NXP

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
 - EVB board for S32V234 silicon, cut 2.0, maskset 1N81U or
 - Synopsis VDK for S32xx presilicon (S32G275, S32R45x)

Building the application
========================
This module is built with Yocto from NXP Auto Linux BSP, but can also be built
manually, if needed.

Note: modules are also included in NXP Auto Linux BSP pre-built binaries that
can be downloaded from `Flexera <https://nxp.flexnetoperations.com/control/frse/product?child_plneID=738347&ver=CURRENT>`_

Building with Yocto
-------------------
Follow the steps for building NXP Auto Linux BSP with Yocto:
 - https://source.codeaurora.org/external/autobsps32/auto_yocto_bsp/tree/README?h=alb/master

Notes:
 - Only the following machines are supported for IPCF: s32g275sim, s32r45xsim,
   s32v234evb.
 - Use image fsl-image-sim for S32xx pre-silicon and fsl-image-auto for
   S32V234 silicon.

Building manually
-----------------
- Get NXP Auto Linux kernel and IPCF driver from Code Aurora::

   git clone https://source.codeaurora.org/external/autobsps32/linux/
   git clone https://source.codeaurora.org/external/autobsps32/ipcf/ipc-shm/

- Export CROSS_COMPILE variable and build modules providing kernel source
  location, e.g.::

   export CROSS_COMPILE=~/toolchain/gcc-linaro-6.3.1-2017.02-x86_64_aarch64-linux-gnu/bin/\
   aarch64-linux-gnu-
   make -C ./ipc-shm/sample KERNELDIR=./linux modules

.. _run-shm-linux:

Running the application
=======================
1. If sample was built manually, copy ipc-shm-dev.ko and ipc-shm-sample.ko in
   rootfs

2. Boot Linux: for silicon, see section "How to boot" from Auto Linux BSP user
   manual. For pre-silicon read only the VDK related section 3.3.6.

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
