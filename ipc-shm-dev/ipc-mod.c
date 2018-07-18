/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018 NXP
 */
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include "ipc-os.h"
#include "ipc-hw.h"
#include "ipc-shm.h"

#define DRIVER_VERSION	"0.1"

/**
 * struct ipc_os_priv - OS specific private data
 * @shm_rx_cb:	upper layer rx callback
 * @irq_num:	Linux IRQ number
 */
struct ipc_os_priv {
	int (*rx_cb)(int budget);
	int irq_num;
};

/* OS specific private data */
static struct ipc_os_priv priv;

static void ipc_shm_rx_softirq(unsigned long arg);
DECLARE_TASKLET(ipc_shm_rx_tasklet, ipc_shm_rx_softirq, IPC_SOFTIRQ_BUDGET);

/* sotfirq routine for deferred interrupt handling */
static void ipc_shm_rx_softirq(unsigned long budget)
{
	int work = 0;

	work = priv.rx_cb(IPC_SOFTIRQ_BUDGET);

	if (work < IPC_SOFTIRQ_BUDGET) {
		/* work done, re-enable irq */
		ipc_shm_hw_irq_enable(PLATFORM_DEFAULT);
	} else {
		/* work not done, reschedule softirq */
		tasklet_schedule(&ipc_shm_rx_tasklet);
	}
}

/* rx interrupt handler */
static irqreturn_t ipc_shm_rx_irq(int irq, void *dev)
{
	ipc_shm_hw_irq_disable(PLATFORM_DEFAULT);
	ipc_shm_hw_irq_clear(PLATFORM_DEFAULT);

	tasklet_schedule(&ipc_shm_rx_tasklet);

	return IRQ_HANDLED;
}

/**
 * ipc_shm_os_init() - OS specific initialization code
 *
 * rx_cb:	rx callback to be called from rx softirq
 */
int ipc_os_init(int (*rx_cb)(int))
{
	struct device_node *mscm = NULL;
	int err;

	if (!rx_cb)
		return -EINVAL;

	priv.rx_cb = rx_cb;

	/* get interrupt number from device tree */
	mscm = of_find_compatible_node(NULL, NULL, ipc_shm_hw_get_dt_comp());
	if (!mscm) {
		shm_err("Unable to find MSCM node in device tree\n");
		return -ENXIO;
	}

	priv.irq_num = of_irq_get(mscm,
				ipc_shm_hw_get_dt_irq(PLATFORM_DEFAULT));
	of_node_put(mscm); /* release refcount to mscm DT node */

	/* init rx interrupt */
	err = request_irq(priv.irq_num, ipc_shm_rx_irq, 0, DRIVER_NAME, &priv);
	if (err) {
		shm_err("Request interrupt %d failed\n", priv.irq_num);
		return err;
	}

	return 0;
}

void ipc_os_free(void)
{
	tasklet_kill(&ipc_shm_rx_tasklet);
	free_irq(priv.irq_num, &priv);
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

module_init(shm_mod_init);
module_exit(shm_mod_exit);

MODULE_AUTHOR("NXP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS(DRIVER_NAME);
MODULE_DESCRIPTION("NXP Shared Memory Inter-Processor Communication Driver");
MODULE_VERSION(DRIVER_VERSION);
