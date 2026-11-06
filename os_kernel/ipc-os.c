/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018-2025 NXP
 */
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/version.h>

#include "ipc-os.h"
#include "ipc-hw.h"
#include "ipc-shm.h"

#define DRIVER_VERSION	"0.1"

/* Device tree MSCM node compatible property (search key) */
#if defined(PLATFORM_FLAVOR_s32g2) || defined(PLATFORM_FLAVOR_s32g3) || \
	defined(PLATFORM_FLAVOR_s32r45) || defined(PLATFORM_FLAVOR_s32r47)
	#define DT_INTC_NODE_COMP "nxp,s32cc-mscm"
#elif defined(PLATFORM_FLAVOR_s32v234)
	#define DT_INTC_NODE_COMP "fsl,s32v234-mscm"
#else
	#error "Platform not supported"
#endif

/**
 * struct ipc_os_priv_instance - OS specific private data each instance
 * @shm_size:           local/remote shared memory size
 * @local_phys_shm:     local shared memory physical address
 * @remote_phys_shm:    remote shared memory physical address
 * @local_virt_shm:     local shared memory virtual address
 * @remote_virt_shm:    remote shared memory virtual address
 * @irq_num:            Linux IRQ number
 * @state:              state to indicate whether instance is initialized
 * @rx_cb:              upper layer rx callback
 */
struct ipc_os_priv_instance {
	int shm_size;
	uintptr_t local_phys_shm;
	uintptr_t remote_phys_shm;
	uintptr_t local_virt_shm;
	uintptr_t remote_virt_shm;
	int irq_num;
	int state;
	uint32_t (*rx_cb)(const uint8_t instance, int budget);
};

/**
 * struct ipc_os_priv - OS specific private data
 * @id:             private data per instance
 */
static struct ipc_os_priv {
	struct ipc_os_priv_instance id[IPC_SHM_MAX_INSTANCES];
} priv;

static void ipc_shm_softirq(unsigned long arg);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
static DECLARE_TASKLET(ipc_shm_rx_tasklet, ipc_shm_softirq, 0);
#else
static DECLARE_TASKLET_OLD(ipc_shm_rx_tasklet, ipc_shm_softirq);
#endif

/* sotfirq routine for deferred interrupt handling */
static void ipc_shm_softirq(unsigned long arg)
{
	int work = 0;
	unsigned long budget = IPC_SOFTIRQ_BUDGET;
	uint8_t i = 0;

	for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
		if ((priv.id[i].state == IPC_SHM_INSTANCE_DISABLED)
					|| (priv.id[i].irq_num == IPC_IRQ_NONE))
			continue;

		/* Do the budgeted work */
		work = priv.id[i].rx_cb(i, budget);

		/* If we used the full budget, schedule again.
		 * We don't know how much work remains until we do the work.
		 * The next tasklet will check for more work
		 */
		if (work >= budget)
			tasklet_schedule(&ipc_shm_rx_tasklet);
		else
			ipc_hw_irq_enable(i);

		/* work done, re-enable irq */
		ipc_hw_irq_enable(i);
	}
}

/* driver interrupt service routine */
static irqreturn_t ipc_shm_hardirq(int irq, void *dev)
{
	uint8_t i = 0;

	for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
		if ((priv.id[i].state == IPC_SHM_INSTANCE_DISABLED)
				|| (priv.id[i].irq_num == IPC_IRQ_NONE))
			continue;

		/* disable notifications from remote */
		ipc_hw_irq_disable(i);

		/* clear notification */
		ipc_hw_irq_clear(i);
	}

	tasklet_schedule(&ipc_shm_rx_tasklet);

	return IRQ_HANDLED;
}

static int8_t get_irq_number(const uint8_t instance,
	const struct ipc_shm_cfg *cfg, int *irq_num)
{
	struct device_node *compatible_node = NULL;

	/* get node with compatible string from device tree */
	compatible_node = of_find_compatible_node(
			NULL, NULL, DT_INTC_NODE_COMP);
	if (!compatible_node) {
		shm_err("Cannot find node with compatible %s in device tree\n",
			DT_INTC_NODE_COMP);
		return -ENXIO;
	}
	*irq_num = of_irq_get(compatible_node, ipc_hw_get_rx_irq(instance));
	if (*irq_num <= 0) {
		shm_err("Unable to decode node's IRQ\n");
		of_node_put(compatible_node);
		return -EINVAL;
	}
	shm_dbg("Rx IRQ of instance %d = %d\n",
		instance, *irq_num);

	/* release refcount to interrupt DT node */
	of_node_put(compatible_node);

	return 0;
}

int8_t ipc_os_init(const uint8_t instance, const struct ipc_shm_cfg *cfg,
		uint32_t (*rx_cb)(const uint8_t, int))
{
	struct resource *res;
	int err;
	int i;

	if (!rx_cb)
		return -EINVAL;

	/* check valid instance */
	if ((instance > IPC_SHM_MAX_INSTANCES) || (instance < 0))
		return -EINVAL;

	/* request and map local physical shared memory */
	res = request_mem_region((phys_addr_t)cfg->local_shm_addr,
		cfg->shm_size, DRIVER_NAME" local");
	if (!res) {
		shm_err("Unable to reserve local shm region\n");
		return -EADDRINUSE;
	}

	priv.id[instance].local_virt_shm
		= (uintptr_t)ioremap(cfg->local_shm_addr, cfg->shm_size);
	if (!priv.id[instance].local_virt_shm) {
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

	priv.id[instance].remote_virt_shm
		= (uintptr_t)ioremap(cfg->remote_shm_addr, cfg->shm_size);
	if (!priv.id[instance].remote_virt_shm) {
		err = -ENOMEM;
		goto err_release_remote_region;
	}

	/* save params */
	priv.id[instance].shm_size = cfg->shm_size;
	priv.id[instance].local_phys_shm = cfg->local_shm_addr;
	priv.id[instance].remote_phys_shm = cfg->remote_shm_addr;
	priv.id[instance].rx_cb = rx_cb;

	if (cfg->inter_core_rx_irq == IPC_IRQ_NONE) {
		priv.id[instance].irq_num = IPC_IRQ_NONE;
	} else {
		err = get_irq_number(instance, cfg, &priv.id[instance].irq_num);
		if (err != 0) {
			goto err_unmap_remote_shm;
		}
	}

	/* check duplicate irq number */
	for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
		if (instance == i)
			continue;
		if (priv.id[instance].irq_num == priv.id[i].irq_num) {
			priv.id[instance].state = IPC_SHM_INSTANCE_ENABLED;
			return 0;
		}
	}

	if (priv.id[instance].irq_num != IPC_IRQ_NONE) {
		/* init rx interrupt */
		err = request_irq(priv.id[instance].irq_num, ipc_shm_hardirq,
			0, DRIVER_NAME, &priv);
		if (err) {
			shm_err("Request interrupt %d failed\n",
				priv.id[instance].irq_num);
			goto err_unmap_remote_shm;
		}
	}

	priv.id[instance].state = IPC_SHM_INSTANCE_ENABLED;

	return 0;

err_unmap_remote_shm:
	iounmap((void *)priv.id[instance].remote_virt_shm);
err_release_remote_region:
	release_mem_region((phys_addr_t)cfg->remote_shm_addr, cfg->shm_size);
err_unmap_local_shm:
	iounmap((void *)priv.id[instance].local_virt_shm);
err_release_local_region:
	release_mem_region((phys_addr_t)cfg->local_shm_addr, cfg->shm_size);

	return err;
}

void ipc_os_free(const uint8_t instance)
{
	uint8_t inst_id = 0;
	int keep_irq_enable = 0;

	priv.id[instance].state = IPC_SHM_INSTANCE_DISABLED;

	if (priv.id[instance].irq_num != IPC_IRQ_NONE) {
		for (inst_id = 0; inst_id < IPC_SHM_MAX_INSTANCES; inst_id++) {
			if (instance == inst_id)
				continue;
			if (priv.id[inst_id].irq_num
					== priv.id[instance].irq_num) {
				keep_irq_enable++;
				break;
			}
		}
		/* disable notifications from remote */
		if (keep_irq_enable == 0) {
			ipc_hw_irq_disable(instance);
			/* only free irq if irq number is requested */
			if (priv.id[instance].irq_num != 0) {
				free_irq(priv.id[instance].irq_num, &priv);
			}
		}
		priv.id[instance].irq_num = 0;
	}

	iounmap((void *)priv.id[instance].remote_virt_shm);
	release_mem_region((phys_addr_t)priv.id[instance].remote_phys_shm,
		priv.id[instance].shm_size);
	iounmap((void *)priv.id[instance].local_virt_shm);
	release_mem_region((phys_addr_t)priv.id[instance].local_phys_shm,
		priv.id[instance].shm_size);
}

uintptr_t ipc_os_get_local_shm(const uint8_t instance)
{
	return priv.id[instance].local_virt_shm;
}

uintptr_t ipc_os_get_remote_shm(const uint8_t instance)
{
	return priv.id[instance].remote_virt_shm;
}

/**
 * ipc_os_map_intc() - I/O memory map interrupt controller register space
 *
 * I/O memory map the inter-core interrupts HW block (MSCM for ARM processors)
 * or Mru HW block
 */
void *ipc_os_map_intc(void)
{
	struct device_node *node = NULL;
	struct resource res;
	int err;

	/* get DT node */
	node = of_find_compatible_node(NULL, NULL, DT_INTC_NODE_COMP);
	if (!node) {
		shm_err("Unable to find interrupt node in device tree\n");
		return NULL;
	}

	/* get base address from DT node */
	err = of_address_to_resource(node, 0, &res);
	of_node_put(node);
	if (err) {
		shm_err("Unable to read regs address from DT interrupt node\n");
		return NULL;
	}

	/* map configuration register space */
	return ioremap(res.start, resource_size(&res));
}

/**
 * ipc_os_unmap_intc() - I/O memory unmap interrupt controller register space
 */
void ipc_os_unmap_intc(void *addr)
{
	iounmap(addr);
}

int8_t ipc_os_poll_channels(const uint8_t instance)
{
	/* the softirq will handle rx operation if rx interrupt is configured */
	if (priv.id[instance].irq_num == IPC_IRQ_NONE) {
		if (priv.id[instance].rx_cb != NULL)
			return priv.id[instance].rx_cb(instance,
				IPC_SOFTIRQ_BUDGET);
		return -EINVAL;
	}

	return -EOPNOTSUPP;
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
	/* kill softirq task */
	tasklet_kill(&ipc_shm_rx_tasklet);
}

EXPORT_SYMBOL(ipc_shm_init_instance);
EXPORT_SYMBOL(ipc_shm_init);
EXPORT_SYMBOL(ipc_shm_free_instance);
EXPORT_SYMBOL(ipc_shm_free);
EXPORT_SYMBOL(ipc_shm_acquire_buf);
EXPORT_SYMBOL(ipc_shm_release_buf);
EXPORT_SYMBOL(ipc_shm_tx);
EXPORT_SYMBOL(ipc_shm_unmanaged_acquire);
EXPORT_SYMBOL(ipc_shm_unmanaged_tx);
EXPORT_SYMBOL(ipc_shm_is_remote_ready);
EXPORT_SYMBOL(ipc_shm_poll_channels);

module_init(shm_mod_init);
module_exit(shm_mod_exit);

MODULE_AUTHOR("NXP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS(DRIVER_NAME);
MODULE_DESCRIPTION("NXP Shared Memory Inter-Processor Communication Driver");
MODULE_VERSION(DRIVER_VERSION);
