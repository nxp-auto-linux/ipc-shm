.. SPDX-License-Identifier: BSD-3-Clause

==========================================================
IPCF Shared Memory User-space Sample Application for Linux
==========================================================

:Copyright: 2023 NXP

Overview
========
This sample application demonstrates a ping-pong message communication with an
RTOS application, using the user-space shared memory driver.

The application initializes the shared memory driver and sends messages to the
remote sample application, waiting for a reply after each message is sent. When
a reply is received from remote application, it wakes up and sends another
message.

This application can be built to notify the remote application using inter-core
interrupts (default behavior) or to transmit without notifying the remote
application. If the latter is used, the remote application polls for available
messages.

Prerequisites
=============
 - EVB board for supported processors: S32G274A, S32R45, S32G399A
 - NXP Automotive Linux BSP

Building the application
========================

Building with Yocto
-------------------
1. Follow the steps for building NXP Auto Linux BSP with Yocto::
   Linux BSP User Manual from Flexera catalog

* user must change the branch release/**IPCF_RELEASE_NAME** and modify in
  build/sources/meta-alb/recipes-kernel/ipc-shm/ipc-shm.bb::

    - BRANCH ?= "${RELEASE_BASE}"
    + BRANCH ?= "release/**IPCF_RELEASE_NAME**"

    - SRCREV = "xxxxxxxxxx"
    + SRCREV = "${AUTOREV}"

  where **IPCF_RELEASE_NAME** is the name of Inter-Platform Communication
  Framework release from Flexera catalog and "xxxxxxxxxx" is the commit ID
  which must be replaced with "${AUTOREV}"

* enable User-space I/O driver in case of using UIO driver, e.g.::

    bitbake virtual/kernel -c menuconfig

  then select::

    device driver --->
    {*} Userspace I/O drivers

* use image fsl-image-auto with any of the following machines supported for IPCF:
  s32g274aevb, s32r45xevb, s32g399aevb.

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

3. Build IPCF-ShM driver modules providing kernel source location, e.g.::

    make -C ./ipc-shm PLATFORM_FLAVOR=[PLATFORM] KERNELDIR=$PWD/linux modules

    User must specific the platform::

        PLATFORM_FLAVOR=s32g2
        PLATFORM_FLAVOR=s32r45
        PLATFORM_FLAVOR=s32g3

4. Build sample application with IPCF-ShM library, providing the location of the
   IPC UIO (if using UIO driver) or IPC CDEV (if using character device driver)
   kernel module in the target board rootfs and the platform name, e.g.::

    If using UIO driver::
      make -C ./ipc-shm/sample_user \
            PLATFORM_FLAVOR=[PLATFORM] \
            IPCF_OS=uio \
            IPC_UIO_MODULE_DIR="/lib/modules/<kernel-release>/extra"

    If using character device driver::
      make -C ./ipc-shm/sample_user \
            PLATFORM_FLAVOR=[PLATFORM] \
            IPCF_OS=cdev \
            IPC_CDEV_MODULE_DIR="/lib/modules/<kernel-release>/extra"

    User must specific the platform and IPCF kernel driver::

        PLATFORM_FLAVOR=s32g2
        PLATFORM_FLAVOR=s32r45
        PLATFORM_FLAVOR=s32g3
        IPCF_OS=cdev
        IPCF_OS=uio
  
    where <kernel-release> can be obtained executing ``uname -r`` in the target board

.. _run-shm-us-linux:

Running the application
=======================
1. Copy ipc-shm-sample_uio.elf or ipc-shm-sample_cdev.elf to the target board rootfs.
   In case of building the sample manually, also copy IPC UIO kernel module (ipc-shm-uio.ko)
   or IPC CDEV kernel module (ipc-shm-cdev.ko) to the directory provided during
   compilation via IPC_UIO_MODULE_DIR/IPC_CDEV_MODULE_DIR.

Notes:
  IPC UIO/IPC CDEV kernel module must be located in the same directory as provided
  via IPC_UIO_MODULE_DIR/IPC_CDEV_MODULE_DIR when building the sample.

2. Boot Linux: for silicon, see section "How to boot" from Auto Linux BSP user
   manual.

3. Run sample and then specify the number of ping messages to be exchanged with
   peer when prompted::

    ./ipc-shm-sample_uio.elf
    or
    ./ipc-shm-sample_cdev.elf

    Input number of messages to send:

Notes:
  To exit the sample, input number of messages 0 or send interrupt signal (e.g.
  Ctrl + C)

4. Result may appear like this::

                Input number of messages to send: 5
                ipc-shm-us-app: ch 0 >> 19 bytes: SENDING MESSAGES: 5
                ipc-shm-us-app: ch 1 >> 32 bytes: #0 HELLO WORLD! FROM USER
                ipc-shm-us-app: ch 1 << 27 bytes: #0 HELLO WORLD! from CORE 4
                ipc-shm-us-app: ch 2 >> 32 bytes: #1 HELLO WORLD! FROM USER
                ipc-shm-us-app: ch 2 << 27 bytes: #1 HELLO WORLD! from CORE 4
                ipc-shm-us-app: ch 1 >> 32 bytes: #2 HELLO WORLD! FROM USER
                ipc-shm-us-app: ch 1 << 27 bytes: #2 HELLO WORLD! from CORE 4
                ipc-shm-us-app: ch 2 >> 32 bytes: #3 HELLO WORLD! FROM USER
                ipc-shm-us-app: ch 2 << 27 bytes: #3 HELLO WORLD! from CORE 4
                ipc-shm-us-app: ch 1 >> 32 bytes: #4 HELLO WORLD! FROM USER
                ipc-shm-us-app: ch 1 << 27 bytes: #4 HELLO WORLD! from CORE 4
                ipc-shm-us-app: ch 0 << 19 bytes: REPLIED MESSAGES: 5
