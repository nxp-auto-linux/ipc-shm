/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018 NXP
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/of_irq.h>
#include "ipc-shm.h"
#include "ipc-fifo.h"
#include "ipc-hw-s32.h"

#define DRIVER_NAME	"ipc-shm-dev"
#define DRIVER_VERSION	"0.1"

MODULE_AUTHOR("NXP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS(DRIVER_NAME);
MODULE_DESCRIPTION("NXP Shared Memory Inter-Processor Communication Driver");
MODULE_VERSION(DRIVER_VERSION);

#if IPC_SHM_CHANNEL_COUNT != 1
#error "Only 1 channel is supported in current implementation!"
#endif

#if IPC_SHM_POOL_COUNT != 1
#error "Only 1 pool per channel is supported in current implementation!"
#endif

/* convenience wrappers for printing errors and debug messages */
#define shm_fmt(fmt) DRIVER_NAME": %s(): "fmt
#define shm_err(fmt, ...) pr_err(shm_fmt(fmt), __func__, ##__VA_ARGS__)
#define shm_dbg(fmt, ...) pr_debug(shm_fmt(fmt), __func__, ##__VA_ARGS__)

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
 * our acquire fifo = remote release fifo
 * our release fifo = remote acquire fifo
 */
struct ipc_shm_pool {
	uint16_t num_bufs;
	uint32_t buf_size;
	uint32_t shm_size;
	struct ipc_fifo *acquire_bd_fifo;
	struct ipc_fifo *release_bd_fifo;
	void *local_pool_addr;
	void *remote_pool_addr;
};

/**
 * struct ipc_shm_channel - ipc channel private data
 * @id:		channel id
 * @type:	channel type (see ipc_shm_channel_type)
 * @ops:	channel rx API callbacks
 * @local_shm:  local channel shared memory virtual address
 * @remote_shm:	remote channel shared memory virtual address
 * @tx_fifo:	tx fifo mapped at the start of local channel shared memory
 * @rx_fifo:	rx fifo mapped at the start of remote channel shared memory
 * @pools:	buffer pools private data
 *
 * our tx_fifo = remote rx_fifo
 * our rx_fifo = remote tx_fifo
 */
struct ipc_shm_channel {
	int id;
	enum ipc_shm_channel_type type;
	struct ipc_shm_channel_ops ops;
	void *local_shm;
	void *remote_shm;
	struct ipc_fifo *tx_fifo;
	struct ipc_fifo *rx_fifo;
	struct ipc_shm_pool pools[IPC_SHM_POOL_COUNT];
};

/**
 * struct ipc_shm_priv - ipc shm private data
 * @local_phys_shm:	local shared memory physical address
 * @remote_phys_shm:	remote shared memory physical address
 * @shm_size:		local/remote shared memory size
 * @local_virt_shm:	local shared memory virtual address
 * @remote_virt_shm:	remote shared memory virtual address
 * @irq_num:		Linux interrupt
 * @channels:		ipc channels private data
 */
struct ipc_shm_priv {
	void *local_phys_shm;
	void *remote_phys_shm;
	int shm_size;
	void *local_virt_shm;
	void *remote_virt_shm;
	int irq_num;
	struct ipc_shm_channel channels[IPC_SHM_CHANNEL_COUNT];
};

/* ipc shm private data */
static struct ipc_shm_priv ipc_shm_priv;

static inline struct ipc_shm_priv *get_ipc_priv(void)
{
	return &ipc_shm_priv;
}

static inline struct ipc_shm_channel *get_channel(int chan_id)
{
	return &ipc_shm_priv.channels[chan_id];
}

/* rx interrupt handler */
static irqreturn_t ipc_shm_rx_irq(int irq, void *dev)
{
	/* TODO: handle multiple channels not just the first one */
	struct ipc_shm_channel *chan = get_channel(0);
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	void *buf_addr;

	/* TODO: read rx fifo in a separate tasklet (intr coalescing) */
	while (ipc_fifo_pop(chan->rx_fifo, &bd, sizeof(bd))) {
		pool = &chan->pools[bd.pool_id];
		buf_addr = pool->remote_pool_addr + bd.buf_id * pool->buf_size;

		chan->ops.rx_cb(chan->ops.cb_arg, chan->id, buf_addr,
				bd.data_size);
	}

	ipc_hw_irq_clear();

	return IRQ_HANDLED;
}

static int ipc_buf_pool_init(int chan_id, int pool_id,
			     void *local_shm, void *remote_shm,
			     const struct ipc_shm_pool_cfg *cfg)
{
	struct ipc_shm_priv *priv = get_ipc_priv();
	struct ipc_shm_channel *chan = get_channel(chan_id);
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

	if (local_shm + pool->shm_size >
		priv->local_virt_shm + priv->shm_size) {
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
			shm_err("Unable to init release buffer descriptors fifo for pool %d of channel %d\n",
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
				const struct ipc_shm_channel_cfg *cfg)
{
	struct ipc_shm_priv *priv = get_ipc_priv();
	struct ipc_shm_channel *chan = get_channel(chan_id);
	const struct ipc_shm_pool_cfg *pool_cfg;
	void *local_pool_shm;
	void *remote_pool_shm;
	int total_bufs, fifo_size;
	int fifo_mem_size;
	int err, i;

	if (!cfg) {
		shm_err("NULL channel configuration argument\n");
		return -EINVAL;
	}

	if (!cfg->ops.rx_cb) {
		shm_err("Receive callback not specified\n");
		return -EINVAL;
	}

	/* TODO: only managed channels are supported for now */
	if (cfg->type != SHM_CHANNEL_MANAGED) {
		shm_err("Channel type not supported\n");
		return -EOPNOTSUPP;
	}

	/* save cfg params */
	chan->type = cfg->type;
	chan->ops = cfg->ops;

	/* TODO: handle multiple channels when assigning shm */
	chan->local_shm = priv->local_virt_shm;
	chan->remote_shm = priv->remote_virt_shm;

	/* compute total number of buffers from all pools */
	total_bufs = 0;
	for (i = 0; i < IPC_SHM_POOL_COUNT; i++) {
		total_bufs += cfg->memory.managed.pools[i].num_bufs;
	}

	/* init tx/rx fifos */
	fifo_size = total_bufs * sizeof(struct ipc_shm_bd);

	/* init tx fifo mapped at the start of local channel shm */
	chan->tx_fifo = ipc_fifo_init(chan->local_shm, fifo_size);

	/* map rx fifo (remote tx fifo) at the start of remote channel */
	chan->rx_fifo = (struct ipc_fifo *) chan->remote_shm;

	/* TODO: sort pool configurations ascending by buf size */

	/* init&map buffer pools after tx fifo */
	fifo_mem_size = ipc_fifo_mem_size(chan->tx_fifo);
	local_pool_shm = chan->local_shm + fifo_mem_size;
	remote_pool_shm = chan->remote_shm + fifo_mem_size;

	for (i = 0; i < IPC_SHM_POOL_COUNT; i++) {
		pool_cfg = &cfg->memory.managed.pools[i];

		err = ipc_buf_pool_init(chan->id, i, local_pool_shm,
					remote_pool_shm, pool_cfg);
		if (err)
			return err;

		/* compute next pool local/remote shm base address */
		local_pool_shm += chan->pools[i].shm_size;
		remote_pool_shm += chan->pools[i].shm_size;
	}

	shm_dbg("ipc shm channel %d initialized\n", chan_id);
	return 0;
}

int ipc_shm_init(const struct ipc_shm_cfg *cfg)
{
	struct ipc_shm_priv *priv = get_ipc_priv();
	struct device_node *mscm = NULL;
	struct resource *res;
	int err, i;

	if (!cfg) {
		shm_err("NULL argument\n");
		return -EINVAL;
	}

	/* save api params */
	priv->local_phys_shm = cfg->local_shm_addr;
	priv->remote_phys_shm = cfg->remote_shm_addr;
	priv->shm_size = cfg->shm_size;

	/* request and map local physical shared memory */
	res = request_mem_region((phys_addr_t)cfg->local_shm_addr,
				 cfg->shm_size, DRIVER_NAME" local");
	if (!res) {
		shm_err("Unable to reserve local shm region\n");
		return -EADDRINUSE;
	}

	priv->local_virt_shm = ioremap_nocache((phys_addr_t)cfg->local_shm_addr,
					       cfg->shm_size);
	if (!priv->local_virt_shm) {
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

	priv->remote_virt_shm = ioremap_nocache(
		(phys_addr_t) cfg->remote_shm_addr, cfg->shm_size);
	if (!priv->remote_virt_shm) {
		err = -ENOMEM;
		goto err_release_remote_region;
	}

	/* init channels */
	for (i = 0; i < IPC_SHM_CHANNEL_COUNT; i++) {
		err = ipc_shm_channel_init(i, &cfg->channels[i]);
		if (err)
			goto err_unmap_remote_shm;
	}

	/* init rx interrupt */
	mscm = of_find_compatible_node(NULL, NULL, DT_MSCM_NODE_COMP);
	if (!mscm) {
		err = -ENXIO;
		shm_err("Unable to find MSCM in device tree\n");
		goto err_unmap_remote_shm;
	}

	priv->irq_num = of_irq_get(mscm, DT_MSCM_CPU2CPU_INTR);
	of_node_put(mscm); /* release refcount to mscm DT node*/

	err = request_irq(priv->irq_num, ipc_shm_rx_irq, 0, DRIVER_NAME, priv);
	if (err) {
		shm_err("Request interrupt %d failed\n", priv->irq_num);
		goto err_unmap_remote_shm;
	}

	/* init MSCM */
	err = ipc_hw_init();
	if (err) {
		shm_err("Core-to-core interrupt initialization failed\n");
		goto err_request_irq;
	}

	/* clear driver notifications */
	ipc_hw_irq_clear();

	/* enable driver notifications */
	ipc_hw_irq_enable();

	shm_dbg("ipc shm initialized\n");
	return 0;

err_request_irq:
	free_irq(priv->irq_num, priv);
err_unmap_remote_shm:
	iounmap(cfg->remote_shm_addr);
err_release_remote_region:
	release_mem_region((phys_addr_t)cfg->remote_shm_addr, cfg->shm_size);
err_unmap_local_shm:
	iounmap(cfg->local_shm_addr);
err_release_local_region:
	release_mem_region((phys_addr_t)cfg->local_shm_addr, cfg->shm_size);

	return err;
}
EXPORT_SYMBOL(ipc_shm_init);

int ipc_shm_free(void)
{
	struct ipc_shm_priv *priv = get_ipc_priv();

	ipc_hw_irq_disable();
	free_irq(priv->irq_num, priv);
	iounmap(priv->remote_virt_shm);
	release_mem_region((phys_addr_t)priv->remote_phys_shm, priv->shm_size);
	iounmap(priv->local_virt_shm);
	release_mem_region((phys_addr_t)priv->local_phys_shm, priv->shm_size);

	shm_dbg("ipc shm released\n");
	return 0;
}
EXPORT_SYMBOL(ipc_shm_free);

void *ipc_shm_acquire_buf(int chan_id, size_t request_size)
{
	struct ipc_shm_channel *chan;
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	void *buf_addr = NULL;
	int count;
	int pool_id;

	if (chan_id < 0 || chan_id >= IPC_SHM_CHANNEL_COUNT) {
		shm_err("Channel id outside valid range: 0 - %d\n",
			IPC_SHM_CHANNEL_COUNT);
		return NULL;
	}

	chan = get_channel(chan_id);

	/* find first non-empty pool that accommodates the requested size */
	for (pool_id = 0; pool_id < IPC_SHM_POOL_COUNT; pool_id++) {
		pool = &chan->pools[pool_id];

		/* check if pool buf size covers the requested size */
		if (request_size > pool->buf_size)
			continue;

		/* check if pool has any free buffers left */
		count = ipc_fifo_pop(pool->acquire_bd_fifo, &bd, sizeof(bd));

		if (count == sizeof(bd))
			break;
	}

	if (pool_id == IPC_SHM_POOL_COUNT) {
		shm_dbg("No free buffer found in channel %d\n", chan_id);
		return NULL;
	}

	buf_addr = pool->local_pool_addr + (bd.buf_id * pool->buf_size);

	shm_dbg("channel %d: pool %d: acquired buffer %d with address %p\n",
		chan_id, pool_id, bd.buf_id, buf_addr);
	return buf_addr;
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
static int find_pool_for_buf(int chan_id, const void *buf, int remote)
{
	struct ipc_shm_channel *chan = get_channel(chan_id);
	struct ipc_shm_pool *pool;
	void *addr;
	int pool_id;
	int pool_size;

	for (pool_id = 0; pool_id < IPC_SHM_POOL_COUNT; pool_id++) {
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
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	int count;

	if (chan_id < 0 || chan_id >= IPC_SHM_CHANNEL_COUNT) {
		shm_err("Channel id outside valid range: 0 - %d\n",
			IPC_SHM_CHANNEL_COUNT);
		return -EINVAL;
	}

	/* Find the pool that owns the buffer */
	bd.pool_id = find_pool_for_buf(chan_id, buf, 1);
	if (bd.pool_id == -1) {
		shm_err("Buffer address %p doesn't belong to channel %d\n",
			buf, chan_id);
		return -EINVAL;
	}

	pool = &get_channel(chan_id)->pools[bd.pool_id];
	bd.buf_id = (buf - pool->remote_pool_addr) / pool->buf_size;
	bd.data_size = 0; /* reset size of written data in buffer */

	count = ipc_fifo_push(pool->release_bd_fifo, &bd, sizeof(bd));
	if (count != sizeof(bd)) {
		shm_err("Unable to release buffer %d from pool %d from channel %d with address %p\n",
			bd.buf_id, bd.pool_id, chan_id, buf);
		return -EIO;
	}

	shm_dbg("channel %d: pool %d: released buffer %d with address %p\n",
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

	if (chan_id < 0 || chan_id >= IPC_SHM_CHANNEL_COUNT) {
		shm_err("Channel id outside valid range: 0 - %d\n",
			IPC_SHM_CHANNEL_COUNT);
		return -EINVAL;
	}

	/* Find the pool that owns the buffer */
	bd.pool_id = find_pool_for_buf(chan_id, buf, 0);
	if (bd.pool_id == -1) {
		shm_err("Buffer address %p doesn't belong to channel %d\n",
			buf, chan_id);
		return -EINVAL;
	}

	chan = get_channel(chan_id);
	pool = &chan->pools[bd.pool_id];
	bd.buf_id = (buf - pool->local_pool_addr) / pool->buf_size;
	bd.data_size = data_size;

	/* push buffer descriptor in tx fifo */
	count = ipc_fifo_push(chan->tx_fifo, &bd, sizeof(bd));
	if (count != sizeof(bd)) {
		shm_err("Unable to push buffer descriptor in tx fifo\n");
		return -EIO;
	}

	/* notify remote that data is available */
	ipc_hw_irq_notify();

	return 0;
}
EXPORT_SYMBOL(ipc_shm_tx);

int ipc_shm_tx_unmanaged(int chan_id)
{
	/* notify remote that data is available */
	ipc_hw_irq_notify();

	shm_err("Unmanaged channels not supported in current implementation\n");
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(ipc_shm_tx_unmanaged);

static int __init shm_mod_init(void)
{
	shm_dbg("driver version %s init\n", DRIVER_VERSION);
	return 0;
}

static void __exit shm_mod_exit(void)
{
	shm_dbg("driver version %s exit\n", DRIVER_VERSION);
}

module_init(shm_mod_init);
module_exit(shm_mod_exit);
