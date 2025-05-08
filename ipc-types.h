/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2024-2025 NXP
 */
#ifndef IPC_TYPES_H
#define IPC_TYPES_H

/* Maximum value of standard type */
#define IPC_UINT16_MAX                  (0xFFFFu)

/* Maximum number of total buffers per channel */
#define IPC_SHM_MAX_BUFS_PER_CHANNEL    (IPC_UINT16_MAX - 1u)

/* Maximum unmanaged channel size */
#define IPC_SHM_MAX_UMNG_SIZE           (IPC_UINT16_MAX)

/* magic number to indicate the queue is initializing */
#define IPC_QUEUE_INIT_IN_PROGRESS           0x54494E4946435049ULL

/* magic number to indicate the queue integrity */
#define IPC_QUEUE_INIT_DONE                  0x474E495246435049ULL

#if defined(PLATFORM_FLAVOR_s32n7)
/*
 * Used when using MRU driver
 */
#define IPC_IRQ_MRU -2
#endif

/*
 * Used when polling is desired on either transmit or receive path
 */
#define IPC_IRQ_NONE -1

/*
 * Maximum number of shared memory channels that can be configured
 */
#ifndef IPC_SHM_MAX_CHANNELS
#define IPC_SHM_MAX_CHANNELS 8U
#endif

/*
 * Maximum number of buffer pools that can be configured for a managed channel
 */
#ifndef IPC_SHM_MAX_POOLS
#define IPC_SHM_MAX_POOLS 4U
#endif

/*
 * Maximum number of buffers per pool
 */
#ifndef IPC_SHM_MAX_BUFS_PER_POOL
#define IPC_SHM_MAX_BUFS_PER_POOL 4096U
#endif

/*
 * Maximum number of instances
 */
#ifndef IPC_SHM_MAX_INSTANCES
#define IPC_SHM_MAX_INSTANCES	4u
#endif

#ifdef PLATFORM_FLAVOR_s32n7

/**
 * enum IPC_MRU_INSTANCE_ENUM_Type - MRU instance type
 * @IPC_MRU_APS_INSTANCE:   Present MRU in APS
 * @IPC_MRU_FSS_INSTANCE:   Present MRU in FSS
 * @IPC_MRU_RTU_INSTANCE:   Present MRU in RTU
 * @IPC_MRU_RCU_C_INSTANCE: Present MRU in RCU
 * @IPC_MRU_RCU_H_INSTANCE: Present MRU_H in RCU
 * @IPC_MRU_RCU_L_INSTANCE: Present MRU_L in RCU
 */
typedef enum {
	IPC_MRU_APS_INSTANCE,
	IPC_MRU_FSS_INSTANCE,
	IPC_MRU_RTU_INSTANCE,
	IPC_MRU_RCU_C_INSTANCE,
	IPC_MRU_RCU_H_INSTANCE,
	IPC_MRU_RCU_L_INSTANCE,
} IPC_MRU_INSTANCE_ENUM_Type;

/**
 * enum IPC_MRU_INDEX_ENUM_Type - MRU instance index
 * @IPC_MRU_0:   MRU index 0
 * @IPC_MRU_1:   MRU index 1
 * @IPC_MRU_2:   MRU index 2
 * @IPC_MRU_3:   MRU index 3
 * @IPC_MRU_4:   MRU index 4
 * @IPC_MRU_5:   MRU index 5
 * @IPC_MRU_6:   MRU index 6
 * @IPC_MRU_7:   MRU index 7
 * @IPC_MRU_8:   MRU index 8
 * @IPC_MRU_9:   MRU index 9
 * @IPC_MRU_10:  MRU index 10
 * @IPC_MRU_11:  MRU index 11
 * @IPC_MRU_12:  MRU index 12
 * @IPC_MRU_13:  MRU index 13
 * @IPC_MRU_14:  MRU index 14
 * @IPC_MRU_15:  MRU index 15
 */
typedef enum {
	IPC_MRU_0,
	IPC_MRU_1,
	IPC_MRU_2,
	IPC_MRU_3,
	IPC_MRU_4,
	IPC_MRU_5,
	IPC_MRU_6,
	IPC_MRU_7,
	IPC_MRU_8,
	IPC_MRU_9,
	IPC_MRU_10,
	IPC_MRU_11,
	IPC_MRU_12,
	IPC_MRU_13,
	IPC_MRU_14,
	IPC_MRU_15,
} IPC_MRU_INDEX_ENUM_Type;

/**
 * enum IPC_SUBSYSTEM_INDEX_ENUM_Type - Subsystem index
 * @IPC_SUBSYSTEM_0:   Subsystem index 0
 * @IPC_SUBSYSTEM_0:   Subsystem index 1
 * @IPC_SUBSYSTEM_0:   Subsystem index 2
 * @IPC_SUBSYSTEM_0:   Subsystem index 3
 * @IPC_SUBSYSTEM_0:   Subsystem index 4
 * @IPC_SUBSYSTEM_0:   Subsystem index 5
 * @IPC_SUBSYSTEM_0:   Subsystem index 6
 * @IPC_SUBSYSTEM_0:   Subsystem index 7
 *
 * For determine which MRU address is using in ipc-shm driver configuration
 */
typedef enum {
	IPC_SUBSYSTEM_0,
	IPC_SUBSYSTEM_1,
	IPC_SUBSYSTEM_2,
	IPC_SUBSYSTEM_3,
	IPC_SUBSYSTEM_4,
	IPC_SUBSYSTEM_5,
	IPC_SUBSYSTEM_6,
	IPC_SUBSYSTEM_7,
} IPC_SUBSYSTEM_INDEX_ENUM_Type;

/**
 * enum IPC_MRU_CHANNEL_ENUM_Type - MRU channel index
 * @IPC_MRU_CHANNEL_0:   MRU channel index 0
 * @IPC_MRU_CHANNEL_1:   MRU channel index 1
 * @IPC_MRU_CHANNEL_2:   MRU channel index 2
 * @IPC_MRU_CHANNEL_3:   MRU channel index 3
 * @IPC_MRU_CHANNEL_4:   MRU channel index 4
 * @IPC_MRU_CHANNEL_5:   MRU channel index 5
 * @IPC_MRU_CHANNEL_6:   MRU channel index 6
 * @IPC_MRU_CHANNEL_7:   MRU channel index 7
 * @IPC_MRU_CHANNEL_8:   MRU channel index 8
 * @IPC_MRU_CHANNEL_9:   MRU channel index 9
 * @IPC_MRU_CHANNEL_10:  MRU channel index 10
 * @IPC_MRU_CHANNEL_11:  MRU channel index 11
 * @IPC_MRU_CHANNEL_12:  MRU channel index 12
 * @IPC_MRU_CHANNEL_13:  MRU channel index 13
 * @IPC_MRU_CHANNEL_14:  MRU channel index 14
 * @IPC_MRU_CHANNEL_15:  MRU channel index 15
 */
typedef enum {
	IPC_MRU_CHANNEL_0,
	IPC_MRU_CHANNEL_1,
	IPC_MRU_CHANNEL_2,
	IPC_MRU_CHANNEL_3,
	IPC_MRU_CHANNEL_4,
	IPC_MRU_CHANNEL_5,
	IPC_MRU_CHANNEL_6,
	IPC_MRU_CHANNEL_7,
	IPC_MRU_CHANNEL_8,
	IPC_MRU_CHANNEL_9,
	IPC_MRU_CHANNEL_10,
	IPC_MRU_CHANNEL_11,
	IPC_MRU_CHANNEL_12,
	IPC_MRU_CHANNEL_13,
	IPC_MRU_CHANNEL_14,
	IPC_MRU_CHANNEL_15,
} IPC_MRU_CHANNEL_ENUM_Type;

#endif

/**
 * enum ipc_shm_channel_type - channel type
 * @IPC_SHM_MANAGED:     channel with buffer management enabled
 * @IPC_SHM_UNMANAGED:   buf mgmt disabled, app owns entire channel memory
 *
 * For unmanaged channels the application has full control over channel memory
 * and no buffer management is done by ipc-shm device.
 */
enum ipc_shm_channel_type {
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

#if !defined(PLATFORM_FLAVOR_s32n7)
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
#endif

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
 * @size:     unmanaged channel memory size
 * @rx_cb:    receive callback
 * @cb_arg:   optional receive callback argument
 */
struct ipc_shm_unmanaged_cfg {
	uint32_t size;
	void (*rx_cb)(void *cb_arg, const uint8_t instance, uint8_t chan_id,
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

#if !defined(PLATFORM_FLAVOR_s32n7)
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
#else


/**
 * struct IPC_MRU_Type - MRU configuration parameters
 * @mru_instance:   MRU instance type where MRU located
 * @subsystem_id:   Subsystem index where MRU located
 * @mru_id:         MRU number used in configuration
 * @mru_channel_id: MRU channel used in configuration
 *
 */

typedef struct {
	IPC_MRU_INSTANCE_ENUM_Type    mru_instance;
	IPC_SUBSYSTEM_INDEX_ENUM_Type subsystem_id;
	IPC_MRU_INDEX_ENUM_Type       mru_id;
	IPC_MRU_CHANNEL_ENUM_Type     mru_channel_id;
} IPC_MRU_Type;
#endif

/**
 * struct ipc_shm_cfg - IPC shm parameters
 * @local_shm_addr:      local shared memory physical address
 * @remote_shm_addr:     remote shared memory physical address
 * @shm_size:            local/remote shared memory size
 * @inter_core_tx_irq:   inter-core interrupt reserved for shm driver Tx
 * @inter_core_rx_irq:   inter-core interrupt reserved for shm driver Rx
 * @mru_tx:              MRU configuration reserved for shm driver Tx
 * @mru_rx:              MRU configuration reserved for shm driver Rx
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
 * be set to IPC_IRQ_MRU depends on use case. The configuration for mru_tx and
 * mru_rx must be configured corresponding to inter_core_tx_irq and
 * inter_core_rx_irq
 *
 * Local and remote channel and buffer pool configurations must be symmetric.
 */
struct ipc_shm_cfg {
	uintptr_t local_shm_addr;
	uintptr_t remote_shm_addr;
	uint32_t shm_size;
	int16_t inter_core_tx_irq;
	int16_t inter_core_rx_irq;
#ifdef PLATFORM_FLAVOR_s32n7
	IPC_MRU_Type mru_tx;
	IPC_MRU_Type mru_rx;
#else
	struct ipc_shm_local_core local_core;
	struct ipc_shm_remote_core remote_core;
#endif
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
