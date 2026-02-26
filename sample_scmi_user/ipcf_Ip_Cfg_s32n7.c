/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2025-2026 NXP
 */

#include "ipcf_Ip_Cfg.h"

static struct ipc_shm_channel_cfg ipcf_shm_cfg_channels[1] = {
	{
		.type = IPC_SHM_SCMI,
		.ch = {
			.scmi = {
				.scmi_addr = 0x381E0000,
				.scmi_size = 192,
				.rx_cb = scmi_chan_rx_cb,
				.cb_arg = &rx_cb_arg,
			},
		},
	},
};

/* ipc shm configuration */
struct ipc_shm_cfg ipcf_shm_cfg_instances[] = {
	{
		.local_shm_addr  = 0x381C0000,
		.remote_shm_addr = 0x381D0000,
		.inter_core_tx_irq = IPC_IRQ_MRU,
		.inter_core_rx_irq = IPC_IRQ_MRU,
		.mbox_cfg_id = 0,
		.shm_size  = 0x1000,
		.num_channels = 1,
		.channels = ipcf_shm_cfg_channels,
	},
};

struct ipc_shm_instances_cfg ipcf_shm_instances_cfg = {
	.num_instances = 1u,
	.shm_cfg = ipcf_shm_cfg_instances,
};
