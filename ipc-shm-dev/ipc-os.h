/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018 NXP
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

int ipc_os_init(int (*rx_cb)(int));
void ipc_os_free(void);

#endif /* IPC_OS_H */
