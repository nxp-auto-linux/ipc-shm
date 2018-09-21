/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2018 NXP Semiconductors
 */
#ifndef IPC_HW_H
#define IPC_HW_H

#include "ipc-shm.h"

#define PLATFORM_DEFAULT    (-1) /* use platform default instead of parameter */

int ipc_hw_init(const struct ipc_shm_cfg *cfg);

void ipc_hw_free(void);

char *ipc_hw_get_dt_comp(void);

int ipc_hw_get_dt_irq(void);

int ipc_hw_irq_enable(void);

int ipc_hw_irq_disable(void);

int ipc_hw_irq_notify(void);

int ipc_hw_irq_clear(void);

#endif /* IPC_HW_H */
