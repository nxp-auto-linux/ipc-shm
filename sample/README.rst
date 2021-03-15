.. SPDX-License-Identifier: BSD-3-Clause

===============================================
IPCF Shared Memory Sample Application for Linux
===============================================

:Copyright: 2018-2021 NXP

Overview
========
This sample application is a kernel module that demonstrates a ping-pong message
communication with an RTOS application, using the shared memory driver.

The application initializes the shared memory driver and sends messages to the
remote application, waiting for a reply after each message is sent. When a reply
is received from remote application, it wakes up and sends another message.

This application can be built to notify the remote application using inter-core
interrupts (default behavior) or to transmit without notifying the remote
application. If the latter is used, the remote application polls for available
messages.

This application is controlled from console via a sysfs file (see Running the
Application).

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
  https://source.codeaurora.org/external/autobsps32/auto_yocto_bsp/tree/README

* for S32V234 use branch release/bsp23.0 and modify build/sources/meta-alb/recipes-kernel/ipc-shm/ipc-shm.bb::

    - SRCREV = "af9a41d262a57a2c3f4be0f4042adc10b47ffdd6"
    + SRCREV = "a32bb41885c21fd440385c2a382a672d40d2397f"

    + KERNEL_MODULE_PROBECONF += "ipc-shm-uio"
    + module_conf_ipc-shm-uio = "blacklist ipc-shm-uio"
    + FILES_${PN} += "${sysconfdir}/modprobe.d/*"

* for S32R45 use branch release/bsp24.0 and do the following modifications:

  * in build/sources/meta-alb/recipes-kernel/ipc-shm/ipc-shm.bb::

     - SRCREV = "a32bb41885c21fd440385c2a382a672d40d2397f"
     + SRCREV = "90d0aa48d557ae8099ae39553e0ba0154f8b5f28"

     - PROVIDES_append_s32v2xx = " kernel-module-ipc-shm-uio${KERNEL_MODULE_PACKAGE_SUFFIX}"
     - RPROVIDES_${PN}_append_s32v2xx = " kernel-module-ipc-shm-uio${KERNEL_MODULE_PACKAGE_SUFFIX}"
     + PROVIDES_append = " kernel-module-ipc-shm-uio${KERNEL_MODULE_PACKAGE_SUFFIX}"
     + RPROVIDES_${PN}_append = " kernel-module-ipc-shm-uio${KERNEL_MODULE_PACKAGE_SUFFIX}"

     + KERNEL_MODULE_PROBECONF += "ipc-shm-uio"
     + module_conf_ipc-shm-uio = "blacklist ipc-shm-uio"
     + FILES_${PN} += "${sysconfdir}/modprobe.d/*"

  * in build/sources/meta-alb/recipes-fsl/images/fsl-image-s32-common.inc::

     + IMAGE_INSTALL_append_s32r45xevb += " ipc-shm "

* for S32G274A use branch release/bsp28.0 and modify build/sources/meta-alb/recipes-kernel/ipc-shm/ipc-shm.bb::

    - SRCREV = "f75873b946dc6e6b8b3612ad2b0d4eb34ffaca68"
    + SRCREV = "0997660b0101e30c2e6a8779775c31084aa21471"

Note: use image fsl-image-auto with any of the following machines supported for IPCF:
      s32g274aevb, s32r45xevb, s32v234evb.

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

Configuration Notes
===================

Polling
-------
In order to compile the shared memory sample application with polling support,
the makefile parameter ``POLLING`` must be set to ``yes``, e.g.::

    make -C ./ipc-shm/sample POLLING=yes KERNELDIR=$PWD/linux modules

Note: the remote sample application must be built with polling support as well.
Please refer to the remote sample build instructions for more details.

This sample demonstrates how shared memory polling API can be used to poll for
incoming messages instead of using inter-core interrupts notifications.
