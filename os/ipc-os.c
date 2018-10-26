/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018 NXP
 */
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include "ipc-os.h"
#include "ipc-hw.h"

#define DRIVER_VERSION	"0.1"

/**
 * struct ipc_os_priv - OS specific private data
 * @shm_size:		local/remote shared memory size
 * @local_phys_shm:	local shared memory physical address
 * @remote_phys_shm:	remote shared memory physical address
 * @local_virt_shm:	local shared memory virtual address
 * @remote_virt_shm:	remote shared memory virtual address
 * @rx_cb:		upper layer rx callback
 * @irq_num:		Linux IRQ number
 */
struct ipc_os_priv {
	int shm_size;
	uintptr_t local_phys_shm;
	uintptr_t remote_phys_shm;
	uintptr_t local_virt_shm;
	uintptr_t remote_virt_shm;
	int (*rx_cb)(int budget);
	int irq_num;
};

/* OS specific private data */
static struct ipc_os_priv priv;

static void ipc_shm_softirq(unsigned long arg);
DECLARE_TASKLET(ipc_shm_rx_tasklet, ipc_shm_softirq, IPC_SOFTIRQ_BUDGET);

/* sotfirq routine for deferred interrupt handling */
static void ipc_shm_softirq(unsigned long budget)
{
	int work = 0;

	work = priv.rx_cb(budget);

	if (work < budget) {
		/* work done, re-enable irq */
		ipc_hw_irq_enable();
	} else {
		/* work not done, reschedule softirq */
		tasklet_schedule(&ipc_shm_rx_tasklet);
	}
}

/* driver interrupt service routine */
static irqreturn_t ipc_shm_hardirq(int irq, void *dev)
{
	ipc_hw_irq_disable();
	ipc_hw_irq_clear();

	tasklet_schedule(&ipc_shm_rx_tasklet);

	return IRQ_HANDLED;
}

/**
 * ipc_shm_os_init() - OS specific initialization code
 * @cfg:         configuration parameters
 * @rx_cb:	rx callback to be called from rx softirq
 *
 * Return: 0 on success, error code otherwise
 */
int ipc_os_init(const struct ipc_shm_cfg *cfg, int (*rx_cb)(int))
{
	struct device_node *mscm = NULL;
	struct resource *res;
	int err;

	if (!rx_cb)
		return -EINVAL;

	/* save params */
	priv.shm_size = cfg->shm_size;
	priv.local_phys_shm = cfg->local_shm_addr;
	priv.remote_phys_shm = cfg->remote_shm_addr;
	priv.rx_cb = rx_cb;

	/* request and map local physical shared memory */
	res = request_mem_region((phys_addr_t)cfg->local_shm_addr,
				 cfg->shm_size, DRIVER_NAME" local");
	if (!res) {
		shm_err("Unable to reserve local shm region\n");
		return -EADDRINUSE;
	}

	priv.local_virt_shm = (uintptr_t)ioremap_nocache(cfg->local_shm_addr,
						     cfg->shm_size);
	if (!priv.local_virt_shm) {
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

	priv.remote_virt_shm = (uintptr_t)ioremap_nocache(cfg->remote_shm_addr,
						      cfg->shm_size);
	if (!priv.remote_virt_shm) {
		err = -ENOMEM;
		goto err_release_remote_region;
	}


	/* get interrupt number from device tree */
	mscm = of_find_compatible_node(NULL, NULL, ipc_hw_get_dt_comp());
	if (!mscm) {
		shm_err("Unable to find MSCM node in device tree\n");
		err = -ENXIO;
		goto err_unmap_remote_shm;
	}

	priv.irq_num = of_irq_get(mscm, ipc_hw_get_dt_irq());
	of_node_put(mscm); /* release refcount to mscm DT node */

	/* init rx interrupt */
	err = request_irq(priv.irq_num, ipc_shm_hardirq, 0, DRIVER_NAME, &priv);
	if (err) {
		shm_err("Request interrupt %d failed\n", priv.irq_num);
		goto err_unmap_remote_shm;
	}

	return 0;

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

/**
 * ipc_os_free() - free OS specific resources
 */
void ipc_os_free(void)
{
	/* disable hardirq */
	ipc_hw_irq_disable();

	/* kill softirq task */
	tasklet_kill(&ipc_shm_rx_tasklet);

	free_irq(priv.irq_num, &priv);
	iounmap((void *)priv.remote_virt_shm);
	release_mem_region((phys_addr_t)priv.remote_phys_shm, priv.shm_size);
	iounmap((void *)priv.local_virt_shm);
	release_mem_region((phys_addr_t)priv.local_phys_shm, priv.shm_size);
}

/**
 * ipc_os_get_local_shm() - get local shared mem address
 */
uintptr_t ipc_os_get_local_shm(void)
{
	return priv.local_virt_shm;
}

/**
 * ipc_os_get_remote_shm() - get remote shared mem address
 */
uintptr_t ipc_os_get_remote_shm(void)
{
	return priv.remote_virt_shm;
}

/* module init function */
static int __init shm_mod_init(void)
{
	shm_dbg("driver version %s init\n", DRIVER_VERSION);
	return 0;
}

/* module exit function */
static void __exit shm_mod_exit(void)
{
	shm_dbg("driver version %s exit\n", DRIVER_VERSION);
}

EXPORT_SYMBOL(ipc_shm_init);
EXPORT_SYMBOL(ipc_shm_free);
EXPORT_SYMBOL(ipc_shm_acquire_buf);
EXPORT_SYMBOL(ipc_shm_release_buf);
EXPORT_SYMBOL(ipc_shm_tx);
EXPORT_SYMBOL(ipc_shm_unmanaged_acquire);
EXPORT_SYMBOL(ipc_shm_unmanaged_tx);

module_init(shm_mod_init);
module_exit(shm_mod_exit);

MODULE_AUTHOR("NXP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS(DRIVER_NAME);
MODULE_DESCRIPTION("NXP Shared Memory Inter-Processor Communication Driver");
MODULE_VERSION(DRIVER_VERSION);
