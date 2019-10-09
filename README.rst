.. SPDX-License-Identifier: BSD-3-Clause

==========================================
IPCF Shared Memory Kernel Driver for Linux
==========================================

:Copyright: 2018-2019 NXP

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
The driver includes support of the following processors:
 - S32V234 EVB board with silicon cut 2.1, maskset #1N81U
 - Synopsis VDK for S32xx presilicon (same version as for NXP Auto Linux BSP)

The supported processors are listed in the sample application documentation.

Configuration notes
===================
There are three hardware-related parameters that can be configured at the driver
API level: TX and RX inter-core interrupt IDs and remote core ID.

The interrupt IDs are MSCM core-to-core directed interrupt IDs. Valid values are
in range [0..2] for S32xx and [0..3] for S32V234.

Note: the interrupt ID expected in the driver configuration is different from
the corresponding processor exception number (used to register the interrupt
handler); see Reference Manual of each platform for specific information.

For any platform, the TX and RX interrupts must be different.
For ARM platforms, a default value can be assigned to the remote core using
IPC_CORE_DEFAULT.

Cautions
========
This driver provides direct access to physical memory that is mapped as
non-cachable. Therefore, applications should make only aligned accesses in the
shared memory buffers. Caution should be used when working with functions that
may do unaligned accesses (e.g., string processing functions).

For technical support please go to:
    https://www.nxp.com/support
