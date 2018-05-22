/*******************************************************************************
*                                                                              *
*  Shared Memory Inter-Process(or) Communication sample application for Linux  *
*                                                                              *
*   (c) Copyright 2018 NXP. All rights reserved.                               *
*                                                                              *
*******************************************************************************/

This package contains the IPCF share memory Linux kernel module.
This driver is integrated in NXP Auto Linux BSP17 and can be used to 
communicate over shared memory with another driver running on another OS.

Disclaimer: This is a Demo implementation of a shared memory driver. Demo
drivers/applications are not fully qualified and are not intended for
production. The purpose of this demo implementation is to provide basic driver
functionality having minimal quality in terms of testing and documentation.

This sample application demonstrates a ping-pong communication between 2 apps
(one in Linux, one in Autosar OS) using the shared memory driver.
The Linux app initializes the shared memory driver, sends messages to the 
remote app as long as there are available transmit buffers and sleeps when
there is no free transmit buffer left. The Linux app wakes after a message is
received from the remote app. This implies that remote app has released a buffer
and the Linux app can resume acquiring and sending buffer.
The Linux app prints all receiving messages from the remote task.
The Linux app is started and stopped via sysfs (see sections below).

1. Prerequisites:
1.1 NXP Auto Linux BSP17
This package is integrated with NXP Auto Linux BSP17 Yocto build system.
The kernel must be built for s32g275sim machine, details in the NXP Auto Linux
BSP User Manual.
1.2 latest Virtualizer Studio (VSDK) and NXP_S32G_ECU
1.3 Cross compiler, the same as the one used to build the NXP Auto Linux BSP.
See the Release Notes for details.

2. Building the application:
This package is build with Yocto with the whole NXP Auto Linux BSP, but can
also be build manually, if needed (a BSP is still needed to be build at least
once).

2.1 Build with NXP Auto Linux BSP Yocto
2.1.1 follow steps in https://source.codeaurora.org/external/autobsps32/
auto_yocto_bsp/tree/README?h=alb/master. See the NXP Auto Linux BSP for details.
2.1.2 start Yocto build, e.g. bitbake fsl-image-base

2.2 Build manually:
2.2.1 get the package source from
https://source.codeaurora.org/external/autobsps32/ipcf/ipc-shm/
For the rest of the document, it is assumed that the package is installed in
/home/<user>/ipc-shm.
2.2.2 change working directory to the sample location,
e.g. cd /home/<user>/ipc-shm/ipc-shm-sample
2.2.3 export CROSS_COMPILE variable,
e.g. export CROSS_COMPILE=~/toolchain/gcc-linaro-6.3.1-2017.02-x86_64_aarch64-
linux-gnu/bin/aarch64-linux-gnu- 
2.2.4 build the modules providing kernel sources location (i.e. KERNELDIR)
e.g. make KERNELDIR=/home/<user>/kernel clean modules.
If you want to use the modules with an already built Linux image, you must use
the same kernel source version as the already built image, otherwise it is
impossible to load the modules (i.e. insmod ipc-shm.ko will fail).

3. Running the application
3.1 copy ipc-shm.ko and ipc-shm-sample.ko in the rootfs (only if ipc-shm was
manually built)
3.2 Start the S32G simulation in the VSDK with the NXP Auto Linux BSP
(i.e. Image, rootfs, dtb, vmlinux, u-boot). See the S32G Auto Linux BSP User
Manual for details.
3.3 After Linux boot, insert the kernel modules, e.g.:
    insmod /lib/modules/`uname -r`/extra/ipc-shm.ko
    insmod /lib/modules/`uname -r`/extra/ipc-shm-sample.ko
3.4 start the demo
    echo 1 > /sys/kernel/ipc-shm-sample/run
3.5 you should see messages printed in the console
3.6 stop the demo
    echo 0 > /sys/kernel/ipc-shm-sample/run
3.7 unload the modules
    rmmod ipc-shm-sample ipc-shm

This sample application is designed for S32Gxxx NXP Auto Linux BSP17, latest
NXP_S32G_ECU and latest VSDK.
