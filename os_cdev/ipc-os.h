/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2023 NXP
 */
#ifndef IPC_OS_H
#define IPC_OS_H

#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* softirq work budget used to prevent CPU starvation */
#define IPC_SOFTIRQ_BUDGET 128u

#define IPC_SHM_INSTANCE_DISABLED   0
#define IPC_SHM_INSTANCE_ENABLED    1

/* convenience wrappers for printing errors and debug messages */
#define pr_fmt(fmt) "ipc-shm-usr-lib: %s(): "fmt
#define shm_err(fmt, ...) printf(pr_fmt(fmt), __func__, ##__VA_ARGS__)
#ifdef DEBUG
#define shm_dbg(fmt, ...) printf(pr_fmt(fmt), __func__, ##__VA_ARGS__)
#else
#define shm_dbg(fmt, ...)
#endif

/* forward declarations */
struct ipc_shm_cfg;

/* function declarations */
int ipc_os_init(const uint8_t instance, const struct ipc_shm_cfg *cfg,
		int (*rx_cb)(const uint8_t, int));
void ipc_os_free(const uint8_t instance);
uintptr_t ipc_os_get_local_shm(const uint8_t instance);
uintptr_t ipc_os_get_remote_shm(const uint8_t instance);
int ipc_os_poll_channels(const uint8_t instance);

#endif /* IPC_OS_H */
