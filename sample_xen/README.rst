.. SPDX-License-Identifier: BSD-3-Clause

=========================================================
IPCF Shared Memory Sample Application with XEN hypervisor
=========================================================

:Copyright: 2023 NXP

Overview
========
This sample application is a kernel module that demonstrates a polling message
communication between Dom0 and Dom0less, using the shared memory driver.

The application initializes the shared memory driver and sends messages to the
remote application. When a reply is received from remote application, it wakes up
to process the messages and replies after received all expected messages.

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
1. Follow the steps in **Linux BSP User Manual** from Flexera catalog for
building NXP Auto Linux BSP with Yocto and enable Xen Hypervisor with
custom XEN setup is **xen-examples-dom0less-passthrough-gmac**::

2. Open conf/local.conf file and add::

    BRANCH:pn-ipc-shm="release/**IPCF_RELEASE_NAME**"
    SRCREV:pn-ipc-shm="${AUTOREV}"
    DEMO_IPCF_APPS:pn-ipc-shm="sample sample_multi_instance sample_xen/dom0 sample_xen/dom0less"

  where **IPCF_RELEASE_NAME** is the name of Inter-Platform Communication
  Framework release from Flexera catalog and replace the line with SRCREV
  with SRCREV = "${AUTOREV}".

  Modify sources/meta-alb/recipes-extended/xen/xen/xen_s32cc.cfg and add::
    CONFIG_STATIC_SHM=y
  Modify sources/meta-alb/recipes-fsl/images/fsl-image-dom0less.bb and add::
    IMAGE_INSTALL += " devmem2 dtc bridge-utils ipc-shm"
  
3. Build fsl-image-auto::

    time bitbake fsl-image-auto 2>&1


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

    make -C ./ipc-shm/sample_xen/dom0 PLATFORM_FLAVOR=[PLATFORM] KERNELDIR=$PWD/linux modules
    make -C ./ipc-shm/sample_xen/dom0less PLATFORM_FLAVOR=[PLATFORM] KERNELDIR=$PWD/linux modules

    User must specific the platform::

        PLATFORM_FLAVOR=s32g2
        PLATFORM_FLAVOR=s32r45
        PLATFORM_FLAVOR=s32g3

.. _run-shm-linux:

Running the application
=======================
1. If sample was built manually, copy ipc-shm-xen.ko and ipc-shm-sample-dom0.ko
   and ipc-shm-sample-dom0less.ko in rootfs

2. Boot Linux: see section "How to boot" from Auto Linux BSP user manual.

3. Stop in uboot::
  
  a. Load the boot script::

    ext4load mmc ${mmcdev}:${mmcpart_ext} ${script_addr} boot/${script}; source ${script_addr}

  b. Set a shared memory node in device tree::

    fdt resize 1024;
    fdt mknode /chosen shmem@84500000;
    fdt set /chosen/shmem@84500000 compatible "xen,domain-shared-memory-v1";
    fdt set /chosen/shmem@84500000 role "owner";
    fdt set /chosen/shmem@84500000 xen,shm-id "shmem0";
    fdt set /chosen/shmem@84500000 xen,shared-mem <0x0 0x84500000 0x0 0x84500000 0x0 0x20000>;
    fdt mknode /chosen/domU0 shmem@84500000;
    fdt set /chosen/domU0/shmem@84500000 compatible "xen,domain-shared-memory-v1";
    fdt set /chosen/domU0/shmem@84500000 role "borrower";
    fdt set /chosen/domU0/shmem@84500000 xen,shm-id "shmem0";
    fdt set /chosen/domU0/shmem@84500000 xen,shared-mem <0x0 0x84500000 0x0 0x84500000 0x0 0x20000>;

  c. Boot::

    booti ${xen_addr} - ${fdt_addr}

3. Login with root and insert IPCF kernel modules::

  In Dom0::
    insmod /lib/modules/`uname -r`/extra/ipc-shm-xen.ko
    insmod /lib/modules/`uname -r`/extra/ipc-shm-sample-dom0.ko
  In Dom0less::
    insmod /lib/modules/`uname -r`/extra/ipc-shm-xen.ko
    insmod /lib/modules/`uname -r`/extra/ipc-shm-sample-dom0less.ko

4. Clear the kernel log::

    dmesg -c > /dev/null

5. Send 10 ping messages to remote domain and display output from kernel log::

  To send message from Dom0less to Dom0::
    In Dom0less::
      cat /sys/kernel/ipc-shm-sample/ping
    Then switch to Dom0::
      echo 10 > /sys/kernel/ipc-shm-sample/ping

6. Repeat previous step with different number of messages

7. Unload the modules::

  In Dom0::
    rmmod ipc-shm-sample-dom0 ipc-shm-xen
  In Dom0less::
    rmmod ipc-shm-sample-dom0less ipc-shm-xen

8. Result::

  In Dom0less kernel log, there could be message like below::

    (XEN) [  319.082159] ipc-shm-sample: run_demo_poll(): starting demo...
    (XEN) [  319.082193] ipc-shm-sample: send_ctrl_msg(): ch 0 >> 20 bytes: SENDING MESSAGES: 10
    (XEN) [  319.082210] ipc-shm-sample: send_data_poll(): ch 1 >> 32 bytes: #0 HELLO WORLD! from DOM0
    (XEN) [  319.082235] ipc-shm-sample: send_data_poll(): ch 2 >> 32 bytes: #1 HELLO WORLD! from DOM0
    (XEN) [  319.082257] ipc-shm-sample: send_data_poll(): ch 1 >> 32 bytes: #2 HELLO WORLD! from DOM0
    (XEN) [  319.082280] ipc-shm-sample: send_data_poll(): ch 2 >> 32 bytes: #3 HELLO WORLD! from DOM0
    (XEN) [  319.082301] ipc-shm-sample: send_data_poll(): ch 1 >> 32 bytes: #4 HELLO WORLD! from DOM0
    (XEN) [  319.082323] ipc-shm-sample: send_data_poll(): ch 2 >> 32 bytes: #5 HELLO WORLD! from DOM0
    (XEN) [  319.082344] ipc-shm-sample: send_data_poll(): ch 1 >> 32 bytes: #6 HELLO WORLD! from DOM0
    (XEN) [  319.082366] ipc-shm-sample: send_data_poll(): ch 2 >> 32 bytes: #7 HELLO WORLD! from DOM0
    (XEN) [  319.082387] ipc-shm-sample: send_data_poll(): ch 1 >> 32 bytes: #8 HELLO WORLD! from DOM0
    (XEN) [  319.082410] ipc-shm-sample: send_data_poll(): ch 2 >> 32 bytes: #9 HELLO WORLD! from DOM0
    (XEN) [  319.094574] ipc-shm-sample: ctrl_chan_rx_cb(): ch 0 << 19 bytes: SENDING MESSAGES: 0
    (XEN) [  319.094604] ipc-shm-sample: data_chan_rx_cb(): ch 1 << 32 bytes: #0 HELLO WORLD! from DOM1
    (XEN) [  319.094626] ipc-shm-sample: data_chan_rx_cb(): ch 1 << 32 bytes: #2 HELLO WORLD! from DOM1
    (XEN) [  319.094645] ipc-shm-sample: data_chan_rx_cb(): ch 1 << 32 bytes: #4 HELLO WORLD! from DOM1
    (XEN) [  319.094664] ipc-shm-sample: data_chan_rx_cb(): ch 1 << 32 bytes: #6 HELLO WORLD! from DOM1
    (XEN) [  319.094683] ipc-shm-sample: data_chan_rx_cb(): ch 1 << 32 bytes: #8 HELLO WORLD! from DOM1
    (XEN) [  319.094702] ipc-shm-sample: data_chan_rx_cb(): ch 2 << 32 bytes: #1 HELLO WORLD! from DOM1
    (XEN) [  319.094721] ipc-shm-sample: data_chan_rx_cb(): ch 2 << 32 bytes: #3 HELLO WORLD! from DOM1
    (XEN) [  319.094740] ipc-shm-sample: data_chan_rx_cb(): ch 2 << 32 bytes: #5 HELLO WORLD! from DOM1
    (XEN) [  319.094758] ipc-shm-sample: data_chan_rx_cb(): ch 2 << 32 bytes: #7 HELLO WORLD! from DOM1
    (XEN) [  319.094777] ipc-shm-sample: data_chan_rx_cb(): ch 2 << 32 bytes: #9 HELLO WORLD! from DOM1
    (XEN) [  319.094797] ipc-shm-sample: run_demo_poll(): exit demo
