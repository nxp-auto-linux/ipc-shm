/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2018 NXP Semiconductors
 */
#ifndef IPC_HW_H
#define IPC_HW_H

#define PLATFORM_DEFAULT    (-1) /* use platform default instead of parameter */

int ipc_shm_hw_init(void);

void ipc_shm_hw_free(void);

char *ipc_shm_hw_get_dt_comp(void);

int ipc_shm_hw_get_dt_irq(int shm_irq_id);

int ipc_shm_hw_irq_enable(int shm_irq_id);

int ipc_shm_hw_irq_disable(int shm_irq_id);

int ipc_shm_hw_irq_notify(int shm_irq_id, int remote_cpu);

int ipc_shm_hw_irq_clear(int shm_irq_id);

#endif /* IPC_HW_H */
