/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018-2019 NXP
 */
#ifndef IPC_HW_H
#define IPC_HW_H

int ipc_hw_init(const struct ipc_shm_cfg *cfg);

void ipc_hw_free(void);

int ipc_hw_get_rx_irq(void);

void ipc_hw_irq_enable(void);

void ipc_hw_irq_disable(void);

void ipc_hw_irq_notify(void);

void ipc_hw_irq_clear(void);

struct ipc_shm_remote_core;
int _ipc_hw_init(int tx_irq, int rx_irq,
		 const struct ipc_shm_remote_core *remote_core,
		 void *mscm_addr);

int ipc_hw_set_tx_irq(int tx_irq);

#endif /* IPC_HW_H */
