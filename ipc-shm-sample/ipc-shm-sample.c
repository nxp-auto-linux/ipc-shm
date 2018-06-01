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

MODULE_AUTHOR("NXP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS(MODULE_NAME);
MODULE_DESCRIPTION("NXP Shared Memory IPC Sample Application Module");
MODULE_VERSION(MODULE_VER);

#define LOCAL_SHM_ADDR 0x34000000
#define IPC_SHM_SIZE 0x100000 /* 1M local shm, 1M remote shm */
#define REMOTE_SHM_ADDR (LOCAL_SHM_ADDR + IPC_SHM_SIZE)
#define SHM_SAMPLE_BUF_SIZE 256

/* convenience wrappers for printing messages */
#define sample_fmt(fmt) MODULE_NAME": %s(): "fmt
#define sample_err(fmt, ...) pr_err(sample_fmt(fmt), __func__, ##__VA_ARGS__)
#define sample_warn(fmt, ...) pr_warn(sample_fmt(fmt), __func__, ##__VA_ARGS__)
#define sample_info(fmt, ...) pr_info(MODULE_NAME": "fmt, ##__VA_ARGS__)
#define sample_dbg(fmt, ...) pr_debug(sample_fmt(fmt), __func__, ##__VA_ARGS__)

static char *ipc_shm_sample_msg = "Hello from "MODULE_NAME"!";
module_param(ipc_shm_sample_msg, charp, 0660);
MODULE_PARM_DESC(ipc_shm_sample_msg, "Message to be sent to the remote app.");

static void shm_sample_rx_cb(void *cb_arg, int chan_id, void *buf, size_t size);

/**
 * struct ipc_sample_priv - sample private data
 * @run_cmd:		run command state: 1 - start; 0 - stop
 * @ipc_kobj:		sysfs kernel object
 * @run_attr:		sysfs run command attributes
 * @shm_sample_sema:	semaphore used to sync with Autosar app
 */
struct ipc_sample_priv {
	int run_cmd;
	struct kobject *ipc_kobj;
	struct kobj_attribute run_attr;
	struct semaphore shm_sample_sema;
};

/* sample private data */
static struct ipc_sample_priv priv;

/* IPC shared memory parameters (see ipc-shm.h) */
static struct ipc_shm_cfg shm_cfg = {
	.local_shm_addr = (void *)LOCAL_SHM_ADDR,
	.remote_shm_addr = (void *)REMOTE_SHM_ADDR,
	.shm_size = IPC_SHM_SIZE,
	.channels = {
		{
			.type = SHM_CHANNEL_MANAGED,
			.memory = {
				.managed = {
					.pools = {
						{
							.num_bufs = 5,
							.buf_size = 256
						},
					},
				},
			},
			.ops = {
				.cb_arg = &priv,
				.rx_cb = shm_sample_rx_cb,
				.rx_unmanaged_cb = NULL,
			},
		},
	}
};

/*
 * shm RX callback. Prints the data, releases the channel and releases the
 * semaphore.
 */
static void shm_sample_rx_cb(void *cb_arg, int chan_id, void *buf,
			     size_t size)
{
	int err = 0;

	/* process the received data */
	sample_info("channel %d: received %d bytes message: %s",
		    chan_id, (int)size, (char *)buf);

	/* release the buffer */
	err = ipc_shm_release_buf(chan_id, buf);
	if (err) {
		sample_err("failed to free buffer for channel %d,"
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
	int chan_id = 0;
	uint8_t *buf = NULL;

	sample_info("starting demo...\n");

	/* initialize the semaphore with total number of available buffers */
	sema_init(&priv.shm_sample_sema, 1);

	sample_dbg("semaphore initialized...\n");

	while (priv.run_cmd) {

		/* get semaphore */
		err = down_interruptible(&priv.shm_sample_sema);
		if (err == -EINTR) {
			sample_info("[loop %d]:interrupted...\n", i);
			break;
		}

		sample_info("[loop %d]: starting...\n", ++i);
		if (err) {
			sample_err("failed to get semaphore for channel ID %d, "
				   "error code %d\n", 0, err);
			break;
		}

		buf = ipc_shm_acquire_buf(chan_id, SHM_SAMPLE_BUF_SIZE);
		if (!buf) {
			sample_err("failed to get buffer for channel ID %d with size %d\n",
				chan_id, SHM_SAMPLE_BUF_SIZE);
			err = -ENOMEM;
			break;
		}

		/* write data to buf */
		snprintf(buf, SHM_SAMPLE_BUF_SIZE, "%s #%d",
			 ipc_shm_sample_msg, i);

		/* send data to peer */
		err = ipc_shm_tx(chan_id, buf, strlen(buf));
		if (err) {
			sample_err("tx failed for channel ID %d, size %d, "
				   "error code %d\n", 0,
				   SHM_SAMPLE_BUF_SIZE, err);
			break;
		}

		sample_info("channel %d: sent %d bytes message: %s",
			    chan_id, SHM_SAMPLE_BUF_SIZE, (char *)buf);
	}

	sample_dbg("exit, error code is %d\n", err);
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
			sample_info("stopping demo...\n");
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
		sample_err("sysfs file creation failed, error code %d\n", err);
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

static int __init sample_mod_init(void)
{
	int err = 0;

	sample_dbg("module version "MODULE_VER" init\n");

	err = ipc_sysfs_init();
	if (err)
		return err;

	err = ipc_shm_init(&shm_cfg);
	if (err)
		goto err_sysfs_free;

	return 0;

err_sysfs_free:
	ipc_sysfs_free();
	return err;
}

static void __exit sample_mod_exit(void)
{
	sample_dbg("module version "MODULE_VER" exit\n");
	/* stop the demo */
	priv.run_cmd = 0;

	ipc_shm_free();
	ipc_sysfs_free();
}

module_init(sample_mod_init);
module_exit(sample_mod_exit);
