/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018 NXP
 */
#include "ipc-os.h"
#include "ipc-hw.h"
#include "ipc-queue.h"

#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

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
 * @bd_queue:		queue containing BDs of free buffers
 * @local_pool_addr:	address of local buffer pool
 * @remote_pool_addr:	address of remote buffer pool
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
	struct ipc_queue bd_queue;
	uintptr_t local_pool_addr;
	uintptr_t remote_pool_addr;
};

/**
 * struct ipc_shm_channel - ipc channel private data
 * @id:		channel id
 * @type:	channel type (see ipc_shm_channel_type)
 * @ops:	channel Rx API callbacks
 * @bd_queue:	queue containing BDs of sent/received buffers
 * @pools:	buffer pools private data
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
struct ipc_shm_channel {
	int id;
	enum ipc_shm_channel_type type;
	struct ipc_shm_channel_ops ops;
	struct ipc_queue bd_queue;
	int num_pools;
	struct ipc_shm_pool pools[IPC_SHM_MAX_POOLS];
};

/**
 * struct ipc_shm_priv - ipc shm private data
 * @shm_size:		local/remote shared memory size
 * @num_channels:	number of shared memory channels
 * @channels:		ipc channels private data
 */
struct ipc_shm_priv {
	int shm_size;
	int num_channels;
	struct ipc_shm_channel channels[IPC_SHM_MAX_CHANNELS];
};

/* ipc shm private data */
static struct ipc_shm_priv ipc_shm_priv;

static inline struct ipc_shm_priv *get_ipc_priv(void)
{
	return &ipc_shm_priv;
}

/* get channel without id validation (to be used in internal functions) */
static inline struct ipc_shm_channel *__get_channel(int chan_id)
{
	return &ipc_shm_priv.channels[chan_id];
}

/* get channel with id validation (to be used in API functions) */
static inline struct ipc_shm_channel *get_channel(int chan_id)
{
	if (chan_id < 0 || chan_id >= get_ipc_priv()->num_channels) {
		shm_err("Channel id outside valid range: 0 - %d\n",
			get_ipc_priv()->num_channels);
		return NULL;
	}

	return __get_channel(chan_id);
}

/**
 * ipc_channel_rx() - handle Rx for a single channel
 * @chan_id:	channel id
 * @budget:	available work budget (number of messages to be processed)
 *
 * Return:	work done
 */
static int ipc_channel_rx(int chan_id, int budget)
{
	struct ipc_shm_channel *chan = __get_channel(chan_id);
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	uintptr_t buf_addr;
	int work = 0;

	while (work < budget &&
	       ipc_queue_pop(&chan->bd_queue, &bd) == 0) {

		pool = &chan->pools[bd.pool_id];
		buf_addr = pool->remote_pool_addr + bd.buf_id * pool->buf_size;

		chan->ops.rx_cb(chan->ops.cb_arg, chan->id,
				(void *)buf_addr, bd.data_size);
		work++;
	}

	return work;
}

/**
 * ipc_shm_rx() - shm Rx handler, called from softirq
 * @budget:	available work budget (number of messages to be processed)
 *
 * This function handles all channels using a fair handling algorithm: all
 * channels are treated equally and no channel is starving.
 *
 * Return:	work done
 */
static int ipc_shm_rx(int budget)
{
	int num_chans = get_ipc_priv()->num_channels;
	int chan_budget, chan_work;
	int more_work = 1;
	int work = 0;
	int i;

	/* fair channel handling algorithm */
	while (work < budget && more_work) {
		chan_budget = max((budget - work) / num_chans, 1);
		more_work = 0;

		for (i = 0; i < num_chans; i++) {
			chan_work = ipc_channel_rx(i, chan_budget);
			work += chan_work;

			if (chan_work == chan_budget)
				more_work = 1;
		}
	}

	return work;
}
/*
 * ipc_buf_pool_init() - init buffer pool
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
static int ipc_buf_pool_init(int chan_id, int pool_id,
			     uintptr_t local_shm, uintptr_t remote_shm,
			     const struct ipc_shm_pool_cfg *cfg)
{
	struct ipc_shm_priv *priv = get_ipc_priv();
	struct ipc_shm_channel *chan = __get_channel(chan_id);
	struct ipc_shm_pool *pool = &chan->pools[pool_id];
	struct ipc_shm_bd bd;
	int queue_mem_size;
	int i, err;

	if (!cfg) {
		shm_err("NULL buffer pool configuration argument\n");
		return -EINVAL;
	}

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
			     sizeof(struct ipc_shm_bd), local_shm, remote_shm);
	if (err)
		return err;

	/* init local/remote buffer pool addrs */
	queue_mem_size = ipc_queue_mem_size(&pool->bd_queue);

	/* init actual local buffer pool addr */
	pool->local_pool_addr = local_shm + queue_mem_size;

	/* init actual remote buffer pool addr */
	pool->remote_pool_addr = remote_shm + queue_mem_size;

	pool->shm_size = queue_mem_size + (cfg->buf_size * cfg->num_bufs);

	/* check if pool fits into shared memory */
	if (local_shm + pool->shm_size >
		ipc_os_get_local_shm() + priv->shm_size) {
		shm_err("Not enough shared memory for pool %d from channel %d\n",
			pool_id, chan_id);
		return -ENOMEM;
	}

	/* populate bd_queue with free BDs from remote pool */
	for (i = 0; i < pool->num_bufs; i++) {
		bd.pool_id = pool_id;
		bd.buf_id = i;
		bd.data_size = 0;

		err = ipc_queue_push(&pool->bd_queue, &bd);
		if (err) {
			shm_err("Unable to init queue with free buffer descriptors "
				"for pool %d of channel %d\n",
				pool_id, chan_id);
			return err;
		}
	}

	shm_dbg("ipc shm pool %d of chan %d initialized\n", pool_id, chan_id);
	return 0;
}

/**
 * ipc_shm_channel_init() - initialize shared memory IPC channel
 * @chan_id:	channel index
 * @local_shm:	local channel shared memory address
 * @remote_shm: remote channel shared memory address
 * @cfg:	channel configuration parameters
 *
 * Return: 0 for success, error code otherwise
 */
static int ipc_shm_channel_init(int chan_id,
				uintptr_t local_shm, uintptr_t remote_shm,
				const struct ipc_shm_channel_cfg *cfg)
{
	struct ipc_shm_channel *chan = __get_channel(chan_id);
	const struct ipc_shm_pool_cfg *pool_cfg;
	uintptr_t local_pool_shm;
	uintptr_t remote_pool_shm;
	int queue_mem_size;
	uint32_t prev_buf_size = 0;
	int total_bufs = 0;
	int err, i;

	if (!cfg) {
		shm_err("NULL channel configuration argument\n");
		return -EINVAL;
	}

	if (!cfg->ops->rx_cb) {
		shm_err("Receive callback not specified\n");
		return -EINVAL;
	}

	/* only managed channels are supported for now */
	if (cfg->type != SHM_CHANNEL_MANAGED) {
		shm_err("Channel type not supported\n");
		return -EOPNOTSUPP;
	}

	if (cfg->memory.managed.num_pools < 1 ||
	    cfg->memory.managed.num_pools > IPC_SHM_MAX_POOLS) {
		shm_err("Number of pools must be between 1 and %d\n",
			IPC_SHM_MAX_POOLS);
		return -EINVAL;
	}

	/* save cfg params */
	chan->type = cfg->type;
	chan->id = chan_id;
	chan->num_pools = cfg->memory.managed.num_pools;
	memcpy(&chan->ops, cfg->ops, sizeof(chan->ops));

	/* check that pools are sorted in ascending order by buf size
	 * and count total number of buffers from all pools
	 */
	for (i = 0; i < chan->num_pools; i++) {
		pool_cfg = &cfg->memory.managed.pools[i];

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
			     sizeof(struct ipc_shm_bd), local_shm, remote_shm);
	if (err)
		return err;

	/* init&map buffer pools after channel bd_queue */
	queue_mem_size = ipc_queue_mem_size(&chan->bd_queue);
	local_pool_shm = local_shm + queue_mem_size;
	remote_pool_shm = remote_shm + queue_mem_size;

	for (i = 0; i < chan->num_pools; i++) {
		err = ipc_buf_pool_init(chan->id, i,
					local_pool_shm, remote_pool_shm,
					&cfg->memory.managed.pools[i]);
		if (err)
			return err;

		/* compute next pool local/remote shm base address */
		local_pool_shm += chan->pools[i].shm_size;
		remote_pool_shm += chan->pools[i].shm_size;
	}

	shm_dbg("ipc shm channel %d initialized\n", chan_id);
	return 0;
}

/* Get channel memory size = bd_queue size + size of buffer pools */
static int get_chan_mem_size(int chan_id)
{
	struct ipc_shm_channel *chan = __get_channel(chan_id);
	int size = ipc_queue_mem_size(&chan->bd_queue);
	int i;

	for (i = 0; i < chan->num_pools; i++) {
		size += chan->pools[i].shm_size;
	}

	return size;
}

int ipc_shm_init(const struct ipc_shm_cfg *cfg)
{
	struct ipc_shm_priv *priv = get_ipc_priv();
	uintptr_t local_chan_shm;
	uintptr_t remote_chan_shm;
	int chan_size;
	int err, i;

	if (!cfg) {
		shm_err("NULL argument\n");
		return -EINVAL;
	}

	if (cfg->num_channels < 1 ||
	    cfg->num_channels > IPC_SHM_MAX_CHANNELS) {
		shm_err("Number of channels must be between 1 and %d\n",
			IPC_SHM_MAX_CHANNELS);
		return -EINVAL;
	}

	/* save api params */
	priv->shm_size = cfg->shm_size;
	priv->num_channels = cfg->num_channels;

	/* pass interrupt and core data to hw */
	err = ipc_hw_init(cfg);
	if (err)
		return err;

	/* init OS specific resources */
	err = ipc_os_init(cfg, ipc_shm_rx);
	if (err)
		goto err_free_hw;

	/* init channels */
	local_chan_shm = ipc_os_get_local_shm();
	remote_chan_shm = ipc_os_get_remote_shm();
	shm_dbg("initializing channels...\n");
	for (i = 0; i < priv->num_channels; i++) {
		err = ipc_shm_channel_init(i, local_chan_shm, remote_chan_shm,
					   &cfg->channels[i]);
		if (err)
			goto err_free_os;

		/* compute next channel local/remote shm base address */
		chan_size = get_chan_mem_size(i);
		local_chan_shm += chan_size;
		remote_chan_shm += chan_size;
	}

	/* enable interrupt notifications */
	ipc_hw_irq_enable();

	shm_dbg("ipc shm initialized\n");
	return 0;

err_free_os:
	ipc_os_free();
err_free_hw:
	ipc_hw_free();
	return err;
}

void ipc_shm_free(void)
{
	/* disable hardirq */
	ipc_hw_irq_disable();

	ipc_os_free();
	ipc_hw_free();

	shm_dbg("ipc shm released\n");
}

void *ipc_shm_acquire_buf(int chan_id, size_t request_size)
{
	struct ipc_shm_channel *chan = NULL;
	struct ipc_shm_pool *pool = NULL;
	struct ipc_shm_bd bd;
	uintptr_t buf_addr;
	int pool_id;

	chan = get_channel(chan_id);
	if (!chan)
		return NULL;

	/* find first non-empty pool that accommodates the requested size */
	for (pool_id = 0; pool_id < chan->num_pools; pool_id++) {
		pool = &chan->pools[pool_id];

		/* check if pool buf size covers the requested size */
		if (request_size > pool->buf_size)
			continue;

		/* check if pool has any free buffers left */
		if (ipc_queue_pop(&pool->bd_queue, &bd) == 0)
			break;
	}

	if (pool_id == chan->num_pools) {
		shm_dbg("No free buffer found in channel %d\n", chan_id);
		return NULL;
	}

	buf_addr = pool->local_pool_addr + bd.buf_id * pool->buf_size;

	shm_dbg("ch %d: pool %d: acquired buffer %d with address %lx\n",
		chan_id, pool_id, bd.buf_id, buf_addr);
	return (void *)buf_addr;
}

/**
 * find_pool_for_buf() - Find the pool that owns the specified buffer.
 * @chan_id:	channel index
 * @buf:	buffer pointer
 * @remote:	flag telling if buffer is from remote OS
 *
 * Return: pool index on success, -1 otherwise
 */
static int find_pool_for_buf(int chan_id, uintptr_t buf, int remote)
{
	struct ipc_shm_channel *chan = __get_channel(chan_id);
	struct ipc_shm_pool *pool;
	uintptr_t addr;
	int pool_id;
	int pool_size;

	for (pool_id = 0; pool_id < chan->num_pools; pool_id++) {
		pool = &chan->pools[pool_id];

		addr = remote ? pool->remote_pool_addr : pool->local_pool_addr;
		pool_size = pool->num_bufs * pool->buf_size;

		if (buf >= addr && buf < addr + pool_size)
			return pool_id;
	}

	return -1;
}

int ipc_shm_release_buf(int chan_id, const void *buf)
{
	struct ipc_shm_channel *chan;
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	int err;

	chan = get_channel(chan_id);
	if (!chan)
		return -EINVAL;

	/* Find the pool that owns the buffer */
	bd.pool_id = find_pool_for_buf(chan_id, (uintptr_t)buf, 1);
	if (bd.pool_id == -1) {
		shm_err("Buffer address %p doesn't belong to channel %d\n",
			buf, chan_id);
		return -EINVAL;
	}

	pool = &chan->pools[bd.pool_id];
	bd.buf_id = ((uintptr_t)buf - pool->remote_pool_addr) / pool->buf_size;
	bd.data_size = 0; /* reset size of written data in buffer */

	err = ipc_queue_push(&pool->bd_queue, &bd);
	if (err) {
		shm_err("Unable to release buffer %d from pool %d from channel %d with address %p\n",
			bd.buf_id, bd.pool_id, chan_id, buf);
		return err;
	}

	shm_dbg("ch %d: pool %d: released buffer %d with address %p\n",
		chan_id, bd.pool_id, bd.buf_id, buf);
	return 0;
}

int ipc_shm_tx(int chan_id, void *buf, size_t data_size)
{
	struct ipc_shm_channel *chan;
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	int err;

	chan = get_channel(chan_id);
	if (!chan)
		return -EINVAL;

	/* Find the pool that owns the buffer */
	bd.pool_id = find_pool_for_buf(chan_id, (uintptr_t)buf, 0);
	if (bd.pool_id == -1) {
		shm_err("Buffer address %p doesn't belong to channel %d\n",
			buf, chan_id);
		return -EINVAL;
	}

	pool = &chan->pools[bd.pool_id];
	bd.buf_id = ((uintptr_t)buf - pool->local_pool_addr) / pool->buf_size;
	bd.data_size = data_size;

	/* push buffer descriptor in queue */
	err = ipc_queue_push(&chan->bd_queue, &bd);
	if (err) {
		shm_err("Unable to push buffer descriptor in channel queue\n");
		return err;
	}

	/* notify remote that data is available */
	ipc_hw_irq_notify();

	return 0;
}

int ipc_shm_tx_unmanaged(int chan_id)
{
	(void)chan_id;

	shm_err("Unmanaged channels not supported in current implementation\n");
	return -EOPNOTSUPP;
}
