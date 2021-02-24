/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018-2019,2021 NXP
 */
#ifndef IPC_HW_H
#define IPC_HW_H

int ipc_hw_init(const uint8_t instance, const struct ipc_shm_cfg *cfg);

void ipc_hw_free(const uint8_t instance);

int ipc_hw_get_rx_irq(const uint8_t instance);

void ipc_hw_irq_enable(const uint8_t instance);

void ipc_hw_irq_disable(const uint8_t instance);

void ipc_hw_irq_notify(const uint8_t instance);

void ipc_hw_irq_clear(const uint8_t instance);

struct ipc_shm_remote_core;
struct ipc_shm_local_core;
int _ipc_hw_init(const uint8_t instance, int tx_irq, int rx_irq,
		 const struct ipc_shm_remote_core *remote_core,
		 const struct ipc_shm_local_core *local_core, void *mscm_addr);

int ipc_hw_set_tx_irq(const uint8_t instance, int tx_irq);

#endif /* IPC_HW_H */
