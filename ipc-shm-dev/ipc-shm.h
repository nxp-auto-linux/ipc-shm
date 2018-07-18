/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018 NXP
 */
#ifndef IPC_SHM_H
#define IPC_SHM_H

/**
 * Maximum number of shared memory channels that can be configured
 */
#define IPC_SHM_MAX_CHANNELS 8

/**
 * Maximum number of buffer pools that can be configured for a managed channel
 */
#define IPC_SHM_MAX_POOLS 4

/**
 * Maximum number of buffers per pool
 */
#define IPC_SHM_MAX_BUFS_PER_POOL 4096

/**
 * enum ipc_shm_channel_type - channel type
 * @SHM_CHANNEL_MANAGED:   buffer management enabled
 * @SHM_CHANNEL_UNMANAGED: buf mgmt disabled, app owns entire channel memory
 */
enum ipc_shm_channel_type {
	SHM_CHANNEL_MANAGED,
	SHM_CHANNEL_UNMANAGED
};

/**
 * struct ipc_shm_pool_cfg - memory buffer pool parameters
 * @num_bufs:	number of buffers
 * @buf_size:	buffer size
 */
struct ipc_shm_pool_cfg {
	uint16_t num_bufs;
	uint32_t buf_size;
};

/**
 * struct ipc_shm_channel_ops -  channel rx API callbacks
 * @cb_arg:              optional argument for callbacks
 * @shm_rx_cb:           receive callback for managed channels
 * @shm_rx_unmanaged_cb: receive callback for unmanaged channels
 */
struct ipc_shm_channel_ops {
	void *cb_arg;
	void (*rx_cb)(void *cb_arg, int chan_id, void *buf, size_t size);
	void (*rx_unmanaged_cb)(void *cb_arg, int chan_id, uint8_t *buf);
};

/**
 * struct ipc_shm_channel_cfg - channel parameters
 * @type:			channel type (see ipc_shm_channel_type)
 * @memory:			channel managed/unmanaged memory parameters
 * @memory.managed		managed channel parameters
 * @memory.managed.pools	memory buffer pools parameters
 * @memory.unmanaged		unmanaged channel parameters
 * @memory.unmanaged.size	unmanaged memory size
 * @ops:			channel rx API callbacks
 */
struct ipc_shm_channel_cfg {
	enum ipc_shm_channel_type type;

	/* managed/unmanaged channel memory params */
	union {
		struct {
			int num_pools;
			struct ipc_shm_pool_cfg *pools;
		} managed;
		struct {
			uint32_t size;
		} unmanaged;
	} memory;

	/* rx API callbacks */
	struct ipc_shm_channel_ops *ops;
};

/**
 * struct ipc_shm_cfg - IPC shm parameters
 * @local_shm_addr:	local shared memory physical address
 * @remote_shm_addr:	remote shared memory physical address
 * @shm_size:		local/remote shared memory size
 * @channels:		IPC channels' parameters array
 */
struct ipc_shm_cfg {
	uintptr_t local_shm_addr;
	uintptr_t remote_shm_addr;
	int shm_size;
	int num_channels;
	struct ipc_shm_channel_cfg *channels;
};

/**
 * ipc_shm_init() - initialize shared memory device
 * @cfg:         configuration parameters
 *
 * Return: 0 on success, error code otherwise
 */
int ipc_shm_init(const struct ipc_shm_cfg *cfg);

/**
 * ipc_shm_free() - release shared memory device
 *
 * Return: 0 on success, error code otherwise
 */
int ipc_shm_free(void);

/**
 * ipc_shm_acquire_buf() - request a buffer for the given channel
 * @chan_id:        channel ID
 * @size:           required size
 *
 * Return: pointer to the buffer base address or NULL if buffer not found
 */
void *ipc_shm_acquire_buf(int chan_id, size_t size);

/**
 * ipc_shm_release_buf() - release a buffer for the given channel
 * @chan_id:        channel index
 * @buf:            buffer pointer
 *
 * Return: 0 on success, error code otherwise
 */
int ipc_shm_release_buf(int chan_id, const void *buf);

/**
 * ipc_shm_tx() - send data on given channel and notify remote
 * @chan_id:        channel index
 * @buf:            buffer pointer
 * @size:           size of data written in buffer
 *
 * Function used only for managed channels where buffer management is enabled.
 *
 * Return: 0 on success, error code otherwise
 */
int ipc_shm_tx(int chan_id, void *buf, size_t size);

/**
 * ipc_shm_tx_unmanaged() - notify remote that data has been written in channel
 * @chan_id:        channel ID
 *
 * Function used only for unmanaged channels where application has full control
 * over channel memory and no buffer management is done by ipc-shm device.
 *
 * Return: 0 on success, error code otherwise
 */
int ipc_shm_tx_unmanaged(int chan_id);

#endif /* IPC_SHM_H */
