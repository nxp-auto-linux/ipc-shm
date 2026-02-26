/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018-2026 NXP
 */
#include "ipc-os.h"

#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/interrupt.h>
/* Conditionally include mailbox support */
#if IPC_MAILBOX_ENABLED
#include <linux/mailbox_client.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#endif
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/version.h>

#include "ipc-hw.h"
#include "ipc-shm.h"

#define DRIVER_VERSION	"0.1"

/* Device tree MSCM node compatible property (search key) */
#if defined(PLATFORM_FLAVOR_s32g2) || defined(PLATFORM_FLAVOR_s32g3) || \
	defined(PLATFORM_FLAVOR_s32r45) || defined(PLATFORM_FLAVOR_s32r47)
	#define DT_INTC_NODE_COMP "nxp,s32cc-mscm"
#elif defined(PLATFORM_FLAVOR_s32v234)
	#define DT_INTC_NODE_COMP "fsl,s32v234-mscm"
#elif defined(PLATFORM_FLAVOR_s32n7)
	#define DT_INTC_NODE_COMP "nxp,s32n79-ipc-shm"
	#define MAILBOX_NAME_SIZE (8U)
	#define IPC_SHM_MAGIC_NUM (0x49504346U)
#if !IPC_MAILBOX_ENABLED
	#warning "CONFIG_MAILBOX and CONFIG_NXP_S32N79_MRU must be enabled \
		for interrupt support on s32n7 platform"
#endif
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
 * @mbox_tx_client:     mailbox transmit client (if mailbox enabled)
 * @mbox_rx_client:     mailbox receive client (if mailbox enabled)
 * @mbox_tx_channel:    mailbox transmit channel (if mailbox enabled)
 * @mbox_rx_channel:    mailbox receive channel (if mailbox enabled)
 * @rx_irq:             Linux IRQ number
 * @state:              state to indicate whether instance is initialized
 * @rx_cb:              upper layer rx callback
 */
struct ipc_os_priv_instance {
	int shm_size;
	uintptr_t local_phys_shm;
	uintptr_t remote_phys_shm;
	uintptr_t local_virt_shm;
	uintptr_t remote_virt_shm;
#if IPC_MAILBOX_ENABLED
	struct mbox_client mbox_tx_client;
	struct mbox_client mbox_rx_client;
	struct mbox_chan *mbox_tx_channel;
	struct mbox_chan *mbox_rx_channel;
#endif
	int rx_irq;
	int state;
	uint32_t (*rx_cb)(const uint8_t instance, int budget);
};

/**
 * struct ipc_os_priv_type - OS specific private data
 * @id:             private data per instance
 */
static struct ipc_os_priv_type {
	struct ipc_os_priv_instance id[IPC_SHM_MAX_INSTANCES];
} ipc_os_priv;

static void ipc_shm_softirq(unsigned long arg);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
static DECLARE_TASKLET(ipc_shm_rx_tasklet, ipc_shm_softirq, 0);
#else
static DECLARE_TASKLET_OLD(ipc_shm_rx_tasklet, ipc_shm_softirq);
#endif

/* sotfirq routine for deferred interrupt handling */
static void ipc_shm_softirq(unsigned long arg)
{
	unsigned long budget = IPC_SOFTIRQ_BUDGET;
	uint8_t i = 0;
	int work = 0;

	for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
		if ((ipc_os_priv.id[i].state == IPC_SHM_INSTANCE_DISABLED)
				|| (ipc_os_priv.id[i].rx_irq == IPC_IRQ_NONE))
			continue;

		/* Do the budgeted work */
		work = ipc_os_priv.id[i].rx_cb(i, budget);

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

#if IPC_S32N7_MAILBOX_ENABLED
static void ipc_shm_hardirq(struct mbox_client *client, void *msg)
#else
/* driver interrupt service routine */
static irqreturn_t ipc_shm_hardirq(int irq, void *dev)
#endif
{
	uint8_t i = 0;

	for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
		if ((ipc_os_priv.id[i].state == IPC_SHM_INSTANCE_DISABLED)
				|| (ipc_os_priv.id[i].rx_irq == IPC_IRQ_NONE))
			continue;

		/* disable notifications from remote */
		ipc_hw_irq_disable(i);

		/* clear notification */
		ipc_hw_irq_clear(i);
	}

	tasklet_schedule(&ipc_shm_rx_tasklet);

#if IPC_S32N7_MAILBOX_ENABLED
	/* Do nothing */
#else
	return IRQ_HANDLED;
#endif
}


#if IPC_S32N7_MAILBOX_ENABLED

/**
 * ipc_os_mailbox_init() - Initialize mailbox channels for IPC instance
 * @instance: IPC instance number
 * @cfg:      IPC shared memory configuration structure
 *
 * Initializes mailbox communication channels for the specified IPC instance.
 * Sets up both RX and TX channels based on the configuration. If no IRQs are
 * configured, the instance is enabled without mailbox setup.
 *
 * Return:
 * * %0                   - On success
 * * %Negative error code - On error
 */
static int ipc_os_mailbox_init(const uint8_t instance,
	const struct ipc_shm_cfg *cfg)
{
	struct device_node *compatible_node = NULL;
	char tx_name[MAILBOX_NAME_SIZE];
	char rx_name[MAILBOX_NAME_SIZE];
	struct platform_device *pdev;
	int err;

	if ((IPC_IRQ_NONE == cfg->inter_core_rx_irq)
			&& (IPC_IRQ_NONE == cfg->inter_core_tx_irq)) {
		ipc_os_priv.id[instance].rx_irq = IPC_IRQ_NONE;
		ipc_os_priv.id[instance].state = IPC_SHM_INSTANCE_ENABLED;
		return 0;
	}

	/* Find device tree node by compatible */
	compatible_node = of_find_compatible_node(
			NULL, NULL, DT_INTC_NODE_COMP);
	if (!compatible_node) {
		shm_err("Cannot find node with compatible %s in device tree\n",
			DT_INTC_NODE_COMP);
		return -ENXIO;
	}

	pdev = of_find_device_by_node(compatible_node);
	of_node_put(compatible_node);
	if (!pdev) {
		shm_err("Cannot find platform device for node %s\n",
			DT_INTC_NODE_COMP);
		return -ENODEV;
	}

	/* Get mailbox names based on instance */
	snprintf(tx_name, sizeof(tx_name), "tx_%d", instance);
	snprintf(rx_name, sizeof(rx_name), "rx_%d", instance);

	if (IPC_IRQ_NONE != cfg->inter_core_rx_irq) {
		ipc_os_priv.id[instance].mbox_rx_client.dev
			= &pdev->dev;
		ipc_os_priv.id[instance].mbox_rx_client.rx_callback
			= ipc_shm_hardirq;
		ipc_os_priv.id[instance].mbox_rx_client.tx_done = NULL;

		/* Request RX channel from mailbox controller
		 * (receives from A78)
		 */
		ipc_os_priv.id[instance].mbox_rx_channel
			= mbox_request_channel_byname(
				&ipc_os_priv.id[instance].mbox_rx_client,
				rx_name);
		if (IS_ERR(ipc_os_priv.id[instance].mbox_rx_channel)) {
			err = PTR_ERR(ipc_os_priv.id[instance].mbox_rx_channel);
			shm_err("Failed to request RX mailbox %s: %d\n",
				rx_name, err);
			platform_device_put(pdev);
			return err;
		}
	}

	if (IPC_IRQ_NONE != cfg->inter_core_tx_irq) {
		ipc_os_priv.id[instance].mbox_tx_client.dev
			= &pdev->dev;
		ipc_os_priv.id[instance].mbox_tx_client.rx_callback = NULL;
		ipc_os_priv.id[instance].mbox_tx_client.tx_done = NULL;
		ipc_os_priv.id[instance].mbox_tx_client.knows_txdone = true;
		ipc_os_priv.id[instance].mbox_tx_client.tx_block = false;

		/* Request TX channel from mailbox controller
		 * (sends to another core)
		 */
		ipc_os_priv.id[instance].mbox_tx_channel
			= mbox_request_channel_byname(
				&ipc_os_priv.id[instance].mbox_tx_client,
				tx_name);
		if (IS_ERR(ipc_os_priv.id[instance].mbox_tx_channel)) {
			err = PTR_ERR(ipc_os_priv.id[instance].mbox_tx_channel);
			shm_err("Failed to request TX mailbox %s: %d\n",
				tx_name, err);
			mbox_free_channel(
				ipc_os_priv.id[instance].mbox_rx_channel);
			platform_device_put(pdev);
			return err;
		}
	}

	ipc_os_priv.id[instance].rx_irq = cfg->inter_core_rx_irq;
	ipc_os_priv.id[instance].state = IPC_SHM_INSTANCE_ENABLED;

	/* Clean up platform device reference */
	platform_device_put(pdev);

	return 0;
}

/**
 * ipc_os_mailbox_free() - Free mailbox channels for an IPC instance
 * @instance: IPC instance number
 *
 * Releases both RX and TX mailbox channels associated with the specified
 * IPC instance and sets the channel pointers to NULL.
 */
static void ipc_os_mailbox_free(const uint8_t instance)
{
	if (ipc_os_priv.id[instance].mbox_rx_channel) {
		mbox_free_channel(ipc_os_priv.id[instance].mbox_rx_channel);
		ipc_os_priv.id[instance].mbox_rx_channel = NULL;
	}

	if (ipc_os_priv.id[instance].mbox_tx_channel) {
		mbox_free_channel(ipc_os_priv.id[instance].mbox_tx_channel);
		ipc_os_priv.id[instance].mbox_tx_channel = NULL;
	}
}
#else
/* Return interrupt number from a device tree node */
static int8_t ipc_os_get_irq_id(const uint8_t instance,
	const struct ipc_shm_cfg *cfg, int *rx_irq)
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
	*rx_irq = of_irq_get(compatible_node, ipc_hw_get_rx_irq(instance));
	if (*rx_irq <= 0) {
		shm_err("Unable to decode node's IRQ\n");
		of_node_put(compatible_node);
		return -EINVAL;
	}
	shm_dbg("Rx IRQ of instance %d = %d\n",
		instance, *rx_irq);

	/* release refcount to interrupt DT node */
	of_node_put(compatible_node);

	return 0;
}

static int8_t ipc_os_irq_init(const uint8_t instance,
	const struct ipc_shm_cfg *cfg)
{
	int err;
	int i;

	if (cfg->inter_core_rx_irq == IPC_IRQ_NONE) {
		ipc_os_priv.id[instance].rx_irq = IPC_IRQ_NONE;
		ipc_os_priv.id[instance].state = IPC_SHM_INSTANCE_ENABLED;
		return 0;
	}

	err = ipc_os_get_irq_id(instance, cfg,
		&ipc_os_priv.id[instance].rx_irq);
	if (err != 0) {
		return -EINVAL;
	}

	/* check duplicate irq number */
	for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
		if (instance == i)
			continue;
		if (ipc_os_priv.id[instance].rx_irq
				== ipc_os_priv.id[i].rx_irq) {
			ipc_os_priv.id[instance].state
				= IPC_SHM_INSTANCE_ENABLED;
			return 0;
		}
	}

	/* init rx interrupt */
	err = request_irq(ipc_os_priv.id[instance].rx_irq, ipc_shm_hardirq,
		0, DRIVER_NAME, &ipc_os_priv);
	if (err) {
		shm_err("Request interrupt %d failed\n",
			ipc_os_priv.id[instance].rx_irq);
		return -EINVAL;
	}

	return 0;
}
#endif

int8_t ipc_os_init(const uint8_t instance, const struct ipc_shm_cfg *cfg,
		uint32_t (*rx_cb)(const uint8_t, int))
{
	struct resource *res;
	int err;

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

	ipc_os_priv.id[instance].local_virt_shm
		= (uintptr_t)ioremap(cfg->local_shm_addr, cfg->shm_size);
	if (!ipc_os_priv.id[instance].local_virt_shm) {
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

	ipc_os_priv.id[instance].remote_virt_shm
		= (uintptr_t)ioremap(cfg->remote_shm_addr, cfg->shm_size);
	if (!ipc_os_priv.id[instance].remote_virt_shm) {
		err = -ENOMEM;
		goto err_release_remote_region;
	}

	/* save params */
	ipc_os_priv.id[instance].shm_size = cfg->shm_size;
	ipc_os_priv.id[instance].local_phys_shm = cfg->local_shm_addr;
	ipc_os_priv.id[instance].remote_phys_shm = cfg->remote_shm_addr;
	ipc_os_priv.id[instance].rx_cb = rx_cb;

#if IPC_S32N7_MAILBOX_ENABLED
	err = ipc_os_mailbox_init(instance, cfg);
	if (err != 0) {
		shm_err("Failed to initialize mailbox for instance %d\n",
			instance);
		goto err_unmap_remote_shm;
	}
#else
	err = ipc_os_irq_init(instance, cfg);
	if (err != 0) {
		shm_err("Failed to initialize IRQ for instance %d\n", instance);
		goto err_unmap_remote_shm;
	}
#endif

	ipc_os_priv.id[instance].state = IPC_SHM_INSTANCE_ENABLED;

	return 0;

err_unmap_remote_shm:
	iounmap((void *)ipc_os_priv.id[instance].remote_virt_shm);
err_release_remote_region:
	release_mem_region((phys_addr_t)cfg->remote_shm_addr, cfg->shm_size);
err_unmap_local_shm:
	iounmap((void *)ipc_os_priv.id[instance].local_virt_shm);
err_release_local_region:
	release_mem_region((phys_addr_t)cfg->local_shm_addr, cfg->shm_size);

	return err;
}

void ipc_os_free(const uint8_t instance)
{
#if !(IPC_S32N7_MAILBOX_ENABLED)
	int keep_irq_enable = 0;
	uint8_t inst_id = 0;
#endif

	ipc_os_priv.id[instance].state = IPC_SHM_INSTANCE_DISABLED;
	ipc_os_priv.id[instance].rx_cb = NULL;

	if (ipc_os_priv.id[instance].rx_irq != IPC_IRQ_NONE) {
#if IPC_S32N7_MAILBOX_ENABLED
		ipc_os_mailbox_free(instance);
#else
		for (inst_id = 0; inst_id < IPC_SHM_MAX_INSTANCES; inst_id++) {
			if (instance == inst_id)
				continue;
			if (ipc_os_priv.id[inst_id].rx_irq
					== ipc_os_priv.id[instance].rx_irq) {
				keep_irq_enable++;
				break;
			}
		}
		/* disable notifications from remote */
		if (keep_irq_enable == 0) {
			ipc_hw_irq_disable(instance);
			/* only free irq if irq number is requested */
			if (ipc_os_priv.id[instance].rx_irq != 0) {
				free_irq(ipc_os_priv.id[instance].rx_irq,
					&ipc_os_priv);
			}
		}
		ipc_os_priv.id[instance].rx_irq = 0;
#endif
	}

	iounmap((void *)ipc_os_priv.id[instance].remote_virt_shm);
	release_mem_region(
		(phys_addr_t)ipc_os_priv.id[instance].remote_phys_shm,
		ipc_os_priv.id[instance].shm_size);
	iounmap((void *)ipc_os_priv.id[instance].local_virt_shm);
	release_mem_region(
		(phys_addr_t)ipc_os_priv.id[instance].local_phys_shm,
		ipc_os_priv.id[instance].shm_size);
}

uintptr_t ipc_os_get_local_shm(const uint8_t instance)
{
	return ipc_os_priv.id[instance].local_virt_shm;
}

uintptr_t ipc_os_get_remote_shm(const uint8_t instance)
{
	return ipc_os_priv.id[instance].remote_virt_shm;
}

uintptr_t ipc_os_ioremap(uintptr_t phys_addr, uint32_t size)
{
	struct resource *res;
	uintptr_t virt_mem;

	/* request and map target physical memory */
	res = request_mem_region((phys_addr_t)phys_addr, size,
		DRIVER_NAME" remote");
	if (!res) {
		shm_err("Unable to reserve target memory region\n");
		return (uintptr_t)NULL;
	}

	virt_mem = (uintptr_t)ioremap(phys_addr, size);
	if (!virt_mem) {
		release_mem_region((phys_addr_t)phys_addr, size);
		return (uintptr_t)NULL;
	}

	return virt_mem;
}

void ipc_os_iounmap(uintptr_t phys_addr, uintptr_t vir_addr, uint32_t size)
{
	/* Unmap and release memory */
	iounmap((void *)vir_addr);
	release_mem_region((phys_addr_t)phys_addr, size);
}

#if IPC_S32N7_MAILBOX_ENABLED
void ipc_os_mailbox_tx(const uint8_t instance)
{
	uint32_t IPC_SHM_SENDING = IPC_SHM_MAGIC_NUM;
	int err;

	/* Send IPC notification message through mailbox channel */
	err = mbox_send_message(ipc_os_priv.id[instance].mbox_tx_channel,
		&IPC_SHM_SENDING);
	if (err < 0) {
		shm_err("Failed to send test message: %d\n", err);
		return;
	}

	/* Signal mailbox framework that transmission is complete */
	mbox_client_txdone(ipc_os_priv.id[instance].mbox_tx_channel, 0);
}
#endif

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

void ipc_os_unmap_intc(void *addr)
{
	iounmap(addr);
}

int8_t ipc_os_poll_channels(const uint8_t instance)
{
	/* the softirq will handle rx operation if rx interrupt is configured */
	if (ipc_os_priv.id[instance].rx_irq == IPC_IRQ_NONE) {
		if (ipc_os_priv.id[instance].rx_cb != NULL)
			return ipc_os_priv.id[instance].rx_cb(instance,
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
EXPORT_SYMBOL(ipc_shm_free_scmi_channel);
EXPORT_SYMBOL(ipc_shm_is_channel_not_empty);
EXPORT_SYMBOL(ipc_shm_get_message_from_channel);
EXPORT_SYMBOL(ipc_shm_get_channel_type);
EXPORT_SYMBOL(ipc_shm_scmi_tx);
EXPORT_SYMBOL(ipc_shm_acquire_scmi);

module_init(shm_mod_init);
module_exit(shm_mod_exit);

MODULE_AUTHOR("NXP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS(DRIVER_NAME);
MODULE_DESCRIPTION("NXP Shared Memory Inter-Processor Communication Driver");
MODULE_VERSION(DRIVER_VERSION);
