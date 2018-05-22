/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2018 NXP Semiconductors
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include "ipc-shm.h"

#define DRV_NAME	"ipc-shm-dev"
#define DRV_VERSION	"0.1"

MODULE_AUTHOR("NXP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS(DRV_NAME);
MODULE_DESCRIPTION("NXP Shared Memory Inter-Processor Communication Driver");
MODULE_VERSION(DRV_VERSION);

int ipc_shm_init(const struct ipc_shm *cfg)
{
	pr_debug("%s: %s\n", DRV_NAME, __func__);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(ipc_shm_init);

int ipc_shm_free(void)
{
	pr_debug("%s: %s\n", DRV_NAME, __func__);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(ipc_shm_free);

void *ipc_shm_acquire_buf(int chan_id, size_t size)
{
	pr_debug("%s: %s\n", DRV_NAME, __func__);
	return NULL;
}
EXPORT_SYMBOL(ipc_shm_acquire_buf);

int ipc_shm_release_buf(int chan_id, void *buf)
{
	pr_debug("%s: %s\n", DRV_NAME, __func__);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(ipc_shm_release_buf);

int ipc_shm_tx(int chan_id, void *buf, size_t size)
{
	pr_debug("%s: %s\n", DRV_NAME, __func__);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(ipc_shm_tx);

int ipc_shm_tx_unmanaged(int chan_id)
{
	pr_debug("%s: %s\n", DRV_NAME, __func__);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(ipc_shm_tx_unmanaged);

static int __init shm_mod_init(void)
{
	pr_info("%s: module v%s init\n", DRV_NAME, DRV_VERSION);
	return 0;
}

static void __exit shm_mod_exit(void)
{
	pr_info("%s: module exit\n", DRV_NAME);
}

module_init(shm_mod_init);
module_exit(shm_mod_exit);
