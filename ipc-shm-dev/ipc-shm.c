/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2018 NXP Semiconductors
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
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

/* convenience wrappers for printing errors and debug messages */
#define shm_fmt(fmt) DRIVER_NAME": %s(): "fmt
#define shm_err(fmt, ...) pr_err(shm_fmt(fmt), __func__, ##__VA_ARGS__)
#define shm_dbg(fmt, ...) pr_debug(shm_fmt(fmt), __func__, ##__VA_ARGS__)

/**
 * struct ipc_shm_buf_pool - memory buffer pool private data
 * @base_addr:		buffer pool shared memory virtual address
 * @num_buffers:	number of buffers in pool
 * @buf_size:		size of buffers
 * @free_bufs:		stack containing indexes of free buffers
 * @free_bufs_top:	top of free bufs stack
 */
struct ipc_shm_pool {
	void *base_addr;
	uint16_t num_buffers;
	uint32_t buf_size;
	uint16_t free_bufs[IPC_SHM_MAX_BUFS_PER_POOL];
	uint16_t free_bufs_top;
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
 */
struct ipc_shm_channel {
	int id;
	enum ipc_shm_channel_type type;
	struct ipc_shm_channel_ops ops;
	void *local_shm;
	void *remote_shm;
	struct ipc_fifo tx_fifo;
	struct ipc_fifo rx_fifo;
	struct ipc_shm_pool pools[IPC_SHM_POOL_COUNT];
};

/**
 * struct ipc_shm_priv - ipc shm private data
 * @local_phys_shm:	local shared memory physical address
 * @remote_phys_shm:	remote shared memory physical address
 * @shm_size:		local/remote shared memory size
 * @local_virt_shm:	local shared memory virtual address
 * @remote_virt_shm:	remote shared memory virtual address
 * @channels:		ipc channels private data
 */
struct ipc_shm_priv {
	void *local_phys_shm;
	void *remote_phys_shm;
	int shm_size;
	void *local_virt_shm;
	void *remote_virt_shm;
	struct ipc_shm_channel channels[IPC_SHM_CHANNEL_COUNT];
};

/**
 * struct ipc_buffer - used for buffer info and data size encapsulation
 * @pool_index:	index of buffer pool
 * @buf_index:	index of buffer from buffer pool
 * @data_size:	size of data written in buffer
 *
 * Used to push/pop buffers with user data in tx/rx fifo.
 */
struct ipc_shm_buf {
	uint16_t pool_index;
	uint16_t buf_index;
	uint32_t data_size;
};

/* ipc shm private data */
static struct ipc_shm_priv ipc_shm_priv;

static struct ipc_shm_priv *get_ipc_priv(void)
{
	return &ipc_shm_priv;
}

/* rx interrupt handler */
static irqreturn_t ipc_shm_rx_irq(int irq, void *dev)
{
	struct ipc_shm_priv *priv = (struct ipc_shm_priv *)dev;
	struct ipc_shm_channel *channel = &priv->channels[0];

	/* TODO: call rx_cb with the actual received buffer (next PR) */
	channel->ops.rx_cb(channel->ops.cb_arg, channel->id, NULL, 0);

	return IRQ_HANDLED;
}

static int ipc_buf_pool_init(int chan_id, int pool_id, void *pool_addr,
				const struct ipc_shm_pool_cfg *cfg)
{
	struct ipc_shm_priv *priv = get_ipc_priv();
	struct ipc_shm_channel *chan = &priv->channels[chan_id];
	struct ipc_shm_pool *pool = &chan->pools[pool_id];
	int pool_size, i;

	if (!cfg) {
		shm_err("NULL buffer pool configuration argument\n");
		return -EINVAL;
	}

	if (cfg->num_bufs > IPC_SHM_MAX_BUFS_PER_POOL) {
		shm_err("Too many buffers configured in pool. "
			"Increase IPC_SHM_MAX_BUFS_PER_POOL if needed\n");
		return -EINVAL;
	}

	pool_size = cfg->buf_size * cfg->num_bufs;
	if (pool_addr + pool_size > priv->local_virt_shm + priv->shm_size) {
		shm_err("Not enough shared memory for pool %d from channel %d\n",
			pool_id, chan_id);
		return -ENOMEM;
	}

	pool->num_buffers = cfg->num_bufs;
	pool->buf_size = cfg->buf_size;
	pool->base_addr = pool_addr;

	/* init free bufs stack */
	for (i = 0; i < pool->num_buffers; i++)
		pool->free_bufs[i] = i;

	pool->free_bufs_top = pool->num_buffers - 1;

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
	struct ipc_shm_priv *ipc_priv = get_ipc_priv();
	struct ipc_shm_channel *chan = &ipc_priv->channels[chan_id];
	const struct ipc_shm_pool_cfg *pool;
	int total_bufs, err, i;
	void *pool_addr;

	if (!cfg) {
		shm_err("NULL channel configuration argument\n");
		return -EINVAL;
	}

	/* TODO: only managed channels are supported for now */
	if (cfg->type != SHM_CHANNEL_MANAGED) {
		shm_err("Channel type not supported\n");
		return -EOPNOTSUPP;
	}

	if (!cfg->ops.rx_cb) {
		shm_err("Receive callback not specified\n");
		return -EINVAL;
	}

	/* save cfg params */
	chan->type = cfg->type;
	chan->ops = cfg->ops;

	/* TODO: handle multiple channels when assigning shm */
	chan->local_shm = ipc_priv->local_virt_shm;
	chan->remote_shm = ipc_priv->remote_virt_shm;

	/* compute total number of buffers from all pools */
	total_bufs = 0;
	for (i = 0; i < IPC_SHM_POOL_COUNT; i++) {
		total_bufs += cfg->memory.managed.pools[i].num_bufs;
	}

	/* init tx/rx fifos */
	ipc_fifo_init(&chan->tx_fifo, chan->local_shm,
			total_bufs * sizeof(struct ipc_shm_buf) + 1);

	ipc_fifo_init(&chan->rx_fifo, chan->remote_shm,
			total_bufs * sizeof(struct ipc_shm_buf) + 1);

	/* init buffer pools */
	pool_addr = chan->local_shm;
	for (i = 0; i < IPC_SHM_POOL_COUNT; i++) {
		pool = &cfg->memory.managed.pools[i];

		err = ipc_buf_pool_init(chan->id, i, pool_addr, pool);
		if (err)
			return err;

		/* compute next pool base address */
		pool_addr += pool->buf_size * pool->num_bufs;
	}

	shm_dbg("ipc shm channel %d initialized\n", chan_id);
	return 0;
}

int ipc_shm_init(const struct ipc_shm_cfg *cfg)
{
	struct ipc_shm_priv *priv = get_ipc_priv();
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
	err = request_irq(MSCM_IRQ_GIC_ID, ipc_shm_rx_irq, 0, DRIVER_NAME, priv);
	if (err) {
		shm_err("Request interrupt %d failed\n", MSCM_IRQ_GIC_ID);
		goto err_unmap_remote_shm;
	}


	err = ipc_hw_init();
	if (err) {
		shm_err("Core-to-core interrupt initialization failed\n");
		goto err_unmap_remote_shm;
	}

	/* clear driver notifications */
	ipc_hw_irq_clear();
	
	/* enable driver notifications */
	ipc_hw_irq_enable();

	shm_dbg("ipc shm initialized\n");
	return 0;

err_free_irq:
	ipc_hw_irq_disable();
	free_irq(MSCM_IRQ_GIC_ID, priv);
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
	free_irq(MSCM_IRQ_ID, priv);
	iounmap(priv->remote_virt_shm);
	release_mem_region((phys_addr_t)priv->remote_phys_shm, priv->shm_size);
	iounmap(priv->local_virt_shm);
	release_mem_region((phys_addr_t)priv->local_phys_shm, priv->shm_size);

	shm_dbg("ipc shm released\n");
	return 0;
}
EXPORT_SYMBOL(ipc_shm_free);

void *ipc_shm_acquire_buf(int chan_id, size_t size)
{
	shm_err("Not implemented yet\n");
	return NULL;
}
EXPORT_SYMBOL(ipc_shm_acquire_buf);

int ipc_shm_release_buf(int chan_id, void *buf)
{
	shm_err("Not implemented yet\n");
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(ipc_shm_release_buf);

int ipc_shm_tx(int chan_id, void *buf, size_t size)
{
	/* notify remote that data is available */
	ipc_hw_irq_notify();
	
	shm_err("Not implemented yet\n");
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(ipc_shm_tx);

int ipc_shm_tx_unmanaged(int chan_id)
{
	/* notify remote that data is available */
	ipc_hw_irq_notify();

	shm_err("Unmanaged channels not supported yet\n");
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
