/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2021-2023 NXP
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/completion.h>
#include <linux/of_address.h>

#include "ipc-shm.h"
#include "ipcf_Ip_Cfg.h"

#define MODULE_NAME "ipc-shm-sample_multi-instance"
#define MODULE_VER "0.1"

MODULE_AUTHOR("NXP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS(MODULE_NAME);
MODULE_DESCRIPTION("NXP Shared Memory IPC Sample Application Module");
MODULE_VERSION(MODULE_VER);

#define CTRL_CHAN_ID 0
#define CTRL_CHAN_SIZE 64
#define MAX_SAMPLE_MSG_LEN 32

#define INSTANCE_ID0	0
#define INSTANCE_ID1	1

/* convenience wrappers for printing messages */
#define sample_fmt(fmt) MODULE_NAME": %s(): "fmt
#define sample_err(fmt, ...) pr_err(sample_fmt(fmt), __func__, ##__VA_ARGS__)
#define sample_warn(fmt, ...) pr_warn(sample_fmt(fmt), __func__, ##__VA_ARGS__)
#define sample_info(fmt, ...) pr_info(MODULE_NAME": "fmt, ##__VA_ARGS__)
#define sample_dbg(fmt, ...) pr_debug(sample_fmt(fmt), __func__, ##__VA_ARGS__)

static int msg_sizes[IPC_SHM_MAX_POOLS] = {MAX_SAMPLE_MSG_LEN};
static int msg_sizes_argc = 1;
module_param_array(msg_sizes, int, &msg_sizes_argc, 0000);
MODULE_PARM_DESC(msg_sizes, "Sample message sizes");

/**
 * struct ipc_sample_app - sample app private data
 * @num_channels:	number of channels configured in this sample
 * @num_msgs:		number of messages to be sent to remote app
 * @ctrl_shm:		control channel local shared memory
 * @last_rx_no_msg:	last number of received message
 * @ipc_kobj:		sysfs kernel object
 * @ping_attr:		sysfs ping command attributes
 * @local_virt_shm:	local shared memory virtual address
 */
static struct ipc_sample_app {
	int num_channels;
	int num_msgs;
	char *ctrl_shm[2];
	int last_rx_no_msg;
	struct kobject *ipc_kobj_ins[2];
	struct kobj_attribute ping_attr_ins[2];
	uintptr_t local_virt_shm;
} app;

/* link with generated variables */
const void *rx_cb_arg = &app;

/* Completion variable used to sync send_msg func with shm_rx_cb */
static DECLARE_COMPLETION(reply_received);

/* Init IPC shared memory driver (see ipc-shm.h for API) */
static int init_ipc_shm(void)
{
	int err;
	int i = 0;

	err = ipc_shm_init(&ipcf_shm_instances_cfg);
	if (err)
		return err;
	app.num_channels = ipcf_shm_instances_cfg.shm_cfg[0].num_channels;

	/* acquire control channel memory once */
	for (i = 0; i <= INSTANCE_ID1; i++) {
		app.ctrl_shm[i] = ipc_shm_unmanaged_acquire(i, CTRL_CHAN_ID);
		if (!app.ctrl_shm[i]) {
			sample_err("failed to get memory of control channel");
			return -ENOMEM;
		}
	}

	return 0;
}

/* alternative implementation of kstrtol */
uint32_t ipc_strtol(char *src)
{
	uint32_t res = 0;

	while ((*src >= '0') && (*src <= '9'))
		res = res*10 + (*(src++) - '0');

	return res;
}

/*
 * data channel Rx callback: print message, release buffer and signal the
 * completion variable.
 */
void data_chan_rx_cb(void *arg, const uint8_t instance, int chan_id,
		void *buf, size_t size)
{
	int err = 0;
	struct ipc_sample_app *cb_arg_sample =
			(struct ipc_sample_app *)(*((uintptr_t *)arg));

	WARN_ON(size > MAX_SAMPLE_MSG_LEN);
	/* process the received data */
	sample_info("ch %d << %ld bytes: %s\n", chan_id, size, (char *)buf);

	/* consume received data: get number of message */
	/* Note: without being copied locally */
	cb_arg_sample->last_rx_no_msg = ipc_strtol((char *)buf + strlen("#"));

	/* release the buffer */
	err = ipc_shm_release_buf(instance, chan_id, buf);
	if (err) {
		sample_err("failed to free buffer for channel %d,"
			"err code %d\n", chan_id, err);
	}

	/* notify send_msg function a reply was received */
	complete(&reply_received);
}

/*
 * control channel Rx callback: print control message
 */
void ctrl_chan_rx_cb(void *arg, const uint8_t instance, int chan_id,
		void *mem)
{
	WARN_ON(chan_id != CTRL_CHAN_ID);
	WARN_ON(strlen(mem) > MAX_SAMPLE_MSG_LEN);

	sample_info("ch %d << %ld bytes: %s\n",
		    chan_id, strlen(mem), (char *)mem);
}

/* send control message with number of data messages to be sent */
static int send_ctrl_msg(const uint8_t instance)
{
	/* last channel is control channel */
	const int chan_id = CTRL_CHAN_ID;
	char tmp[CTRL_CHAN_SIZE];
	int err;

	/* Write number of messages to be sent in control channel memory.
	 * Use stack temp buffer because sprintf may do unaligned memory writes
	 * in SRAM and A53 will complain about unaligned accesses.
	 */
	sprintf(tmp, "SENDING MESSAGES: %d", app.num_msgs);
	strcpy(app.ctrl_shm[instance], tmp);

	sample_info("INST%d ch %d >> %ld bytes: %s\n", instance, chan_id,
		strlen(tmp), tmp);

	/* notify remote */
	err = ipc_shm_unmanaged_tx(instance, chan_id);
	if (err) {
		sample_err("tx failed on control channel");
		return err;
	}

	return 0;
}

/**
 * generate_msg() - generates message from module param ipc_shm_sample_msg
 * @s:		destination buffer
 * @len:	destination buffer length
 * @msg_no:	message number
 *
 * Generate message by repeated concatenation of a pattern
 */
static void generate_msg(char *s, int len, int msg_no)
{
	char tmp[MAX_SAMPLE_MSG_LEN];

	/* Write number of messages to be sent in control channel memory.
	 * Use stack temp buffer because sprintf may do unaligned memory writes
	 * in SRAM and A53 will complain about unaligned accesses.
	 */
	sprintf(tmp, "#%d HELLO WORLD! from KERNEL", msg_no);
	strcpy(s, tmp);
}

/**
 * send_data_msg() - Send generated data message to remote peer
 * @msg_len: message length
 * @msg_no: message sequence number to be written in the test message
 * @chan_id: ipc channel to be used for remote CPU communication
 *
 * It uses a completion variable for synchronization with reply callback.
 */
static int send_data_msg(const uint8_t instance, int msg_len, int msg_no,
		int chan_id)
{
	int err = 0;
	char *buf = NULL;

	buf = ipc_shm_acquire_buf(instance, chan_id, msg_len);
	if (!buf) {
		sample_err("failed to get buffer on instance %d for channel ID"
			" %d and size %d\n", instance, chan_id, msg_len);
		return -ENOMEM;
	}

	/* write data to acquired buffer */
	generate_msg(buf, msg_len, msg_no);

	sample_info("INST%d ch %d >> %d bytes: %s\n", instance, chan_id,
		msg_len, buf);

	/* send data to remote peer */
	err = ipc_shm_tx(instance, chan_id, buf, msg_len);
	if (err) {
		sample_err("tx failed for channel ID %d, size "
			"%d, error code %d\n", 0, msg_len, err);
		return err;
	}

	/* wait for echo reply from remote (signaled from Rx callback) */
	err = wait_for_completion_interruptible(&reply_received);
	if (err == -ERESTARTSYS) {
		sample_info("interrupted...\n");
		return err;
	}

	/* check if received message not match with sent message */
	if (app.last_rx_no_msg != msg_no) {
		sample_err("last_rx_no_msg != msg_no\n");
		sample_err(">> #%d\n", msg_no);
		sample_err("<< #%d\n", app.last_rx_no_msg);
		return -EINVAL;
	}

	return 0;
}

/*
 * Send requested number of messages to remote peer on instance 0,
 * cycling through all data, channels and wait for an echo reply
 * after each sent message.
 */
static int run_demo_ins0(int num_msgs)
{
	int err, msg, ch, i;

	sample_info("starting demo on instance 0...\n");

	/* signal number of messages to remote via control channel */
	err = send_ctrl_msg(INSTANCE_ID0);
	if (err)
		return err;

	/* send generated data messages */
	msg = 0;
	while (msg < num_msgs) {
		for (ch = CTRL_CHAN_ID + 1; ch < app.num_channels; ch++) {
			for (i = 0; i < msg_sizes_argc; i++) {
				err = send_data_msg(INSTANCE_ID0, msg_sizes[i],
					msg, ch);
				if (err)
					return err;

				if (++msg == num_msgs) {
					sample_info("exit demo - instance 0\n");
					return 0;
				}
			}
		}
	}

	return 0;
}

/*
 * Send requested number of messages to remote peer, cycling through all data
 * channels and wait for an echo reply after each sent message.
 */
static int run_demo_ins1(int num_msgs)
{
	int err, msg, ch, i;

	sample_info("starting demo on instance 1...\n");

	/* signal number of messages to remote via control channel */
	err = send_ctrl_msg(INSTANCE_ID1);
	if (err)
		return err;

	/* send generated data messages */
	msg = 0;
	while (msg < num_msgs) {
		for (ch = CTRL_CHAN_ID + 1; ch < app.num_channels; ch++) {
			for (i = 0; i < msg_sizes_argc; i++) {
				err = send_data_msg(INSTANCE_ID1,
						msg_sizes[i], msg, ch);
				if (err)
					return err;

				if (++msg == num_msgs) {
					sample_info("exit demo - instance 1\n");
					return 0;
				}
			}
		}
	}

	return 0;
}

/*
 * callback called when reading sysfs instance 0 command file
 */
static ssize_t ipc_sysfs_show_ins0(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	int value = 0;

	if (strcmp(attr->attr.name, app.ping_attr_ins[0].attr.name) == 0)
		value = app.num_msgs;

	return sprintf(buf, "%d\n", value);
}

/*
 * callback called when reading sysfs instance 1 command file
 */
static ssize_t ipc_sysfs_show_ins1(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	int value = 0;

	if (strcmp(attr->attr.name, app.ping_attr_ins[1].attr.name) == 0)
		value = app.num_msgs;

	return sprintf(buf, "%d\n", value);
}

/*
 * callback called when writing in sysfs instance 0 command file
 */
static ssize_t ipc_sysfs_store_ins0(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf,
				size_t count)
{
	int value;
	int err;

	err = kstrtoint(buf, 0, &value);
	if (err)
		return err;

	if (strcmp(attr->attr.name, app.ping_attr_ins[0].attr.name) == 0) {
		app.num_msgs = value;
		run_demo_ins0(value);
	}

	return count;
}

/*
 * callback called when writing in sysfs instance 1 command file
 */
static ssize_t ipc_sysfs_store_ins1(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t count)
{
	int value;
	int err;

	err = kstrtoint(buf, 0, &value);
	if (err)
		return err;

	if (strcmp(attr->attr.name, app.ping_attr_ins[1].attr.name) == 0) {
		app.num_msgs = value;
		run_demo_ins1(value);
	}

	return count;
}

/*
 * Init sysfs folder and command file for two instances
 */
static int init_sysfs(void)
{
	int err = 0;
	int i = 0;
	struct kobj_attribute ping_attr_ins0 =
		__ATTR(ping, 0600, ipc_sysfs_show_ins0, ipc_sysfs_store_ins0);
	struct kobj_attribute ping_attr_ins1 =
		__ATTR(ping, 0600, ipc_sysfs_show_ins1, ipc_sysfs_store_ins1);

	app.ping_attr_ins[0] = ping_attr_ins0;
	app.ping_attr_ins[1] = ping_attr_ins1;

	/* create ipc-sample folder in sys/kernel */
	app.ipc_kobj_ins[0] = kobject_create_and_add("ipc-shm-sample-instance0",
							kernel_kobj);
	if (!app.ipc_kobj_ins[0])
		return -ENOMEM;
	app.ipc_kobj_ins[1] = kobject_create_and_add("ipc-shm-sample-instance1",
							kernel_kobj);
	if (!app.ipc_kobj_ins[1])
		return -ENOMEM;

	/* create sysfs file for ipc sample ping command */
	for (i = 0; i <= INSTANCE_ID1; i++) {
		err = sysfs_create_file(app.ipc_kobj_ins[i],
						&app.ping_attr_ins[i].attr);
		if (err) {
			sample_err("INST%d creates sysfs "
					"file failed error %d\n", i, err);
			kobject_put(app.ipc_kobj_ins[i]);
			return err;
		}
	}

	return 0;
}

static void free_sysfs(void)
{
	int i = 0;

	for (i = 0; i <= INSTANCE_ID1; i++)
		kobject_put(app.ipc_kobj_ins[i]);
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
	struct resource *err = NULL;
	int i;
	uint32_t local_addr;
	uint32_t shm_size;

	sample_dbg("module version "MODULE_VER" exit\n");

	free_sysfs();
	ipc_shm_free();

	/* clear shared memory region */
	for (i = 0; i < ipcf_shm_instances_cfg.num_instances; i++) {
		local_addr = ipcf_shm_instances_cfg.shm_cfg[i].local_shm_addr;
		shm_size = ipcf_shm_instances_cfg.shm_cfg[i].shm_size;

		err = request_mem_region((phys_addr_t) local_addr, shm_size,
								"sample");
		if (err == NULL) {
			sample_info("Unable to reserve local shm region\n");
			return;
		}
		app.local_virt_shm = (uintptr_t)ioremap(local_addr, shm_size);
		memset_io((void *)app.local_virt_shm, 0, shm_size);
		iounmap((void *)app.local_virt_shm);
		release_mem_region((phys_addr_t) local_addr, shm_size);
	}
}

module_init(sample_mod_init);
module_exit(sample_mod_exit);

