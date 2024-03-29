/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2023 NXP
 */

#include "ipcf_Ip_Cfg.h"

/* Pools must be sorted in ascending order by buffer size */
static struct ipc_shm_pool_cfg ipcf_shm_cfg_buf_pools0[3] = {
	{
		.num_bufs = 5,
		.buf_size = 32,
	},
	{
		.num_bufs = 5,
		.buf_size = 256,
	},
	{
		.num_bufs = 5,
		.buf_size = 4096,
	},
};
/* Pools must be sorted in ascending order by buffer size */
static struct ipc_shm_pool_cfg ipcf_shm_cfg_buf_pools1[3] = {
	{
		.num_bufs = 5,
		.buf_size = 32,
	},
	{
		.num_bufs = 5,
		.buf_size = 256,
	},
	{
		.num_bufs = 5,
		.buf_size = 4096,
	},
};

static struct ipc_shm_channel_cfg ipcf_shm_cfg_channels[3] = {
	{
		.type = IPC_SHM_UNMANAGED,
		.ch = {
			.unmanaged = {
				.size = 64,
				.rx_cb = ctrl_chan_rx_cb,
				.cb_arg = &rx_cb_arg,
			},
		},
	},
	{
		.type = IPC_SHM_MANAGED,
		.ch = {
			.managed = {
				.num_pools = 3,
				.pools = ipcf_shm_cfg_buf_pools0,
				.rx_cb = data_chan_rx_cb,
				.cb_arg = &rx_cb_arg,
			},
		},
	},
	{
		.type = IPC_SHM_MANAGED,
		.ch = {
			.managed = {
				.num_pools = 3,
				.pools = ipcf_shm_cfg_buf_pools1,
				.rx_cb = data_chan_rx_cb,
				.cb_arg = &rx_cb_arg,
			},
		},
	},
};

/* ipc shm configuration */
struct ipc_shm_cfg ipcf_shm_cfg_instances[] = {
	{
		.local_shm_addr  = 0x34100000,
		.remote_shm_addr = 0x34200000,
		.inter_core_tx_irq = 2,
		.inter_core_rx_irq = 1,
		.local_core = {
			.type = IPC_CORE_DEFAULT,
			.index = IPC_CORE_INDEX_0,  /* automatically assigned */
			.trusted = IPC_CORE_INDEX_0 | IPC_CORE_INDEX_1
				| IPC_CORE_INDEX_2 | IPC_CORE_INDEX_3
				| IPC_CORE_INDEX_4 | IPC_CORE_INDEX_5
				| IPC_CORE_INDEX_6 | IPC_CORE_INDEX_7
		},
		.remote_core = {
			.type = IPC_CORE_M7,
			.index = IPC_CORE_INDEX_0,
		},
		.shm_size  = 0x100000,
		.num_channels = 3,
		.channels = ipcf_shm_cfg_channels,
	},
};

struct ipc_shm_instances_cfg ipcf_shm_instances_cfg = {
	.num_instances = 1u,
	.shm_cfg = ipcf_shm_cfg_instances,
};
