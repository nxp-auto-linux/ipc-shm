.. SPDX-License-Identifier: BSD-3-Clause

==============================================================
IPCF Shared Memory Sample Multi-Instance Application for Linux
==============================================================

:Copyright: 2021-2022,2023 NXP

Overview
========
This sample application is a kernel module that demonstrates a ping-pong message
communication with an RTOS application, using the shared memory driver.

The application initializes the shared memory driver with two instances and sends
messages to the remote application, waiting for a reply after each message is sent.
When a reply is received from remote application, it wakes up and sends another
message.

This application can be built to notify the remote application using inter-core
interrupts (default behavior) or to transmit without notifying the remote
application. If the latter is used, the remote application polls for available
messages.

This application is controlled from console via a sysfs file (see Running the
Application).

Prerequisites
=============
 - EVB board for supported processors
 - NXP Automotive Linux BSP

Building the application
========================
This module is built with Yocto from NXP Auto Linux BSP, but can also be built
manually, if needed.

Note: modules are also included in NXP Auto Linux BSP pre-built images that can
      be downloaded from NXP Auto Linux BSP Flexera catalog.

Building with Yocto
-------------------
1. Follow the steps for building NXP Auto Linux BSP with Yocto::

   Linux BSP User Manual from Flexera catalog

2. Use the branch release/**IPCF_RELEASE_NAME** and modify in
   build/sources/meta-alb/recipes-kernel/ipc-shm/ipc-shm.bb::

    - BRANCH ?= "${RELEASE_BASE}"
    + BRANCH ?= "release/**IPCF_RELEASE_NAME**"

    - SRCREV = "xxxxxxxxxx"
    + SRCREV = "${AUTOREV}"

  where **IPCF_RELEASE_NAME** is the name of Inter-Platform Communication
  Framework release from Flexera catalog and replace the line with SRCREV
  with SRCREV = "${AUTOREV}".

Note: use image **fsl-image-auto** with any of machine supported or
      add the following line in conf/local.conf file:
      *IMAGE_INSTALL_append_pn-fsl-image-auto = " ipc-shm"*

Building manually
-----------------
1. Get NXP Auto Linux kernel and IPCF driver from GitHub::

    git clone https://github.com/nxp-auto-linux/linux
    git clone https://github.com/nxp-auto-linux/ipc-shm
    git -C ipc-shm checkout --track origin/release/**IPCF_RELEASE_NAME**

Note: use the release branch: release/**IPCF_RELEASE_NAME**
      where **IPCF_RELEASE_NAME** is the name of Inter-Platform Communication
      Framework release from Flexera catalog

2. Export CROSS_COMPILE and ARCH variables and build Linux kernel providing the
   desired config::

    export CROSS_COMPILE=/<toolchain-path>/aarch64-linux-gnu-
    export ARCH=arm64
    make -C ./linux s32gen1_defconfig
    make -C ./linux

3. Build IPCF driver and sample modules providing kernel source location, e.g.::

    make -C ./ipc-shm/sample PLATFORM_FLAVOR=[PLATFORM] KERNELDIR=$PWD/linux modules

    User must specific the platform::

        PLATFORM_FLAVOR=s32g2
        PLATFORM_FLAVOR=s32r45
        PLATFORM_FLAVOR=s32g3

.. _run-shm-linux:

Running the application
=======================
1. If sample was built manually, copy ipc-shm-dev.ko and ipc-shm-sample_multi-instance.ko
   in rootfs

2. Boot Linux: see section "How to boot" from Auto Linux BSP user manual.

3. Insert IPCF kernel modules after Linux boot::

    insmod /lib/modules/`uname -r`/extra/ipc-shm-dev.ko
    insmod /lib/modules/`uname -r`/extra/ipc-shm-sample_multi-instance.ko

4. Clear the kernel log::

    dmesg -c > /dev/null

5. Send 10 ping messages to remote OS on instance 0 and display output from kernel log::

    echo 10 > /sys/kernel/ipc-shm-sample-instance0/ping
    dmesg -c

6. Send 10 ping messages to remote OS on instance 1 and display output from kernel log::

    echo 10 > /sys/kernel/ipc-shm-sample-instance1/ping
    dmesg -c

7. Repeat previous step with different number of messages

8. Unload the modules::

    rmmod ipc-shm-sample-instance ipc-shm-dev
