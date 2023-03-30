/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018-2023 NXP
 */
#ifndef IPC_SHM_H
#define IPC_SHM_H

/*
 * Maximum number of shared memory channels that can be configured
 */
#ifndef IPC_SHM_MAX_CHANNELS
#define IPC_SHM_MAX_CHANNELS 8
#endif

/*
 * Maximum number of buffer pools that can be configured for a managed channel
 */
#ifndef IPC_SHM_MAX_POOLS
#define IPC_SHM_MAX_POOLS 4
#endif

/*
 * Maximum number of buffers per pool
 */
#ifndef IPC_SHM_MAX_BUFS_PER_POOL
#define IPC_SHM_MAX_BUFS_PER_POOL 4096u
#endif

/*
 * Used when using MU driver
 */
#define IPC_IRQ_MU -3

/*
 * Used when using MRU driver
 */
#define IPC_IRQ_MRU -2

/*
 * Used when polling is desired on either transmit or receive path
 */
#define IPC_IRQ_NONE -1

/*
 * Maximum number of instances
 */
#ifndef IPC_SHM_MAX_INSTANCES
#define IPC_SHM_MAX_INSTANCES	4u
#endif

/**
 * enum ipc_shm_channel_type - channel type
 * @IPC_SHM_MANAGED:	channel with buffer management enabled
 * @IPC_SHM_UNMANAGED:	buf mgmt disabled, app owns entire channel memory
 *
 * For unmanaged channels the application has full control over channel memory
 * and no buffer management is done by ipc-shm device.
 */
enum ipc_shm_channel_type {
	IPC_SHM_MANAGED,
	IPC_SHM_UNMANAGED
};

/**
 * enum ipc_shm_core_type - core type
 * @IPC_CORE_A53:       ARM Cortex-A53 core
 * @IPC_CORE_M7:        ARM Cortex-M7 core
 * @IPC_CORE_M4:        ARM Cortex-M4 core
 * @IPC_CORE_Z7:        PowerPC e200z7 core
 * @IPC_CORE_Z4:        PowerPC e200z4 core
 * @IPC_CORE_Z2:        PowerPC e200z2 core
 * @IPC_CORE_R52:       ARM Cortex-R52 core
 * @IPC_CORE_M33:       ARM Cortex-M33 core
 * @IPC_CORE_BBE32:     Tensilica ConnX BBE32EP core
 * @IPC_CORE_DEFAULT:   used for letting driver auto-select remote core type
 */
enum ipc_shm_core_type {
	IPC_CORE_DEFAULT,
	IPC_CORE_A53,
	IPC_CORE_M7,
	IPC_CORE_M4,
	IPC_CORE_Z7,
	IPC_CORE_Z4,
	IPC_CORE_Z2,
	IPC_CORE_R52,
	IPC_CORE_M33,
	IPC_CORE_BBE32,
};

/**
 * enum ipc_shm_core_index - core index
 * @IPC_CORE_INDEX_0:	Processor index 0
 * @IPC_CORE_INDEX_1:	Processor index 1
 * @IPC_CORE_INDEX_2:	Processor index 2
 * @IPC_CORE_INDEX_3:	Processor index 3
 * @IPC_CORE_INDEX_4:	Processor index 4
 * @IPC_CORE_INDEX_5:	Processor index 5
 * @IPC_CORE_INDEX_6:	Processor index 6
 * @IPC_CORE_INDEX_7:	Processor index 7
 */
enum ipc_shm_core_index {
	IPC_CORE_INDEX_0 = 0x01u,
	IPC_CORE_INDEX_1 = 0x02u,
	IPC_CORE_INDEX_2 = 0x04u,
	IPC_CORE_INDEX_3 = 0x08u,
	IPC_CORE_INDEX_4 = 0x10u,
	IPC_CORE_INDEX_5 = 0x20u,
	IPC_CORE_INDEX_6 = 0x40u,
	IPC_CORE_INDEX_7 = 0x80u,
};

/**
 * struct ipc_shm_pool_cfg - memory buffer pool parameters
 * @num_bufs:   number of buffers
 * @buf_size:   buffer size
 */
struct ipc_shm_pool_cfg {
	uint16_t num_bufs;
	uint32_t buf_size;
};

/**
 * struct ipc_shm_managed_cfg - managed channel parameters
 * @num_pools:   number of buffer pools
 * @pools:       memory buffer pools parameters
 * @rx_cb:       receive callback
 * @cb_arg:      optional receive callback argument
 */
struct ipc_shm_managed_cfg {
	int num_pools;
	struct ipc_shm_pool_cfg *pools;
	void (*rx_cb)(void *cb_arg, const uint8_t instance, int chan_id,
			void *buf, size_t size);
	void *cb_arg;
};

/**
 * struct ipc_shm_unmanaged_cfg - unmanaged channel parameters
 * @size:     unmanaged channel memory size
 * @rx_cb:    receive callback
 * @cb_arg:   optional receive callback argument
 */
struct ipc_shm_unmanaged_cfg {
	uint32_t size;
	void (*rx_cb)(void *cb_arg, const uint8_t instance, int chan_id,
			void *mem);
	void *cb_arg;
};

/**
 * struct ipc_shm_channel_cfg - channel parameters
 * @type:	channel type from &enum ipc_shm_channel_type
 * @ch.managed:     managed channel parameters
 * @ch.unmanaged:   unmanaged channel parameters
 */
struct ipc_shm_channel_cfg {
	enum ipc_shm_channel_type type;
	union {
		struct ipc_shm_managed_cfg managed;
		struct ipc_shm_unmanaged_cfg unmanaged;
	} ch;
};

/**
 * struct ipc_shm_remote_core - remote core type and index
 * @type:    core type from &enum ipc_shm_core_type
 * @index:   core number
 *
 * Core type can be IPC_CORE_DEFAULT, in which case core index doesn't matter
 * because it's chosen automatically by the driver.
 */
struct ipc_shm_remote_core {
	enum ipc_shm_core_type type;
	enum ipc_shm_core_index index;
};

/**
 * struct ipc_shm_local_core - local core type, index and trusted cores
 * @type:      core type from &enum ipc_shm_core_type
 * @index:     core number targeted by remote core interrupt
 * @trusted:   trusted cores mask
 *
 * Core type can be IPC_CORE_DEFAULT, in which case core index doesn't matter
 * because it's chosen automatically by the driver.
 *
 * Trusted cores mask specifies which cores (of the same core type) have access
 * to the inter-core interrupt status register of the targeted core. The mask
 * can be formed from &enum ipc_shm_core_index.
 */
struct ipc_shm_local_core {
	enum ipc_shm_core_type type;
	enum ipc_shm_core_index index;
	uint32_t trusted;
};

/**
 * struct ipc_shm_cfg - IPC shm parameters
 * @local_shm_addr:	local shared memory physical address
 * @remote_shm_addr:	remote shared memory physical address
 * @shm_size:		local/remote shared memory size
 * @inter_core_tx_irq:	inter-core interrupt reserved for shm driver Tx
 * @inter_core_rx_irq:	inter-core interrupt reserved for shm driver Rx
 * @local_core:		local core targeted by remote core interrupt
 * @remote_core:	remote core to trigger the interrupt on
 * @num_channels:	number of shared memory channels
 * @channels:		IPC channels' parameters array
 *
 * The TX and RX interrupts used must be different. For ARM platforms, a default
 * value can be assigned to the local and remote core using IPC_CORE_DEFAULT.
 * Local core is only used for platforms on which Linux may be running on
 * multiple cores, and is ignored for RTOS and baremetal implementations.
 *
 * Local and remote channel and buffer pool configurations must be symmetric.
 */
struct ipc_shm_cfg {
	uintptr_t local_shm_addr;
	uintptr_t remote_shm_addr;
	uint32_t shm_size;
	int inter_core_tx_irq;
	int inter_core_rx_irq;
	struct ipc_shm_local_core local_core;
	struct ipc_shm_remote_core remote_core;
	int num_channels;
	struct ipc_shm_channel_cfg *channels;
};

/**
 * struct ipc_shm_instances_cfg - IPC shm parameters
 * @num_instances:   number of shared memory instances
 * @shm_cfg:         IPC shm parameters array
 *
 */
struct ipc_shm_instances_cfg {
	uint8_t num_instances;
	struct ipc_shm_cfg *shm_cfg;
};

/**
 * ipc_shm_init() - initialize shared memory device
 * @cfg:              configuration parameters
 *
 * Function is non-reentrant.
 *
 * Return: 0 on success, error code otherwise
 */
int ipc_shm_init(const struct ipc_shm_instances_cfg *cfg);

/**
 * ipc_shm_free() - release all instances of shared memory device
 *
 * Function is non-reentrant.
 */
void ipc_shm_free(void);

/**
 * ipc_shm_acquire_buf() - request a buffer for the given channel
 * @instance:       instance id
 * @chan_id:        channel index
 * @size:           required size
 *
 * Function used only for managed channels where buffer management is enabled.
 * Function is thread-safe for different channels but not for the same channel.
 *
 * Return: pointer to the buffer base address or NULL if buffer not found
 */
void *ipc_shm_acquire_buf(const uint8_t instance, int chan_id, size_t size);

/**
 * ipc_shm_release_buf() - release a buffer for the given channel
 * @instance:       instance id
 * @chan_id:        channel index
 * @buf:            buffer pointer
 *
 * Function used only for managed channels where buffer management is enabled.
 * Function is thread-safe for different channels but not for the same channel.
 *
 * Return: 0 on success, error code otherwise
 */
int ipc_shm_release_buf(const uint8_t instance, int chan_id, const void *buf);

/**
 * ipc_shm_tx() - send data on given channel and notify remote
 * @instance:       instance id
 * @chan_id:        channel index
 * @buf:            buffer pointer
 * @size:           size of data written in buffer
 *
 * Function used only for managed channels where buffer management is enabled.
 * Function is thread-safe for different channels but not for the same channel.
 *
 * Return: 0 on success, error code otherwise
 */
int ipc_shm_tx(const uint8_t instance, int chan_id, void *buf, size_t size);

/**
 * ipc_shm_unmanaged_acquire() - acquire the unmanaged channel local memory
 * @instance:       instance id
 * @chan_id:        channel index
 *
 * Function used only for unmanaged channels. The memory must be acquired only
 * once after the channel is initialized. There is no release function needed.
 * Function is thread-safe for different channels but not for the same channel.
 *
 * Return: pointer to the channel memory or NULL if invalid channel
 */
void *ipc_shm_unmanaged_acquire(const uint8_t instance, int chan_id);

/**
 * ipc_shm_unmanaged_tx() - notify remote that data has been written in channel
 * @instance:       instance id
 * @chan_id:        channel index
 *
 * Function used only for unmanaged channels. It can be used after the channel
 * memory has been acquired whenever is needed to signal remote that new data
 * is available in channel memory.
 * Function is thread-safe for different channels but not for the same channel.
 *
 * Return: 0 on success, error code otherwise
 */
int ipc_shm_unmanaged_tx(const uint8_t instance, int chan_id);

/**
 * ipc_shm_is_remote_ready() - check whether remote is initialized
 * @instance:        instance id
 *
 * Function used to check if the remote is initialized and ready to receive
 * messages. It should be invoked at least before the first transmit operation.
 * Function is thread-safe.
 *
 * Return: 0 if remote is initialized, error code otherwise
 */
int ipc_shm_is_remote_ready(const uint8_t instance);

/**
 * ipc_shm_poll_channels() - poll the channels for available messages to process
 * @instance:        instance id
 *
 * This function handles all channels using a fair handling algorithm: all
 * channels are treated equally and no channel is starving.
 * Function is thread-safe for different instances but not for same instance.
 *
 * Return: number of messages processed, error code otherwise
 */
int ipc_shm_poll_channels(const uint8_t instance);

#endif /* IPC_SHM_H */
