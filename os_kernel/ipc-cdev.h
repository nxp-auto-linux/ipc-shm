/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2023 NXP
 */
#ifndef IPC_USR_H
#define IPC_USR_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

/* An available IOCTL number */
#define IPC_USR_TYPE		0xA6

/* Generic command */
enum ipc_shm_usr_cmd {
	CMD_SET_INST   = 0x00,
	CMD_INIT_INST  = 0x01,
	CMD_DISABLE_RX = 0x02,
	CMD_ENABLE_RX  = 0x03,
	CMD_TRIGGER_TX = 0x04,
};

/* set target instance */
#define IPC_USR_CMD_SET_INSTANCE     _IOW(IPC_USR_TYPE, CMD_SET_INST, uint8_t)

/* initialize an instance */
#define IPC_USR_CMD_INIT_INSTANCE    _IOW(IPC_USR_TYPE, CMD_INIT_INST, void *)

/* disable Rx inter-core interrupt */
#define IPC_USR_CMD_DISABLE_RX_IRQ   _IOW(IPC_USR_TYPE, CMD_DISABLE_RX, uint8_t)

/* enable Rx inter-core interrupt */
#define IPC_USR_CMD_ENABLE_RX_IRQ    _IOW(IPC_USR_TYPE, CMD_ENABLE_RX, uint8_t)

/* trigger Tx inter-core interrupt */
#define IPC_USR_CMD_TRIGGER_TX_IRQ   _IOW(IPC_USR_TYPE, CMD_TRIGGER_TX, uint8_t)

#endif /* IPC_USR_H */
