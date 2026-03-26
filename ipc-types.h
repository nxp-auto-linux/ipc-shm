/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2024-2026 NXP
 */
#ifndef IPC_TYPES_H
#define IPC_TYPES_H

/* Maximum value of standard type */
#define IPC_UINT16_MAX                      (0xFFFFu)

/* Maximum number of total buffers per channel */
#define IPC_SHM_MAX_BUFS_PER_CHANNEL        (IPC_UINT16_MAX - 1u)

/* Maximum unmanaged channel size */
#define IPC_SHM_MAX_UMNG_SIZE               (IPC_UINT16_MAX)

/* magic number to indicate the queue is initializing */
#define IPC_QUEUE_INIT_IN_PROGRESS          (0x54494E4946435049ULL)

/* magic number to indicate the queue integrity */
#define IPC_QUEUE_INIT_DONE                 (0x474E495246435049ULL)

#if defined(PLATFORM_FLAVOR_s32n7)

/*
 * Used when using MRU driver
 */
#define IPC_IRQ_MRU                         (-2)
#endif

/*
 * Used when polling is desired on either transmit or receive path
 */
#define IPC_IRQ_NONE                        (-1)

/*
 * Maximum number of shared memory channels that can be configured
 */
#ifndef IPC_SHM_MAX_CHANNELS
#define IPC_SHM_MAX_CHANNELS                (8U)
#endif

/*
 * Maximum number of buffer pools that can be configured for a managed channel
 */
#ifndef IPC_SHM_MAX_POOLS
#define IPC_SHM_MAX_POOLS                   (4U)
#endif

/*
 * Maximum number of buffers per pool
 */
#ifndef IPC_SHM_MAX_BUFS_PER_POOL
#define IPC_SHM_MAX_BUFS_PER_POOL           (4096U)
#endif

/*
 * Maximum number of instances
 */
#ifndef IPC_SHM_MAX_INSTANCES
#define IPC_SHM_MAX_INSTANCES               (4u)
#endif

/*
 * The index of status byte in SCMI channel
 */
#define IPC_SCMI_CHANNEL_STATUS_IDX         (1u)

/**
 * enum ipc_shm_channel_type - channel type
 * @IPC_SHM_MANAGED:     channel with buffer management enabled
 * @IPC_SHM_UNMANAGED:   buf mgmt disabled, app owns entire channel memory
 *
 * For unmanaged channels the application has full control over channel memory
 * and no buffer management is done by ipc-shm device.
 */
enum ipc_shm_channel_type {
	IPC_SHM_SCMI,
	IPC_SHM_MANAGED,
	IPC_SHM_UNMANAGED
};

/**
 * enum ipc_shm_queue_type -  queue type
 * @IPC_SHM_CHANNEL_QUEUE:    Channel queue
 * @IPC_SHM_POOL_QUEUE:       Pool buffer queue
 *
 */
enum ipc_shm_queue_type {
	IPC_SHM_CHANNEL_QUEUE = 0,
	IPC_SHM_POOL_QUEUE = 1,
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
	IPC_CORE_KQ8,
};

/**
 * enum ipc_shm_core_index - core index
 * @IPC_CORE_INDEX_0:   Processor index 0
 * @IPC_CORE_INDEX_1:   Processor index 1
 * @IPC_CORE_INDEX_2:   Processor index 2
 * @IPC_CORE_INDEX_3:   Processor index 3
 * @IPC_CORE_INDEX_4:   Processor index 4
 * @IPC_CORE_INDEX_5:   Processor index 5
 * @IPC_CORE_INDEX_6:   Processor index 6
 * @IPC_CORE_INDEX_7:   Processor index 7
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
	uint8_t num_pools;
	struct ipc_shm_pool_cfg *pools;
	void (*rx_cb)(void *cb_arg, const uint8_t instance, uint8_t chan_id,
			void *buf, uint32_t size);
	void *cb_arg;
};

/**
 * struct ipc_shm_unmanaged_cfg - unmanaged channel parameters
 * @addr:     detached buffer physical address (used only if detached is true)
 * @size:     unmanaged channel memory size
 * @detached: if true, channel uses external buffer at @addr instead of shm
 * @rx_cb:    receive callback
 * @cb_arg:   optional receive callback argument
 */
struct ipc_shm_unmanaged_cfg {
	volatile uintptr_t addr;
	uint32_t size;
	bool detached;
	void (*rx_cb)(void *cb_arg, const uint8_t instance, uint8_t chan_id,
			void *mem);
	void *cb_arg;
};

/**
 * struct ipc_shm_scmi_cfg - scmi channel parameters
 * @scmi_addr:      scmi memory physical address
 * @scmi_size:      channel memory size
 * @rx_cb:          receive callback
 * @cb_arg:         optional receive callback argument
 */
struct ipc_shm_scmi_cfg {
	volatile uintptr_t scmi_addr;
	uint32_t scmi_size;
	void (*rx_cb)(void *cb_arg, const uint8_t instance, uint8_t chan_id,
			void *buf);
	void *cb_arg;
};

/**
 * enum ipc_scmi_channel_status_type - IPC SCMI channel status
 * @IPC_SCMI_CHANNEL_BUSY:    SCMI channel is busy (0)
 * @IPC_SCMI_CHANNEL_FREE:    SCMI channel is free (1)
 */
enum ipc_scmi_channel_status_type {
	IPC_SCMI_CHANNEL_BUSY = 0,
	IPC_SCMI_CHANNEL_FREE
};

/**
 * struct ipc_shm_channel_cfg - channel parameters
 * @type:	channel type from &enum ipc_shm_channel_type
 * @ch.managed:     managed channel parameters
 * @ch.unmanaged:   unmanaged channel parameters
 * @ch.scmi:        scmi channel parameters
 */
struct ipc_shm_channel_cfg {
	enum ipc_shm_channel_type type;
	union {
		struct ipc_shm_managed_cfg managed;
		struct ipc_shm_unmanaged_cfg unmanaged;
		struct ipc_shm_scmi_cfg scmi;
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
 * @local_shm_addr:      local shared memory physical address
 * @remote_shm_addr:     remote shared memory physical address
 * @shm_size:            local/remote shared memory size
 * @inter_core_tx_irq:   inter-core interrupt reserved for shm driver Tx
 * @inter_core_rx_irq:   inter-core interrupt reserved for shm driver Rx
 * @mbox_cfg_id:         MRU mailbox configuration reserved for shm driver
 * @local_core:          local core targeted by remote core interrupt
 * @remote_core:         remote core to trigger the interrupt on
 * @num_channels:        number of shared memory channels
 * @channels:            IPC channels parameters array
 *
 * When using MSCM core-to-core interrupt, the TX and RX interrupts used must be
 * different. For ARM platforms, a default value can be assigned to the local
 * and remote core using IPC_CORE_DEFAULT. Local and remote core is only used
 * for platforms that support MSCM core-to-core interrupts
 * When using MRU interrupt, the inter_core_tx_irq or inter_core_rx_irq must
 * be set to IPC_IRQ_NONE or IPC_IRQ_MRU depends on use case.
 *
 * Local and remote channel and buffer pool configurations must be symmetric.
 */
struct ipc_shm_cfg {
	uintptr_t local_shm_addr;
	uintptr_t remote_shm_addr;
	uint32_t shm_size;
	int16_t inter_core_tx_irq;
	int16_t inter_core_rx_irq;
	struct ipc_shm_local_core local_core;
	struct ipc_shm_remote_core remote_core;
	uint8_t mbox_cfg_id;
	uint8_t num_channels;
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

#endif /* IPC_TYPES_H */
