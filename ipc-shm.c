/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018-2023 NXP
 */
#include "ipc-os.h"
#include "ipc-hw.h"
#include "ipc-queue.h"
#include "ipc-shm.h"

#define ipc_max(x, y) (((x) > (y)) ? (x) : (y))

/* magic number to indicate the driver is initialized */
#define IPC_SHM_STATE_READY 0x4950434646435049ULL
#define IPC_SHM_STATE_CLEAR 0u

/**
 * enum ipc_shm_instance_state - used for IPC instance status
 * @IPC_SHM_INSTANCE_USED:	instance is used
 * @IPC_SHM_INSTANCE_FREE:	instance is free and can be used
 * @IPC_SHM_INSTANCE_ERROR: there are some errors
 */
enum ipc_shm_instance_state {
	IPC_SHM_INSTANCE_USED = 0u,
	IPC_SHM_INSTANCE_FREE = 1u,
	IPC_SHM_INSTANCE_ERROR = 2u,
};

/**
 * struct ipc_shm_bd - buffer descriptor (store buffer location and data size)
 * @pool_id:	index of buffer pool
 * @buf_id:	index of buffer from buffer pool
 * @data_size:	size of data written in buffer
 */
struct ipc_shm_bd {
	int16_t pool_id;
	uint16_t buf_id;
	uint32_t data_size;
};

/**
 * struct ipc_shm_pool - buffer pool private data
 * @num_bufs:		number of buffers in pool
 * @buf_size:		size of buffers
 * @shm_size:		size of shared memory mapped by this pool (queue + bufs)
 * @local_pool_addr:	address of local buffer pool
 * @remote_pool_addr:	address of remote buffer pool
 * @bd_queue:		queue containing BDs of free buffers
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
};

/**
 * struct ipc_managed_channel - managed channel private data
 * @bd_queue:	queue containing BDs of sent/received buffers
 * @num_pools:	number of buffer pools
 * @pools:	buffer pools private data
 * @rx_cb:	receive callback
 * @cb_arg:	optional receive callback argument
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
	int num_pools;
	struct ipc_shm_pool pools[IPC_SHM_MAX_POOLS];
	void (*rx_cb)(void *cb_arg, const uint8_t instance, int chan_id,
			void *buf, size_t size);
	void *cb_arg;
};

/**
 * struct ipc_channel_umem - unmanaged channel memory control structure
 * @tx_count:	local channel Tx counter (it wraps around at max uint32_t)
 * @reserved:	reserved field for memory alignment
 * @mem:	local channel unmanaged memory buffer
 *
 * tx_count is used by remote peer in Rx intr handler to determine if this
 * channel had a Tx operation and decide whether to call the app Rx callback.
 */
struct ipc_channel_umem {
	volatile uint32_t tx_count;
	uint8_t reserved[4];
	uint8_t mem[];
};

/**
 * struct ipc_unmanaged_channel - unmanaged channel private data
 * @size:		unmanaged channel memory size requested by app
 * @local_umem:		local channel unmanaged memory
 * @remote_umem:	remote channel unmanaged memory
 * @remote_tx_count:	copy of remote Tx counter
 * @rx_cb:		receive callback
 * @cb_arg:		optional receive callback argument
 */
struct ipc_unmanaged_channel {
	uint32_t size;
	struct ipc_channel_umem *local_mem;
	struct ipc_channel_umem *remote_mem;
	uint32_t remote_tx_count;
	void (*rx_cb)(void *cb_arg, const uint8_t instance, int chan_id,
			void *buf);
	void *cb_arg;
};

/**
 * struct ipc_shm_channel - ipc channel private data
 * @id:		channel id
 * @type:	channel type (see ipc_shm_channel_type)
 * @ch:		managed/unmanaged channel private data
 */
struct ipc_shm_channel {
	int id;
	enum ipc_shm_channel_type type;
	union {
		struct ipc_managed_channel mng;
		struct ipc_unmanaged_channel umng;
	} ch;
};

/**
 * struct ipc_shm_global - ipc shm global data shared with remote
 * @state:		state to indicate whether local is initialized
 *
 * Global data is located at beginning of local/remote shared memory so the size
 * of this struct should chosen so that memory alignment is preserved.
 */
struct ipc_shm_global {
	uint64_t state;
};

/**
 * struct ipc_shm_priv - ipc shm private data
 * @shm_size:		local/remote shared memory size
 * @num_channels:	number of shared memory channels
 * @channels:		ipc channels private data
 * @global:		local global data shared with remote
 */
struct ipc_shm_priv {
	uint32_t shm_size;
	int num_channels;
	struct ipc_shm_channel channels[IPC_SHM_MAX_CHANNELS];
	struct ipc_shm_global *global;
};

/* ipc shm private data */
static struct ipc_shm_priv ipc_shm_priv_data[IPC_SHM_MAX_INSTANCES];

/* get channel without validation (used in internal functions only) */
static inline struct ipc_shm_channel *get_channel_priv(const uint8_t instance,
		int chan_id)
{
	return &ipc_shm_priv_data[instance].channels[chan_id];
}

/* get channel with validation (can be used in API functions) */
static inline struct ipc_shm_channel *get_channel(const uint8_t instance,
		int chan_id)
{
	if ((chan_id < 0)
		|| (chan_id >= ipc_shm_priv_data[instance].num_channels)) {
		shm_err("Channel id outside valid range: 0 - %d\n",
				ipc_shm_priv_data[instance].num_channels);
		return NULL;
	}

	return get_channel_priv(instance, chan_id);
}

/* get managed channel with validation */
static inline struct ipc_managed_channel *get_managed_chan(
		const uint8_t instance, int chan_id)
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
		const uint8_t instance, int chan_id)
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

/**
 * ipc_channel_rx() - handle Rx for a single channel
 * @instance:	instance id
 * @chan_id:	channel id
 * @budget:	available work budget (number of messages to be processed)
 *
 * Return:	work done
 */
static int ipc_channel_rx(const uint8_t instance, int chan_id, int budget)
{
	struct ipc_shm_channel *chan = get_channel_priv(instance, chan_id);
	struct ipc_managed_channel *mchan = &chan->ch.mng;
	struct ipc_unmanaged_channel *uchan = &chan->ch.umng;
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	uintptr_t buf_addr;
	uint32_t remote_tx_count;
	int err;
	int work = 0;

	/* unmanaged channels: call Rx callback if channel Tx counter changed */
	if (chan->type == IPC_SHM_UNMANAGED) {
		remote_tx_count = uchan->remote_mem->tx_count;

		/* call Rx cb if remote Tx counter changed */
		if (remote_tx_count != uchan->remote_tx_count) {

			/* save new remote Tx counter */
			uchan->remote_tx_count = remote_tx_count;

			uchan->rx_cb(uchan->cb_arg, instance, chan->id,
					(void *)uchan->remote_mem->mem);

			return budget;
		}
		return 0;
	}

	/* managed channels: process incoming BDs in the limit of budget */
	while (work < budget) {
		err = ipc_queue_pop(&mchan->bd_queue, &bd);
		if (err != 0) {
			break;
		}
		pool = &mchan->pools[bd.pool_id];
		buf_addr = pool->remote_pool_addr +
			(bd.buf_id * pool->buf_size);

		mchan->rx_cb(mchan->cb_arg, instance, chan->id,
				(void *)buf_addr, bd.data_size);
		work++;
	}

	return work;
}

/**
 * ipc_instance_is_free() - determine if the instance is used or not
 * @instance:	instance id
 *
 * This function return the state of instance.
 *
 * Return: IPC_SHM_INSTANCE_FREE if instance is free,
 *     IPC_SHM_INSTANCE_USED otherwise or
 *     IPC_SHM_INSTANCE_ERROR if there is errors
 */
static uint8_t ipc_instance_is_free(const uint8_t instance)
{
	if (instance >= IPC_SHM_MAX_INSTANCES)
		return IPC_SHM_INSTANCE_ERROR;

	if (ipc_shm_priv_data[instance].global == NULL)
		return IPC_SHM_INSTANCE_FREE;
	if (ipc_shm_priv_data[instance].global->state == IPC_SHM_STATE_CLEAR)
		return IPC_SHM_INSTANCE_FREE;

	return IPC_SHM_INSTANCE_USED;
}

/**
 * ipc_shm_rx() - shm Rx handler, called from softirq
 * @instance:	instance id
 * @budget:	available work budget (number of messages to be processed)
 *
 * This function handles all channels using a fair handling algorithm: all
 * channels are treated equally and no channel is starving.
 *
 * Return:	work done
 */
static int ipc_shm_rx(const uint8_t instance, int budget)
{
	int num_chans = ipc_shm_priv_data[instance].num_channels;
	int chan_budget, chan_work;
	int more_work = 1;
	int work = 0;
	int i;

	/* fair channel handling algorithm */
	while ((work < budget) && (more_work > 0)) {
		chan_budget = ipc_max(((budget - work) / num_chans), 1);
		more_work = 0;

		for (i = 0; i < num_chans; i++) {
			chan_work = ipc_channel_rx(instance, i, chan_budget);
			work += chan_work;

			if (chan_work == chan_budget)
				more_work = 1;
		}
	}

	return work;
}

/**
 * ipc_buf_pool_init() - init buffer pool
 * @instance:	instance id
 * @chan_id:	channel index
 * @pool_id:	pool index in channel
 * @local_shm:	local pool shared memory address
 * @remote_shm: remote pool shared memory address
 * @cfg:	channel configuration parameters
 *
 * To ensure freedom from interference when writing in shared memory, only one
 * IPC is allowed to write in a BD ring, so the IPC that pushes BDs in the
 * release ring at the end of an Rx operation must also initialize it. That's
 * why local IPC initializes bd_queue with BDs pointing to remote free buffers.
 * Since the shared memory configuration is symmetric and remote base address
 * is known, local IPC can compute the remote BD info.
 *
 * Return: 0 for success, error code otherwise
 */
static int ipc_buf_pool_init(const uint8_t instance, int chan_id, int pool_id,
		uintptr_t local_shm, uintptr_t remote_shm,
		const struct ipc_shm_pool_cfg *cfg)
{
	struct ipc_managed_channel *chan = get_managed_chan(instance, chan_id);
	struct ipc_shm_pool *pool = &chan->pools[pool_id];
	struct ipc_shm_bd bd;
	uint32_t queue_mem_size;
	uint16_t i;
	int err;

	if (cfg->num_bufs > IPC_SHM_MAX_BUFS_PER_POOL) {
		shm_err("Too many buffers configured in pool. "
				"Increase IPC_SHM_MAX_BUFS_PER_POOL if needed\n");
		return -EINVAL;
	}

	pool->num_bufs = cfg->num_bufs;
	pool->buf_size = cfg->buf_size;

	/* init pool bd_queue with push ring mapped at the start of local
	 * pool shm and pop ring mapped at start of remote pool shm
	 */
	err = ipc_queue_init(&pool->bd_queue, pool->num_bufs,
		(uint16_t)sizeof(struct ipc_shm_bd), local_shm, remote_shm);
	if (err != 0)
		return err;

	/* init local/remote buffer pool addrs */
	queue_mem_size = ipc_queue_mem_size(&pool->bd_queue);

	/* init actual local buffer pool addr */
	pool->local_pool_addr = local_shm + queue_mem_size;

	/* init actual remote buffer pool addr */
	pool->remote_pool_addr = remote_shm + queue_mem_size;

	pool->shm_size = queue_mem_size + (cfg->buf_size * cfg->num_bufs);

	/* check if pool fits into shared memory */
	if ((local_shm + pool->shm_size)
			> (ipc_os_get_local_shm(instance)
				+ ipc_shm_priv_data[instance].shm_size)) {
		shm_err("Not enough shared memory for pool %d from channel %d\n",
				pool_id, chan_id);
		return -ENOMEM;
	}

	/* populate bd_queue with free BDs from remote pool */
	for (i = 0; i < pool->num_bufs; i++) {
		bd.pool_id = (int16_t) pool_id;
		bd.buf_id = i;
		bd.data_size = 0;

		err = ipc_queue_push(&pool->bd_queue, &bd);
		if (err != 0) {
			shm_err("Unable to init queue with free buffer descriptors "
					"for pool %d of channel %d\n",
					pool_id, chan_id);
			return err;
		}
	}

	shm_dbg("ipc shm pool %d of chan %d initialized\n", pool_id, chan_id);
	return 0;
}

static int managed_channel_init(const uint8_t instance, int chan_id,
		uintptr_t local_shm, uintptr_t remote_shm,
		const struct ipc_shm_managed_cfg *cfg)
{
	struct ipc_managed_channel *chan = get_managed_chan(instance, chan_id);
	const struct ipc_shm_pool_cfg *pool_cfg;
	uintptr_t local_pool_shm;
	uintptr_t remote_pool_shm;
	uint32_t queue_mem_size;
	uint32_t prev_buf_size = 0;
	uint16_t total_bufs = 0;
	int err, i;

	if (cfg->rx_cb == NULL) {
		shm_err("Receive callback not specified\n");
		return -EINVAL;
	}

	if (cfg->pools == NULL) {
		shm_err("NULL buffer pool configuration argument\n");
		return -EINVAL;
	}

	if ((cfg->num_pools < 1) ||
		(cfg->num_pools > IPC_SHM_MAX_POOLS)) {
		shm_err("Number of pools must be between 1 and %d\n",
				IPC_SHM_MAX_POOLS);
		return -EINVAL;
	}

	/* save managed channel parameters */
	chan->rx_cb = cfg->rx_cb;
	chan->cb_arg = cfg->cb_arg;
	chan->num_pools = cfg->num_pools;

	/* check that pools are sorted in ascending order by buf size
	 * and count total number of buffers from all pools
	 */
	for (i = 0; i < chan->num_pools; i++) {
		pool_cfg = &cfg->pools[i];

		if (pool_cfg->buf_size < prev_buf_size) {
			shm_dbg("Pools must be sorted in ascending order by buffer size\n");
			return -EINVAL;
		}
		prev_buf_size = pool_cfg->buf_size;

		total_bufs += pool_cfg->num_bufs;
	}

	/* init channel bd_queue with push ring mapped at the start of local
	 * channel shm and pop ring mapped at start of remote channel shm
	 */
	err = ipc_queue_init(&chan->bd_queue, total_bufs,
			     (uint16_t)sizeof(struct ipc_shm_bd),
			     local_shm, remote_shm);
	if (err != 0)
		return err;

	/* init&map buffer pools after channel bd_queue */
	queue_mem_size = ipc_queue_mem_size(&chan->bd_queue);
	local_pool_shm = local_shm + queue_mem_size;
	remote_pool_shm = remote_shm + queue_mem_size;

	/* check if pool fits into shared memory */
	if ((local_pool_shm)
			> (ipc_os_get_local_shm(instance)
				+ ipc_shm_priv_data[instance].shm_size)) {
		shm_err("Not enough shared memory for channel %d\n",
				chan_id);
		return -ENOMEM;
	}

	for (i = 0; i < chan->num_pools; i++) {
		err = ipc_buf_pool_init(instance, chan_id, i, local_pool_shm,
				remote_pool_shm, &cfg->pools[i]);
		if (err != 0)
			return err;

		/* compute next pool local/remote shm base address */
		local_pool_shm += chan->pools[i].shm_size;
		remote_pool_shm += chan->pools[i].shm_size;
	}

	return 0;
}

static int unmanaged_channel_init(const uint8_t instance, int chan_id,
		uintptr_t local_shm, uintptr_t remote_shm,
		const struct ipc_shm_unmanaged_cfg *cfg)
{
	struct ipc_unmanaged_channel *chan = get_unmanaged_chan(instance,
			chan_id);

	if (cfg->rx_cb == NULL) {
		shm_err("Receive callback not specified\n");
		return -EINVAL;
	}

	/* save unmanaged channel parameters */
	chan->size = cfg->size;
	chan->rx_cb = cfg->rx_cb;
	chan->cb_arg = cfg->cb_arg;

	chan->local_mem = (struct ipc_channel_umem *) local_shm;
	chan->remote_mem = (struct ipc_channel_umem *) remote_shm;

	chan->local_mem->tx_count = 0;
	chan->remote_tx_count = 0;

	return 0;
}

/**
 * ipc_shm_channel_init() - initialize shared memory IPC channel
 * @instance:	instance id
 * @chan_id:	channel index
 * @local_shm:	local channel shared memory address
 * @remote_shm: remote channel shared memory address
 * @cfg:	channel configuration parameters
 *
 * Return: 0 for success, error code otherwise
 */
static int ipc_shm_channel_init(const uint8_t instance, int chan_id,
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
	} else {
		shm_err("Invalid channel type\n");
		err = -EINVAL;
	}
	if (err != 0)
		return err;

	shm_dbg("ipc shm channel %d initialized\n", chan_id);
	return 0;
}

/* Get channel local mapped memory size */
static uint32_t get_chan_memmap_size(const uint8_t instance, int chan_id)
{
	struct ipc_shm_channel *chan = get_channel_priv(instance, chan_id);
	struct ipc_managed_channel *mchan;
	uint32_t size = 0;
	int i;

	/* unmanaged channels: control structure size + channel memory size */
	if (chan->type == IPC_SHM_UNMANAGED) {
		return (uint32_t)(sizeof(struct ipc_channel_umem) +
			chan->ch.umng.size);
	}

	/* managed channels: size of BD queue + size of buf pools */
	mchan = get_managed_chan(instance, chan_id);
	size = ipc_queue_mem_size(&mchan->bd_queue);
	for (i = 0; i < mchan->num_pools; i++) {
		size += mchan->pools[i].shm_size;
	}

	return size;
}

/* Initialize only one instance shared memory device */
static int ipc_shm_init_instance(uint8_t instance,
	const struct ipc_shm_cfg *cfg)
{
	uintptr_t local_chan_shm;
	uintptr_t remote_chan_shm;
	uintptr_t local_shm;
	uint32_t chan_size;
	size_t chan_offset;
	int err, i;

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
	if (err != 0)
		goto err_free_hw;

	/* global data stored at beginning of local shared memory */
	local_shm = ipc_os_get_local_shm(instance);
	ipc_shm_priv_data[instance].global = (struct ipc_shm_global *)local_shm;

	/* init channels */
	chan_offset = sizeof(struct ipc_shm_global);
	local_chan_shm = local_shm + (uintptr_t) chan_offset;
	remote_chan_shm = ipc_os_get_remote_shm(instance)
			+ (uintptr_t) chan_offset;
	shm_dbg("initializing channels...\n");
	for (i = 0; i < ipc_shm_priv_data[instance].num_channels; i++) {
		err = ipc_shm_channel_init(instance, i, local_chan_shm,
				remote_chan_shm, &cfg->channels[i]);
		if (err != 0)
			goto err_free_os;

		/* compute next channel local/remote shm base address */
		chan_size = get_chan_memmap_size(instance, i);
		local_chan_shm += chan_size;
		remote_chan_shm += chan_size;
	}

	/* enable interrupt notifications */
	ipc_hw_irq_enable(instance);

	ipc_shm_priv_data[instance].global->state = IPC_SHM_STATE_READY;
	shm_dbg("ipc shm initialized\n");

	return 0;

err_free_os:
	ipc_os_free(instance);
err_free_hw:
	ipc_hw_free(instance);
	return err;
}

void ipc_shm_free(void)
{
	uint8_t i = 0;

	/* check if instance must be free */
	for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
		if (ipc_instance_is_free(i) == IPC_SHM_INSTANCE_USED) {

			/* reset state */
			ipc_shm_priv_data[i].global->state =
				IPC_SHM_STATE_CLEAR;
			ipc_shm_priv_data[i].global = NULL;

			/* disable hardirq */
			ipc_hw_irq_disable(i);

			ipc_os_free(i);
			ipc_hw_free(i);
		}
	}

	shm_dbg("ipc shm released\n");
}

void *ipc_shm_acquire_buf(const uint8_t instance, int chan_id, size_t size)
{
	struct ipc_managed_channel *chan;
	struct ipc_shm_pool *pool = NULL;
	struct ipc_shm_bd bd = {.pool_id = 0, .buf_id = 0u, .data_size = 0u};
	uintptr_t buf_addr;
	int pool_id;

	/* check if instance is valid */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED) {
		return NULL;
	}

	chan = get_managed_chan(instance, chan_id);
	if ((chan == NULL) || (size == 0u))
		return NULL;

	/* find first non-empty pool that accommodates the requested size */
	for (pool_id = 0; pool_id < chan->num_pools; pool_id++) {
		pool = &chan->pools[pool_id];

		/* check if pool buf size covers the requested size */
		if (size > pool->buf_size)
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

	shm_dbg("ch %d: pool %d: acquired buffer %d with address %lx\n",
			chan_id, pool_id, bd.buf_id, buf_addr);
	return (void *) buf_addr;
}

int ipc_shm_init(const struct ipc_shm_instances_cfg *cfg)
{
	uint8_t i = 0;
	int err = 0;

	if (cfg == NULL) {
		shm_err("NULL argument\n");
		return -EINVAL;
	}

	if ((cfg->num_instances > IPC_SHM_MAX_INSTANCES)
			|| (cfg->num_instances == 0u))
		return -EINVAL;

	/* init all instances */
	for (i = 0; i < cfg->num_instances; i++) {
		err = ipc_shm_init_instance(i, &cfg->shm_cfg[i]);
		if (err != 0)
			return err;
	}
	return 0;
}

/**
 * find_pool_for_buf() - Find the pool that owns the specified buffer.
 * @chan:	managed channel pointer
 * @buf:	buffer pointer
 * @remote:	flag telling if buffer is from remote OS
 *
 * Return: pool index on success, -1 otherwise
 */
static int16_t find_pool_for_buf(struct ipc_managed_channel *chan,
		uintptr_t buf, int remote)
{
	struct ipc_shm_pool *pool;
	uintptr_t addr;
	int16_t pool_id;
	uint32_t pool_size;

	for (pool_id = 0; pool_id < chan->num_pools; pool_id++) {
		pool = &chan->pools[pool_id];

		addr = (remote == 1) ?
				pool->remote_pool_addr : pool->local_pool_addr;
		pool_size = pool->num_bufs * pool->buf_size;

		if ((buf >= addr) && (buf < (addr + pool_size)))
			return pool_id;
	}

	return -1;
}

int ipc_shm_release_buf(const uint8_t instance, int chan_id, const void *buf)
{
	struct ipc_managed_channel *chan;
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	int err;

	/* check if instance is valid */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED) {
		return -EINVAL;
	}

	chan = get_managed_chan(instance, chan_id);
	if ((chan == NULL) || (buf == NULL))
		return -EINVAL;

	/* Find the pool that owns the buffer */
	bd.pool_id = find_pool_for_buf(chan, (uintptr_t) buf, 1);
	if (bd.pool_id == -1) {
		shm_err("Buffer address %p doesn't belong to channel %d\n",
				buf, chan_id);
		return -EINVAL;
	}

	pool = &chan->pools[bd.pool_id];
	bd.buf_id = (uint16_t)(((uintptr_t)buf - pool->remote_pool_addr) /
			pool->buf_size);
	bd.data_size = 0; /* reset size of written data in buffer */

	err = ipc_queue_push(&pool->bd_queue, &bd);
	if (err != 0) {
		shm_err("Unable to release buffer %d from pool %d from channel %d with address %p\n",
				bd.buf_id, bd.pool_id, chan_id, buf);
		return err;
	}

	shm_dbg("ch %d: pool %d: released buffer %d with address %p\n",
			chan_id, bd.pool_id, bd.buf_id, buf);
	return 0;
}

int ipc_shm_tx(const uint8_t instance, int chan_id, void *buf, size_t size)
{
	struct ipc_managed_channel *chan;
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	int err;

	/* check if instance is used */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED) {
		return -EINVAL;
	}

	chan = get_managed_chan(instance, chan_id);
	if ((chan == NULL) || (buf == NULL) || (size == 0u))
		return -EINVAL;

	/* Find the pool that owns the buffer */
	bd.pool_id = find_pool_for_buf(chan, (uintptr_t) buf, 0);
	if (bd.pool_id == -1) {
		shm_err("Buffer address %p doesn't belong to channel %d\n",
				buf, chan_id);
		return -EINVAL;
	}

	pool = &chan->pools[bd.pool_id];
	bd.buf_id = (uint16_t) (((uintptr_t) buf - pool->local_pool_addr)
			/ pool->buf_size);
	bd.data_size = (uint32_t) size;

	/* push buffer descriptor in queue */
	err = ipc_queue_push(&chan->bd_queue, &bd);
	if (err != 0) {
		shm_err("Unable to push buffer descriptor in channel queue\n");
		return err;
	}

	/* notify remote that data is available */
	ipc_hw_irq_notify(instance);

	return 0;
}

void *ipc_shm_unmanaged_acquire(const uint8_t instance, int chan_id)
{
	struct ipc_unmanaged_channel *chan = NULL;

	/* check if instance is used */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED) {
		return NULL;
	}

	chan = get_unmanaged_chan(instance, chan_id);
	if (chan == NULL)
		return NULL;

	/* for unmanaged channels return entire channel memory */
	return (void *) chan->local_mem->mem;
}

int ipc_shm_unmanaged_tx(const uint8_t instance, int chan_id)
{
	struct ipc_unmanaged_channel *chan = NULL;

	/* check if instance is used */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED) {
		return -EINVAL;
	}

	chan = get_unmanaged_chan(instance, chan_id);
	if (chan == NULL)
		return -EINVAL;

	/* bump Tx counter */
	chan->local_mem->tx_count++;

	/* notify remote that data is available */
	ipc_hw_irq_notify(instance);

	return 0;
}

int ipc_shm_is_remote_ready(const uint8_t instance)
{
	struct ipc_shm_global *remote_global;

	/* check if instance is used */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED) {
		return -EINVAL;
	}

	/* global data of remote at beginning of remote shared memory */
	remote_global = (struct ipc_shm_global *)ipc_os_get_remote_shm(
			instance);

	if (remote_global->state != IPC_SHM_STATE_READY)
		return -EAGAIN;

	return 0;
}

int ipc_shm_poll_channels(const uint8_t instance)
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
	if (remote_global->state != IPC_SHM_STATE_READY)
		return -EAGAIN;

	return ipc_os_poll_channels(instance);
}
