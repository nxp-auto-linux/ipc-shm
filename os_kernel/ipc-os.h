/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018,2021,2024-2025 NXP
 */
#ifndef IPC_OS_H
#define IPC_OS_H

#include <linux/module.h>

#define DRIVER_NAME	"ipc-shm-dev"

/* softirq work budget used to prevent CPU starvation */
#define IPC_SOFTIRQ_BUDGET 128

#define IPC_SHM_INSTANCE_DISABLED   0
#define IPC_SHM_INSTANCE_ENABLED    1

/* convenience wrappers for printing errors and debug messages */
#define shm_fmt(fmt) DRIVER_NAME": %s(): "fmt
#define shm_err(fmt, ...) pr_err(shm_fmt(fmt), __func__, ##__VA_ARGS__)
#define shm_dbg(fmt, ...) pr_debug(shm_fmt(fmt), __func__, ##__VA_ARGS__)

/* forward declarations */
struct ipc_shm_cfg;

/* function declarations */
/**
 * ipc_shm_os_init() - OS specific initialization code
 * @instance:	 instance id
 * @cfg:         configuration parameters
 * @rx_cb:	rx callback to be called from rx softirq
 *
 * Return:
 * * %0                   - on success
 * * %Negative error code - On error
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

/* ipc_os_ioremap() - I/O memory map for a physical address */
uintptr_t ipc_os_ioremap(uintptr_t phys_addr, uint32_t size);

/* ipc_os_ioremap() - I/O memory unmap for a physical address */
void ipc_os_iounmap(uintptr_t phys_addr, uintptr_t vir_addr, uint32_t size);

#ifndef PLATFORM_FLAVOR_s32n7
/**
 * ipc_os_map_intc() - I/O memory map interrupt controller register space
 *
 * I/O memory map the inter-core interrupts HW block (MSCM for ARM processors)
 * or Mru HW block
 */
void *ipc_os_map_intc(void);
#else
/**
 * ipc_os_mru_map_intc() - I/O memory map interrupt controller register space
 *
 * I/O memory map the inter-core interrupts HW block (Mru HW block)
 */
void *ipc_os_mru_map_intc(uint8_t mru_instance,
	uint8_t subsystem_idx, uint8_t mru_id);
#endif

/**
 * ipc_os_unmap_intc() - I/O memory unmap interrupt controller register space
 */
void ipc_os_unmap_intc(void *addr);

/**
 * ipc_os_poll_channels() - invoke rx callback configured at initialization
 *
 * Not implemented for Linux.
 *
 * Return:
 * * %Number of messages processed	- on success
 * * %-EINVAL		- Invalid instance
 * * %-EOPNOTSUPP	- instance is not configuring for polling
 */
int8_t ipc_os_poll_channels(const uint8_t instance);

#endif /* IPC_OS_H */
