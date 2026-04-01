/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2019,2021-2026 NXP
 */
#include "ipc-os.h"

#include <linux/cdev.h>
#if IPC_MAILBOX_ENABLED
#include <linux/mailbox_client.h>
#endif
#include <linux/mm.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/uio_driver.h>
#include <linux/version.h>

#include "ipc-shm.h"
#include "ipc-hw.h"
#include "ipc-uio.h"

#define IPC_CDEV_NAME		"ipc-cdev-uio"
#define IPC_UIO_NAME		"ipc-shm-uio"
#define IPC_UIO_VERSION		"2.0"

#if IPC_MAILBOX_ENABLED
	#define MAILBOX_NAME_SIZE (8U)
	#define IPC_SHM_MAGIC_NUM (0x49504346U)
	/* Using this define instead of IPC_IRQ_NONE
	 * As UIO_IRQ_CUSTOM = -1, indicates custom IRQ handling
	 * has the same value as IPC_IRQ_NONE
	 */
	#define IPC_UIO_POLLING    (0xFFFF)
#else
	#define IPC_UIO_POLLING    (IPC_IRQ_NONE)
#endif

static DEFINE_MUTEX(mmap_device_mutex);
/**
 * struct ipc_uio_priv_type - IPCF SHM UIO device data
 * @state:           instance state (initialized or not)
 * @irq_num:         rx interrupt number using to request irq
 * @dev:             Linux device capabilities
 * @refcnt:          reference counter to allow only 1 UIO device open at a time
 * @info:            UIO device capabilities
 * @data:            Chrdev private data to get user space configuration
 * @mbox_tx_client:  mailbox client for transmit channel (if mailbox enabled)
 * @mbox_rx_client:  mailbox client for receive channel (if mailbox enabled)
 * @mbox_tx_channel: mailbox transmit channel (if mailbox enabled)
 * @mbox_rx_channel: mailbox receive channel (if mailbox enabled)
 * @uio_name:        UIO device name
 */
struct ipc_uio_priv_type {
	int state;
	int irq_num;
	struct device *dev;
	atomic_t refcnt;
	struct uio_info info;
	struct ipc_uio_cdev_data data;
#if IPC_MAILBOX_ENABLED
	struct mbox_client mbox_tx_client;
	struct mbox_client mbox_rx_client;
	struct mbox_chan *mbox_tx_channel;
	struct mbox_chan *mbox_rx_channel;
#endif
	char uio_name[32];
};

/**
 * struct ipc_pdev_priv_type - IPCF SHM platform device data
 * @major:           chrdev major number which is allocated dynamically
 * @irq_num_init:    interrupt index for each instance
 * @cmd:             most recent ioctl command
 * @phys_addr:       most recent physical address from ioctl
 * @ipc_pdev:        Linux platform device capabilities
 * @pdev_reg:        Platform device register
 * @cdev_class:      class use to create device file in /dev
 * @cdev:            variable use for character device
 * @uio_id:          IPCF SHM UIO device data for each instance
 */
struct ipc_pdev_priv_type {
	dev_t major;
	int irq_num_init[IPC_SHM_MAX_INSTANCES];
	uint32_t cmd;
	uintptr_t phys_addr;
	struct platform_device *ipc_pdev;
	void __iomem *pdev_reg;
	struct class *cdev_class;
	struct cdev cdev;
	struct ipc_uio_priv_type uio_id[IPC_SHM_MAX_INSTANCES];
} ipc_pdev_priv;

#if IPC_S32N7_MAILBOX_ENABLED
/**
 * ipc_uio_mailbox_rx_callback() - Mailbox receive callback for IPC UIO
 * @client: mailbox client structure
 * @msg:    received message (unused)
 *
 * This callback is invoked when a message is received on the mailbox RX
 * channel. It disables and clears the hardware interrupt, then notifies
 * the UIO subsystem to wake up userspace applications waiting for events.
 */
static void ipc_uio_mailbox_rx_callback(struct mbox_client *client, void *msg)
{
	struct ipc_uio_priv_type *priv = container_of(client,
		struct ipc_uio_priv_type, mbox_rx_client);
	uint8_t instance = priv->data.instance;

	/* Disable and clear interrupt like traditional IRQ handler */
	ipc_hw_irq_disable(instance);
	ipc_hw_irq_clear(instance);

	/* Notify UIO subsystem  */
	uio_event_notify(&priv->info);
}

/**
 * ipc_uio_irq_init() - Initialize mailbox channels for IPC UIO instance
 * @instance: IPC instance number
 * @cfg:      IPC shared memory configuration structure
 *
 * Sets up mailbox RX and TX channels for the specified IPC instance based on
 * the configuration. If both RX and TX IRQs are disabled, configures the
 * instance for polling mode. Otherwise, requests the appropriate mailbox
 * channels by name and configures the mailbox clients with proper callbacks.
 * The RX channel is configured with a callback to handle incoming messages,
 * while the TX channel is used for sending notifications.
 *
 * * %0                   - On success
 * * %Negative error code - On error
 */
static int ipc_uio_irq_init(const uint8_t instance,
	const struct ipc_shm_cfg *cfg)
{
	char tx_name[MAILBOX_NAME_SIZE];
	char rx_name[MAILBOX_NAME_SIZE];
	struct device *dev;
	int err;

	if ((IPC_IRQ_NONE == cfg->inter_core_rx_irq) &&
			(IPC_IRQ_NONE == cfg->inter_core_tx_irq)) {
		ipc_pdev_priv.irq_num_init[instance] = IPC_UIO_POLLING;
		return 0; /* No mailbox needed */
	}

	/* Use the existing platform device */
	dev = &ipc_pdev_priv.ipc_pdev->dev;

	/* Request RX channel from mailbox controller
	 * (receives from A78)
	 */
	if (IPC_IRQ_NONE != cfg->inter_core_rx_irq) {
		/* Get mailbox names based on instance */
		snprintf(rx_name, sizeof(rx_name), "rx_%d",
			cfg->inter_core_rx_irq);
		ipc_pdev_priv.uio_id[instance].mbox_rx_client.dev = dev;
		ipc_pdev_priv.uio_id[instance].mbox_rx_client.rx_callback
			= ipc_uio_mailbox_rx_callback;
		ipc_pdev_priv.uio_id[instance].mbox_rx_client.tx_done = NULL;

		ipc_pdev_priv.uio_id[instance].mbox_rx_channel
			= mbox_request_channel_byname(
				&ipc_pdev_priv.uio_id[instance].mbox_rx_client,
				rx_name);
		if (IS_ERR(ipc_pdev_priv.uio_id[instance].mbox_rx_channel)) {
			err = PTR_ERR(
				ipc_pdev_priv.uio_id[instance].mbox_rx_channel);
			shm_err("Failed to request RX mailbox %s: %d\n",
				rx_name, err);
			return err;
		}
	}

	/* Request TX channel from mailbox controller
	 * (sends from A78)
	 */
	if (IPC_IRQ_NONE != cfg->inter_core_tx_irq) {
		/* Get mailbox names based on instance */
		snprintf(tx_name, sizeof(tx_name), "tx_%d",
			cfg->inter_core_tx_irq);
		ipc_pdev_priv.uio_id[instance].mbox_tx_client.dev = dev;
		ipc_pdev_priv.uio_id[instance].mbox_tx_client.rx_callback
			= NULL;
		ipc_pdev_priv.uio_id[instance].mbox_tx_client.tx_done = NULL;
		ipc_pdev_priv.uio_id[instance].mbox_tx_client.knows_txdone
			= true;
		ipc_pdev_priv.uio_id[instance].mbox_tx_client.tx_block = false;

		ipc_pdev_priv.uio_id[instance].mbox_tx_channel
			= mbox_request_channel_byname(
				&ipc_pdev_priv.uio_id[instance].mbox_tx_client,
				tx_name);
		if (IS_ERR(ipc_pdev_priv.uio_id[instance].mbox_tx_channel)) {
			err = PTR_ERR(
				ipc_pdev_priv.uio_id[instance].mbox_tx_channel);
			shm_err("Failed to request TX mailbox %s: %d\n",
				tx_name, err);
			if (ipc_pdev_priv.uio_id[instance].mbox_rx_channel)
				mbox_free_channel(
					ipc_pdev_priv.uio_id[instance]
						.mbox_rx_channel);
			return err;
		}
	}

	ipc_pdev_priv.irq_num_init[instance] = UIO_IRQ_CUSTOM;

	return 0;
}

/**
 * ipc_uio_mailbox_free() - free mailbox channels and reset pointers
 * @uio_priv: pointer to UIO private data structure
 *
 * This function releases both RX and TX mailbox channels if they are
 * allocated and sets the channel pointers to NULL to prevent reuse.
 */
static void ipc_uio_mailbox_free(struct ipc_uio_priv_type *uio_priv)
{
	if (uio_priv->mbox_rx_channel) {
		mbox_free_channel(uio_priv->mbox_rx_channel);
		uio_priv->mbox_rx_channel = NULL;
	}

	if (uio_priv->mbox_tx_channel) {
		mbox_free_channel(uio_priv->mbox_tx_channel);
		uio_priv->mbox_tx_channel = NULL;
	}
}
#else
/**
 * ipc_uio_irq_init() - initialize IRQ configuration for IPC UIO instance
 * @instance: IPC instance number
 * @cfg:      pointer to IPC shared memory configuration structure
 *
 * This function initializes the IRQ configuration for a specific IPC instance.
 * If no RX IRQ is configured, it sets the instance to polling mode. Otherwise,
 * it retrieves the Linux IRQ number from the platform device and validates
 * that the IRQ is not already in use by another instance.
 *
 * * %0                   - On success
 * * %Negative error code - On error
 */
static int ipc_uio_irq_init(const uint8_t instance,
	const struct ipc_shm_cfg *cfg)
{
	int irq, i;

	if (cfg->inter_core_rx_irq == IPC_IRQ_NONE) {
		ipc_pdev_priv.irq_num_init[instance] = IPC_UIO_POLLING;
		return 0;
	}

	/* get Linux IRQ number */
	irq = platform_get_irq(ipc_pdev_priv.ipc_pdev,
		ipc_hw_get_rx_irq(instance));
	if (irq < 0) {
		shm_dbg("Failed to get IRQ\n");
		return irq;
	}
	shm_dbg("GIC Rx IRQ = %d\n", irq);

	/* check duplicate irq number */
	for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
		if (irq == ipc_pdev_priv.irq_num_init[i]) {
			shm_dbg("Can't create UIO device with same irq %d\n",
				irq);
			return -EFAULT;
		}
	}
	ipc_pdev_priv.irq_num_init[instance] = irq;

	return 0;
}
#endif

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

/**
 * ipc_uio_init() - Initialize an UIO and ipcf hw
 * @ipc_uio_cdev_data:    Chrdev private data from user space
 *
 * Return:
 * * %0                   - on success
 * * %Negative error code - On error
 */
static int ipc_uio_init(struct ipc_uio_cdev_data *data)
{
	struct ipc_shm_cfg *cfg = &data->cfg;
	int instance = data->instance;
	int err;

	err = sprintf(ipc_pdev_priv.uio_id[instance].uio_name,
			"instance_%d", instance);
	/* init Peripheral HW for inter-core interrupts */
	err = ipc_hw_init(instance, cfg);
	if (err) {
		shm_err("Failed to initialize hardware for instance %d: %d\n",
			instance, err);
		return err;
	}

	err = ipc_uio_irq_init(instance, cfg);
	if (err) {
		shm_err("Failed to initialize IRQ for instance %d: %d\n",
			instance, err);
		return err;
	}

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

	if (IPC_UIO_POLLING != ipc_pdev_priv.irq_num_init[instance]) {
		err = uio_register_device(
				ipc_pdev_priv.uio_id[instance].dev,
				&ipc_pdev_priv.uio_id[instance].info);
		if (err) {
			shm_dbg("UIO registration failed\n");
			return err;
		}
	}
	ipc_pdev_priv.uio_id[instance].state = IPC_SHM_INSTANCE_ENABLED;

	/* Get minor number of UIO */


	return 0;
}

static int ipc_uio_free(struct ipc_uio_cdev_data *data)
{
	uint8_t inst = data->instance;

	if (IPC_SHM_MAX_INSTANCES <= inst)
		return -EINVAL;

	if ((ipc_pdev_priv.uio_id[inst].state
			== IPC_SHM_INSTANCE_ENABLED)
		&& (ipc_pdev_priv.irq_num_init[inst]
			!= IPC_UIO_POLLING)) {
		uio_unregister_device(&ipc_pdev_priv.uio_id[inst].info);
#if IPC_S32N7_MAILBOX_ENABLED
		ipc_uio_mailbox_free(&ipc_pdev_priv.uio_id[inst]);
#endif
	}

	ipc_pdev_priv.uio_id[inst].state = IPC_SHM_INSTANCE_DISABLED;
	ipc_pdev_priv.irq_num_init[inst] = IPC_IRQ_NONE;

	return 0;
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
 * ipc_cdev_ioctl() - ioctl operation will respond to a ioctl request
 *                    from user-space
 */
static long ipc_cdev_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct ipc_uio_cdev_data data;

	ipc_pdev_priv.cmd = cmd;

	switch (ipc_pdev_priv.cmd) {
	case IPC_IOC_INIT:
		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
			return -EFAULT;
		if (data.instance >= IPC_SHM_MAX_INSTANCES)
			return -EINVAL;
		ipc_pdev_priv.uio_id[data.instance].data = data;
		return ipc_uio_init(&ipc_pdev_priv.uio_id[data.instance].data);
	case IPC_IOC_FREE:
		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
			return -EFAULT;
		if (data.instance >= IPC_SHM_MAX_INSTANCES)
			return -EINVAL;
		ipc_pdev_priv.uio_id[data.instance].data = data;
		return ipc_uio_free(&ipc_pdev_priv.uio_id[data.instance].data);
	case IPC_IOC_MMAP_LOCAL:
	case IPC_IOC_MMAP_REMOTE:
	case IPC_IOC_MMAP_SCMI:
		/* Prepare the physical address for mmap */
		ipc_pdev_priv.phys_addr = (uintptr_t)arg;
		return 0;
	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * ipc_cdev_mmap() -  mmap operation will respond to a mmap request
 *                    from user-space
 */
static int ipc_cdev_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long start = vma->vm_start;
	unsigned long offset = 0;
	unsigned long phys_addr = ipc_pdev_priv.phys_addr;
	unsigned long pfn = 0;

	if ((IPC_IOC_MMAP_LOCAL != ipc_pdev_priv.cmd)
			&& (IPC_IOC_MMAP_REMOTE != ipc_pdev_priv.cmd)
			&& (IPC_IOC_MMAP_SCMI != ipc_pdev_priv.cmd)) {
		shm_err("Invalid command for mmap operation");
		return -EINVAL;
	}

	/* Disable cache */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	/* Map each page individually */
	while (offset < size) {
		pfn = (phys_addr + offset) >> PAGE_SHIFT;
		if (remap_pfn_range(vma, start + offset, pfn, PAGE_SIZE,
				vma->vm_page_prot)) {
			shm_err("Failed to map page at offset 0x%lx\n", offset);
			return -EINVAL;
		}
		offset += PAGE_SIZE;
	}

	shm_dbg("Mapped physical memory %p to user space\n",
		(void *)ipc_pdev_priv.phys_addr);
	return 0;
}

/* File operations */
static const struct file_operations ipc_cdev_fops = {
	.owner = THIS_MODULE,
	.open = ipc_cdev_open,
	.release = ipc_cdev_release,
	.unlocked_ioctl = ipc_cdev_ioctl,
	.mmap = ipc_cdev_mmap
};

/* Platform driver probe methods */
static int ipc_shm_uio_probe(struct platform_device *pdev)
{
	int err;
	int i;
	struct resource *res;

	for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
		ipc_pdev_priv.uio_id[i].irq_num = IPC_IRQ_NONE;
		ipc_pdev_priv.irq_num_init[i] = IPC_IRQ_NONE;
	}

	platform_set_drvdata(pdev, ipc_pdev_priv.uio_id);
	ipc_pdev_priv.ipc_pdev = pdev;

	/* map register configuration space */
	res = platform_get_resource(ipc_pdev_priv.ipc_pdev, IORESOURCE_MEM, 0);
#if !defined(PLATFORM_FLAVOR_s32n7)
	ipc_pdev_priv.pdev_reg = devm_ioremap_resource(
					&ipc_pdev_priv.ipc_pdev->dev, res);
	if (IS_ERR_OR_NULL(ipc_pdev_priv.pdev_reg)) {
		shm_err("Failed to map MSCM register space\n");
		return -ENOMEM;
	}
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
	device_destroy(ipc_pdev_priv.cdev_class, ipc_pdev_priv.major);
	class_destroy(ipc_pdev_priv.cdev_class);
	cdev_del(&ipc_pdev_priv.cdev);
	unregister_chrdev_region(ipc_pdev_priv.major, 1);

	shm_dbg("device removed\n");
}

#if IPC_S32N7_MAILBOX_ENABLED
void ipc_os_mailbox_tx(const uint8_t instance)
{
	uint32_t IPC_SHM_SENDING = IPC_SHM_MAGIC_NUM;
	int err;

	/* Send IPC notification message through mailbox channel */
	err = mbox_send_message(ipc_pdev_priv.uio_id[instance].mbox_tx_channel,
		&IPC_SHM_SENDING);
	if (err < 0) {
		shm_err("Failed to send test message: %d\n", err);
		return;
	}

	/* Signal mailbox framework that transmission is complete */
	mbox_client_txdone(ipc_pdev_priv.uio_id[instance].mbox_tx_channel, 0);
}
#endif

void *ipc_os_map_intc(void)
{
	return ipc_pdev_priv.pdev_reg;
}

void ipc_os_unmap_intc(void *addr)
{
}

static const struct of_device_id ipc_shm_ids[] = {
	{ .compatible = "fsl,s32v234-mscm", },
	{ .compatible = "nxp,s32cc-mscm", },
	{ .compatible = "nxp,s32n79-ipc-shm", },
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
