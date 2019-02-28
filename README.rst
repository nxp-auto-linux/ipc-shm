.. SPDX-License-Identifier: BSD-3-Clause

===================================
IPCF Shared Memory Driver for Linux
===================================

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
 - S32V234 silicon, cut 2.1, maskset #1N81U
 - S32xx presilicon (S32G275, S32R45x) - Synopsis VDK (same version used by
   Auto Linux BSP)

Configuration notes
===================
There are three hardware-related parameters that can be configured at the driver
API level: TX and RX inter-core interrupt IDs and remote core ID.

The interrupt IDs are MSCM core-to-core directed interrupt IDs. Valid values are
in range [0..2] for S32xx and [0..3] for S32V234.

Note: the interrupt ID expected in the driver configuration is different from
the corresponding processor exception number (used to register the interrupt
handler); see Reference Manual of each platform for specific information.

For any platform, the TX and RX interrupts must be different. For ARM platforms,
a default value can be assigned to the remote core using IPC_CORE_DEFAULT.

For technical support please go to:
    https://www.nxp.com/support
