.. SPDX-License-Identifier: BSD-3-Clause

===================================
IPCF Shared Memory Driver for Linux
===================================

:Copyright: 2018-2021,2023 NXP

Overview
========
The Linux IPCF Shared Memory Driver enables communication over shared memory
with an RTOS application running on a different core of the same processor.

The driver is accompanied by a sample application which demonstrates a ping-pong
message communication with a RTOS application (for more details see the readme
from the sample directory).

The driver is integrated as an out-of-tree kernel module in NXP Auto
Linux BSP.

The source code of this Linux driver is published on `github.com
<https://github.com/nxp-auto-linux/ipc-shm>`_.

HW platforms
============
The supported processors are listed in the sample application documentation.

Configuration notes
===================
There are five hardware-related parameters that can be configured at the driver
API level: TX and RX inter-core interrupt IDs, local core ID, remote core ID and
trusted cores.

The interrupt IDs are MSCM core-to-core directed interrupt IDs. Valid values are
in range [0..2] for S32xx, [0..11] for S32G3xx and [0..3] for S32V234. The TX and RX
interrupt IDs must be different.

Note: the interrupt ID expected in the driver configuration is different from
the corresponding processor exception number (used to register the interrupt
handler). See the Reference Manual of each platform for specific information.

The TX interrupt can be disabled by setting its ID to IPC_IRQ_NONE. 
When disabled the remote application must check for incoming messages by invoking
the function ipc_shm_poll_channels().

The local and remote core IDs configuration is divided into core type and core
index. Supported values for core type and index are defined in ipc_shm_core_type
and ipc_shm_core_index enum.

For ARM platforms, a default value can be assigned to the local and/or remote
core IDs by choosing IPC_CORE_DEFAULT as the core type. When using this default
value the core index is automatically chosen by the driver.

Note: see the Reference Manual of each platform for the core types and indexes
supported.

The remote core ID specifies the remote core to be interrupted and the local
core ID specifies the core targeted by the remote core interrupt. The local core
ID configuration is only applicable to platforms where Linux may be running SMP
and the interrupt could be serviced by a different core than the targeted (e.g.
interrupt load balancing).

The trusted cores mask specifies which cores (of the same core type as local
core) have access to the inter-core interrupt status register of the local core
targeted by remote. The mask can be formed from valid values defined in
ipc_shm_core_index enum.

Note: the local core ID and trusted cores configuration is only applicable to
S32xx platforms running Linux. For other platforms or Operating Systems, the
local core ID configuration is not used.

If using Linux IPCF Shared Memory User-space Driver, the user-space static library
(libipc-shm) will automatically insert the IPCF UIO/CDEV kernel module at initialization.
The path to the kernel module in the target board rootfs can be overwritten
at compile time by setting IPC_UIO_MODULE_DIR (for using uio driver) or
IPC_CDEV_MODULE_DIR (for using character driver) variable from the caller.

Cautions
========
This driver provides direct access to physical memory that is mapped as
non-cachable in both kernel-space or user-space. Therefore, applications should
make only aligned accesses in the shared memory buffers.
Caution should be used when working with functions that may do unaligned accesses
(e.g., string processing functions).

The driver ensures freedom from interference between local and remote memory domains
by executing all write operations only in the local memory.

The driver is thread safe as long as only one thread is pushing and only one
thread is popping: Single-Producer - Single-Consumer.

This thread safety is lock-free and needs one additional sentinel element in
the ring buffers between the write and read indexes that is never written.

The driver is thread safe for different instances but not for same instance.

For technical support please go to:
    https://www.nxp.com/support
