/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018-2019,2021,2023-2025 NXP
 */
#ifndef IPC_HW_H
#define IPC_HW_H

int8_t ipc_hw_init(const uint8_t instance, const struct ipc_shm_cfg *cfg);

void ipc_hw_free(const uint8_t instance);

int ipc_hw_get_rx_irq(const uint8_t instance);

void ipc_hw_irq_enable(const uint8_t instance);

void ipc_hw_irq_disable(const uint8_t instance);

void ipc_hw_irq_notify(const uint8_t instance);

void ipc_hw_irq_clear(const uint8_t instance);

struct ipc_shm_remote_core;
struct ipc_shm_local_core;
int8_t _ipc_hw_init(const uint8_t instance,
		const struct ipc_shm_cfg *cfg, void *mscm_addr);

#endif /* IPC_HW_H */
