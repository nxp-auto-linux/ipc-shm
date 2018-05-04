/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2018 NXP Semiconductors
 */
#ifndef IPC_SHM_DEV_IPC_SHM_H
#define IPC_SHM_DEV_IPC_SHM_H

/**
 * Number of buffer pools belonging to a managed channel
 */
#define BUF_POOL_COUNT 3

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
 * struct ipc_shm_buf_pool - channel buffer pool
 * @num_buffers:    number of buffers
 * @buf_size:       buffer size
 */
struct ipc_shm_buf_pool {
	int num_buffers;
	int buf_size;
};

/**
 * struct ipc_shm_channel_params - channel initialization parameters
 * @chan_id:             channel ID
 * @type:                channel type
 * @memory:              memory assigned to channel
 * @cb_arg:              optional argument for callback
 * @shm_rx_cb:           receive callback for managed channels
 * @shm_rx_unmanaged_cb: receive callback for unmanaged channels
 */
struct ipc_shm_channel_params {
	int chan_id;

	/* managed/unmanaged channel memory params */
	enum ipc_shm_channel_type type;
	union {
		struct {
			struct ipc_shm_buf_pool pool[BUF_POOL_COUNT];
		} managed;
		struct {
			int size;
		} unmanaged;
	} memory;

	/* rx API callback */
	void *cb_arg;
	void (*shm_rx_cb)(void *cb_arg, int chan_id, uint8_t *buf, size_t size);
	void (*shm_rx_unmanaged_cb)(void *cb_arg, int chan_id, uint8_t *buf);
};

/**
 * struct ipc_shm_params - initialization parameters
 * @shm_base_addr:  shared memory base address
 * @shm_size:       shared memory size
 */
struct ipc_shm_params {
	void *shm_base_addr;
	int shm_size;
};

/**
 * ipc_shm_init - initialize shared memory device
 * @params:         initialization parameters
 *
 * Return: 0 for success, error code otherwise
 */
int ipc_shm_init(struct ipc_shm_params *params);

/**
 * ipc_shm_free - release shared memory device
 *
 * Return: 0 for success, error code otherwise
 */
int ipc_shm_free(void);

/**
 * ipc_shm_acquire_buf - request a buffer for the given channel
 * @chan_id:        channel ID
 * @size:           required size
 *
 * Return: pointer to the buffer base address
 */
uint8_t *ipc_shm_acquire_buf(int chan_id, size_t size);

/**
 * ipc_shm_release_buf - release a buffer for the given channel
 * @chan_id:        channel ID
 * @buf:            buffer pointer
 *
 * Return: 0 for success, error code otherwise
 */
int ipc_shm_release_buf(int chan_id, uint8_t *buf);

/**
 * nxp_shm_tx - send data on given channel and notify remote
 * @chan_id:        channel ID
 * @buf:            buffer pointer
 * @size:           size of data written in buffer
 *
 * Function used only for managed channels where buffer management is enabled.
 *
 * Return: 0 for success, error code otherwise
 */
int ipc_shm_tx(int chan_id, uint8_t *buf, size_t size);

/**
 * ipc_shm_tx_unmanaged - notify remote that data is available in shared memory
 * @chan_id:        channel ID
 *
 * Function used only for unmanaged channels where application has full control
 * over channel memory and no buffer management is done by ipc-shm device.
 *
 * Return: 0 for success, error code otherwise
 */
int ipc_shm_tx_unmanaged(int chan_id);

#endif /* IPC_SHM_DEV_IPC_SHM_H */
