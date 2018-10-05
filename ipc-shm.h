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
 * Default value for inter core interrupt
 */
#define IPC_DEFAULT_INTER_CORE_IRQ (-1)

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
 * enum ipc_shm_core_type - core type
 */
enum ipc_shm_core_type {
	IPC_CORE_A53,
	IPC_CORE_M7,
	IPC_CORE_M4,
	IPC_CORE_Z7,
	IPC_CORE_Z4,
	IPC_CORE_Z2,
	IPC_CORE_DEFAULT,
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
 * @memory.managed:		managed channel parameters
 * @memory.managed.num_pools:	number of buffer pools
 * @memory.managed.pools:	memory buffer pools parameters
 * @memory.unmanaged:		unmanaged channel parameters
 * @memory.unmanaged.size:	unmanaged memory size
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
 * struct ipc_shm_remote_core - remote core type and index
 * @remote_core_type:	enum of core types
 * @index:		core number
 */
struct ipc_shm_remote_core {
	enum ipc_shm_core_type type;
	int index;
};

/**
 * struct ipc_shm_cfg - IPC shm parameters
 * @local_shm_addr:	local shared memory physical address
 * @remote_shm_addr:	remote shared memory physical address
 * @inter_core_tx_irq:	inter-core interrupt reserved for shm driver tx
 * @inter_core_rx_irq:	inter-core interrupt reserved for shm driver rx
 * @remote_core:	remote core to trigger the interrupt on
 * @shm_size:		local/remote shared memory size
 * @num_channels:	number of shared memory channels
 * @channels:		IPC channels' parameters array
 *
 * In the case of ARM platforms, the same interrupt can be used for both TX and
 * RX. Moreover, default values can be assigned using IPC_DEFAULT_INTER_CORE_IRQ
 * for interrupts and IPC_CORE_DEFAULT for the remote core. In the case of
 * PowerPC platforms, the TX and RX interrupts used must be different and no
 * default values can be assigned to them or the remote core.
 *
 * Local and remote channel and buffer pool configurations must be symmetric.
 */
struct ipc_shm_cfg {
	uintptr_t local_shm_addr;
	uintptr_t remote_shm_addr;
	int shm_size;
	int inter_core_tx_irq;
	int inter_core_rx_irq;
	struct ipc_shm_remote_core remote_core;
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
 */
void ipc_shm_free(void);

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
