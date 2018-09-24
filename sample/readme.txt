********************************************************************************
*                                                                              *
*  Shared Memory Inter-Process(or) Communication sample application for Linux  *
*                                                                              *
*  Copyright 2018 NXP                                                          *
*                                                                              *
********************************************************************************

This package contains the IPCF shared memory Linux kernel driver and a sample 
application kernel module.
The driver is integrated in NXP Auto Linux BSP and can be used to
communicate over shared memory with another IPCF driver running on another OS.

The sample application demonstrates a ping-pong communication between 2 apps,
one in Linux, one in RTOS (Autosar/FreeRTOS), using the shared memory driver.
The Linux app initializes the shared memory driver, sends messages to the
remote app as long as there are available transmit buffers and sleeps when
there is no free transmit buffer left. The Linux app wakes after a message is
received from the remote app. This implies that remote app has released a buffer
and the Linux app can resume acquiring and sending buffer.
The Linux app prints all receiving messages from the remote task.
The Linux app is started and stopped via sysfs (see sections below).

1. Prerequisites:
1.1 NXP Auto Linux BSP18
This package is integrated with NXP Auto Linux BSP18 Yocto build system.
The kernel must be built for s32g275sim machine, details in the NXP Auto Linux
BSP User Manual.
1.2 latest Virtualizer Studio (VSDK) and NXP_S32G_ECU
1.3 Cross compiler, the same as the one used to build the NXP Auto Linux BSP.
See the Release Notes for details.
1.4 git, make

2. Building the application:
This package is build with Yocto with the whole NXP Auto Linux BSP, but can
also be build manually, if needed (a BSP is still needed to be build at least
once).

2.1 Build with NXP Auto Linux BSP Yocto
2.1.1 Follow steps in https://source.codeaurora.org/external/autobsps32/
auto_yocto_bsp/tree/README?h=alb/master. See the NXP Auto Linux BSP for details.
2.1.2 Start Yocto build, e.g. bitbake fsl-image-base

2.2 Build manually:
2.2.1 Get the package source:
    cd ~
    git clone https://source.codeaurora.org/external/autobsps32/ipcf/ipc-shm/
    cd ~/ipc-shm/ipc-shm-sample
2.2.2 export CROSS_COMPILE variable, e.g.:
    export CROSS_COMPILE=~/toolchain/gcc-linaro-6.3.1-2017.02-x86_64_aarch64-
linux-gnu/bin/aarch64-linux-gnu-
2.2.3 Build the modules providing kernel sources location (i.e. KERNELDIR) e.g.:
     make KERNELDIR=~/kernel clean modules.
If you want to use the modules with an already built Linux image, you must use
the same kernel source version as the already built image, otherwise it is
impossible to load the modules (i.e. insmod ipc-shm.ko will fail).

3. Running the application
3.1 Copy ipc-shm.ko and ipc-shm-sample.ko in the rootfs (only if ipc-shm was
manually built)
3.2 Start the S32G simulation in the VSDK with the NXP Auto Linux BSP
(i.e. Image, rootfs, dtb, vmlinux, u-boot). See the S32G Auto Linux BSP User
Manual for details.
3.3 Insert the kernel modules after Linux boot:
    insmod /lib/modules/`uname -r`/extra/ipc-shm-dev.ko
    insmod /lib/modules/`uname -r`/extra/ipc-shm-sample.ko
3.4 Clear the kernel log:
    dmesg -c > /dev/null
3.5 Send ping messages to remote OS (output is logged in kernel log):
    echo 10 > /sys/kernel/ipc-shm-sample/ping
3.6 Display sample output from kernel log:
    dmesg
3.7 Optionaly, repeat steps 3.4 - 3.6 with different number of messages
3.8 Unload the modules:
    rmmod ipc-shm-sample ipc-shm-dev

See the IPCF quick start guide for more details.
