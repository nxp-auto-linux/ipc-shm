/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2023 NXP
 */
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <stdlib.h>
#include <dirent.h>

#include "ipc-os.h"
#include "ipc-hw.h"
#include "ipc-shm.h"
#include "ipc-cdev.h"

#define IPC_SHM_DEV_MEM_NAME    "/dev/mem"
#define IPC_SHM_CDEV_DEV_NAME    "/dev/ipc-shm-cdev"
#define DRIVER_VERSION          "2.0"

#define RX_SOFTIRQ_POLICY	SCHED_FIFO

/*
 * Maximum number of instances
 */
#define IPC_SHM_MAX_INSTANCES	4u

/* system call wrappers for loading and unloading kernel modules */
#define finit_module(fd, param_values, flags) \
	syscall(__NR_finit_module, fd, param_values, flags)
#define delete_module(name, flags) \
	syscall(__NR_delete_module, name, flags)

enum ipc_status {
	IPC_STATUS_CLEAR = 0,
	IPC_STATUS_SET = 1,
};

/**
 * struct ipc_os_priv_instance - OS specific private data each instance
 * @state:                state to indicate whether instance is initialized
 * @irq_num:              rx interrupt number using to check for polling
 * @shm_size:             local/remote ShM size
 * @local_virt_shm:       local ShM virtual address
 * @remote_virt_shm:      remote ShM virtual address
 * @local_shm_map:        local ShM mapped page address
 * @remote_shm_map:       remote ShM mapped page address
 * @local_shm_offset:     local ShM offset in mapped page
 * @remote_shm_offset:    remote ShM offset in mapped page
 */
struct ipc_os_priv_instance {
	uint8_t state;
	int irq_num;
	size_t shm_size;
	void *local_virt_shm;
	void *remote_virt_shm;
	void *local_shm_map;
	void *remote_shm_map;
	size_t local_shm_offset;
	size_t remote_shm_offset;
};

/**
 * struct ipc_os_priv - OS specific private data
 * @ipc_files_opened:    indicate whether device files are opened
 * @ipc_soft_created:    indicate whether ipc_shm_softirq thread is opened
 * @ipc_usr_fd:          ipc-shm-usr kernel device file descriptor
 * @dev_mem_fd:          MEM device file descriptor
 * @irq_thread_id:       Rx interrupt thread id
 * @id:                  private data per instance
 * @rx_cb:               upper layer rx callback function
 */
static struct ipc_os_priv {
	uint8_t ipc_files_opened;
	uint8_t ipc_soft_created;
	int ipc_usr_fd;
	int dev_mem_fd;
	pthread_t irq_thread_id;
	struct ipc_os_priv_instance id[IPC_SHM_MAX_INSTANCES];
	int (*rx_cb)(const uint8_t instance, int budget);
} priv;

/* Rx sotfirq thread */
static void *ipc_shm_softirq(void *arg)
{
	const int budget = IPC_SOFTIRQ_BUDGET;
	int irq_count;
	int work;
	uint8_t i = 0;

	while (1) {
		if (priv.ipc_usr_fd > 0)
			/*
			 * block(sleep) until notified from kernel IRQ handler
			 */
			read(priv.ipc_usr_fd, &irq_count, sizeof(irq_count));
		else
			continue;
		for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
			if ((priv.id[i].state == IPC_SHM_INSTANCE_DISABLED)
					|| (priv.id[i].irq_num == IPC_IRQ_NONE))
				continue;
			do {
				work = priv.rx_cb(i, budget);
				/*
				 * work not done, yield and wait for reschedule
				 */
				sched_yield();
			} while (work >= budget);
		}
		for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
			if (priv.id[i].state == IPC_SHM_INSTANCE_DISABLED)
				continue;
			/* enable notifications from remote */
			ipc_hw_irq_enable(i);
		}
	}

	return 0;
}

/**
 * ipc_shm_os_init() - OS specific initialization code
 * @instance:	instance id
 * @cfg:	configuration parameters
 * @rx_cb:	Rx callback to be called from Rx softirq
 *
 * Return: 0 on success, error code otherwise
 */
int ipc_os_init(const uint8_t instance, const struct ipc_shm_cfg *cfg,
		int (*rx_cb)(const uint8_t, int))
{
	size_t page_size = sysconf(_SC_PAGE_SIZE);
	off_t page_phys_addr;
	int ipc_usr_module_fd;
	struct sched_param irq_thread_param;
	pthread_attr_t irq_thread_attr;
	int err;

	if (!rx_cb)
		return -EINVAL;

	/* save params */
	priv.id[instance].shm_size = cfg->shm_size;
	priv.rx_cb = rx_cb;

	if (priv.ipc_files_opened == (uint8_t)IPC_STATUS_CLEAR) {
		/* open ipc-shm-usr kernel module */
		ipc_usr_module_fd = open(IPC_ISR_MODULE_PATH, O_RDONLY);
		if (ipc_usr_module_fd == -1) {
			shm_err("Can't open %s module\n", IPC_ISR_MODULE_PATH);
			return -ENODEV;
		}

		if (finit_module(ipc_usr_module_fd, "", 0) != 0) {
			shm_err("Can't load %s module\n", IPC_ISR_MODULE_PATH);
			err = -ENODEV;
			goto err_close_ipc_shm_usr_module;
		}

		/* open MEM device for interrupt support */
		priv.dev_mem_fd = open(IPC_SHM_DEV_MEM_NAME, O_RDWR);
		if (priv.dev_mem_fd == -1) {
			shm_err("Can't open %s device\n", IPC_SHM_DEV_MEM_NAME);
			err = -ENODEV;
			goto err_close_ipc_shm_usr_module;
		}

		/* open ipc-shm-usr device for interrupt support */
		priv.ipc_usr_fd = open(IPC_SHM_CDEV_DEV_NAME, O_RDWR);
		if (priv.ipc_usr_fd == -1) {
			shm_err("Can't open %s device\n",
						IPC_SHM_CDEV_DEV_NAME);
			err = -ENODEV;
			goto err_close_mem_dev;
		}
		priv.ipc_files_opened = (uint8_t)IPC_STATUS_SET;
	}

	/* map local physical shared memory */
	/* truncate address to a multiple of page size, or mmap will fail */
	page_phys_addr = (cfg->local_shm_addr / page_size) * page_size;
	priv.id[instance].local_shm_offset
		= cfg->local_shm_addr - page_phys_addr;

	priv.id[instance].local_shm_map
		= mmap(NULL, priv.id[instance].local_shm_offset
			+ cfg->shm_size,
			PROT_READ | PROT_WRITE, MAP_SHARED,
			priv.dev_mem_fd, page_phys_addr);
	if (priv.id[instance].local_shm_map == MAP_FAILED) {
		shm_err("Can't map memory: %lx\n", cfg->local_shm_addr);
		err = -ENOMEM;
		goto err_close_usr_dev;
	}

	priv.id[instance].local_virt_shm
		= priv.id[instance].local_shm_map
			+ priv.id[instance].local_shm_offset;

	/* map remote physical shared memory */
	page_phys_addr = (cfg->remote_shm_addr / page_size) * page_size;
	priv.id[instance].remote_shm_offset
		= cfg->remote_shm_addr - page_phys_addr;

	priv.id[instance].remote_shm_map
		= mmap(NULL, priv.id[instance].remote_shm_offset
			+ cfg->shm_size,
			PROT_READ | PROT_WRITE, MAP_SHARED,
			priv.dev_mem_fd, page_phys_addr);
	if (priv.id[instance].remote_shm_map == MAP_FAILED) {
		shm_err("Can't map memory: %lx\n", cfg->remote_shm_addr);
		err = -ENOMEM;
		goto err_unmap_local_shm;
	}

	priv.id[instance].remote_virt_shm
		= priv.id[instance].remote_shm_map
			+ priv.id[instance].remote_shm_offset;

	if (priv.ipc_soft_created == (uint8_t)IPC_STATUS_CLEAR) {
		/*
		 * start softirq thread with the highest priority for its policy
		 */
		err = pthread_attr_init(&irq_thread_attr);
		if (err != 0) {
			goto err_unmap_remote_shm;
			shm_err("Can't initialize Rx softirq attributes\n");
		}

		err = pthread_attr_setschedpolicy(&irq_thread_attr,
							RX_SOFTIRQ_POLICY);
		if (err != 0) {
			goto err_unmap_remote_shm;
			shm_err("Can't set Rx softirq policy\n");
		}

		irq_thread_param.sched_priority = sched_get_priority_max(
			RX_SOFTIRQ_POLICY);
		err = pthread_attr_setschedparam(&irq_thread_attr,
							&irq_thread_param);
		if (err != 0) {
			goto err_unmap_remote_shm;
			shm_err("Can't set Rx softirq scheduler parameters\n");
		}

		err = pthread_create(&priv.irq_thread_id, &irq_thread_attr,
					ipc_shm_softirq, &priv);
		if (err == -1) {
			shm_err("Can't start Rx softirq thread\n");
			goto err_unmap_remote_shm;
		}
		priv.ipc_soft_created = (uint8_t)IPC_STATUS_SET;
	}
	shm_dbg("Created Rx softirq thread with priority=%d\n",
		irq_thread_param.sched_priority);

	err = ioctl(priv.ipc_usr_fd, IPC_USR_CMD_SET_INSTANCE, instance);
	if (err) {
		shm_err("Failed to set target instance %d\n", instance);
		goto err_unmap_remote_shm;
	}

	err = ioctl(priv.ipc_usr_fd, IPC_USR_CMD_INIT_INSTANCE,
			(struct ipc_shm_cfg *)cfg);
	if (err) {
		shm_err("Failed to initialize instance %d\n", instance);
		goto err_unmap_remote_shm;
	}

	priv.id[instance].state = IPC_SHM_INSTANCE_ENABLED;
	if (cfg->inter_core_rx_irq == IPC_IRQ_NONE)
		priv.id[instance].irq_num = IPC_IRQ_NONE;
	else
		priv.id[instance].irq_num = 0;

	shm_dbg("done\n");

	return 0;

err_unmap_remote_shm:
	munmap(priv.id[instance].remote_shm_map,
			priv.id[instance].remote_shm_offset
				+ priv.id[instance].shm_size);
err_unmap_local_shm:
	munmap(priv.id[instance].local_shm_map,
			priv.id[instance].local_shm_offset
				+ priv.id[instance].shm_size);
err_close_usr_dev:
	close(priv.ipc_usr_fd);
err_close_mem_dev:
	close(priv.dev_mem_fd);
err_close_ipc_shm_usr_module:
	close(ipc_usr_module_fd);
	priv.ipc_files_opened = IPC_STATUS_CLEAR;
	priv.id[instance].state = IPC_SHM_INSTANCE_DISABLED;
	return err;
}

/**
 * ipc_os_free() - free OS specific resources
 */
void ipc_os_free(const uint8_t instance)
{
	void *res;
	int i;

	priv.id[instance].state = IPC_SHM_INSTANCE_DISABLED;

	/* disable hardirq */
	ipc_hw_irq_disable(instance);
	/* unmap remote/local shm */
	munmap(priv.id[instance].remote_shm_map,
			priv.id[instance].remote_shm_offset
				+ priv.id[instance].shm_size);
	munmap(priv.id[instance].local_shm_map,
			priv.id[instance].local_shm_offset
				+ priv.id[instance].shm_size);

	/*
	 * Close all file descriptors and cancel soft thread
	 * only when all instances are disabled
	 */
	for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
		if (priv.id[i].state == IPC_SHM_INSTANCE_ENABLED)
			return;
	}
	if (priv.ipc_files_opened == IPC_STATUS_SET) {
		/* stop irq thread */
		pthread_cancel(priv.irq_thread_id);
		pthread_join(priv.irq_thread_id, &res);
		priv.ipc_soft_created = (uint8_t)IPC_STATUS_CLEAR;
		close(priv.ipc_usr_fd);
		close(priv.dev_mem_fd);
		/* unload ipc-shm-usr kernel module */
		if (delete_module(IPC_ISR_MODULE_NAME, O_NONBLOCK) != 0)
			shm_err("Can't unload %s module\n",
						IPC_ISR_MODULE_NAME);
	}
}

/**
 * ipc_os_get_local_shm() - get local shared mem address
 */
uintptr_t ipc_os_get_local_shm(const uint8_t instance)
{
	return (uintptr_t)priv.id[instance].local_virt_shm;
}

/**
 * ipc_os_get_remote_shm() - get remote shared mem address
 */
uintptr_t ipc_os_get_remote_shm(const uint8_t instance)
{
	return (uintptr_t)priv.id[instance].remote_virt_shm;
}

/**
 * ipc_os_poll_channels() - invoke rx callback configured at initialization
 *
 * Return: work done, error code otherwise
 */
int ipc_os_poll_channels(const uint8_t instance)
{
	/* the softirq will handle rx operation if rx interrupt is configured */
	if (priv.id[instance].irq_num == IPC_IRQ_NONE) {
		if (priv.rx_cb != NULL)
			return priv.rx_cb(instance, IPC_SOFTIRQ_BUDGET);
		else
			return -EINVAL;
	}

	return -EOPNOTSUPP;
}

/**
 * ipc_hw_irq_enable() - enable notifications from remote
 */
void ipc_hw_irq_enable(const uint8_t instance)
{
	ioctl(priv.ipc_usr_fd,
			IPC_USR_CMD_ENABLE_RX_IRQ, instance);
}

/**
 * ipc_hw_irq_disable() - disable notifications from remote
 */
void ipc_hw_irq_disable(const uint8_t instance)
{
	ioctl(priv.ipc_usr_fd,
			IPC_USR_CMD_DISABLE_RX_IRQ, instance);
}

/**
 * ipc_hw_irq_notify() - notify remote that data is available
 */
void ipc_hw_irq_notify(const uint8_t instance)
{
	ioctl(priv.ipc_usr_fd,
			IPC_USR_CMD_TRIGGER_TX_IRQ, instance);
}

int ipc_hw_init(const uint8_t instance, const struct ipc_shm_cfg *cfg)
{
	/* dummy implementation: ipc-hw init is handled by kernel module */
	return 0;
}

void ipc_hw_free(const uint8_t instance)
{
	/* dummy implementation: ipc-hw free is handled by kernel module */
}
