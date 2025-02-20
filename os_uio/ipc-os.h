/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2023-2025 NXP
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
#define pr_fmt(fmt) "ipc-shm-uio-lib: %s(): "fmt
#define shm_err(fmt, ...) printf(pr_fmt(fmt), __func__, ##__VA_ARGS__)
#ifdef DEBUG
#define shm_dbg(fmt, ...) printf(pr_fmt(fmt), __func__, ##__VA_ARGS__)
#else
#define shm_dbg(fmt, ...)
#endif

/* forward declarations */
struct ipc_shm_cfg;


/**
 * ipc_shm_os_init() - OS specific initialization code
 * @instance:	instance id
 * @cfg:	configuration parameters
 * @rx_cb:	Rx callback to be called from Rx softirq
 *
 * Return:
 * * %0		- on success
 * * %-EINVAL	- Invalid parameters
 * * %-ENODEV	- Cannot open/load module
 * * %-ENOMEM	- Cannot map memory
 * * %-ENOENT	- Cannot get the UIO name
 */
int8_t ipc_os_init(const uint8_t instance, const struct ipc_shm_cfg *cfg,
		uint32_t (*rx_cb)(const uint8_t, int));

/**
 * ipc_os_free() - free OS specific resources
 */
void ipc_os_free(const uint8_t instance);

/**
 * ipc_os_get_local_shm() - get local shared mem address
 */
uintptr_t ipc_os_get_local_shm(const uint8_t instance);

/**
 * ipc_os_get_remote_shm() - get remote shared mem address
 */
uintptr_t ipc_os_get_remote_shm(const uint8_t instance);

/**
 * ipc_os_poll_channels() - invoke rx callback configured at initialization
 *
 * Return:
 * * %Number of messages processed	- on success
 * * %-EINVAL		- Invalid instance
 * * %-EOPNOTSUPP	- instance is not configuring for polling
 */
int8_t ipc_os_poll_channels(const uint8_t instance);

#endif /* IPC_OS_H */
