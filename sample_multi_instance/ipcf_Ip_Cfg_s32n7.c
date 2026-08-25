/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2025-2026 NXP
 */

#include "ipcf_Ip_Cfg.h"

static struct ipc_shm_buffer_state ipcf_shm_pool0_state_0_0[5];
static struct ipc_shm_buffer_state ipcf_shm_pool0_state_0_1[5];
static struct ipc_shm_buffer_state ipcf_shm_pool0_state_0_2[5];
/* Pools must be sorted in ascending order by buffer size */
static struct ipc_shm_pool_cfg ipcf_shm_cfg_buf_pools0_1[3] = {
	{
		.num_bufs = 5,
		.buf_size = 32,
		.buffer_state = ipcf_shm_pool0_state_0_0,
	},
	{
		.num_bufs = 5,
		.buf_size = 64,
		.buffer_state = ipcf_shm_pool0_state_0_1,
	},
	{
		.num_bufs = 5,
		.buf_size = 128,
		.buffer_state = ipcf_shm_pool0_state_0_2,
	},
};

static struct ipc_shm_buffer_state ipcf_shm_pool0_state_1_0[5];
static struct ipc_shm_buffer_state ipcf_shm_pool0_state_1_1[5];
static struct ipc_shm_buffer_state ipcf_shm_pool0_state_1_2[5];
/* Pools must be sorted in ascending order by buffer size */
static struct ipc_shm_pool_cfg ipcf_shm_cfg_buf_pools0_2[3] = {
	{
		.num_bufs = 5,
		.buf_size = 32,
		.buffer_state = ipcf_shm_pool0_state_1_0,
	},
	{
		.num_bufs = 5,
		.buf_size = 64,
		.buffer_state = ipcf_shm_pool0_state_1_1,
	},
	{
		.num_bufs = 5,
		.buf_size = 128,
		.buffer_state = ipcf_shm_pool0_state_1_2,
	},
};

static struct ipc_shm_channel_cfg ipcf_shm_cfg_channels0[3] = {
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
				.pools = ipcf_shm_cfg_buf_pools0_1,
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
				.pools = ipcf_shm_cfg_buf_pools0_2,
				.rx_cb = data_chan_rx_cb,
				.cb_arg = &rx_cb_arg,
			},
		},
	},
};

static struct ipc_shm_buffer_state ipcf_shm_pool1_state_0_0[5];
static struct ipc_shm_buffer_state ipcf_shm_pool1_state_0_1[5];
static struct ipc_shm_buffer_state ipcf_shm_pool1_state_0_2[5];
/* Pools must be sorted in ascending order by buffer size */
static struct ipc_shm_pool_cfg ipcf_shm_cfg_buf_pools1_1[3] = {
	{
		.num_bufs = 5,
		.buf_size = 32,
		.buffer_state = ipcf_shm_pool1_state_0_0,
	},
	{
		.num_bufs = 5,
		.buf_size = 64,
		.buffer_state = ipcf_shm_pool1_state_0_1,
	},
	{
		.num_bufs = 5,
		.buf_size = 128,
		.buffer_state = ipcf_shm_pool1_state_0_2,
	},
};
static struct ipc_shm_buffer_state ipcf_shm_pool1_state_1_0[5];
static struct ipc_shm_buffer_state ipcf_shm_pool1_state_1_1[5];
static struct ipc_shm_buffer_state ipcf_shm_pool1_state_1_2[5];
/* Pools must be sorted in ascending order by buffer size */
static struct ipc_shm_pool_cfg ipcf_shm_cfg_buf_pools1_2[3] = {
	{
		.num_bufs = 5,
		.buf_size = 32,
		.buffer_state = ipcf_shm_pool1_state_1_0,
	},
	{
		.num_bufs = 5,
		.buf_size = 64,
		.buffer_state = ipcf_shm_pool1_state_1_1,
	},
	{
		.num_bufs = 5,
		.buf_size = 128,
		.buffer_state = ipcf_shm_pool1_state_1_2,
	},
};

static struct ipc_shm_channel_cfg ipcf_shm_cfg_channels1[3] = {
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
				.pools = ipcf_shm_cfg_buf_pools1_1,
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
				.pools = ipcf_shm_cfg_buf_pools1_2,
				.rx_cb = data_chan_rx_cb,
				.cb_arg = &rx_cb_arg,
			},
		},
	},
};

static struct ipc_shm_buffer_state ipcf_shm_pool2_state_0_0[5];
static struct ipc_shm_buffer_state ipcf_shm_pool2_state_0_1[5];
static struct ipc_shm_buffer_state ipcf_shm_pool2_state_0_2[5];
/* Pools must be sorted in ascending order by buffer size */
static struct ipc_shm_pool_cfg ipcf_shm_cfg_buf_pools2_1[3] = {
	{
		.num_bufs = 5,
		.buf_size = 32,
		.buffer_state = ipcf_shm_pool2_state_0_0,
	},
	{
		.num_bufs = 5,
		.buf_size = 64,
		.buffer_state = ipcf_shm_pool2_state_0_1,
	},
	{
		.num_bufs = 5,
		.buf_size = 128,
		.buffer_state = ipcf_shm_pool2_state_0_2,
	},
};
static struct ipc_shm_buffer_state ipcf_shm_pool2_state_1_0[5];
static struct ipc_shm_buffer_state ipcf_shm_pool2_state_1_1[5];
static struct ipc_shm_buffer_state ipcf_shm_pool2_state_1_2[5];
/* Pools must be sorted in ascending order by buffer size */
static struct ipc_shm_pool_cfg ipcf_shm_cfg_buf_pools2_2[3] = {
	{
		.num_bufs = 5,
		.buf_size = 32,
		.buffer_state = ipcf_shm_pool2_state_1_0,
	},
	{
		.num_bufs = 5,
		.buf_size = 64,
		.buffer_state = ipcf_shm_pool2_state_1_1,
	},
	{
		.num_bufs = 5,
		.buf_size = 128,
		.buffer_state = ipcf_shm_pool2_state_1_2,
	},
};
static struct ipc_shm_channel_cfg ipcf_shm_cfg_channels2[3] = {
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
				.pools = ipcf_shm_cfg_buf_pools2_1,
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
				.pools = ipcf_shm_cfg_buf_pools2_2,
				.rx_cb = data_chan_rx_cb,
				.cb_arg = &rx_cb_arg,
			},
		},
	},
};

/* ipc shm configuration */
struct ipc_shm_cfg ipcf_shm_cfg_instances[] = {
	{
		.local_shm_addr  = 0x24800000,
		.remote_shm_addr = 0x24801000,
		.inter_core_tx_irq = 0,
		.inter_core_rx_irq = 0,
		.shm_size  = 0x1000,
		.num_channels = 3,
		.channels = ipcf_shm_cfg_channels0,
	},
	{
		.local_shm_addr  = 0x24802000,
		.remote_shm_addr = 0x24803000,
		.inter_core_tx_irq = IPC_IRQ_NONE,
		.inter_core_rx_irq = 1,
		.shm_size  = 0x1000,
		.num_channels = 3,
		.channels = ipcf_shm_cfg_channels1,
	},
	{
		.local_shm_addr  = 0x24804000,
		.remote_shm_addr = 0x24805000,
		.inter_core_tx_irq = IPC_IRQ_NONE,
		.inter_core_rx_irq = IPC_IRQ_NONE,
		.shm_size  = 0x1000,
		.num_channels = 3,
		.channels = ipcf_shm_cfg_channels2,
	},
};

struct ipc_shm_instances_cfg ipcf_shm_instances_cfg = {
	.num_instances = 3u,
	.shm_cfg = ipcf_shm_cfg_instances,
};
