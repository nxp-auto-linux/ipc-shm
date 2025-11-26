.. SPDX-License-Identifier: BSD-3-Clause

===============================================
IPCF Shared Memory Sample Application for Linux
===============================================

:Copyright: 2025 NXP

Overview
========
This sample application is a kernel module that demonstrates communication with
an RTOS application, using the SCMI channel with shared memory driver.

The application initializes the shared memory driver and sends a request to the
remote application for a specified number of messages. It then waits for an
equal number of replies. Upon receiving each reply, the application wakes up
and processes the message payload according to the SCMI protocol.

This application can be built to notify the remote application using inter-core
interrupts (default behavior) or to transmit without notifying the remote
application. If the latter is used, the remote application polls for available
messages.

This application is controlled from console via a sysfs file (see Running the
Application).

Prerequisites
=============
 - EVB board for supported processors (see platform **Release Notes**)
 - NXP Automotive Linux BSP

Building the application
========================
This module is built with Yocto from NXP Auto Linux BSP, but can also be built
manually, if needed.

Note:
 - Modules are also included in NXP Auto Linux BSP pre-built images that can be
   downloaded from NXP Auto Linux BSP Flexera catalog.
 - When building examples for S32N79, in case of using mailbox interrupt, ensure
   that the mailbox configuration is enabled in the Linux kernel build. Note
   that the NXP Auto Linux BSP pre-built images may not include these mailbox
   configuration options by default. Therefore, the following steps must be
   performed::

    bitbake virtual/kernel -c menuconfig

    then select::

        Device Drivers --->
            [*] Mailbox Hardware Support --->
                <*> NXP S32N79 Message Receive Unit (MRU)

Building with Yocto
-------------------
1. Follow the steps for building NXP Auto Linux BSP with Yocto::

   Linux BSP User Manual from Flexera catalog

2. Use the branch release/**IPCF_RELEASE_NAME** and after creating the build
   directory in the SDK root (with "source nxp-setup-alb.sh -m <machine>"),
   modify in <builddirectory>/conf/local.conf::

    + BRANCH:pn-ipc-shm="release/**IPCF_RELEASE_NAME**"
    + SRCREV:pn-ipc-shm="${AUTOREV}"

  where **IPCF_RELEASE_NAME** is the name of Inter-Platform Communication
  Framework release from Flexera catalog and replace the git hash with 
  "${AUTOREV}".

Note: - use image **fsl-image-auto** with any of machine supported or
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
    make -C ./linux s32cc_defconfig
    make -C ./linux

3. Build IPCF driver and sample modules providing kernel source location, e.g.::

    make -C ./ipc-shm/sample PLATFORM_FLAVOR=[PLATFORM] KERNELDIR=$PWD/linux modules

    User must specify the platform::

        PLATFORM_FLAVOR=s32n7

.. _run-shm-linux:

Running the application
=======================
1. If sample was built manually, copy ipc-shm-dev.ko and ipc-shm-sample_scmi.ko
   in rootfs

2. Boot Linux: see section "How to boot" from Auto Linux BSP user manual.

3. Insert IPCF kernel modules after Linux boot::

    insmod /lib/modules/`uname -r`/extra/ipc-shm-dev.ko
    insmod /lib/modules/`uname -r`/extra/ipc-shm-sample_scmi.ko

4. Clear the kernel log::

    dmesg -c > /dev/null

5. Send 10 ping messages to remote OS and display output from kernel log::

    echo 10 > /sys/kernel/ipc-shm-sample_scmi/ping
    dmesg -c

7. Result should be similar to this::

    root@s32n79sim:/mnt/host# echo 10 > /sys/kernel/ipc-shm-sample_scmi/ping 
    [   11.959028] ipc-shm-sample_scmi: starting demo...
    [   11.959105] ipc-shm-sample_scmi: MSG PAYLOAD: CALL #10
    [   11.965017] ipc-shm-sample_scmi: << MSG PAYLOAD: DONE #0
    [   11.970978] ipc-shm-sample_scmi: << MSG PAYLOAD: DONE #1
    [   11.976882] ipc-shm-sample_scmi: << MSG PAYLOAD: DONE #2
    [   11.982781] ipc-shm-sample_scmi: << MSG PAYLOAD: DONE #3
    [   11.988679] ipc-shm-sample_scmi: << MSG PAYLOAD: DONE #4
    [   11.994577] ipc-shm-sample_scmi: << MSG PAYLOAD: DONE #5
    [   12.000475] ipc-shm-sample_scmi: << MSG PAYLOAD: DONE #6
    [   12.006374] ipc-shm-sample_scmi: << MSG PAYLOAD: DONE #7
    [   12.012272] ipc-shm-sample_scmi: << MSG PAYLOAD: DONE #8
    [   12.018170] ipc-shm-sample_scmi: << MSG PAYLOAD: DONE #9
    [   12.018305] ipc-shm-sample_scmi: exit demo

8. Repeat previous step with different number of messages

9. Unload the modules::

    rmmod ipc-shm-sample_scmi ipc-shm-dev
