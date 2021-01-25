.. SPDX-License-Identifier: BSD-3-Clause

==========================================
IPCF Shared Memory Kernel Driver for Linux
==========================================

:Copyright: 2018-2021 NXP

Overview
========
Linux IPCF Shared Memory kernel driver enables communication over shared memory
with an RTOS running on different cores of the same processor.

The driver is accompanied by a sample application that is also an out-of-tree
kernel module which demonstrates a ping-pong message communication with an RTOS
application (for more details see the readme from sample directory).

The driver and sample application are integrated as out-of-tree kernel modules
in NXP Auto Linux BSP.

The source code of this Linux driver is published on `source.codeaurora.org
<https://source.codeaurora.org/external/autobsps32/ipcf/ipc-shm/>`_.

HW platforms
============
The supported processors are listed in the sample application documentation.

Configuration notes
===================
There are four hardware-related parameters that can be configured at the driver
API level: TX and RX inter-core interrupt IDs, local core ID and remote core ID.

The interrupt IDs are MSCM core-to-core directed interrupt IDs. Valid values are
in range [0..2] for S32xx and [0..3] for S32V234. TX and RX interrupt IDs must
be different.

Note: the interrupt ID expected in the driver configuration is different from
the corresponding processor exception number (used to register the interrupt
handler); see Reference Manual of each platform for specific information.

The local and remote core IDs configuration is divided into core type and core
index. Supported values for core type are defined in ipc_shm_core_type enum.
For ARM platforms, a default value can be assigned to the local and/or remote
core IDs passing IPC_CORE_DEFAULT as the core type. When using this default
value, the core index is not used.

Note: see Reference Manual of each platform for the core types and indexes
supported.

The remote core ID specifies the remote core to be interrupted and the local
core ID specifies the core targeted by the remote core interrupt. The local core
ID configuration is only applicable to S32xx platforms where Linux may be
running SMP and the interrupt could be serviced by a different core than the
targeted (e.g. interrupt load balancing). For other platforms or Operating
Systems, the local core ID configuration is not used.

Cautions
========
This driver provides direct access to physical memory that is mapped as
non-cachable. Therefore, applications should make only aligned accesses in the
shared memory buffers. Caution should be used when working with functions that
may do unaligned accesses (e.g., string processing functions).

For technical support please go to:
    https://www.nxp.com/support
