/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2019,2021-2025 NXP
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/uio_driver.h>
#include <linux/cdev.h>
#include <linux/version.h>

#include "ipc-shm.h"
#include "ipc-os.h"
#include "ipc-hw.h"
#include "ipc-uio.h"

#if defined(PLATFORM_FLAVOR_s32n7)
	#include <linux/io.h>
	#include <linux/of_address.h>
	#include <linux/of_irq.h>

	#define DT_INTC_NODE_COMP     "nxp,s32n79-mru"
	#define IPCF_MAX_COMP_STR 32u
#endif
#define IPC_CDEV_NAME		"ipc-cdev-uio"
#define IPC_UIO_NAME		"ipc-shm-uio"
#define IPC_UIO_VERSION		"2.0"

static DEFINE_MUTEX(mmap_device_mutex);
/**
 * struct ipc_uio_priv_type - IPCF SHM UIO device data
 * @state:      instance state (initialized or not)
 * @irq_num:    rx interrupt number using to request irq
 * @dev:        Linux device capabilities
 * @refcnt:     reference counter to allow a single UIO device open at a time
 * @info:       UIO device capabilities
 * @data:       Chrdev private data to get user space configuration
 * @uio_name:   UIO device name
 */
struct ipc_uio_priv_type {
	int state;
	int irq_num;
	struct device *dev;
	atomic_t refcnt;
	struct uio_info info;
	struct ipc_uio_cdev_data data;
	char uio_name[32];
};

/**
 * struct ipc_pdev_priv_type - IPCF SHM platform device data
 * @major:           chrdev major number which is allocated dynamically
 * @irq_num_init:    interrupt index for each instance
 * @ipc_pdev:        Linux platform device capabilities
 * @pdev_reg:        Platform device register
 * @cdev_class:      class use to create device file in /dev
 * @cdev:            variable use for character device
 * @uio_id:          IPCF SHM UIO device data for each instance
 */
struct ipc_pdev_priv_type {
#if defined(PLATFORM_FLAVOR_s32n7)
	uint8_t dev_created;
	char mru_node_name[IPCF_MAX_COMP_STR];
#endif
	dev_t major;
	int irq_num_init[IPC_SHM_MAX_INSTANCES];
	struct platform_device *ipc_pdev;
#if !defined(PLATFORM_FLAVOR_s32n7)
	void __iomem *pdev_reg;
#else
	void __iomem *pdev_reg[IPC_SHM_MAX_INSTANCES];
#endif
	struct class *cdev_class;
	struct cdev cdev;
	struct ipc_uio_priv_type uio_id[IPC_SHM_MAX_INSTANCES];
} ipc_pdev_priv;

/**
 * ipc_shm_uio_open() - open operation will respond to a open UIO request
 */
static int ipc_shm_uio_open(struct uio_info *info, struct inode *inode)
{
	struct ipc_uio_priv_type *priv = info->priv;

	if (!atomic_dec_and_test(&priv->refcnt)) {
		shm_err("%s device already opened\n", info->name);
		atomic_inc(&priv->refcnt);
		return -EBUSY;
	}

	return 0;
}

/**
 * ipc_shm_uio_release() - release operation will respond to a close UIO request
 */
static int ipc_shm_uio_release(struct uio_info *info, struct inode *inode)
{
	struct ipc_uio_priv_type *priv = info->priv;

	atomic_inc(&priv->refcnt);

	return 0;
}
/**
 * ipc_shm_uio_irqcontrol() - irqcontrol operation will respond to
 *                            a irqcontrol UIO request
 */
static int ipc_shm_uio_irqcontrol(struct uio_info *dev_info, int cmd)
{
	struct ipc_uio_priv_type *info = dev_info->priv;
	uint8_t instance = info->data.instance;

	switch (cmd) {
	case IPC_UIO_DISABLE_CMD:
		ipc_hw_irq_disable(instance);
		break;
	case IPC_UIO_ENABLE_CMD:
		ipc_hw_irq_enable(instance);
		break;
	case IPC_UIO_TRIGGER_CMD:
		ipc_hw_irq_notify(instance);
		break;
	default:
		break;
	}
	return 0;
}

/* hardirq handler */
static irqreturn_t ipc_shm_uio_handler(int irq, struct uio_info *dev_info)
{
	struct ipc_uio_priv_type *info = dev_info->priv;
	uint8_t instance = info->data.instance;

	ipc_hw_irq_disable(instance);
	ipc_hw_irq_clear(instance);

	/* return IRQ_HANDLED to trigger uio_event_notify() to user-space */
	return IRQ_HANDLED;
}

#if defined(PLATFORM_FLAVOR_s32n7)
static int8_t get_irq_number(const uint8_t instance,
	const struct ipc_shm_cfg *cfg, int *irq_num)
{
	struct device_node *compatible_node = NULL;
	uint8_t subsystem_id = cfg->mru_rx.subsystem_id;
	uint8_t mru_id = cfg->mru_rx.mru_id;

	/* Get the node name */
	snprintf(ipc_pdev_priv.mru_node_name, IPCF_MAX_COMP_STR,
			"aps%d_mru%d", subsystem_id, mru_id);

	for_each_compatible_node(compatible_node, NULL, DT_INTC_NODE_COMP) {
		if (of_node_name_eq(compatible_node,
				ipc_pdev_priv.mru_node_name)) {
			shm_dbg("Get node with name is %s\n",
				ipc_pdev_priv.mru_node_name);

			*irq_num = of_irq_get(
				compatible_node, ipc_hw_get_rx_irq(instance));
			if (*irq_num <= 0) {
				shm_err("Unable decode node's IRQ\n");
				of_node_put(compatible_node);
				return -EINVAL;
			}
			shm_dbg("Rx IRQ of instance %d = %d\n",
				instance, *irq_num);

			/* release refcount to interrupt DT node */
			of_node_put(compatible_node);
		}
	}

	return 0;
}
#endif

/**
 * ipc_uio_init() - Initialize an UIO and ipcf hw
 * @ipc_uio_cdev_data:    Chrdev private data from user space
 *
 * Return:
 * * %0				- on success
 * * %Negative error code	- On error
 */
static int ipc_uio_init(struct ipc_uio_cdev_data *data)
{
	int err, irq, i;
	int instance = data->instance;
	struct ipc_shm_cfg *cfg = &data->cfg;

	err = sprintf(ipc_pdev_priv.uio_id[instance].uio_name,
			"instance_%d", instance);
	/* init Peripheral HW for inter-core interrupts */
	err = ipc_hw_init(instance, cfg);
	if (err)
		return err;

	if (cfg->inter_core_rx_irq == IPC_IRQ_NONE)
		return 0;
	/* get Linux IRQ number */
#if !defined(PLATFORM_FLAVOR_s32n7)
	irq = platform_get_irq(ipc_pdev_priv.ipc_pdev,
		ipc_hw_get_rx_irq(instance));
	if (irq < 0) {
		shm_dbg("Failed to get IRQ\n");
		return irq;
	}
	shm_dbg("GIC Rx IRQ = %d\n", irq);
#else
	err = get_irq_number(instance, cfg, &irq);
	if (err) {
		shm_dbg("Failed to get IRQ get_irq_number\n");
		return err;
	}
#endif

	/* check duplicate irq number */
	for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
		if (irq == ipc_pdev_priv.irq_num_init[i]) {
			shm_dbg("Can't create UIO device with same irq %d\n",
				irq);
			return -EFAULT;
		}
	}
	ipc_pdev_priv.irq_num_init[instance] = irq;
	ipc_pdev_priv.uio_id[instance].dev = &ipc_pdev_priv.ipc_pdev->dev;
	/* Register UIO device */
	atomic_set(&ipc_pdev_priv.uio_id[instance].refcnt, 1);
	ipc_pdev_priv.uio_id[instance].info.version = IPC_UIO_VERSION;
	ipc_pdev_priv.uio_id[instance].info.name
				= ipc_pdev_priv.uio_id[instance].uio_name;
	ipc_pdev_priv.uio_id[instance].info.irq
				= ipc_pdev_priv.irq_num_init[instance];
	ipc_pdev_priv.uio_id[instance].info.irq_flags = 0;
	ipc_pdev_priv.uio_id[instance].info.handler = ipc_shm_uio_handler;
	ipc_pdev_priv.uio_id[instance].info.irqcontrol = ipc_shm_uio_irqcontrol;
	ipc_pdev_priv.uio_id[instance].info.open = ipc_shm_uio_open;
	ipc_pdev_priv.uio_id[instance].info.release = ipc_shm_uio_release;
	ipc_pdev_priv.uio_id[instance].info.priv
			= &ipc_pdev_priv.uio_id[instance];

	err = uio_register_device(
			ipc_pdev_priv.uio_id[instance].dev,
			&ipc_pdev_priv.uio_id[instance].info);
	if (err) {
		shm_dbg("UIO registration failed\n");
		return err;
	}
	ipc_pdev_priv.uio_id[instance].state = IPC_SHM_INSTANCE_ENABLED;

	/* Get minor number of UIO */


	return 0;
}

static int ipc_uio_free(struct ipc_uio_cdev_data *data)
{
	int err = -EINVAL;
	uint8_t inst = data->instance;

	if ((inst < IPC_SHM_MAX_INSTANCES)
		&& (ipc_pdev_priv.uio_id[inst].state
			== IPC_SHM_INSTANCE_ENABLED)
		&& (ipc_pdev_priv.irq_num_init[inst]
			!= IPC_IRQ_NONE)) {
		uio_unregister_device(&ipc_pdev_priv.uio_id[inst].info);
		ipc_pdev_priv.irq_num_init[inst] = IPC_IRQ_NONE;
		err = 0;
	}

	return err;
}

/**
 * ipc_cdev_open() - open operation will respond to a open request
 *                   from user-space
 */
static int ipc_cdev_open(struct inode *inode, struct file *filp)
{
	if (!mutex_trylock(&mmap_device_mutex)) {
		shm_dbg("Another process is accessing the device\n");
		return -EBUSY;
	}

	filp->private_data = &ipc_pdev_priv;

	return 0;
}
/**
 * ipc_cdev_release() - release operation will respond to a close request
 *                      from user-space
 */
static int ipc_cdev_release(struct inode *inode, struct file *filp)
{
	mutex_unlock(&mmap_device_mutex);

	filp->private_data = NULL;
	return 0;
}

/**
 * ipc_cdev_write() - write operation will respond to a write request
 *                    from user-space
 */
static ssize_t ipc_cdev_write(struct file *file, const char __user *user_buffer,
					size_t size, loff_t *offset)
{
	int err;
	struct ipc_uio_cdev_data data;

	/* read data from user buffer to my_data->buffer */
	if (copy_from_user(&data, user_buffer,
			sizeof(struct ipc_uio_cdev_data)))
		return -EFAULT;

	if (data.instance >= IPC_SHM_MAX_INSTANCES)
		return -EINVAL;

	ipc_pdev_priv.uio_id[data.instance].data = data;
	if (data.cmd == IPC_UIO_INIT) {
		err = ipc_uio_init(&ipc_pdev_priv.uio_id[data.instance].data);
		if (err) {
			shm_err("Failed to initialized UIO device\n");
			return err;
		}
	} else if (data.cmd == IPC_UIO_FREE) {
		err = ipc_uio_free(&ipc_pdev_priv.uio_id[data.instance].data);
		if (err) {
			shm_err("Failed to free UIO device\n");
			return err;
		}
	}

	return sizeof(struct ipc_uio_cdev_data);
}

/* File operations */
static const struct file_operations ipc_cdev_fops = {
	.owner = THIS_MODULE,
	.open = ipc_cdev_open,
	.release = ipc_cdev_release,
	.write = ipc_cdev_write,
};

/* Platform driver probe methods */
static int ipc_shm_uio_probe(struct platform_device *pdev)
{
	int err;
	int i;
	struct resource *res;

#if defined(PLATFORM_FLAVOR_s32n7)
	if (ipc_pdev_priv.dev_created == 1)
		return 0;
#endif

	for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
		ipc_pdev_priv.uio_id[i].irq_num = IPC_IRQ_NONE;
		ipc_pdev_priv.irq_num_init[i] = IPC_IRQ_NONE;
	}

	platform_set_drvdata(pdev, ipc_pdev_priv.uio_id);
	ipc_pdev_priv.ipc_pdev = pdev;

#if !defined(PLATFORM_FLAVOR_s32n7)
	/* map MSCM register configuration space */
	res = platform_get_resource(ipc_pdev_priv.ipc_pdev, IORESOURCE_MEM, 0);
	ipc_pdev_priv.pdev_reg = devm_ioremap_resource(
					&ipc_pdev_priv.ipc_pdev->dev, res);
	if (IS_ERR_OR_NULL(ipc_pdev_priv.pdev_reg)) {
		shm_err("Failed to map MSCM register space\n");
		return -ENOMEM;
	}
#else
	/* Surpress warning of unused variable */
	(void)res;
#endif

	/* Create a chrdev to initial user-kernel communication */
	/* Dynamic allocate device major number */
	err = alloc_chrdev_region(&ipc_pdev_priv.major, 0, 1, IPC_CDEV_NAME);
	if (err < 0) {
		shm_err("Failed to alloc chrdev region\n");
		goto fail_alloc_chrdev_region;
	}
	cdev_init(&ipc_pdev_priv.cdev, &ipc_cdev_fops);
	err = cdev_add(&ipc_pdev_priv.cdev, ipc_pdev_priv.major, 1);
	if (err < 0) {
		shm_err("Failed to add cdev\n");
		goto fail_add_cdev;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
	ipc_pdev_priv.cdev_class = class_create(THIS_MODULE, IPC_CDEV_NAME);
#else
	ipc_pdev_priv.cdev_class = class_create(IPC_CDEV_NAME);
#endif
	if (!ipc_pdev_priv.cdev_class) {
		err = -EEXIST;
		shm_err("Failed to create class\n");
		goto fail_create_class;
	}
	if (!device_create(ipc_pdev_priv.cdev_class, NULL,
			ipc_pdev_priv.major, NULL, IPC_CDEV_NAME)) {
		err = -EINVAL;
		shm_err("Failed to create device\n");
		goto fail_create_device;
	}
	mutex_init(&mmap_device_mutex);
	shm_dbg("%s registered with major %d\n",
		IPC_CDEV_NAME, MAJOR(ipc_pdev_priv.major));

#if defined(PLATFORM_FLAVOR_s32n7)
	ipc_pdev_priv.dev_created = 1;
#endif

	shm_dbg("device ready\n");

	return 0;

fail_create_device:
	class_destroy(ipc_pdev_priv.cdev_class);
fail_create_class:
	cdev_del(&ipc_pdev_priv.cdev);
fail_add_cdev:
	unregister_chrdev_region(ipc_pdev_priv.major, 1);
fail_alloc_chrdev_region:
	return err;
}

/* Platform driver remove methods */
static void ipc_shm_uio_remove(struct platform_device *pdev)
{
	int i;

#if defined(PLATFORM_FLAVOR_s32n7)
	if (ipc_pdev_priv.dev_created == 0)
		return;
#endif
	device_destroy(ipc_pdev_priv.cdev_class, ipc_pdev_priv.major);
	class_destroy(ipc_pdev_priv.cdev_class);
	cdev_del(&ipc_pdev_priv.cdev);
	unregister_chrdev_region(ipc_pdev_priv.major, 1);
	for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
		if (ipc_pdev_priv.uio_id[i].state == IPC_SHM_INSTANCE_ENABLED) {
			ipc_pdev_priv.uio_id[i].state
				= IPC_SHM_INSTANCE_DISABLED;
			if (ipc_pdev_priv.irq_num_init[i] == IPC_IRQ_NONE)
				continue;
			uio_unregister_device(&ipc_pdev_priv.uio_id[i].info);
		}
	}

#if defined(PLATFORM_FLAVOR_s32n7)
	ipc_pdev_priv.dev_created = 0;
#endif
	shm_dbg("device removed\n");
}

#if !defined(PLATFORM_FLAVOR_s32n7)
void *ipc_os_map_intc(void)
{
	return ipc_pdev_priv.pdev_reg;
}
#else
/**
 * ipc_os_mru_map_intc() - I/O memory map interrupt controller register space
 *
 * I/O memory map the inter-core interrupts HW block (Mru HW block)
 */
void *ipc_os_mru_map_intc(uint8_t mru_instance,
	uint8_t subsystem_id, uint8_t mru_id)
{
	struct device_node *node = NULL;
	struct resource res;
	void *peripheral_addr = NULL;
	int err;

	/* get DT node */
	switch (mru_instance) {
	case (uint8_t)IPC_MRU_APS_INSTANCE:
		snprintf(ipc_pdev_priv.mru_node_name, IPCF_MAX_COMP_STR,
			"aps%d_mru%d", subsystem_id, mru_id);
		break;
	case (uint8_t)IPC_MRU_FSS_INSTANCE:
		snprintf(ipc_pdev_priv.mru_node_name, IPCF_MAX_COMP_STR,
			"fss_mru%d", mru_id);
		break;
	case (uint8_t)IPC_MRU_RTU_INSTANCE:
		snprintf(ipc_pdev_priv.mru_node_name, IPCF_MAX_COMP_STR,
			"rtu%d_mru%d", subsystem_id, mru_id);
		break;
	case (uint8_t)IPC_MRU_RCU_C_INSTANCE:
		snprintf(ipc_pdev_priv.mru_node_name, IPCF_MAX_COMP_STR,
			"rcu_mru%d", mru_id);
		break;
	case (uint8_t)IPC_MRU_RCU_L_INSTANCE:
		snprintf(ipc_pdev_priv.mru_node_name, IPCF_MAX_COMP_STR,
			"rcu_mru_l_%d", mru_id);
		break;
	case (uint8_t)IPC_MRU_RCU_H_INSTANCE:
		snprintf(ipc_pdev_priv.mru_node_name, IPCF_MAX_COMP_STR,
			"rcu_mru_h_%d", mru_id);
		break;
	default:
		return NULL;
	}

	for_each_compatible_node(node, NULL, DT_INTC_NODE_COMP) {
		if (of_node_name_eq(node, ipc_pdev_priv.mru_node_name)) {
			/* get base address from DT node */
			err = of_address_to_resource(node, 0, &res);
			of_node_put(node);
			if (err) {
				shm_err("Unable to read regs address "
					"from DT interrupt node %s\n",
					ipc_pdev_priv.mru_node_name);
				return NULL;
			}

			peripheral_addr = ioremap(
				res.start, resource_size(&res));
		}
	}
	of_node_put(node);
	/* map configuration register space */
	return peripheral_addr;
}
#endif

void ipc_os_unmap_intc(void *addr)
{
#if defined(PLATFORM_FLAVOR_s32n7)
	iounmap(addr);
#endif
}

static const struct of_device_id ipc_shm_ids[] = {
	{ .compatible = "fsl,s32v234-mscm", },
	{ .compatible = "nxp,s32cc-mscm", },
	{ .compatible = "nxp,s32n79-mru", },
	{}
};

MODULE_DEVICE_TABLE(of, ipc_shm_ids);

static struct platform_driver ipc_shm_driver = {
	.driver = {
		.name = IPC_UIO_NAME,
		.of_match_table = ipc_shm_ids,
	},
	.probe = ipc_shm_uio_probe,
	.remove_new = ipc_shm_uio_remove,
};

module_platform_driver(ipc_shm_driver);

MODULE_AUTHOR("NXP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS(IPC_UIO_NAME);
MODULE_DESCRIPTION("NXP ShM Inter-Processor Communication UIO Driver");
MODULE_VERSION(IPC_UIO_VERSION);
