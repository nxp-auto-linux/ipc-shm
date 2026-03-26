/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018-2026 NXP
 */
#include "ipc-os.h"
#include "ipc-hw.h"
#include "ipc-shm.h"
#include "ipc-queue.h"

/* magic number to indicate the driver is initialized */
#define IPC_SHM_STATE_READY 0x3252455646435049ULL
#define IPC_SHM_STATE_CLEAR 0u

/* Indicates that the unmanaged channel initialization is in progress */
#define IPC_SHM_UCHAN_INIT_IN_PROGRESS   0x54494E49UL
/* Indicates that the unmanaged channel initialization is done */
#define IPC_UCHAN_INIT_DONE              0x55435049UL

/* flag telling if buffer is from remote OS */
#define IPC_BUFFER_FROM_LOCAL  0u
#define IPC_BUFFER_FROM_REMOTE 1u

/**
 * enum ipc_shm_instance_state - used for IPC instance status
 * @IPC_SHM_INSTANCE_USED:  instance is used
 * @IPC_SHM_INSTANCE_FREE:  instance is free and can be used
 * @IPC_SHM_INSTANCE_ERROR: there are some errors
 */
enum ipc_shm_instance_state {
	IPC_SHM_INSTANCE_USED = 0u,
	IPC_SHM_INSTANCE_FREE = 1u,
	IPC_SHM_INSTANCE_ERROR = 2u,
};

/**
 * struct ipc_shm_pool_addr - struct stores temporary addresses of local/remote
 *                            memory
 * @local_pool_shm:  address of local buffer pool
 * @remote_pool_shm: address of remote buffer pool
 */
struct ipc_shm_pool_addr {
	uintptr_t local_pool_shm;
	uintptr_t remote_pool_shm;
};

/**
 * struct ipc_shm_bd - buffer descriptor (store buffer location and data size)
 * @pool_id:   index of buffer pool
 * @buf_id:    index of buffer from buffer pool
 * @data_size: size of data written in buffer
 */
struct ipc_shm_bd {
	int16_t pool_id;
	uint16_t buf_id;
	uint32_t data_size;
};

/**
 * struct ipc_shm_pool - buffer pool private data
 * @num_bufs:         number of buffers in pool
 * @buf_size:         size of buffers
 * @shm_size:         size of shared memory mapped by this pool (queue + bufs)
 * @local_pool_addr:  address of local buffer pool
 * @remote_pool_addr: address of remote buffer pool
 * @bd_queue:         queue containing BDs of free buffers
 * @rx_buffer_is_free: array tracking release status of RX buffers
 * @tx_buffer_is_free: array tracking transmit status of TX buffers
 *
 * bd_queue has two rings: one for pushing BDs (release ring) and one for
 * popping BDs (acquire ring).
 * Local IPC pushes BDs into release ring when local app finishes processing a
 * received buffer and calls ipc_shm_release_buf(). Remote IPC pops BDs from its
 * acquire ring (our release ring) when remote app calls ipc_shm_acquire_buf()
 * to prepare for a Tx operation.
 *
 * The relation between local and remote bd_queue rings is:
 *     local acquire ring == remote release ring
 *     local release ring == remote acquire ring
 */
struct ipc_shm_pool {
	uint16_t num_bufs;
	uint32_t buf_size;
	uint32_t shm_size;
	uintptr_t local_pool_addr;
	uintptr_t remote_pool_addr;
	struct ipc_queue bd_queue;
	bool *rx_buffer_is_free;
	bool *tx_buffer_is_free;
};

/**
 * struct ipc_managed_channel - managed channel private data
 * @bd_queue:  queue containing BDs of sent/received buffers
 * @num_pools: number of buffer pools
 * @pools:     buffer pools private data
 * @rx_cb:     receive callback
 * @cb_arg:    optional receive callback argument
 *
 * bd_queue has two rings: one for pushing BDs (Tx ring) and one for popping
 * BDs (Rx ring).
 * Local IPC device reads BDs pushed into bd_queue by remote IPC and remote
 * IPC device reads BDs pushed into bd_queue by local IPC.
 *
 * The relation between local and remote bd_queue rings is:
 *     local Tx ring == remote Rx ring
 *     local Rx ring == remote Tx ring
 */
struct ipc_managed_channel {
	struct ipc_queue bd_queue;
	uint16_t num_pools;
	struct ipc_shm_pool pools[IPC_SHM_MAX_POOLS];
	void (*rx_cb)(void *cb_arg, const uint8_t instance, uint8_t chan_id,
			void *buf, uint32_t size);
	void *cb_arg;
};

/**
 * struct ipc_channel_umem - unmanaged channel memory control structure
 * @sentinel:        magic word to ensure unmanaged channel integrity
 * @tx_count:        local channel Tx counter (it wraps around at max uint32)
 * @remote_tx_count: copy of remote Tx counter
 * @mem:             local channel unmanaged memory buffer
 *
 * tx_count is used by remote peer in Rx intr handler to determine if this
 * channel had a Tx operation and decide whether to call the app Rx callback.
 */
struct ipc_channel_umem {
	uint32_t sentinel;
	volatile uint32_t tx_count;
	volatile uint32_t remote_tx_count;
	uint8_t reserved[4];
	uint8_t mem[];
};

/**
 * struct ipc_unmanaged_channel - unmanaged channel private data
 * @size:                      unmanaged channel memory size requested by app
 * @detached:                  if true, channel uses external buffer
 * @detached_phys_addr:        local detached buffer physical address
 * @detached_virt_addr:        local detached buffer virtual address
 * @remote_detached_phys_addr: cached remote detached buffer physical address
 * @remote_detached_virt_addr: cached remote detached buffer virtual address
 * @local_umem:                local channel unmanaged memory
 * @remote_umem:               remote channel unmanaged memory
 * @rx_cb:                     receive callback
 * @cb_arg:                    optional receive callback argument
 */
struct ipc_unmanaged_channel {
	uint32_t size;
	bool detached;
	uintptr_t detached_phys_addr;
	uintptr_t detached_virt_addr;
	uintptr_t remote_phys_addr;
	uintptr_t remote_virt_addr;
	struct ipc_channel_umem *local_mem;
	struct ipc_channel_umem *remote_mem;
	void (*rx_cb)(void *cb_arg, const uint8_t instance, uint8_t chan_id,
			void *buf);
	void *cb_arg;
};

/**
 * struct ipc_shm_scmi_cfg - scmi channel parameters
 * @scmi_virt_addr: scmi memory virtual address
 * @scmi_phys_addr: scmi memory physical address
 * @scmi_size:      channel memory size
 * @rx_cb:          receive callback
 * @cb_arg:         optional receive callback argument
 */
struct ipc_scmi_channel {
	volatile uintptr_t scmi_virt_addr;
	volatile uintptr_t scmi_phys_addr;
	uint32_t scmi_size;
	void (*rx_cb)(void *cb_arg, const uint8_t instance, uint8_t chan_id,
			void *buf);
	void *cb_arg;
};

/**
 * struct ipc_shm_channel - ipc channel private data
 * @id:   channel id
 * @type: channel type (see ipc_shm_channel_type)
 * @ch:   managed/unmanaged channel private data
 */
struct ipc_shm_channel {
	uint8_t id;
	enum ipc_shm_channel_type type;
	union {
		struct ipc_managed_channel mng;
		struct ipc_unmanaged_channel umng;
		struct ipc_scmi_channel scmi;
	} ch;
};

/**
 * struct ipc_shm_global - ipc shm global data shared with remote
 * @state: state to indicate whether local is initialized
 *
 * Global data is located at beginning of local/remote shared memory so the size
 * of this struct should chosen so that memory alignment is preserved.
 */
struct ipc_shm_global {
	uint64_t state;
};

/**
 * struct ipc_shm_priv - ipc shm private data
 * @shm_size:     local/remote shared memory size
 * @num_channels: number of shared memory channels
 * @channels:     ipc channels private data
 * @global:       local global data shared with remote
 */
struct ipc_shm_priv {
	uint32_t shm_size;
	uint8_t num_channels;
	struct ipc_shm_channel channels[IPC_SHM_MAX_CHANNELS];
	struct ipc_shm_global *global;
	struct ipc_shm_cfg *shm_cfg_addr;
};

/* ipc shm private data */
static struct ipc_shm_priv ipc_shm_priv_data[IPC_SHM_MAX_INSTANCES];

/* get channel without validation (used in internal functions only) */
static inline struct ipc_shm_channel *get_channel_priv(const uint8_t instance,
		uint8_t chan_id)
{
	return &ipc_shm_priv_data[instance].channels[chan_id];
}

/* get channel with validation (can be used in API functions) */
static inline struct ipc_shm_channel *get_channel(const uint8_t instance,
		uint8_t chan_id)
{
	if (chan_id >= ipc_shm_priv_data[instance].num_channels) {
		shm_err("Channel id outside valid range: 0 - %d\n",
				ipc_shm_priv_data[instance].num_channels);
		return NULL;
	}

	return get_channel_priv(instance, chan_id);
}

/* get managed channel with validation */
static inline struct ipc_managed_channel *get_managed_chan(
		const uint8_t instance, uint8_t chan_id)
{
	struct ipc_shm_channel *chan = get_channel(instance, chan_id);

	if (chan == NULL)
		return NULL;

	if (chan->type != IPC_SHM_MANAGED) {
		shm_err("Invalid channel type for this operation\n");
		return NULL;
	}

	return &chan->ch.mng;
}

/* get unmanaged channel with validation */
static inline struct ipc_unmanaged_channel *get_unmanaged_chan(
		const uint8_t instance, uint8_t chan_id)
{
	struct ipc_shm_channel *chan = get_channel(instance, chan_id);

	if (chan == NULL)
		return NULL;

	if (chan->type != IPC_SHM_UNMANAGED) {
		shm_err("Invalid channel type for this operation\n");
		return NULL;
	}

	return &chan->ch.umng;
}

/* get scmi channel with validation */
static inline struct ipc_scmi_channel *get_scmi_chan(
		const uint8_t instance, uint8_t chan_id)
{
	struct ipc_shm_channel *chan = get_channel(instance, chan_id);

	if ((chan == NULL) || (chan->type != IPC_SHM_SCMI)) {
		shm_err("Invalid channel type for this operation\n");
		return NULL;
	}

	return &chan->ch.scmi;
}

/* check integrity of uchan: the boundaries have not been altered */
static int8_t ipc_check_uchan_integrity(
		const struct ipc_unmanaged_channel *uchan)
{
	if ((uchan->local_mem->sentinel == (uint32_t)IPC_UCHAN_INIT_DONE)
			&& (uchan->remote_mem->sentinel
				== (uint32_t)IPC_UCHAN_INIT_DONE))
		return 0;

	return -EINVAL;
}

/* check integrity of uchan: the boundaries have not been altered */
static int8_t ipc_check_uchan_local_integrity(
		const struct ipc_unmanaged_channel *uchan)
{
	if (uchan->local_mem->sentinel == (uint32_t)IPC_UCHAN_INIT_DONE)
		return 0;

	return -EINVAL;
}

/* check integrity of mchan: the boundaries have not been altered */
static int8_t ipc_check_mchan_integrity(struct ipc_managed_channel *mchan)
{
	uint8_t pool_id;
	struct ipc_shm_pool *pool = NULL;

	if (0 == ipc_queue_check_integrity(&mchan->bd_queue)) {
		/* check all the pool bd boundaries */
		for (pool_id = 0; pool_id < mchan->num_pools; pool_id++) {
			pool = &mchan->pools[pool_id];
			if (0 != ipc_queue_check_integrity(&pool->bd_queue))
				return -EINVAL;
		}
		return 0;
	}

	return -EINVAL;
}

/**
 * ipc_unmanaged_buf_addr() - get remote unmanaged channel buffer address
 * @uchan: unmanaged channel pointer
 *
 * For attached channels, returns the remote mem buffer directly.
 * For detached channels, reads the remote physical address from shared memory
 * and remaps it if it changed since the last call.
 *
 * Return: virtual buffer address, or (uintptr_t)NULL on failure
 */
static uintptr_t ipc_unmanaged_buf_addr(
		struct ipc_unmanaged_channel *uchan)
{
	uintptr_t rphys;

	if (uchan->detached == false)
		return (uintptr_t)uchan->remote_mem->mem;

	rphys = *((uintptr_t *)(uchan->remote_mem->mem));
	if (rphys == (uintptr_t)NULL)
		return (uintptr_t)NULL;

	/* remap only if remote phys addr changed */
	if (rphys != uchan->remote_phys_addr) {
		if (uchan->remote_virt_addr != (uintptr_t)NULL)
			ipc_os_iounmap(uchan->remote_phys_addr,
				uchan->remote_virt_addr, uchan->size);
		uchan->remote_phys_addr = rphys;
		uchan->remote_virt_addr =
			ipc_os_ioremap(rphys, uchan->size);
	}

	return uchan->remote_virt_addr;
}

/**
 * ipc_channel_rx() - handle Rx for a single channel
 * @instance: instance id
 * @chan_id:  channel id
 * @budget:   available work budget (number of messages to be processed)
 *
 * Return:
 * * %Number of processed messages  - on mananged
 * * %Budget                        - on unmanaged channel
 */
static uint32_t ipc_channel_rx(const uint8_t instance, uint8_t chan_id,
		uint32_t budget)
{
	struct ipc_shm_channel *chan = get_channel_priv(instance, chan_id);
	struct ipc_managed_channel *mchan = &chan->ch.mng;
	struct ipc_unmanaged_channel *uchan = &chan->ch.umng;
	struct ipc_scmi_channel *scmi_chan = &chan->ch.scmi;
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	uintptr_t buf_addr;
	uint32_t remote_tx_count;
	int8_t result;
	uint32_t work = 0;
	enum ipc_scmi_channel_status_type status = IPC_SCMI_CHANNEL_FREE;
	volatile uint32_t *scmi_buf = NULL;


	if (chan->type == IPC_SHM_SCMI) {
		/* scmi channels: call Rx callback if channel status is BUSY*/
		scmi_buf = (volatile uint32_t *)scmi_chan->scmi_virt_addr;
		if (scmi_buf == NULL)
			return 0;

		status = (enum ipc_scmi_channel_status_type)(
				scmi_buf[IPC_SCMI_CHANNEL_STATUS_IDX] & 0x1U);
		if (status != IPC_SCMI_CHANNEL_BUSY)
			return 0;

		scmi_chan->rx_cb(scmi_chan->cb_arg, instance, chan->id,
					(void *) scmi_chan->scmi_virt_addr);
		return budget;

	}

	/* unmanaged channels: call Rx callback if channel Tx counter changed */
	if (chan->type == IPC_SHM_UNMANAGED) {
		if (0 == ipc_check_uchan_integrity(uchan)) {
			remote_tx_count = uchan->remote_mem->tx_count;

			/* call Rx cb if remote Tx counter changed */
			if (remote_tx_count
					!= uchan->local_mem->remote_tx_count) {

				/* save new remote Tx counter */
				uchan->local_mem->remote_tx_count
					= remote_tx_count;

				buf_addr = ipc_unmanaged_buf_addr(uchan);

				if (buf_addr != (uintptr_t)NULL)
					uchan->rx_cb(uchan->cb_arg, instance,
						chan->id, (void *)buf_addr);

				return budget;
			}
		}
		return 0;
	}

	/* managed channels: process incoming BDs in the limit of budget */
	while (work < budget) {
		result = ipc_queue_pop(&mchan->bd_queue, &bd);
		if (result != 0) {
			return work;
		}
		pool = &mchan->pools[bd.pool_id];
		buf_addr = pool->remote_pool_addr
				+ (bd.buf_id * pool->buf_size);

		/* check if buf_addr is valid */
		if ((buf_addr >= ipc_os_get_remote_shm(instance))
			&& ((buf_addr + pool->buf_size)
				<= ipc_os_get_remote_shm(instance) +
				ipc_shm_priv_data[instance].shm_size)) {
			pool->rx_buffer_is_free[bd.buf_id] = false;
			mchan->rx_cb(mchan->cb_arg, instance, chan->id,
				(void *)buf_addr, bd.data_size);
		}
		work++;
	}

	return work;
}

/**
 * ipc_instance_is_free() - determine if the instance is used or not
 * @instance: instance id
 *
 * This function return the state of instance.
 *
 * Return:
 * * %IPC_SHM_INSTANCE_FREE   - Instance is free
 * * %IPC_SHM_INSTANCE_USED   - Instance is initialized
 * * %IPC_SHM_INSTANCE_ERROR  - Invalid instance
 */
static enum ipc_shm_instance_state ipc_instance_is_free(const uint8_t instance)
{
	if (instance >= IPC_SHM_MAX_INSTANCES)
		return IPC_SHM_INSTANCE_ERROR;

	if (ipc_shm_priv_data[instance].global == NULL)
		return IPC_SHM_INSTANCE_FREE;
	if (ipc_shm_priv_data[instance].global->state
			== (uint64_t)IPC_SHM_STATE_CLEAR)
		return IPC_SHM_INSTANCE_FREE;

	return IPC_SHM_INSTANCE_USED;
}

/**
 * ipc_shm_rx() - shm Rx handler, called from softirq
 * @instance: instance id
 * @budget:   available work budget (number of messages to be processed)
 *
 * This function handles all channels using a fair handling algorithm: all
 * channels are treated equally and no channel is starving.
 *
 * Return:
 * * %Number of processed messages
 */
static uint32_t ipc_shm_rx(const uint8_t instance, int budget)
{
	uint8_t num_chans = ipc_shm_priv_data[instance].num_channels;
	uint32_t chan_budget, chan_work;
	uint32_t work = 0;
	uint8_t more_work = 1;
	uint8_t chan_id = 0u;

	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED)
		return 0;

	/* fair channel handling algorithm */
	while ((work < budget) && (more_work > 0)) {
		chan_budget = (budget - work) / (num_chans);
		if (chan_budget == 0u)
			chan_budget = 1u;
		more_work = 0;

		for (chan_id = 0; chan_id < num_chans; chan_id++) {
			chan_work = ipc_channel_rx(instance,
					chan_id, chan_budget);
			work += chan_work;

			if (chan_work == chan_budget)
				more_work = 1;
		}
	}

	return work;
}

/**
 * ipc_buf_pool_init() - init buffer pool
 * @instance:   instance id
 * @chan_id:    channel index
 * @pool_id:    pool index in channel
 * @local_shm:  local pool shared memory address
 * @remote_shm: remote pool shared memory address
 * @cfg:        channel configuration parameters
 *
 * To ensure freedom from interference when writing in shared memory, only one
 * IPC is allowed to write in a BD ring, so the IPC that pushes BDs in the
 * release ring at the end of an Rx operation must also initialize it. That's
 * why local IPC initializes bd_queue with BDs pointing to remote free buffers.
 * Since the shared memory configuration is symmetric and remote base address
 * is known, local IPC can compute the remote BD info.
 *
 * Return:
 * * %0		- on success
 * * %-EINVAL	- when input parameters, read or write indexes are invalid
 * * %-EAGAIN	- when remote is initializing the corresponding queue
 * * %-ENOMEM	- when pool doesn't fit in shared-memory
 */
static int8_t ipc_buf_pool_init(const uint8_t instance, uint8_t chan_id,
		uint16_t pool_id, struct ipc_shm_pool_addr *mng_pool,
		const struct ipc_shm_pool_cfg *cfg)
{
	struct ipc_managed_channel *chan = get_managed_chan(instance, chan_id);
	struct ipc_shm_pool *pool = &chan->pools[pool_id];
	struct ipc_queue_data queue_data;
	uint32_t buf_state_size;
	uint32_t queue_mem_size;
	uint8_t align_padding;
	struct ipc_shm_bd bd;
	uint16_t buf_id;
	int8_t err;

	if (cfg->num_bufs > IPC_SHM_MAX_BUFS_PER_POOL) {
		shm_err("Too many buffers configured in pool. "
				"Increase IPC_SHM_MAX_BUFS_PER_POOL if needed\n");
		return -EINVAL;
	}

	pool->num_bufs = cfg->num_bufs;
	pool->buf_size = cfg->buf_size;

	/* Preapare queue data parameter */
	queue_data.queue_type = IPC_SHM_POOL_QUEUE;
	queue_data.elem_size = (uint8_t)sizeof(struct ipc_shm_bd);
	queue_data.elem_num = (uint16_t)cfg->num_bufs;
	queue_data.push_addr = mng_pool->local_pool_shm;
	queue_data.pop_addr = mng_pool->remote_pool_shm;
	/* init pool bd_queue with push ring mapped at the start of local
	 * pool shm and pop ring mapped at start of remote pool shm
	 */
	err = ipc_queue_init(&pool->bd_queue, queue_data);
	if (err != 0)
		return err;

	/* init local/remote buffer pool addrs */
	queue_mem_size = ipc_queue_mem_size(&pool->bd_queue);
	align_padding = (uint8_t)((8U - (cfg->num_bufs % 8U)) % 8U);
	buf_state_size = (uint32_t)cfg->num_bufs + (uint32_t)align_padding;



	/* Place rx buffer state array in shared memory right after queue */
	pool->rx_buffer_is_free = (bool *)(mng_pool->local_pool_shm
					+ queue_mem_size);

	/**
	 * Place tx buffer state array in shared memory after rx buffer state array
	 * with padding alignment to avoid unaligned access
	 */
	pool->tx_buffer_is_free = (bool *)(pool->rx_buffer_is_free
					+ buf_state_size);

	/* init actual local buffer pool addr */
	pool->local_pool_addr = mng_pool->local_pool_shm
				+ queue_mem_size
				+ (2U * buf_state_size);
	/* init actual remote buffer pool addr */
	pool->remote_pool_addr = mng_pool->remote_pool_shm
				+ queue_mem_size
				+ (2U * buf_state_size);

	pool->shm_size = queue_mem_size
			+ (cfg->buf_size * cfg->num_bufs)
			+ (2U * buf_state_size);

	/* Initialize all buffer states to FREE */
	for (buf_id = 0; buf_id < cfg->num_bufs; buf_id++) {
		pool->tx_buffer_is_free[buf_id] = true;
		pool->rx_buffer_is_free[buf_id] = true;
	}

	/* check if pool fits into shared memory */
	if ((mng_pool->local_pool_shm + pool->shm_size)
			> (ipc_os_get_local_shm(instance)
				+ ipc_shm_priv_data[instance].shm_size)) {
		shm_err("Not enough shared memory for pool %d from channel %d\n",
				pool_id, chan_id);
		return -ENOMEM;
	}

	/* populate bd_queue with free BDs from remote pool */
	for (buf_id = 0; buf_id < pool->num_bufs; buf_id++) {
		bd.pool_id = (int16_t) pool_id;
		bd.buf_id = buf_id;
		bd.data_size = 0;

		err = ipc_queue_push(&pool->bd_queue, &bd);
		if (err != 0) {
			shm_err("Unable to init queue with free buffer descriptors "
					"for pool %d of channel %d\n",
					pool_id, chan_id);
			return err;
		}
	}

	/* Mark queue as initialized if everything is ok */
	pool->bd_queue.push_ring->sentinel = (uint64_t)IPC_QUEUE_INIT_DONE;
	shm_dbg("ipc shm pool %d of chan %d initialized\n", pool_id, chan_id);

	return 0;
}

/**
 * ipc_get_total_buf_per_chan() - get total buffers of an managed channel
 *
 * @instance: instance id
 * @chan_id:  channel id
 * @cfg:      managed channel configuration
 *
 * Return:
 * * %Total buffers	- on success
 * * %0			- if the total buffer are invalid
 */
static uint32_t ipc_get_total_buf_per_chan(const uint8_t instance,
		uint8_t chan_id, const struct ipc_shm_managed_cfg *cfg)
{
	struct ipc_managed_channel *chan =
		&ipc_shm_priv_data[instance].channels[chan_id].ch.mng;
	const struct ipc_shm_pool_cfg *pool_cfg;
	uint32_t prev_buf_size = 0u;
	uint32_t total_bufs = 0u;
	uint8_t pool_id = 0;

	/* save managed channel parameters */
	chan->rx_cb = cfg->rx_cb;
	chan->cb_arg = cfg->cb_arg;
	chan->num_pools = cfg->num_pools;

	/* count total number of buffers from all pools */
	for (pool_id = 0; pool_id < chan->num_pools; pool_id++) {
		pool_cfg = &cfg->pools[pool_id];
		/*
		 * check pools are sorted in ascending order
		 * by buf size
		 */
		if (pool_cfg->buf_size < prev_buf_size)
			return 0;

		prev_buf_size = pool_cfg->buf_size;
		total_bufs += pool_cfg->num_bufs;

		if (total_bufs > IPC_SHM_MAX_BUFS_PER_CHANNEL)
			return 0;
	}

	return total_bufs;
}

static int8_t managed_channel_init(const uint8_t instance, int chan_id,
		uintptr_t local_shm, uintptr_t remote_shm,
		const struct ipc_shm_managed_cfg *cfg)
{
	struct ipc_managed_channel *chan = get_managed_chan(instance, chan_id);
	struct ipc_queue_data queue_data;
	struct ipc_shm_pool_addr mng_pool_addr
			= { .local_pool_shm = (uintptr_t)NULL,
				.remote_pool_shm = (uintptr_t)NULL};
	uint32_t queue_mem_size;
	uint32_t total_bufs = 0;
	int16_t pool_id = 0;
	int8_t err;

	if (cfg->rx_cb == NULL) {
		shm_err("Receive callback not specified\n");
		return -EINVAL;
	}

	if (cfg->pools == NULL) {
		shm_err("NULL buffer pool configuration argument\n");
		return -EINVAL;
	}

	if (cfg->num_pools > IPC_SHM_MAX_POOLS) {
		shm_err("Number of pools must be between 1 and %d\n",
				IPC_SHM_MAX_POOLS);
		return -EINVAL;
	}

	total_bufs = ipc_get_total_buf_per_chan(instance, chan_id, cfg);
	if (total_bufs == 0u)
		return -EINVAL;

	/* Preapare queue data parameter */
	queue_data.queue_type = IPC_SHM_CHANNEL_QUEUE;
	queue_data.elem_size = (uint8_t)sizeof(struct ipc_shm_bd);
	queue_data.elem_num = (uint16_t)total_bufs;
	queue_data.push_addr = local_shm;
	queue_data.pop_addr = remote_shm;

	/* init channel bd_queue with push ring mapped at the start of local
	 * channel shm and pop ring mapped at start of remote channel shm
	 */
	err = ipc_queue_init(&chan->bd_queue, queue_data);
	if (err != 0)
		return err;

	chan->bd_queue.push_ring->sentinel = (uint64_t)IPC_QUEUE_INIT_DONE;
	/* init&map buffer pools after channel bd_queue */
	queue_mem_size = ipc_queue_mem_size(&chan->bd_queue);
	mng_pool_addr.local_pool_shm = local_shm + queue_mem_size;
	mng_pool_addr.remote_pool_shm = remote_shm + queue_mem_size;

	/* check if pool fits into shared memory */
	if ((mng_pool_addr.local_pool_shm) > (ipc_os_get_local_shm(instance)
				+ ipc_shm_priv_data[instance].shm_size)) {
		shm_err("Not enough shared memory for channel %d\n",
				chan_id);
		return -ENOMEM;
	}

	for (pool_id = 0; pool_id < chan->num_pools; pool_id++) {
		err = ipc_buf_pool_init(instance, chan_id, pool_id,
			&mng_pool_addr, &cfg->pools[pool_id]);
		if (err != 0)
			return err;

		/* compute next pool local/remote shm base address */
		mng_pool_addr.local_pool_shm += chan->pools[pool_id].shm_size;
		mng_pool_addr.remote_pool_shm += chan->pools[pool_id].shm_size;
	}

	return 0;
}

static int8_t unmanaged_channel_init(const uint8_t instance, int chan_id,
		uintptr_t local_shm, uintptr_t remote_shm,
		const struct ipc_shm_unmanaged_cfg *cfg)
{
	struct ipc_unmanaged_channel *chan = get_unmanaged_chan(instance,
			chan_id);

	if ((cfg->rx_cb == NULL) || (cfg->size == 0u)
			|| (cfg->size > IPC_SHM_MAX_UMNG_SIZE)) {
		shm_err("Receive callback not specified\n");
		return -EINVAL;
	}

	/* save unmanaged channel parameters */
	chan->size = cfg->size;
	chan->detached = cfg->detached;
	chan->detached_phys_addr = (uintptr_t)NULL;
	chan->detached_virt_addr = (uintptr_t)NULL;
	chan->remote_phys_addr = (uintptr_t)NULL;
	chan->remote_virt_addr = (uintptr_t)NULL;
	chan->rx_cb = cfg->rx_cb;
	chan->cb_arg = cfg->cb_arg;

	chan->local_mem = (struct ipc_channel_umem *) local_shm;
	chan->remote_mem = (struct ipc_channel_umem *) remote_shm;

	/* Check if remote unmanaged channel initialization is in progress */
	if (chan->remote_mem->sentinel
			== (uint32_t)IPC_SHM_UCHAN_INIT_IN_PROGRESS)
		return -EAGAIN;

	/* Mark that the queue initialization is in progress */
	chan->local_mem->sentinel = (uint32_t)IPC_SHM_UCHAN_INIT_IN_PROGRESS;
	/* Check if remote initialization is in progress */
	if (chan->remote_mem->sentinel == (uint32_t)IPC_UCHAN_INIT_DONE) {
		/* Use values from remote if it is already initialized */
		chan->local_mem->tx_count = chan->remote_mem->remote_tx_count;
		chan->local_mem->remote_tx_count = chan->remote_mem->tx_count;
	} else {
		chan->local_mem->tx_count = 0;
		chan->local_mem->remote_tx_count = 0;
	}

	if (cfg->detached == false) {
		chan->local_mem->sentinel = (uint32_t)IPC_UCHAN_INIT_DONE;
	} else {
		if (cfg->addr == (uintptr_t)NULL)
			return -EINVAL;

		chan->detached_phys_addr = cfg->addr;

		/* ioremap local detached buffer (phys -> virt) */
		chan->detached_virt_addr = ipc_os_ioremap(
				chan->detached_phys_addr, cfg->size);
		if (chan->detached_virt_addr == (uintptr_t)NULL)
			return -EINVAL;

		/* store physical address in shared memory for remote */
		*((uintptr_t *)(chan->local_mem->mem)) = cfg->addr;

		chan->local_mem->sentinel = (uint32_t)IPC_UCHAN_INIT_DONE;
	}

	return 0;
}

static int8_t scmi_channel_init(const uint8_t instance, uint8_t chan_id,
		const struct ipc_shm_scmi_cfg *cfg)
{
	struct ipc_scmi_channel *chan
		= &ipc_shm_priv_data[instance].channels[chan_id].ch.scmi;
	uint32_t *scmi_buf = NULL;

	if ((uintptr_t)NULL == cfg->scmi_addr)
		return -EINVAL;

	/* save scmi channel parameters TBD mapping */
	chan->scmi_phys_addr = cfg->scmi_addr;
	chan->scmi_size = cfg->scmi_size;
	chan->rx_cb = cfg->rx_cb;
	chan->cb_arg = cfg->cb_arg;

	chan->scmi_virt_addr = ipc_os_ioremap(chan->scmi_phys_addr,
		chan->scmi_size);
	if ((uintptr_t)NULL == chan->scmi_virt_addr)
		return -EINVAL;

	/* Set scmi status to free */
	scmi_buf = (uint32_t *)chan->scmi_virt_addr;
	scmi_buf[IPC_SCMI_CHANNEL_STATUS_IDX]
		= IPC_SCMI_CHANNEL_FREE & 0x1U;

	return 0;
}

/**
 * ipc_shm_init_channels() - initialize shared memory IPC channels
 * @instance:	instance id
 * @chan_id:	channel index
 * @local_shm:	local channel shared memory address
 * @remote_shm: remote channel shared memory address
 * @cfg:	channel configuration parameters
 *
 * Return:
 * * %0		- on success
 * * %-EINVAL	- when input parameters are invalid
 * * %-EAGAIN	- when remote is initializing the corresponding queue
 */
static int ipc_shm_init_channels(const uint8_t instance, int chan_id,
		uintptr_t local_shm, uintptr_t remote_shm,
		const struct ipc_shm_channel_cfg *cfg)
{
	struct ipc_shm_channel *chan = get_channel_priv(instance, chan_id);
	int err;

	if (cfg == NULL) {
		shm_err("NULL channel configuration argument\n");
		return -EINVAL;
	}

	/* save common channel parameters */
	chan->id = chan_id;
	chan->type = cfg->type;

	if (cfg->type == IPC_SHM_MANAGED) {
		err = managed_channel_init(instance, chan_id, local_shm,
				remote_shm, &cfg->ch.managed);
	} else if (cfg->type == IPC_SHM_UNMANAGED) {
		err = unmanaged_channel_init(instance, chan_id, local_shm,
				remote_shm, &cfg->ch.unmanaged);
	} else if (cfg->type == IPC_SHM_SCMI) {
		err = scmi_channel_init(instance, chan_id, &cfg->ch.scmi);
	} else {
		shm_err("Invalid channel type\n");
		err = -EINVAL;
	}
	if (err != 0)
		return err;

	shm_dbg("ipc shm channel %d initialized\n", chan_id);
	return 0;
}

/**
 * ipc_shm_free_managed_channel() - Free the specified managed channel
 *
 * @instance:  instance id
 * @chan_id:   channel id
 * @chan_type: channel type
 *
 */
static void ipc_shm_free_managed_channel(struct ipc_managed_channel *mchan)
{
	uint16_t pool_id = 0;

	if ((mchan->bd_queue.push_ring->sentinel
			== (uint64_t)IPC_QUEUE_INIT_DONE)
		|| (mchan->bd_queue.push_ring->sentinel
			== (uint64_t)IPC_QUEUE_INIT_IN_PROGRESS)) {
		/* Free channel queue */
		ipc_queue_free(&mchan->bd_queue);
		/* Free the queues from all pools */
		for (pool_id = 0; pool_id < mchan->num_pools; pool_id++) {
			if (mchan->pools[pool_id]
					.bd_queue.push_ring == NULL)
				continue;
			if ((mchan->pools[pool_id].bd_queue.push_ring->sentinel
					== (uint64_t)IPC_QUEUE_INIT_DONE)
				|| (mchan->pools[pool_id]
						.bd_queue.push_ring->sentinel
					== (uint64_t)
						IPC_QUEUE_INIT_IN_PROGRESS)) {
				ipc_queue_free(&mchan->pools[pool_id].bd_queue);
			}
		}
	}
}

/**
 * ipc_shm_free_channel() - Free the specified channel
 *                          from the specified instance
 *
 * @instance:  instance id
 * @chan_id:   channel id
 * @chan_type: channel type
 *
 */
static void ipc_shm_free_channel(const uint8_t instance, uint8_t chan_id,
					enum ipc_shm_channel_type chan_type)
{
	struct ipc_unmanaged_channel *uchan = NULL;
	struct ipc_managed_channel *mchan = NULL;
	struct ipc_scmi_channel *scmi = NULL;

	if (chan_type == IPC_SHM_MANAGED) {
		mchan = &ipc_shm_priv_data[instance].channels[chan_id].ch.mng;

		if (mchan->bd_queue.push_ring != NULL) {
			/* Free all managed channel queue */
			ipc_shm_free_managed_channel(mchan);
		}
	} else if (chan_type == IPC_SHM_UNMANAGED) {
		uchan = &ipc_shm_priv_data[instance].channels[chan_id].ch.umng;
		if (uchan->local_mem != NULL) {
			if (uchan->detached == true) {
				*((uintptr_t *)(uchan->local_mem->mem))
					= (uintptr_t)NULL;
				if (uchan->detached_virt_addr
						!= (uintptr_t)NULL) {
					ipc_os_iounmap(
						uchan->detached_phys_addr,
						uchan->detached_virt_addr,
						uchan->size);
					uchan->detached_virt_addr
						= (uintptr_t)NULL;
				}
				if (uchan->remote_virt_addr
						!= (uintptr_t)NULL) {
					ipc_os_iounmap(
						uchan->remote_phys_addr,
						uchan->remote_virt_addr,
						uchan->size);
					uchan->remote_virt_addr
						= (uintptr_t)NULL;
					uchan->remote_phys_addr
						= (uintptr_t)NULL;
				}
			}
			uchan->local_mem->sentinel = 0;
			uchan->local_mem->tx_count = 0;
			uchan->local_mem->remote_tx_count = 0;
			uchan->local_mem = NULL;
		}
	} else if (chan_type == IPC_SHM_SCMI) {
		scmi = &ipc_shm_priv_data[instance].channels[chan_id].ch.scmi;
		if (scmi->scmi_virt_addr != (uintptr_t)NULL) {
			ipc_os_iounmap(scmi->scmi_phys_addr,
						scmi->scmi_virt_addr,
						scmi->scmi_size);
			scmi->scmi_phys_addr = (uintptr_t)NULL;
			scmi->scmi_size = 0;
		}
	} else {
		/* Not any kind of channel above */
		return;
	}
}

/* Get channel local mapped memory size */
static uint32_t get_chan_memmap_size(const uint8_t instance, int chan_id)
{
	struct ipc_shm_channel *chan = get_channel_priv(instance, chan_id);
	struct ipc_managed_channel *mchan;
	uint32_t size = 0;
	int pool_id;

	/* unmanaged channels: control structure size + channel memory size */
	if (chan->type == IPC_SHM_UNMANAGED) {
		if (chan->ch.umng.detached == false)
			return (uint32_t)(sizeof(struct ipc_channel_umem) +
				chan->ch.umng.size);

		/* 64 bits reserved to store the unmanaged detached address */
		return (uint32_t)(sizeof(struct ipc_channel_umem) + 8u);
	} else if (chan->type == IPC_SHM_SCMI) {
		return 0;
	}

	/* managed channels: size of BD queue + size of buf pools */
	mchan = get_managed_chan(instance, chan_id);
	size = ipc_queue_mem_size(&mchan->bd_queue);
	for (pool_id = 0; pool_id < mchan->num_pools; pool_id++) {
		size += mchan->pools[pool_id].shm_size;
	}

	return size;
}

/* Initialize only one instance shared memory device */
int8_t ipc_shm_init_instance(uint8_t instance)
{
	uintptr_t local_chan_shm;
	uintptr_t remote_chan_shm;
	uintptr_t local_shm;
	uint32_t chan_size;
	uint8_t chan_id;
	uint32_t chan_offset = (uint32_t)sizeof(struct ipc_shm_global);
	int err;
	struct ipc_shm_cfg *cfg = ipc_shm_priv_data[instance].shm_cfg_addr;

	if (ipc_instance_is_free(instance) == IPC_SHM_INSTANCE_USED)
		return -EINVAL;

	if (cfg == NULL) {
		shm_err("NULL argument\n");
		return -EINVAL;
	}

	if ((cfg->local_shm_addr == (uintptr_t) NULL)
			|| (cfg->remote_shm_addr == (uintptr_t) NULL)) {
		shm_err("NULL local or remote address\n");
		return -EINVAL;
	}

	if ((cfg->num_channels < 1) ||
		(cfg->num_channels > IPC_SHM_MAX_CHANNELS)) {
		shm_err("Number of channels must be between 1 and %d\n",
				IPC_SHM_MAX_CHANNELS);
		return -EINVAL;
	}

	/* save api params */
	ipc_shm_priv_data[instance].shm_size = cfg->shm_size;
	ipc_shm_priv_data[instance].num_channels = cfg->num_channels;

	/* pass interrupt and core data to hw */
	err = ipc_hw_init(instance, cfg);
	if (err != 0)
		return err;

	/* init OS specific resources */
	err = ipc_os_init(instance, cfg, ipc_shm_rx);
	if (err != 0) {
		ipc_hw_free(instance);
		return err;
	}

	/* global data stored at beginning of local shared memory */
	local_shm = ipc_os_get_local_shm(instance);
	ipc_shm_priv_data[instance].global = (struct ipc_shm_global *)local_shm;
	ipc_shm_priv_data[instance].global->state = IPC_SHM_STATE_CLEAR;

	/* init channels */
	local_chan_shm = local_shm + (uintptr_t)chan_offset;
	remote_chan_shm = ipc_os_get_remote_shm(instance)
				+ (uintptr_t)chan_offset;
	shm_dbg("initializing channels...\n");
	for (chan_id = 0; chan_id < ipc_shm_priv_data[instance].num_channels;
			chan_id++) {
		err = ipc_shm_init_channels(instance, chan_id, local_chan_shm,
				remote_chan_shm, &cfg->channels[chan_id]);
		if (err != 0) {
			ipc_shm_priv_data[instance].global = NULL;
			for (chan_id = 0; chan_id
				< ipc_shm_priv_data[instance].num_channels;
					chan_id++) {
				if (cfg->channels != NULL)
					ipc_shm_free_channel(instance, chan_id,
						cfg->channels[chan_id].type);
			}
			ipc_os_free(instance);
			ipc_hw_free(instance);
			return err;
		}

		/* compute next channel local/remote shm base address */
		chan_size = get_chan_memmap_size(instance, chan_id);
		local_chan_shm += chan_size;
		remote_chan_shm += chan_size;
	}

	/* enable interrupt notifications */
	ipc_hw_irq_enable(instance);

	ipc_shm_priv_data[instance].global->state = IPC_SHM_STATE_READY;
	shm_dbg("Instance %d initialized\n", instance);

	return 0;
}

void ipc_shm_free_instance(const uint8_t instance)
{
	uint8_t chan_id = 0;

	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED)
		return;

	/* reset state */
	ipc_shm_priv_data[instance].global->state = IPC_SHM_STATE_CLEAR;
	ipc_shm_priv_data[instance].global = NULL;

	/* disable hardirq */
	ipc_hw_irq_disable(instance);

	/* Free all channels from the specified instance */
	for (chan_id = 0;
			chan_id < ipc_shm_priv_data[instance].num_channels;
			chan_id++) {
		ipc_shm_free_channel(instance, chan_id,
			ipc_shm_priv_data[instance].channels[chan_id].type);
	}

	/* Free OS and HW for the specified instance */
	ipc_os_free(instance);
	ipc_hw_free(instance);
}

void ipc_shm_free(void)
{
	uint8_t instance_id = 0;

	/* Free all instances */
	for (instance_id = 0;
			instance_id < IPC_SHM_MAX_INSTANCES; instance_id++) {
		if (ipc_instance_is_free(instance_id) == IPC_SHM_INSTANCE_USED)
			ipc_shm_free_instance(instance_id);
	}

	shm_dbg("ipc shm released\n");
}

void *ipc_shm_acquire_buf(const uint8_t instance,
				uint8_t chan_id, uint32_t mem_size)
{
	struct ipc_managed_channel *chan;
	struct ipc_shm_pool *pool = NULL;
	struct ipc_shm_bd bd = {.pool_id = 0, .buf_id = 0u, .data_size = 0u};
	uintptr_t buf_addr;
	int pool_id;

	/* check if instance is valid */
	if (ipc_shm_is_remote_ready(instance) != 0)
		return NULL;

	chan = get_managed_chan(instance, chan_id);
	if ((chan == NULL) || (mem_size == 0u)
			|| (ipc_check_mchan_integrity(chan) != 0))
		return NULL;

	/* find first non-empty pool that accommodates the requested size */
	for (pool_id = 0; pool_id < chan->num_pools; pool_id++) {
		pool = &chan->pools[pool_id];

		/* check if pool buf size covers the requested size */
		if (mem_size > pool->buf_size)
			continue;

		/* check if pool has any free buffers left */
		if (ipc_queue_pop(&pool->bd_queue, &bd) == 0)
			break;
	}

	if (pool_id == chan->num_pools) {
		shm_dbg("No free buffer found in channel %d\n", chan_id);
		return NULL;
	}

	buf_addr = pool->local_pool_addr +
		(uint32_t)(bd.buf_id * pool->buf_size);

	/* check if buf_addr is valid */
	if ((buf_addr < ipc_os_get_local_shm(instance))
		|| ((buf_addr + pool->buf_size)
			> ipc_os_get_local_shm(instance)
				+ ipc_shm_priv_data[instance].shm_size))
		return NULL;

	/* Mark buffer as in use */
	pool->tx_buffer_is_free[bd.buf_id] = false;

	shm_dbg("ch %d: pool %d: acquired buffer %d with address %lx\n",
			chan_id, pool_id, bd.buf_id, buf_addr);
	return (void *)buf_addr;
}

void *ipc_shm_acquire_scmi(const uint8_t instance, uint8_t chan_id)
{
	const struct ipc_scmi_channel *chan;

	/* need to check if local instance is valid to be used */
	/* no need to check if remote instance is ready */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED)
		return NULL;

	chan = get_scmi_chan(instance, chan_id);
	if (chan == NULL)
		return NULL;

	return (void *)chan->scmi_virt_addr;
}

int8_t ipc_shm_init(const struct ipc_shm_instances_cfg *cfg)
{
	uint8_t instance_id = 0;
	int err = 0;

	if (cfg == NULL) {
		shm_err("NULL argument\n");
		return -EINVAL;
	}

	if ((cfg->num_instances > IPC_SHM_MAX_INSTANCES)
			|| (cfg->num_instances == 0u))
		return -EINVAL;

	/* init all instances */
	for (instance_id = 0; instance_id < cfg->num_instances; instance_id++) {
		/* Skip initialization of initialized instance */
		if (ipc_instance_is_free(instance_id) == IPC_SHM_INSTANCE_USED)
			continue;

		ipc_shm_priv_data[instance_id].shm_cfg_addr
			= &(cfg->shm_cfg[instance_id]);

#ifndef IPC_SHM_NOT_ENABLED_AT_INIT
		err = ipc_shm_init_instance(instance_id);
#endif
		if (err != 0)
			return err;
	}
	return err;
}

/**
 * find_pool_for_buf() - Find the pool that owns the specified buffer.
 * @chan:	managed channel pointer
 * @buf:	buffer pointer
 * @remote:	flag telling if buffer is from remote OS
 *
 * Return:
 * * %Pool index	- on success
 * * %-EINVAL		- Invalid buffer
 */
static int16_t find_pool_for_buf(struct ipc_managed_channel *chan,
						uintptr_t buf, uint8_t remote)
{
	struct ipc_shm_pool *pool;
	uintptr_t addr;
	uint32_t pool_size;
	uint16_t pool_id;

	for (pool_id = 0; pool_id < chan->num_pools; pool_id++) {
		pool = &chan->pools[pool_id];

		if (remote == IPC_BUFFER_FROM_REMOTE) {
			addr = pool->remote_pool_addr;
		} else {
			addr = pool->local_pool_addr;
		}

		pool_size = pool->num_bufs * pool->buf_size;

		if ((buf >= addr) && (buf < (addr + pool_size)))
			return pool_id;
	}

	return -EINVAL;
}

int8_t ipc_shm_release_buf(const uint8_t instance, uint8_t chan_id,
				const void *buf)
{
	struct ipc_managed_channel *chan;
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	int8_t err;

	/* check if instance is valid */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED)
		return -EINVAL;

	chan = get_managed_chan(instance, chan_id);
	if ((chan == NULL) || (buf == NULL)
			|| (ipc_check_mchan_integrity(chan) != 0))
		return -EINVAL;

	/* Find the pool that owns the buffer */
	bd.pool_id = find_pool_for_buf(chan,
			(uintptr_t)buf, IPC_BUFFER_FROM_REMOTE);
	if (bd.pool_id == -EINVAL) {
		shm_err("Buffer address %p doesn't belong to channel %d\n",
				buf, chan_id);
		return -EINVAL;
	}

	pool = &chan->pools[bd.pool_id];
	bd.buf_id = (uint16_t)(((uintptr_t)buf - pool->remote_pool_addr) /
			pool->buf_size);
	bd.data_size = 0; /* reset size of written data in buffer */

	if (true == pool->rx_buffer_is_free[bd.buf_id]) {
		shm_err("Buffer %d from pool %d is already free\n",bd.buf_id, bd.pool_id);
		return -EADDRNOTAVAIL;
	}

	err = ipc_queue_push(&pool->bd_queue, &bd);
	if (err != 0) {
		shm_err("Unable to release buffer %d from pool %d from channel %d with address %p\n",
				bd.buf_id, bd.pool_id, chan_id, buf);
		return err;
	}

	/* Mark buffer descriptor as free */
	pool->rx_buffer_is_free[bd.buf_id] = true;

	shm_dbg("ch %d: pool %d: released buffer %d with address %p\n",
			chan_id, bd.pool_id, bd.buf_id, buf);
	return 0;
}

int8_t ipc_shm_tx(const uint8_t instance, int chan_id, void *buf, uint32_t size)
{
	struct ipc_managed_channel *chan;
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	int err;

	/* check if instance is used */
	if (ipc_shm_is_remote_ready(instance) != 0) {
		return -EINVAL;
	}

	chan = get_managed_chan(instance, chan_id);
	if ((chan == NULL) || (buf == NULL) || (size == 0u)
			|| (ipc_check_mchan_integrity(chan) != 0))
		return -EINVAL;

	/* Find the pool that owns the buffer */
	bd.pool_id = find_pool_for_buf(chan,
			(uintptr_t)buf, IPC_BUFFER_FROM_LOCAL);
	if (bd.pool_id == -EINVAL) {
		shm_err("Buffer address %p doesn't belong to channel %d\n",
				buf, chan_id);
		return -EINVAL;
	}

	pool = &chan->pools[bd.pool_id];
	bd.buf_id = (uint16_t)(((uintptr_t)buf - pool->local_pool_addr)
			/ pool->buf_size);
	bd.data_size = (uint32_t)size;

	if (true == pool->tx_buffer_is_free[bd.buf_id]) {
		shm_err("Buffer %d from pool %d is already in use\n", bd.buf_id, bd.pool_id);
		return -EADDRNOTAVAIL;
	}

	/* push buffer descriptor in queue */
	err = ipc_queue_push(&chan->bd_queue, &bd);
	if (err != 0) {
		shm_err("Unable to push buffer descriptor in channel queue\n");
		return err;
	}

	/* Mark buffer descriptor as free */
	pool->tx_buffer_is_free[bd.buf_id] = true;

	/* notify remote that data is available */
	ipc_hw_irq_notify(instance);

	return 0;
}

void *ipc_shm_unmanaged_acquire(const uint8_t instance, uint8_t chan_id)
{
	struct ipc_unmanaged_channel *chan = NULL;

	/* check if instance is used */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED) {
		return NULL;
	}

	chan = get_unmanaged_chan(instance, chan_id);
	if ((chan == NULL)
			|| (ipc_check_uchan_local_integrity(chan) != 0))
		return NULL;

	/* for unmanaged channels return entire channel memory */
	if (chan->detached == false)
		return (void *)chan->local_mem->mem;

	return (void *)chan->detached_virt_addr;
}

int8_t ipc_shm_unmanaged_tx(const uint8_t instance, uint8_t chan_id)
{
	struct ipc_unmanaged_channel *chan = NULL;

	/* check if instance is used */
	if (ipc_shm_is_remote_ready(instance) != 0)
		return -EINVAL;

	chan = get_unmanaged_chan(instance, chan_id);
	if ((chan == NULL)
			|| (ipc_check_uchan_integrity(chan) != 0))
		return -EINVAL;

	/* bump Tx counter */
	chan->local_mem->tx_count++;

	/* notify remote that data is available */
	ipc_hw_irq_notify(instance);

	return 0;
}

int8_t ipc_shm_is_remote_ready(const uint8_t instance)
{
	struct ipc_shm_global *remote_global;

	/* check if instance is used */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED) {
		return -EINVAL;
	}

	/* global data of remote at beginning of remote shared memory */
	remote_global = (struct ipc_shm_global *)ipc_os_get_remote_shm(
			instance);

	if (remote_global->state != (uint64_t)IPC_SHM_STATE_READY)
		return -EAGAIN;

	return 0;
}

int8_t ipc_shm_poll_channels(const uint8_t instance)
{
	struct ipc_shm_global *remote_global;

	/* check if instance is used */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED) {
		return -EINVAL;
	}

	/* global data of remote at beginning of remote shared memory */
	remote_global = (struct ipc_shm_global *)ipc_os_get_remote_shm(
			instance);

	/* check if remote is ready before polling */
	if (remote_global->state != (uint64_t)IPC_SHM_STATE_READY)
		return -EAGAIN;

	return ipc_os_poll_channels(instance);
}

static int8_t ipc_set_scmi_channel_status(const uint8_t instance,
	const uint8_t chan_id, enum ipc_scmi_channel_status_type status)
{
	const struct ipc_scmi_channel *chan;
	volatile uint32_t *scmi_buf = NULL;

	chan = get_scmi_chan(instance, chan_id);
	if (chan == NULL)
		return -EINVAL;

	scmi_buf = (volatile uint32_t *)chan->scmi_virt_addr;
	if (scmi_buf == NULL)
		return -EINVAL;

	/* set the scmi channel status */
	scmi_buf[IPC_SCMI_CHANNEL_STATUS_IDX] = (uint32_t)status & 0x1U;

	return 0;
}

int8_t ipc_shm_free_scmi_channel(const uint8_t instance, const uint8_t chan_id)
{
	/* Free the scmi and return free status */
	return ipc_set_scmi_channel_status(instance, chan_id,
		IPC_SCMI_CHANNEL_FREE);
}

int8_t ipc_shm_scmi_tx(const uint8_t instance, uint8_t chan_id)
{
	/*
	 * Ideally channel must be free by now unless OS timeout last
	 * request and platform continued to process the same, wait
	 * until it releases the shared memory, otherwise we may endup
	 * overwriting its response with new message payload or vice-versa
	 */
	if (0 != ipc_set_scmi_channel_status(
			instance, chan_id, IPC_SCMI_CHANNEL_BUSY))
		return -EINVAL;

	ipc_hw_irq_notify(instance);

	return 0;
}

static enum ipc_scmi_channel_status_type ipc_get_scmi_channel_status(
	const uint8_t instance, const uint8_t chan_id)
{
	enum ipc_scmi_channel_status_type status = IPC_SCMI_CHANNEL_BUSY;
	const struct ipc_scmi_channel *chan = get_scmi_chan(instance, chan_id);
	volatile uint32_t *scmi_buf = NULL;

	scmi_buf = (volatile uint32_t *)chan->scmi_virt_addr;
	if (scmi_buf != NULL) {
		status = (enum ipc_scmi_channel_status_type)(
			scmi_buf[IPC_SCMI_CHANNEL_STATUS_IDX] & 0x1);
	}

	return status;
}

int8_t ipc_shm_is_channel_not_empty(const uint8_t instance, uint8_t chan_id,
	bool *not_empty)
{
	struct ipc_shm_channel *chan = get_channel(instance, chan_id);
	struct ipc_managed_channel *mchan;
	struct ipc_unmanaged_channel *uchan;
	uint32_t remote_tx_count;
	enum ipc_scmi_channel_status_type status = IPC_SCMI_CHANNEL_FREE;

	if ((NULL == not_empty) || (NULL == chan)
			|| (IPC_SHM_INSTANCE_USED
				!= ipc_instance_is_free(instance)))
		return -EINVAL;
	/* Initialize *not_empty with false since it might be mistakenly passed
	 * as true by the API caller. This would lead to it not properly being
	 * set false by this API in case no if-else branch is valid
	 */
	*not_empty = false;
	mchan = &chan->ch.mng;
	uchan = &chan->ch.umng;


	/* Get status of scmi channel */
	if (IPC_SHM_SCMI == chan->type) {
		status = ipc_get_scmi_channel_status(instance, chan_id);
		if (status == IPC_SCMI_CHANNEL_BUSY)
			*not_empty = true;
		return 0;
	}

	if (IPC_SHM_UNMANAGED == chan->type) {
		/* call unmanaged Rx callback if channel Tx counter changed */
		if (0 != ipc_check_uchan_integrity(uchan))
			return -EINVAL;

		remote_tx_count = uchan->remote_mem->tx_count;

		/* call Rx cb if remote Tx counter changed */
		if (remote_tx_count != uchan->local_mem->remote_tx_count)
			*not_empty = true;

		return 0;
	}

	return ipc_queue_not_empty(&mchan->bd_queue, not_empty);
}

int8_t ipc_shm_get_message_from_channel(uint8_t instance, uint8_t chan_id,
	void **msg_buffer)
{
	struct ipc_shm_channel *chan = get_channel(instance, chan_id);
	struct ipc_managed_channel *mchan;
	struct ipc_unmanaged_channel *uchan;
	struct ipc_scmi_channel *scmi_chan;
	struct ipc_shm_pool *pool;
	uint32_t remote_tx_count;
	enum ipc_scmi_channel_status_type status = IPC_SCMI_CHANNEL_FREE;
	int8_t result = 0;
	struct ipc_shm_bd bd;
	uintptr_t buf_addr;
	uint32_t buf_offset;

	if ((chan == NULL) || (IPC_SHM_INSTANCE_USED
			!= ipc_instance_is_free(instance)))
		return -EINVAL;

	mchan = &chan->ch.mng;
	uchan = &chan->ch.umng;
	scmi_chan = &chan->ch.scmi;

	/* Get scmi virtual address */
	if (chan->type == IPC_SHM_SCMI) {
		status = ipc_get_scmi_channel_status(instance, chan_id);
		if (IPC_SCMI_CHANNEL_BUSY != status)
			return -EINVAL;

		*msg_buffer = (void *)scmi_chan->scmi_virt_addr;

		return 0;
	}

	if (chan->type == IPC_SHM_UNMANAGED) {
		/* Update buffer and counter if channel Tx counter changed */
		if (0 != ipc_check_uchan_integrity(uchan))
			return -EINVAL;

		remote_tx_count = uchan->remote_mem->tx_count;

		/* Check if there are pending message */
		if (remote_tx_count == uchan->local_mem->remote_tx_count)
			return -EINVAL;

		/* save new remote Tx counter and update the buffer*/
		uchan->local_mem->remote_tx_count = remote_tx_count;

		*msg_buffer = (void *)ipc_unmanaged_buf_addr(uchan);
		if (*msg_buffer == NULL)
			return -EINVAL;

		return 0;
	}

	/* managed channels: process incoming BDs */
	result = ipc_queue_pop(&mchan->bd_queue, (uint8_t *) &bd);

	if (result != 0)
		return -EINVAL;

	pool = &mchan->pools[bd.pool_id];
	buf_offset = pool->buf_size * bd.buf_id;
	buf_addr = pool->remote_pool_addr + buf_offset;

	/* check if buf_addr is valid */
	if ((buf_addr >= ipc_os_get_remote_shm(instance)) &&
			((buf_addr + pool->buf_size)
				<= (ipc_os_get_remote_shm(instance) +
				ipc_shm_priv_data[instance].shm_size))) {
		*msg_buffer = (void *)buf_addr;
		pool->rx_buffer_is_free[bd.buf_id] = false;
		return 0;
	}

	return -EINVAL;
}

int8_t ipc_shm_get_channel_type(const uint8_t instance, uint8_t chan_id,
	enum ipc_shm_channel_type *chan_type)
{
	const struct ipc_shm_channel *chan = get_channel(instance, chan_id);

	if ((chan_type == NULL) || (chan == NULL)
			|| (IPC_SHM_INSTANCE_USED
				!= ipc_instance_is_free(instance)))
		return -EINVAL;

	/* read the channel type for the dedicated channel */
	*chan_type = chan->type;

	return 0;
}
