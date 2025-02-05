.. SPDX-License-Identifier: BSD-3-Clause

===================================
IPCF Shared Memory Driver for Linux
===================================

:Copyright: 2018-2021,2023,2025 NXP

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
In platforms that support MSCM core-to-core directed interrupt, there are five
hardware-related parameters that can be configured at the driver API level:
TX and RX inter-core interrupt IDs, local core ID, remote core ID and trusted
cores.

The interrupt IDs are MSCM core-to-core directed interrupt IDs. Valid values are
in range [0..2] for S32xx, [0..11] for S32G3xx and [0..3] for S32V234. The TX
and RX interrupt IDs must be different.

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

In platforms that support MRU interrupt, there are four hardware-related
parameters that can be configured at the driver API level: TX and RX inter-core
interrupt types, MRU configuration for transmitting, and MRU configuration for
receiving.

The TX/RX interrupt types valid values are IPC_IRQ_NONE when disabling interrupt,
or IPC_IRQ_MRU when using MRU interrupt.

When using MRU interrupt, MRU configuration need to be configured. The MRU
configuration contains 4 parameters: MRU types, subsytem ID, MRU ID, and MRU
channel ID.

MRU types valid values for S32N79 are: IPC_MRU_APS_INSTANCE, IPC_MRU_FSS_INSTANCE,
IPC_MRU_RTU_INSTANCE, IPC_MRU_RCU_C_INSTANCE, IPC_MRU_RCU_H_INSTANCE,
and IPC_MRU_RCU_L_INSTANCE, which indicate the Subsystem types the MRU located.

Subsystem ID values in range [IPC_SUBSYSTEM_0..IPC_SUBSYSTEM_7], which
indicate the subsystem index the MRU located. The valid values depend on the
subsystem types.

MRU ID is the MRU index used used for transmitting/receiving. Values in range
[IPC_MRU_0..IPC_MRU_15], valid values depend on MRU types and subsystem ID

MRU channel ID is the MRU channel index used for transmitting/receiving. Values
in range [IPC_MRU_CHANNEL_0..IPC_MRU_CHANNEL_15], valid values depend on MRU
types, subsystem ID, and MRU ID

Note: Please see the Reference Manual of S32N79 for Subsystem IDs, MRU IDs, and
MRU channel IDs supported

If using Linux IPCF Shared Memory User-space Driver, the user-space static library
(libipc-shm) will automatically insert the IPCF UIO kernel module at initialization.
The path to the kernel module in the target board rootfs can be overwritten
at compile time by setting IPC_UIO_MODULE_DIR variable from the caller.

If using Linux IPCF Shared Memory XEN Driver, the IPC_USE_XEN=YES should be added
at compile time.

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
