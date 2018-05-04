/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2018 NXP Semiconductors
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include "../ipc-shm-dev/ipc-shm.h"

#define DRIVER_NAME	"nxp-ipc-shm-sample"
#define DRIVER_VERSION	"0.1"

MODULE_AUTHOR("NXP");
MODULE_LICENSE("BSD");
MODULE_ALIAS(DRIVER_NAME);
MODULE_DESCRIPTION("NXP Shared Memory IPC Sample App Module");
MODULE_VERSION(DRIVER_VERSION);

static int __init shm_mod_init(void)
{
	pr_info("%s v%s init\n", DRIVER_NAME, DRIVER_VERSION);
	pr_info("[TEST] ipc_shm_init() return code: %d\n", ipc_shm_init(NULL));
	return 0;
}

static void __exit shm_mod_exit(void)
{
	pr_info("%s exit\n", DRIVER_NAME);
}

module_init(shm_mod_init);
module_exit(shm_mod_exit);
