/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2025 NXP
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/completion.h>
#include <linux/of_address.h>

#include "ipcf_Ip_Cfg.h"
#include "ipc-shm.h"

#define MODULE_NAME "ipc-shm-sample_scmi"
#define MODULE_VER "0.1"

MODULE_AUTHOR("NXP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS(MODULE_NAME);
MODULE_DESCRIPTION("NXP Shared Memory IPC Sample Application Module");
MODULE_VERSION(MODULE_VER);

#define SCMI_MSG_PAYLOAD_INDEX_U32        (7u)
#define SCMI_MSG_PAYLOAD_BYTES_INDEX      (SCMI_MSG_PAYLOAD_INDEX_U32 * 4u)
#define MAX_SAMPLE_MSG_LEN                (192)

/* convenience wrappers for printing messages */
#define sample_fmt(fmt) MODULE_NAME": %s(): "fmt
#define sample_err(fmt, ...) pr_err(sample_fmt(fmt), __func__, ##__VA_ARGS__)
#define sample_warn(fmt, ...) pr_warn(sample_fmt(fmt), __func__, ##__VA_ARGS__)
#define sample_info(fmt, ...) pr_info(MODULE_NAME": "fmt, ##__VA_ARGS__)
#define sample_dbg(fmt, ...) pr_debug(sample_fmt(fmt), __func__, ##__VA_ARGS__)

/**
 * struct ipc_sample_app - sample app private data
 * @num_channels:	number of channels configured in this sample
 * @num_msgs:		number of messages to be sent to remote app
 * @ctrl_shm:		control channel local shared memory
 * @last_rx_no_msg:	last number of received message
 * @ipc_kobj:		sysfs kernel object
 * @ping_attr:		sysfs ping command attributes
 * @instance:		instance id
 * @local_virt_shm:	local shared memory virtual address
 */
static struct ipc_sample_app {
	int num_msgs;
	int last_rx_no_msg;
	struct kobject *ipc_kobj;
	struct kobj_attribute ping_attr;
	uint8_t instance;
} app;

/* link with generated variables */
const void *rx_cb_arg = &app;

/* Completion variable used to sync send_msg func with shm_rx_cb */
static DECLARE_COMPLETION(reply_received);

/* alternative implementation of kstrtol */
uint32_t ipc_strtol(char *src)
{
	uint32_t res = 0;

	while ((*src >= '0') && (*src <= '9'))
		res = res*10 + (*(src++) - '0');

	return res;
}

void scmi_chan_rx_cb(void *arg, const uint8_t instance, uint8_t chan_id,
		void *buf)
{
	int8_t err;
	char *tmp_buf = buf;
	struct ipc_sample_app *cb_arg_sample =
		(struct ipc_sample_app *)(*((uintptr_t *)arg));

	cb_arg_sample->last_rx_no_msg = ipc_strtol(
		(char *)&(tmp_buf[SCMI_MSG_PAYLOAD_BYTES_INDEX])
			+ strlen("DONE #"));

	/* check the data the SCMI channel */
	if ((tmp_buf[SCMI_MSG_PAYLOAD_BYTES_INDEX] == 'D')
		&& (tmp_buf[SCMI_MSG_PAYLOAD_BYTES_INDEX + 1] == 'O')
		&& (tmp_buf[SCMI_MSG_PAYLOAD_BYTES_INDEX + 2] == 'N')
			&& (tmp_buf[SCMI_MSG_PAYLOAD_BYTES_INDEX + 3] == 'E')) {
		sample_info("<< MSG PAYLOAD: DONE #%d\n",
				cb_arg_sample->last_rx_no_msg);
	} else {
		sample_info("NOT RECEIVE DONE!\n");
	}

	/* Set SCMI channel status to FREE */
	err = ipc_shm_free_scmi_channel(0, 0);
	if (0 != err)
		sample_err("failed to free SCMI channel");

	/* notify send_msg function a reply was received */
	complete(&reply_received);
}

/* Init IPC shared memory driver (see ipc-shm.h for API) */
static int init_ipc_shm(void)
{
	int err;

	do {
		err = ipc_shm_init(&ipcf_shm_instances_cfg);
	} while (err == -EAGAIN);
	if (err)
		return err;

	return 0;
}

/*
 * Send requested number of messages to remote peer, cycling through all data
 * channels and wait for an echo reply after each sent message.
 */
static int run_demo(int num_msgs)
{
	int err, msg;
	char *buf = NULL;
	char tmp[MAX_SAMPLE_MSG_LEN];

	sample_info("starting demo...\n");

	buf = ipc_shm_acquire_scmi(0, 0);
	if (!buf) {
		sample_err("failed to get buffer for scmi channel");
		return -ENOMEM;
	}

	/* send generated data messages */
	msg = 0;
	/* write data to acquired buffer */
	sprintf(tmp, "CALL #%d", num_msgs);
	strcpy(&buf[SCMI_MSG_PAYLOAD_BYTES_INDEX], tmp);

	sample_info("MSG PAYLOAD: CALL #%d\n", num_msgs);

	/* send data to remote peer */
	err = ipc_shm_scmi_tx(0, 0);
	if (err) {
		sample_err("tx failed for SCMI error code %d\n", err);
		return err;
	}
	while (msg < num_msgs) {

		/* wait for reply from remote (signaled from Rx callback) */
		err = wait_for_completion_interruptible(&reply_received);
		if (err == -ERESTARTSYS) {
			sample_info("interrupted...\n");
			return err;
		}

		if (++msg == num_msgs) {
			sample_info("exit demo\n");
			return 0;
		}
	}

	return 0;
}

/*
 * callback called when reading sysfs command file
 */
static ssize_t ipc_sysfs_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	int value = 0;

	if (strcmp(attr->attr.name, app.ping_attr.attr.name) == 0)
		value = app.num_msgs;

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

	if (strcmp(attr->attr.name, app.ping_attr.attr.name) == 0) {
		app.num_msgs = value;
		run_demo(value);
	}

	return count;
}

/*
 * Init sysfs folder and command file
 */
static int init_sysfs(void)
{
	int err = 0;
	struct kobj_attribute ping_attr =
		__ATTR(ping, 0600, ipc_sysfs_show, ipc_sysfs_store);
	app.ping_attr = ping_attr;

	/* create ipc-sample folder in sys/kernel */
	app.ipc_kobj = kobject_create_and_add(MODULE_NAME, kernel_kobj);
	if (!app.ipc_kobj) {
		sample_err("ipc-sample folder creation failed %d\n", err);
		return -ENOMEM;
	}

	/* create sysfs file for ipc sample ping command */
	err = sysfs_create_file(app.ipc_kobj, &app.ping_attr.attr);
	if (err) {
		sample_err("sysfs file for ping creation failed %d\n", err);
		goto err_kobj_free;
	}

	return 0;

err_kobj_free:
	kobject_put(app.ipc_kobj);
	return err;
}

static void free_sysfs(void)
{
	kobject_put(app.ipc_kobj);
}

static int __init sample_mod_init(void)
{
	int err = 0;

	sample_dbg("module version "MODULE_VER" init\n");

	/* init ipc shm driver */
	err = init_ipc_shm();
	if (err)
		return err;

	/* init sample sysfs UI */
	err = init_sysfs();
	if (err)
		return err;

	return 0;
}

static void __exit sample_mod_exit(void)
{
	sample_dbg("module version "MODULE_VER" exit\n");

	free_sysfs();
	ipc_shm_free();
}

module_init(sample_mod_init);
module_exit(sample_mod_exit);
