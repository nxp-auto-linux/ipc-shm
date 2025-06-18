/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2019,2023-2025 NXP
 */
#ifndef IPC_UIO_H
#define IPC_UIO_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

/* UIO command */
/* disable Rx inter-core interrupt */
#define IPC_UIO_DISABLE_CMD		(0x0001)

/* enable Rx inter-core interrupt */
#define IPC_UIO_ENABLE_CMD		(0x0002)

/* trigger Tx inter-core interrupt */
#define IPC_UIO_TRIGGER_CMD		(0x0003)

#ifndef IPC_IOCTL_MAGIC
#define IPC_IOCTL_MAGIC       (0xA6)
#endif /* IPC_IOCTL_MAGIC */

#define IPC_IOC_INIT        _IOW(IPC_IOCTL_MAGIC, 1, struct ipc_uio_cdev_data)
#define IPC_IOC_FREE        _IOW(IPC_IOCTL_MAGIC, 2, struct ipc_uio_cdev_data)
#define IPC_IOC_MMAP_LOCAL  _IOW(IPC_IOCTL_MAGIC, 3, struct ipc_uio_cdev_data)
#define IPC_IOC_MMAP_REMOTE _IOW(IPC_IOCTL_MAGIC, 4, struct ipc_uio_cdev_data)
#define IPC_IOC_MMAP_SCMI   _IOW(IPC_IOCTL_MAGIC, 5, struct ipc_uio_cdev_data)

/**
 * struct ipc_uio_cdev_data - store data about init/free
 *                            that write to kernel space
 * @instance: target instance
 * @cfg:      instance's configuration
 */
struct ipc_uio_cdev_data {
	uint8_t instance;
	struct ipc_shm_cfg cfg;
};

#endif /* IPC_UIO_H */
