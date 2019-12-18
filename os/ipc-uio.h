/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2019 NXP
 */
#ifndef IPC_UIO_H
#define IPC_UIO_H

/* IPC-UIO commands */

/* disable Rx inter-core interrupt */
#define IPC_UIO_DISABLE_RX_IRQ_CMD 0

/* enable Rx inter-core interrupt */
#define IPC_UIO_ENABLE_RX_IRQ_CMD 1

/* trigger Tx inter-core interrupt */
#define IPC_UIO_TRIGGER_TX_IRQ_CMD 2

#endif /* IPC_UIO_H */
