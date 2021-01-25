/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2019,2021 NXP
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/uio_driver.h>

#include "ipc-shm.h"
#include "ipc-os.h"
#include "ipc-hw.h"
#include "ipc-uio.h"

#define UIO_DRIVER_NAME	"ipc-shm-uio"
#define DRIVER_VERSION	"0.1"

/* module parameters section */
static int inter_core_rx_irq = -1;
static int inter_core_tx_irq = -1;
static int remote_core_type = -1;
static int remote_core_index = -1;
static int local_core_type = -1;
static int local_core_index = -1;
module_param(inter_core_rx_irq, int, 0);
module_param(inter_core_tx_irq, int, 0);
module_param(remote_core_type, int, 0);
module_param(remote_core_index, int, 0);
module_param(local_core_type, int, 0);
module_param(local_core_index, int, 0);

/**
 * struct ipc_uio_priv - IPCF SHM UIO device data
 * @dev:	Linux device
 * @refcnt:	reference counter to allow a single UIO device open at a time
 * @info:	UIO device capabilities
 */
struct ipc_uio_priv {
	struct device *dev;
	atomic_t refcnt;
	struct uio_info info;
};

static int ipc_shm_uio_open(struct uio_info *info, struct inode *inode)
{
	struct ipc_uio_priv *priv = info->priv;

	if (!atomic_dec_and_test(&priv->refcnt)) {
		dev_err(priv->dev, "%s device already opened\n", info->name);
		atomic_inc(&priv->refcnt);
		return -EBUSY;
	}

	return 0;
}

static int ipc_shm_uio_release(struct uio_info *info, struct inode *inode)
{
	struct ipc_uio_priv *priv = info->priv;

	atomic_inc(&priv->refcnt);

	return 0;
}

static int ipc_shm_uio_irqcontrol(struct uio_info *dev_info, int cmd)
{
	switch (cmd) {
	case IPC_UIO_DISABLE_RX_IRQ_CMD:
		ipc_hw_irq_disable();
		break;
	case IPC_UIO_ENABLE_RX_IRQ_CMD:
		ipc_hw_irq_enable();
		break;
	case IPC_UIO_TRIGGER_TX_IRQ_CMD:
		ipc_hw_irq_notify();
		break;
	default:
		break;
	}
	return 0;
}

/* hardirq handler */
static irqreturn_t ipc_shm_uio_handler(int irq, struct uio_info *dev_info)
{
	ipc_hw_irq_disable();
	ipc_hw_irq_clear();

	/* return IRQ_HANDLED to trigger uio_event_notify() to user-space */
	return IRQ_HANDLED;
}

static int ipc_shm_uio_probe(struct platform_device *pdev)
{
	struct ipc_uio_priv *priv;
	struct ipc_shm_core remote_core = {IPC_CORE_DEFAULT, 0};
	struct ipc_shm_core local_core = {IPC_CORE_DEFAULT, 0};
	struct resource *res;
	void __iomem *mscm;
	int irq, err;

	if ((inter_core_tx_irq < 0 && inter_core_tx_irq != IPC_IRQ_NONE)
		|| (inter_core_rx_irq < 0)
		|| (remote_core_type < 0) || (local_core_type < 0)
		|| ((remote_core_type != IPC_CORE_DEFAULT)
			&& (remote_core_index < 0))
		|| ((local_core_type != IPC_CORE_DEFAULT)
			&& (local_core_index < 0))) {
		dev_err(&pdev->dev, "Module parameters not specified!\n");
		return -EINVAL;
	}
	dev_dbg(&pdev->dev, "inter_core_rx_irq = %d\n", inter_core_rx_irq);
	dev_dbg(&pdev->dev, "inter_core_tx_irq = %d\n", inter_core_tx_irq);
	dev_dbg(&pdev->dev, "remote_core_type = %d\n", remote_core_type);
	dev_dbg(&pdev->dev, "remote_core_index = %d\n", remote_core_index);
	dev_dbg(&pdev->dev, "local_core_index = %d\n", local_core_index);
	dev_dbg(&pdev->dev, "local_core_type = %d\n", local_core_type);

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (IS_ERR_OR_NULL(priv))
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);

	priv->dev = &pdev->dev;

	/* map MSCM register configuration space */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mscm = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(mscm)) {
		dev_err(&pdev->dev, "failed to map MSCM register space\n");
		return -ENOMEM;
	}

	/* init MSCM HW for MSI inter-core interrupts */
	remote_core.type = (enum ipc_shm_core_type)remote_core_type;
	remote_core.index = remote_core_index;
	local_core.type = (enum ipc_shm_core_type)local_core_type;
	local_core.index = local_core_index;
	err = _ipc_hw_init(inter_core_tx_irq, inter_core_rx_irq,
			   &remote_core, &local_core, mscm);
	if (err)
		return err;

	/* get Linux IRQ number */
	irq = platform_get_irq(pdev, inter_core_rx_irq);
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to get IRQ\n");
		return irq;
	}
	dev_info(&pdev->dev, "GIC Rx IRQ = %d\n", irq);

	/* Register UIO device */
	atomic_set(&priv->refcnt, 1);
	priv->info.version = DRIVER_VERSION;
	priv->info.name = UIO_DRIVER_NAME;
	priv->info.irq = irq;
	priv->info.irq_flags = 0;
	priv->info.handler = ipc_shm_uio_handler;
	priv->info.irqcontrol = ipc_shm_uio_irqcontrol;
	priv->info.open = ipc_shm_uio_open;
	priv->info.release = ipc_shm_uio_release;
	priv->info.priv = priv;

	err = uio_register_device(priv->dev, &priv->info);
	if (err) {
		dev_err(priv->dev, "UIO registration failed\n");
		return err;
	}

	dev_info(&pdev->dev, "device ready\n");

	return 0;
}

static int ipc_shm_uio_remove(struct platform_device *pdev)
{
	struct ipc_uio_priv *priv = platform_get_drvdata(pdev);

	uio_unregister_device(&priv->info);

	dev_info(&pdev->dev, "device removed\n");

	return 0;
}

void *ipc_os_map_intc(void)
{
	/* Dummy implementation. Not used by UIO. */
	return NULL;
}

void ipc_os_unmap_intc(void *addr)
{
	/* Dummy implementation. Not used by UIO. */
}

static const struct of_device_id ipc_shm_ids[] = {
	{ .compatible = "fsl,s32v234-mscm", },
	{ .compatible = "fsl,s32gen1-mscm", },
	{}
};

MODULE_DEVICE_TABLE(of, ipc_shm_ids);

static struct platform_driver ipc_shm_driver = {
	.driver = {
		.name = UIO_DRIVER_NAME,
		.of_match_table = ipc_shm_ids,
	},
	.probe = ipc_shm_uio_probe,
	.remove = ipc_shm_uio_remove,
};

module_platform_driver(ipc_shm_driver);

MODULE_AUTHOR("NXP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS(UIO_DRIVER_NAME);
MODULE_DESCRIPTION("NXP ShM Inter-Processor Communication UIO Driver");
MODULE_VERSION(DRIVER_VERSION);
