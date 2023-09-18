/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2019,2023 NXP
 */
#ifndef IPC_UIO_H
#define IPC_UIO_H

/* IPC-UIO commands */
/*
 * Form a command from command and instance:
 * 16 bit integer is used: 0xAABB, which AA is instance, BB is command
 */
#define IPC_USR_CMD_MASK		0xFF00
#define IPC_USR_INST_MASK		0xFF

/* UIO command */
/* disable Rx inter-core interrupt */
#define IPC_UIO_DISABLE_CMD		(0x0001)

/* enable Rx inter-core interrupt */
#define IPC_UIO_ENABLE_CMD		(0x0002)

/* trigger Tx inter-core interrupt */
#define IPC_UIO_TRIGGER_CMD		(0x0003)

struct ipc_uio_cdev_data {
	uint8_t instance;
	struct ipc_shm_cfg cfg;
};

#endif /* IPC_UIO_H */
