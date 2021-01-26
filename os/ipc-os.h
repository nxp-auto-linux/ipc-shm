/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018,2021 NXP
 */
#ifndef IPC_OS_H
#define IPC_OS_H

#include <linux/module.h>

#define DRIVER_NAME	"ipc-shm-dev"

/* softirq work budget used to prevent CPU starvation */
#define IPC_SOFTIRQ_BUDGET 128

/* convenience wrappers for printing errors and debug messages */
#define shm_fmt(fmt) DRIVER_NAME": %s(): "fmt
#define shm_err(fmt, ...) pr_err(shm_fmt(fmt), __func__, ##__VA_ARGS__)
#define shm_dbg(fmt, ...) pr_debug(shm_fmt(fmt), __func__, ##__VA_ARGS__)

/* forward declarations */
struct ipc_shm_cfg;

/* function declarations */
int ipc_os_init(const struct ipc_shm_cfg *cfg, int (*rx_cb)(int));
void ipc_os_free(void);
uintptr_t ipc_os_get_local_shm(void);
uintptr_t ipc_os_get_remote_shm(void);
void *ipc_os_map_intc(void);
void ipc_os_unmap_intc(void *addr);
int ipc_os_poll_channels(void);

#endif /* IPC_OS_H */
