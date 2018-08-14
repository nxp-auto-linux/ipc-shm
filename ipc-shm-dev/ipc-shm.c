/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018 NXP
 */
#include <linux/ioport.h>
#include <linux/io.h>
#include "ipc-os.h"
#include "ipc-shm.h"
#include "ipc-fifo.h"
#include "ipc-hw.h"

#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

/**
 * struct ipc_shm_bd - buffer descriptor (store buffer location and data size)
 * @pool_id:	index of buffer pool
 * @buf_id:	index of buffer from buffer pool
 * @data_size:	size of data written in buffer
 *
 * Used to push/pop buffers in tx/rx fifo.
 */
struct ipc_shm_bd {
	int16_t pool_id;
	uint16_t buf_id;
	uint32_t data_size;
};

/**
 * struct ipc_shm_pool - memory buffer pool private data
 * @num_bufs:		number of buffers in pool
 * @buf_size:		size of buffers
 * @shm_size:		size of shared memory mapped by this pool (fifos + bufs)
 * @acquire_bd_fifo:	fifo with BDs from local pool
 * @release_bd_fifo:	fifo with BDs from remote pool
 * @local_pool_addr:	address of the actual local buffer pool
 * @remote_pool_addr:	address of the actual remote buffer pool
 *
 * Local OS pops BDs from acquire_bd_fifo when local app calls
 * ipc_shm_acquire_buf(). Remote OS pushes BDs in our acquire_bd_fifo (i.e its
 * release BDs fifo) when remote app calls ipc_shm_release_buf().
 * acquire_bd_fifo is initialized by remote OS (mem protection requirement).
 *
 * Local OS pushes BDs into release_bd_fifo when local app calls
 * ipc_shm_release_buf(). Remote OS pops BDs from our release_bd_fifo (i.e. its
 * acquire BDs fifo)when remote app calls ipc_shm_acquire_buf().
 * release_bd_fifo is initialized by us (mem protection requirement).
 *
 * local acquire fifo = remote release fifo
 * local release fifo = remote acquire fifo
 */
struct ipc_shm_pool {
	uint16_t num_bufs;
	uint32_t buf_size;
	uint32_t shm_size;
	struct ipc_fifo *acquire_bd_fifo;
	struct ipc_fifo *release_bd_fifo;
	uintptr_t local_pool_addr;
	uintptr_t remote_pool_addr;
};

/**
 * struct ipc_shm_channel - ipc channel private data
 * @id:		channel id
 * @type:	channel type (see ipc_shm_channel_type)
 * @ops:	channel rx API callbacks
 * @tx_fifo:	tx fifo mapped at the start of local channel shared memory
 * @rx_fifo:	rx fifo mapped at the start of remote channel shared memory
 * @pools:	buffer pools private data
 *
 * local tx_fifo = remote rx_fifo
 * local rx_fifo = remote tx_fifo
 */
struct ipc_shm_channel {
	int id;
	enum ipc_shm_channel_type type;
	struct ipc_shm_channel_ops ops;
	struct ipc_fifo *tx_fifo;
	struct ipc_fifo *rx_fifo;
	int num_pools;
	struct ipc_shm_pool pools[IPC_SHM_MAX_POOLS];
};

/**
 * struct ipc_shm_priv - ipc shm private data
 * @shm_size:		local/remote shared memory size
 * @local_shm:		local shared memory address
 * @remote_shm:		remote shared memory address
 * @local_phys_shm:	local shared memory physical address
 * @remote_phys_shm:	remote shared memory physical address
 * num_channels:	number of shared memory channels
 * @channels:		ipc channels private data
 */
struct ipc_shm_priv {
	int shm_size;
	uintptr_t local_shm;
	uintptr_t remote_shm;
	uintptr_t local_phys_shm;
	uintptr_t remote_phys_shm;
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
 * ipc_channel_rx() - handle rx for a single channel
 * @chan_id:	channel id
 * @budget:	available work budget (number of messaged to be processed)
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

	while (work < budget && ipc_fifo_pop(chan->rx_fifo, &bd, sizeof(bd))) {

		pool = &chan->pools[bd.pool_id];
		buf_addr = pool->remote_pool_addr + bd.buf_id * pool->buf_size;

		chan->ops.rx_cb(chan->ops.cb_arg, chan->id,
				(void *)buf_addr, bd.data_size);
		work++;
	}

	return work;
}

/**
 * ipc_shm_rx() - shm rx handler, called from softirq
 * @budget:	available work budget (number of messaged to be processed)
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
		chan_budget = max(((int)budget - work) / num_chans, 1);
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

static int ipc_buf_pool_init(int chan_id, int pool_id,
			     uintptr_t local_shm, uintptr_t remote_shm,
			     const struct ipc_shm_pool_cfg *cfg)
{
	struct ipc_shm_priv *priv = get_ipc_priv();
	struct ipc_shm_channel *chan = __get_channel(chan_id);
	struct ipc_shm_pool *pool = &chan->pools[pool_id];
	struct ipc_shm_bd bd;
	int fifo_size, fifo_mem_size;
	int i, count;

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

	/* init acquire/release fifos */
	fifo_size = pool->num_bufs * sizeof(struct ipc_shm_bd);

	/* init release_bd_fifo mapped at start of local pool shm and
	 * populated by us with remote pool buffer descriptors
	 */
	pool->release_bd_fifo = ipc_fifo_init(local_shm, fifo_size);

	/* map acquire bufs fifo (remote release bufs fifo) at start of remote
	 * pool shm. It is init and populated by remote OS with local pool BDs
	 */
	pool->acquire_bd_fifo = (struct ipc_fifo *) remote_shm;

	/* init local/remote buffer pool addrs */
	fifo_mem_size = ipc_fifo_mem_size(pool->release_bd_fifo);

	/* init actual local buffer pool addr */
	pool->local_pool_addr = local_shm + fifo_mem_size;

	/* init actual remote buffer pool addr */
	pool->remote_pool_addr = remote_shm + fifo_mem_size;

	pool->shm_size = fifo_mem_size + (cfg->buf_size * cfg->num_bufs);

	/* check if pool fits into shared memory */
	if (local_shm + pool->shm_size > priv->local_shm + priv->shm_size) {
		shm_err("Not enough shared memory for pool %d from channel %d\n",
			pool_id, chan_id);
		return -ENOMEM;
	}

	/* populate release_bd_fifo */
	for (i = 0; i < pool->num_bufs; i++) {
		bd.pool_id = pool_id;
		bd.buf_id = i;
		bd.data_size = 0;

		count = ipc_fifo_push(pool->release_bd_fifo, &bd, sizeof(bd));
		if (count != sizeof(bd)) {
			shm_err("Unable to init release buffer descriptors fifo "
				"for pool %d of channel %d\n",
				pool_id, chan_id);
			return -EIO;
		}
	}

	shm_dbg("ipc shm pool %d of chan %d initialized\n", pool_id, chan_id);
	return 0;
}

/**
 * ipc_shm_channel_init() - initialize shared memory IPC channel
 * @chan_id	channel index
 * @cfg:	configuration parameters
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
	int fifo_mem_size, fifo_size;
	int prev_buf_size = 0;
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

	/* init tx/rx fifos */
	fifo_size = total_bufs * sizeof(struct ipc_shm_bd);

	/* init tx fifo mapped at the start of local channel shm */
	chan->tx_fifo = ipc_fifo_init(local_shm, fifo_size);

	/* map rx fifo (remote tx fifo) at the start of remote channel */
	chan->rx_fifo = (struct ipc_fifo *) remote_shm;

	/* init&map buffer pools after tx fifo */
	fifo_mem_size = ipc_fifo_mem_size(chan->tx_fifo);
	local_pool_shm = local_shm + fifo_mem_size;
	remote_pool_shm = remote_shm + fifo_mem_size;

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

/* Get channel memory size = tx fifo size + size of channel buffer pools */
static int get_chan_mem_size(int chan_id)
{
	struct ipc_shm_channel *chan = __get_channel(chan_id);
	int size = ipc_fifo_mem_size(chan->tx_fifo);
	int i;

	for (i = 0; i < chan->num_pools; i++) {
		size += chan->pools[i].shm_size;
	}

	return size;
}

int ipc_shm_init(const struct ipc_shm_cfg *cfg)
{
	struct ipc_shm_priv *priv = get_ipc_priv();
	struct resource *res;
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
	priv->local_phys_shm = cfg->local_shm_addr;
	priv->remote_phys_shm = cfg->remote_shm_addr;
	priv->shm_size = cfg->shm_size;
	priv->num_channels = cfg->num_channels;

	/* request and map local physical shared memory */
	res = request_mem_region((phys_addr_t)cfg->local_shm_addr,
				 cfg->shm_size, DRIVER_NAME" local");
	if (!res) {
		shm_err("Unable to reserve local shm region\n");
		return -EADDRINUSE;
	}

	priv->local_shm = (uintptr_t)ioremap_nocache(cfg->local_shm_addr,
						     cfg->shm_size);
	if (!priv->local_shm) {
		err = -ENOMEM;
		goto err_release_local_region;
	}

	/* request and map remote physical shared memory */
	res = request_mem_region((phys_addr_t)cfg->remote_shm_addr,
				 cfg->shm_size, DRIVER_NAME" remote");
	if (!res) {
		shm_err("Unable to reserve remote shm region\n");
		err = -EADDRINUSE;
		goto err_unmap_local_shm;
	}

	priv->remote_shm = (uintptr_t)ioremap_nocache(cfg->remote_shm_addr,
						      cfg->shm_size);
	if (!priv->remote_shm) {
		err = -ENOMEM;
		goto err_release_remote_region;
	}

	/* init channels */
	local_chan_shm = priv->local_shm;
	remote_chan_shm = priv->remote_shm;
	shm_dbg("initializing channels...\n");
	for (i = 0; i < priv->num_channels; i++) {
		err = ipc_shm_channel_init(i, local_chan_shm, remote_chan_shm,
					   &cfg->channels[i]);
		if (err)
			goto err_unmap_remote_shm;

		/* compute next channel local/remote shm base address */
		chan_size = get_chan_mem_size(i);
		local_chan_shm += chan_size;
		remote_chan_shm += chan_size;
	}

	/* init OS specific part */
	err = ipc_os_init(ipc_shm_rx);
	if (err)
		goto err_unmap_remote_shm;

	/* init MSCM */
	err = ipc_shm_hw_init();
	if (err) {
		shm_err("Core-to-core interrupt initialization failed\n");
		goto err_free_ipc_os;
	}

	/* clear driver notifications */
	ipc_shm_hw_irq_clear(PLATFORM_DEFAULT);

	/* enable driver notifications */
	ipc_shm_hw_irq_enable(PLATFORM_DEFAULT);

	shm_dbg("ipc shm initialized\n");
	return 0;

err_free_ipc_os:
	ipc_os_free();
err_unmap_remote_shm:
	iounmap((void *)cfg->remote_shm_addr);
err_release_remote_region:
	release_mem_region((phys_addr_t)cfg->remote_shm_addr, cfg->shm_size);
err_unmap_local_shm:
	iounmap((void *)cfg->local_shm_addr);
err_release_local_region:
	release_mem_region((phys_addr_t)cfg->local_shm_addr, cfg->shm_size);

	return err;
}
EXPORT_SYMBOL(ipc_shm_init);

int ipc_shm_free(void)
{
	struct ipc_shm_priv *priv = get_ipc_priv();

	ipc_shm_hw_irq_disable(PLATFORM_DEFAULT);
	ipc_shm_hw_free();
	ipc_os_free();
	iounmap((void *)priv->remote_shm);
	release_mem_region((phys_addr_t)priv->remote_phys_shm, priv->shm_size);
	iounmap((void *)priv->local_shm);
	release_mem_region((phys_addr_t)priv->local_phys_shm, priv->shm_size);

	shm_dbg("ipc shm released\n");
	return 0;
}
EXPORT_SYMBOL(ipc_shm_free);

void *ipc_shm_acquire_buf(int chan_id, size_t request_size)
{
	struct ipc_shm_channel *chan = NULL;
	struct ipc_shm_pool *pool = NULL;
	struct ipc_shm_bd bd;
	uintptr_t buf_addr;
	int count;
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
		count = ipc_fifo_pop(pool->acquire_bd_fifo, &bd, sizeof(bd));
		if (count == sizeof(bd))
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
EXPORT_SYMBOL(ipc_shm_acquire_buf);

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
	int count;

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

	count = ipc_fifo_push(pool->release_bd_fifo, &bd, sizeof(bd));
	if (count != sizeof(bd)) {
		shm_err("Unable to release buffer %d from pool %d from channel %d with address %p\n",
			bd.buf_id, bd.pool_id, chan_id, buf);
		return -EIO;
	}

	shm_dbg("ch %d: pool %d: released buffer %d with address %p\n",
		chan_id, bd.pool_id, bd.buf_id, buf);
	return 0;
}
EXPORT_SYMBOL(ipc_shm_release_buf);

int ipc_shm_tx(int chan_id, void *buf, size_t data_size)
{
	struct ipc_shm_channel *chan;
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	int count;

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

	/* push buffer descriptor in tx fifo */
	count = ipc_fifo_push(chan->tx_fifo, &bd, sizeof(bd));
	if (count != sizeof(bd)) {
		shm_err("Unable to push buffer descriptor in tx fifo\n");
		return -EIO;
	}

	/* notify remote that data is available */
	ipc_shm_hw_irq_notify(PLATFORM_DEFAULT, PLATFORM_DEFAULT);

	return 0;
}
EXPORT_SYMBOL(ipc_shm_tx);

int ipc_shm_tx_unmanaged(int chan_id)
{
	(void)chan_id;

	shm_err("Unmanaged channels not supported in current implementation\n");
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(ipc_shm_tx_unmanaged);
