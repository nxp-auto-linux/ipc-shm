/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2018 NXP Semiconductors
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/semaphore.h>

#include "../ipc-shm-dev/ipc-shm.h"

#define MODULE_NAME "ipc-shm-sample"
#define MODULE_VER "0.1"
#define LOCAL_SHM_ADDR 0x34000000
#define IPC_SHM_SIZE 0x100000 /* 1M local shm, 1M remote shm */
#define REMOTE_SHM_ADDR (LOCAL_SHM_ADDR + IPC_SHM_SIZE)
#define SHM_SAMPLE_CHAN_BUF_SIZE 256

/* convenience wrappers for printing messages */
#define shm_fmt(fmt) MODULE_NAME": %s: "fmt
#define shm_pr_info(fmt, ...) pr_info(shm_fmt(fmt), __func__, ##__VA_ARGS__)
#define shm_pr_warn(fmt, ...) pr_warn(shm_fmt(fmt), __func__, ##__VA_ARGS__)
#define shm_pr_err(fmt, ...) pr_err(shm_fmt(fmt), __func__, ##__VA_ARGS__)
#define shm_pr_debug(fmt, ...) pr_debug(shm_fmt(fmt), __func__, ##__VA_ARGS__)

MODULE_AUTHOR("NXP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS(MODULE_NAME);
MODULE_DESCRIPTION("NXP Shared Memory IPC Sample Application Module");
MODULE_VERSION(MODULE_VER);

static char *ipc_shm_sample_msg = "Hello from "MODULE_NAME"!";
module_param(ipc_shm_sample_msg, charp, 0660);
MODULE_PARM_DESC(ipc_shm_sample_msg, "Message to be sent to the remote app.");

static void shm_sample_rx_cb(void *cb_arg, int chan_id, void *buf,
			     size_t size);

/**
 * struct ipc_sample_priv - sample private data
 * @run_cmd:		run command state: 1 - start; 0 - stop
 * @ipc_kobj:		sysfs kernel object
 * @run_attr:		sysfs run command attributes
 * @channel_params:	channel initialization parameters
 */
struct ipc_sample_priv {
	int run_cmd;
	struct kobject *ipc_kobj;
	struct kobj_attribute run_attr;
	struct semaphore shm_sample_sema;
	struct ipc_shm shm_cfg;
};

static struct ipc_sample_priv priv = {
	.shm_cfg = {
		.local_shm_addr = (void *)LOCAL_SHM_ADDR,
		.remote_shm_addr = (void *)REMOTE_SHM_ADDR,
		.shm_size = IPC_SHM_SIZE,
		.channels = {
		{
			.chan_id = 0,
			.type = SHM_CHANNEL_MANAGED,
			.memory = {
				.managed = {
					.pools = {
						{
							.num_buffers = 1,
							.buf_size = 4
						},
						{
							.num_buffers = 1,
							.buf_size = 256
						},
						{
							.num_buffers = 1,
							.buf_size = 4096
						},
					},
				},
			},
			.cb_arg = &priv,
			.rx_cb = shm_sample_rx_cb,
			.rx_unmanaged_cb = NULL,
		},
		}
	}
};
/*
 * shm RX callback. Prints the data, releases the channel and updates the
 * semaphore.
 */
static void shm_sample_rx_cb(void *cb_arg, int chan_id, void *buf,
			     size_t size)
{
	int err = 0;

	/* process the received data */
	shm_pr_info("channel %d: received %d byte(s) long message: %s",
		    chan_id, (int)size, (char *)buf);

	/* release the buffer */
	err = ipc_shm_release_buf(chan_id, buf);
	if (err) {
		shm_pr_warn("failed to free buffer for channel %d,"
			    "err code %d\n", chan_id, err);
	}

	/* update the semaphore */
	up(&priv.shm_sample_sema);
}

/*
 * Implements the ipc-shm sample.
 * The sample sends a message for each received message from the remote CPU.
 * It uses a semaphore for synchronization (incremented by the RX callback
 * and decremented before this thread sends data to the remote CPU).
 */
static int run_demo(void)
{
	int err = 0;
	int i = 0;
	uint8_t *buf = NULL;

	shm_pr_debug("starting demo...\n");

	/* initialize the semaphore with total number of available buffers */
	sema_init(&priv.shm_sample_sema, 3);

	shm_pr_debug("semaphore initialized...\n");

	while (priv.run_cmd) {

		/* get semaphore */
		err = down_interruptible(&priv.shm_sample_sema);

		if (err == -EINTR) {
			shm_pr_info("[loop %d]:interrupted...\n", i);
			break;
		}

		shm_pr_info("[loop %d]: starting...\n", ++i);

		if (err) {
			shm_pr_err("failed to get semaphore for channel ID %d, "
				   "error code %d\n", 0, err);
			break;
		}

		buf = ipc_shm_acquire_buf(0, SHM_SAMPLE_CHAN_BUF_SIZE);
		if (!buf) {
			shm_pr_err("failed to get buffer for channel ID %d and "
				   "size %d\n", 0, SHM_SAMPLE_CHAN_BUF_SIZE);
			err = -ENOMEM;
			break;
		}

		/* write data to buf */
		snprintf(buf, SHM_SAMPLE_CHAN_BUF_SIZE, "%s #%d",
			 ipc_shm_sample_msg, i);

		/* send data to peer */
		err = ipc_shm_tx(0, buf, strlen(buf));
		if (err) {
			shm_pr_err("tx failed for channel ID %d, size %d, "
				   "error code %d\n", 0,
				   SHM_SAMPLE_CHAN_BUF_SIZE, err);
			break;
		}
	}

	shm_pr_debug("exit, error code is %d\n", err);
	return err;
}

/*
 * callback called when reading sysfs command file
 */
static ssize_t ipc_sysfs_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	int value = 0;

	if (strcmp(attr->attr.name, priv.run_attr.attr.name) == 0) {
		value = priv.run_cmd;
	}

	return sprintf(buf, "%d\n", value);
}

/*
 * callback called when writing in sysfs command file
 */
static ssize_t ipc_sysfs_store(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t count)
{
	int value;
	int err;

	err = kstrtoint(buf, 0, &value);
	if (err)
		return err;

	if (strcmp(attr->attr.name, priv.run_attr.attr.name) == 0) {
		priv.run_cmd = value;
		if (priv.run_cmd == 0)
			shm_pr_debug("stopping demo...\n");
		else if (priv.run_cmd == 1)
			run_demo();
	}

	return count;
}

/*
 * Init sysfs folder and command file
 */
static int ipc_sysfs_init(void)
{
	int err = 0;
	struct kobj_attribute run_attr =
		__ATTR(run, 0600, ipc_sysfs_show, ipc_sysfs_store);
	priv.run_attr = run_attr;

	/* create ipc-sample folder in sys/kernel */
	priv.ipc_kobj = kobject_create_and_add(MODULE_NAME, kernel_kobj);
	if (!priv.ipc_kobj)
		return -ENOMEM;

	/* create sysfs file for ipc sample run command */
	err = sysfs_create_file(priv.ipc_kobj, &priv.run_attr.attr);
	if (err) {
		shm_pr_err("sysfs file creation failed, error code %d\n", err);
		goto err_kobj_free;
	}

	return 0;

err_kobj_free:
	kobject_put(priv.ipc_kobj);
	return err;
}

static void ipc_sysfs_free(void)
{
	kobject_put(priv.ipc_kobj);
}

static int __init shm_mod_init(void)
{
	int err = 0;

	shm_pr_info("module version "MODULE_VER" init\n");

	err = ipc_sysfs_init();
	if (err)
		return err;

	/* TODO: get ipc_shm_init err code after it is properly implemented */
	ipc_shm_init(&priv.shm_cfg);
	if (err)
		goto err_sysfs_free;

	return 0;

err_sysfs_free:
	ipc_sysfs_free();
	return err;
}

static void __exit shm_mod_exit(void)
{
	shm_pr_info("module exit\n");
	/* stop the demo */
	priv.run_cmd = 0;

	ipc_shm_free();
	ipc_sysfs_free();
}

module_init(shm_mod_init);
module_exit(shm_mod_exit);
