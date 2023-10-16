/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2019-2023 NXP
 */
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <stdlib.h>
#include <dirent.h>

#include "ipc-os.h"
#include "ipc-hw.h"
#include "ipc-shm.h"
#include "ipc-uio.h"

#define IPC_UIO_DEV_MEM_NAME    "/dev/mem"
#define IPC_UIO_CDEV_NAME       "/dev/ipc-cdev-uio"
#define IPC_SHM_UIO_BUF_LEN     255
#define IPC_SHM_UIO_DIR         "/sys/class/uio"
#define IPC_UIO_PARAMS_LEN      130
#define UIO_DRIVER_NAME         "ipc-shm-uio"
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
 * @state:              state to indicate whether instance is initialized
 * @instance:           target instance
 * @irq_num:            target instance interrupt index
 * @uio_fd:             UIO device file descriptor
 * @local_virt_shm:     local ShM virtual address
 * @remote_virt_shm:    remote ShM virtual address
 * @local_shm_map:      local ShM mapped page address
 * @remote_shm_map:     remote ShM mapped page address
 * @local_shm_offset:   local ShM offset in mapped page
 * @remote_shm_offset:  remote ShM offset in mapped page
 * @shm_size:           local/remote ShM size
 * @rx_cb:              upper layer Rx callback function
 * @irq_thread_id:      Rx interrupt thread id
 */
struct ipc_os_priv_instance {
	uint8_t state;
	uint8_t instance;
	int irq_num;
	int uio_fd;
	void *local_virt_shm;
	void *remote_virt_shm;
	void *local_shm_map;
	void *remote_shm_map;
	size_t local_shm_offset;
	size_t remote_shm_offset;
	size_t shm_size;
	int (*rx_cb)(const uint8_t instance, int budget);
	pthread_t irq_thread_id;
};

/**
 * struct ipc_os_priv_type - OS specific private data
 * @ipc_files_opened:    indicate whether device files are opened
 * @ipc_cdev_fd:         kernel character device file descriptor
 * @dev_mem_fd:	         MEM device file descriptor
 * @id:                 private data per instance
 */
static struct ipc_os_priv_type {
	uint8_t ipc_files_opened;
	int ipc_cdev_fd;
	int dev_mem_fd;
	struct ipc_os_priv_instance id[IPC_SHM_MAX_INSTANCES];
} ipc_os_priv;

/** read first line from file */
static int line_from_file(char *filename, char *buf)
{
	char *s;
	int i;
	FILE *file = fopen(filename, "r");

	memset(buf, 0, IPC_UIO_PARAMS_LEN);
	if (!file)
		return -ENONET;

	s = fgets(buf, IPC_SHM_UIO_BUF_LEN, file);
	if (!s)
		return -EIO;

	/* read first line only */
	for (i = 0; (*s) && (i < IPC_SHM_UIO_BUF_LEN); i++) {
		if (*s == '\n') {
			*s = 0;
			break;
		}
		s++;
	}

	fclose(file);
	return 0;
}

/** check whether first line from filename matched filter string */
static int line_match(char *filename, char *filter)
{
	int err;
	char linebuf[IPC_SHM_UIO_BUF_LEN];

	err = line_from_file(filename, linebuf);

	if (err != 0)
		return err;

	err = strncmp(linebuf, filter, IPC_SHM_UIO_BUF_LEN);
	if (err != 0)
		return EINVAL;

	return 0;
}

/** find the first UIO device that matched the kernel counterpart*/
static int get_uio_dev_name(char *dev_name, const uint8_t instance)
{
	struct dirent **name_list;
	int nentries, count, i, err;
	char filename[IPC_SHM_UIO_BUF_LEN];
	char uio_name[32];

	err = sprintf(uio_name, "instance_%d", instance);
	if (err <= 0)
		return -EIO;

	nentries = scandir(IPC_SHM_UIO_DIR, &name_list, NULL, alphasort);
	if (nentries < 0)
		return -EIO;

	count = nentries;
	while (count--) {
		/*match name*/
		err = snprintf(filename, sizeof(filename),
			IPC_SHM_UIO_DIR "/%s/name", name_list[count]->d_name);
		if (err <= 0)
			return -EIO;
		if (line_match(filename, uio_name) != 0)
			continue;

		/*match version*/
		err = snprintf(filename,
			sizeof(filename),
			IPC_SHM_UIO_DIR "/%s/version",
			name_list[count]->d_name);
		if (err <= 0)
			return -EIO;
		if (line_match(filename, DRIVER_VERSION) != 0)
			continue;

		break;
	}
	if (count >= 0) {
		snprintf(dev_name, IPC_SHM_UIO_BUF_LEN,
			name_list[count]->d_name);
	}
	/* free memory allocated by scandir */
	for (i = 0; i < nentries; i++)
		free(name_list[i]);
	free(name_list);

	return count >= 0 ? 0 : -ENONET;
}

/* Rx sotfirq thread */
static void *ipc_shm_softirq(void *arg)
{
	const int budget = IPC_SOFTIRQ_BUDGET;
	int irq_count;
	int work;
	struct ipc_os_priv_instance *info = (struct ipc_os_priv_instance *)arg;

	while (1) {
		/* block(sleep) until notified from kernel IRQ handler */
		if (info->rx_cb != NULL)
			read(info->uio_fd, &irq_count, sizeof(irq_count));

		do {
			work = info->rx_cb(info->instance, budget);
			/* work not done, yield and wait for reschedule */
			sched_yield();
		} while (work >= budget);

		/* re-enable irq */
		ipc_hw_irq_enable(info->instance);
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
	int err, i;
	int ipc_uio_module_fd;
	struct sched_param irq_thread_param;
	pthread_attr_t irq_thread_attr;
	struct ipc_uio_cdev_data data_cfg;

	if (!rx_cb)
		return -EINVAL;

	/* save params */
	ipc_os_priv.id[instance].shm_size = cfg->shm_size;
	ipc_os_priv.id[instance].rx_cb = rx_cb;
	ipc_os_priv.id[instance].instance = instance;

	if (ipc_os_priv.ipc_files_opened == (uint8_t)IPC_STATUS_CLEAR) {
		/* open ipc-shm-uio kernel module */
		ipc_uio_module_fd = open(IPC_UIO_MODULE_PATH, O_RDONLY);
		if (ipc_uio_module_fd == -1) {
			shm_err("Can't open %s module\n", IPC_UIO_MODULE_PATH);
			return -ENODEV;
		}

		if (finit_module(ipc_uio_module_fd, "", 0) != 0) {
			shm_err("Can't load %s module\n", IPC_UIO_MODULE_PATH);
			err = -ENODEV;
			goto err_close_ipc_shm_uio;
		}

		/* open ipc-cdev-uio" device for initialization support */
		ipc_os_priv.ipc_cdev_fd = open(IPC_UIO_CDEV_NAME, O_RDWR);
		if (ipc_os_priv.ipc_cdev_fd == -1) {
			shm_err("Can't open %s device\n", IPC_UIO_CDEV_NAME);
			err = -ENODEV;
			goto err_close_ipc_shm_uio;
		}

		/* open MEM device for interrupt support */
		ipc_os_priv.dev_mem_fd = open(IPC_UIO_DEV_MEM_NAME, O_RDWR);
		if (ipc_os_priv.dev_mem_fd == -1) {
			shm_err("Can't open %s device\n", IPC_UIO_DEV_MEM_NAME);
			err = -ENODEV;
			goto err_close_ipc_cdev_uio;
		}

		ipc_os_priv.ipc_files_opened = (uint8_t)IPC_STATUS_SET;
	}
	/* map local physical shared memory */
	/* truncate address to a multiple of page size, or mmap will fail */
	page_phys_addr = (cfg->local_shm_addr / page_size) * page_size;
	ipc_os_priv.id[instance].local_shm_offset
		= cfg->local_shm_addr - page_phys_addr;

	ipc_os_priv.id[instance].local_shm_map
		= mmap(NULL,
			ipc_os_priv.id[instance].local_shm_offset
				+ cfg->shm_size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			ipc_os_priv.dev_mem_fd,
			page_phys_addr);
	if (ipc_os_priv.id[instance].local_shm_map == MAP_FAILED) {
		shm_err("Can't map memory: %lx\n", cfg->local_shm_addr);
		err = -ENOMEM;
		goto err_close_mem_dev;
	}

	ipc_os_priv.id[instance].local_virt_shm
		= ipc_os_priv.id[instance].local_shm_map
			+ ipc_os_priv.id[instance].local_shm_offset;

	/* map remote physical shared memory */
	page_phys_addr = (cfg->remote_shm_addr / page_size) * page_size;
	ipc_os_priv.id[instance].remote_shm_offset
		= cfg->remote_shm_addr - page_phys_addr;

	ipc_os_priv.id[instance].remote_shm_map
		= mmap(NULL,
			ipc_os_priv.id[instance].remote_shm_offset
				+ cfg->shm_size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			ipc_os_priv.dev_mem_fd,
			page_phys_addr);
	if (ipc_os_priv.id[instance].remote_shm_map == MAP_FAILED) {
		shm_err("Can't map memory: %lx\n", cfg->remote_shm_addr);
		err = -ENOMEM;
		goto err_unmap_local_shm;
	}

	ipc_os_priv.id[instance].remote_virt_shm
		= ipc_os_priv.id[instance].remote_shm_map
			+ ipc_os_priv.id[instance].remote_shm_offset;

	ipc_os_priv.id[instance].irq_num = cfg->inter_core_rx_irq;
	/* Write data to char dev */
	data_cfg.instance = instance;
	data_cfg.cfg = *cfg;

	err = write(ipc_os_priv.ipc_cdev_fd,
			&data_cfg,
			sizeof(struct ipc_uio_cdev_data));
	if (err < 0) {
		err = -EINVAL;
		goto err_unmap_remote_shm;
	}

	if (cfg->inter_core_rx_irq == IPC_IRQ_NONE) {
		ipc_os_priv.id[instance].state = IPC_SHM_INSTANCE_ENABLED;
		return 0;
	}

	/* search for UIO device name */
	char uio_dev_name[IPC_SHM_UIO_BUF_LEN];
	char dev_uio[IPC_SHM_UIO_BUF_LEN*2];

	err = get_uio_dev_name(uio_dev_name, instance);
	if (err != 0) {
		err = -ENOENT;
		goto err_unmap_remote_shm;
	}
	snprintf(dev_uio, sizeof(dev_uio), "/dev/%s", uio_dev_name);

	/* open UIO device for interrupt support */
	ipc_os_priv.id[instance].uio_fd = open(dev_uio, O_RDWR);
	if (ipc_os_priv.id[instance].uio_fd == -1) {
		shm_err("Can't open %s device\n", dev_uio);
		err = -ENODEV;
		goto err_unmap_remote_shm;
	}

	/* start Rx softirq thread with the highest priority for its policy */
	err = pthread_attr_init(&irq_thread_attr);
	if (err != 0) {
		goto err_close_uio_dev;
		shm_err("Can't initialize Rx softirq attributes\n");
	}

	err = pthread_attr_setschedpolicy(&irq_thread_attr, RX_SOFTIRQ_POLICY);
	if (err != 0) {
		goto err_close_uio_dev;
		shm_err("Can't set Rx softirq policy\n");
	}

	irq_thread_param.sched_priority = sched_get_priority_max(
		RX_SOFTIRQ_POLICY);
	err = pthread_attr_setschedparam(&irq_thread_attr, &irq_thread_param);
	if (err != 0) {
		goto err_close_uio_dev;
		shm_err("Can't set Rx softirq scheduler parameters\n");
	}

	err = pthread_create(&ipc_os_priv.id[instance].irq_thread_id,
				&irq_thread_attr,
				ipc_shm_softirq,
				&ipc_os_priv.id[instance]);
	if (err == -1) {
		shm_err("Can't start Rx softirq thread\n");
		goto err_close_uio_dev;
	}
	shm_dbg("Created Rx softirq thread with priority=%d\n",
		irq_thread_param.sched_priority);
	shm_dbg("done\n");
	ipc_os_priv.id[instance].state = IPC_SHM_INSTANCE_ENABLED;

	return 0;

err_close_uio_dev:
	close(ipc_os_priv.id[instance].uio_fd);
err_unmap_remote_shm:
	munmap(ipc_os_priv.id[instance].remote_shm_map,
		ipc_os_priv.id[instance].remote_shm_offset
			+ ipc_os_priv.id[instance].shm_size);
err_unmap_local_shm:
	munmap(ipc_os_priv.id[instance].local_shm_map,
		ipc_os_priv.id[instance].local_shm_offset
			+ ipc_os_priv.id[instance].shm_size);
err_close_mem_dev:
	close(ipc_os_priv.dev_mem_fd);
err_close_ipc_cdev_uio:
	close(ipc_os_priv.ipc_cdev_fd);
err_close_ipc_shm_uio:
	ipc_os_priv.id[instance].state = IPC_SHM_INSTANCE_DISABLED;
	for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
		if (ipc_os_priv.id[i].state == IPC_SHM_INSTANCE_ENABLED)
			return err;
	}
	ipc_os_priv.ipc_files_opened = IPC_STATUS_CLEAR;
	close(ipc_uio_module_fd);

	return err;
}

/**
 * ipc_os_free() - free OS specific resources
 */
void ipc_os_free(const uint8_t instance)
{
	void *res;
	int i;

	ipc_os_priv.id[instance].state = IPC_SHM_INSTANCE_DISABLED;
	if (ipc_os_priv.id[instance].irq_num != IPC_IRQ_NONE) {
		/* disable hardirq */
		ipc_hw_irq_disable(instance);

		shm_dbg("stopping irq thread\n");

		/* stop irq thread */
		pthread_cancel(ipc_os_priv.id[instance].irq_thread_id);
		pthread_join(ipc_os_priv.id[instance].irq_thread_id, &res);

		close(ipc_os_priv.id[instance].uio_fd);
	}

	/* unmap remote/local shm */
	munmap(ipc_os_priv.id[instance].remote_shm_map,
		ipc_os_priv.id[instance].remote_shm_offset
			+ ipc_os_priv.id[instance].shm_size);
	munmap(ipc_os_priv.id[instance].local_shm_map,
		ipc_os_priv.id[instance].local_shm_offset
			+ ipc_os_priv.id[instance].shm_size);
	/*
	 * Close all file descriptors and cancel soft thread
	 * only when all instances are disabled
	 */
	for (i = 0; i < IPC_SHM_MAX_INSTANCES; i++) {
		if (ipc_os_priv.id[i].state == IPC_SHM_INSTANCE_ENABLED) {
			return;
		}
	}
	close(ipc_os_priv.ipc_cdev_fd);
	close(ipc_os_priv.dev_mem_fd);

	/* unload ipc-uio kernel module */
	if (delete_module(IPC_UIO_MODULE_NAME, O_NONBLOCK) != 0) {
		shm_err("Can't unload %s module\n", IPC_UIO_MODULE_NAME);
	}
}

/**
 * ipc_os_get_local_shm() - get local shared mem address
 */
uintptr_t ipc_os_get_local_shm(const uint8_t instance)
{
	return (uintptr_t)ipc_os_priv.id[instance].local_virt_shm;
}

/**
 * ipc_os_get_remote_shm() - get remote shared mem address
 */
uintptr_t ipc_os_get_remote_shm(const uint8_t instance)
{
	return (uintptr_t)ipc_os_priv.id[instance].remote_virt_shm;
}

/**
 * ipc_os_poll_channels() - invoke rx callback configured at initialization
 *
 * Return: work done, error code otherwise
 */
int ipc_os_poll_channels(const uint8_t instance)
{
	/* the softirq will handle rx operation if rx interrupt is configured */
	if (ipc_os_priv.id[instance].irq_num == IPC_IRQ_NONE) {
		if (ipc_os_priv.id[instance].rx_cb != NULL)
			return ipc_os_priv.id[instance].rx_cb(instance,
							IPC_SOFTIRQ_BUDGET);
		else
			return -EINVAL;
	}

	return -EOPNOTSUPP;
}

static void ipc_send_uio_cmd(uint32_t uio_fd, int32_t cmd)
{
	int ret;

	ret = write(uio_fd, &cmd, sizeof(int));
	if (ret != sizeof(int)) {
		shm_dbg("Failed to execute UIO command %d", cmd);
	}
}

/**
 * ipc_hw_irq_enable() - enable notifications from remote
 */
void ipc_hw_irq_enable(const uint8_t instance)
{
	ipc_send_uio_cmd(ipc_os_priv.id[instance].uio_fd, IPC_UIO_ENABLE_CMD);
}

/**
 * ipc_hw_irq_disable() - disable notifications from remote
 */
void ipc_hw_irq_disable(const uint8_t instance)
{
	ipc_send_uio_cmd(ipc_os_priv.id[instance].uio_fd, IPC_UIO_DISABLE_CMD);
}

/**
 * ipc_hw_irq_notify() - notify remote that data is available
 */
void ipc_hw_irq_notify(const uint8_t instance)
{
	ipc_send_uio_cmd(ipc_os_priv.id[instance].uio_fd, IPC_UIO_TRIGGER_CMD);
}

int ipc_hw_init(const uint8_t instance, const struct ipc_shm_cfg *cfg)
{
	/* dummy implementation: ipc-hw init is handled by kernel UIO module */
	return 0;
}

void ipc_hw_free(const uint8_t instance)
{
	/* dummy implementation: ipc-hw free is handled by kernel UIO module */
}
