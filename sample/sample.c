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
#include <linux/completion.h>

#include "../ipc-shm.h"

#define MODULE_NAME "ipc-shm-sample"
#define MODULE_VER "0.1"

MODULE_AUTHOR("NXP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS(MODULE_NAME);
MODULE_DESCRIPTION("NXP Shared Memory IPC Sample Application Module");
MODULE_VERSION(MODULE_VER);

#if defined(CONFIG_SOC_S32GEN1)
	#define LOCAL_SHM_ADDR 0x34400000
#elif defined(CONFIG_SOC_S32V234)
	#define LOCAL_SHM_ADDR 0x3E800000
#else
	#error "Platform not supported"
#endif

#define IPC_SHM_SIZE 0x100000 /* 1M local shm, 1M remote shm */
#define REMOTE_SHM_ADDR (LOCAL_SHM_ADDR + IPC_SHM_SIZE)
#define S_BUF_LEN 16
#define M_BUF_LEN 256
#define L_BUF_LEN 4096

/* convenience wrappers for printing messages */
#define sample_fmt(fmt) MODULE_NAME": %s(): "fmt
#define sample_err(fmt, ...) pr_err(sample_fmt(fmt), __func__, ##__VA_ARGS__)
#define sample_warn(fmt, ...) pr_warn(sample_fmt(fmt), __func__, ##__VA_ARGS__)
#define sample_info(fmt, ...) pr_info(MODULE_NAME": "fmt, ##__VA_ARGS__)
#define sample_dbg(fmt, ...) pr_debug(sample_fmt(fmt), __func__, ##__VA_ARGS__)

static char *ipc_shm_sample_msg = "Hello world! ";
module_param(ipc_shm_sample_msg, charp, 0660);
MODULE_PARM_DESC(ipc_shm_sample_msg, "Message to be sent to the remote app.");

static int msg_sizes[IPC_SHM_MAX_POOLS] = {S_BUF_LEN};
static int msg_sizes_argc = 1;
module_param_array(msg_sizes, int, &msg_sizes_argc, 0000);
MODULE_PARM_DESC(msg_sizes, "Sample message sizes");

static void shm_sample_rx_cb(void *cb_arg, int chan_id, void *buf, size_t size);

/**
 * struct ipc_sample_priv - sample private data
 * @num_msgs:		number of messages to be sent to remote app
 * @ipc_kobj:		sysfs kernel object
 * @ping_attr:		sysfs ping command attributes
 * @last_rx_msg:	last received message
 * @last_tx_msg:	last transmitted message
 * @num_channels:	number of channels configured in this sample
 */
struct ipc_sample_priv {
	int num_msgs;
	struct kobject *ipc_kobj;
	struct kobj_attribute ping_attr;
	char last_rx_msg[L_BUF_LEN];
	char last_tx_msg[L_BUF_LEN];
	int num_channels;
};

/* sample private data */
static struct ipc_sample_priv priv;


/* Init IPC shared memory driver (see ipc-shm.h) */
static int init_ipc_shm(void)
{
	/* channel call-backs */
	struct ipc_shm_channel_ops ops = {
		.cb_arg = &priv,
		.rx_cb = shm_sample_rx_cb,
		.rx_unmanaged_cb = NULL,
	};

	/* memory buffer pools */
	struct ipc_shm_pool_cfg buf_pools[] = {
		{
			.num_bufs = 5,
			.buf_size = S_BUF_LEN
		},
		{
			.num_bufs = 5,
			.buf_size = M_BUF_LEN
		},
		{
			.num_bufs = 5,
			.buf_size = L_BUF_LEN
		},
	};

	/* channel configuration */
	struct ipc_shm_channel_cfg chan_cfg = {
		.type = SHM_CHANNEL_MANAGED,
		.memory = {
			.managed = {
				.num_pools = ARRAY_SIZE(buf_pools),
				.pools = buf_pools,
			},
		},
		.ops = &ops
	};

	/* use same configuration for all channels in this sample */
	struct ipc_shm_channel_cfg channels[] = {chan_cfg, chan_cfg};

	/* ipc shm core type */
	struct ipc_shm_remote_core remote = {
		.type = IPC_CORE_DEFAULT,
		.index = 0,
	};

	/* ipc shm configuration */
	struct ipc_shm_cfg shm_cfg = {
		.local_shm_addr = LOCAL_SHM_ADDR,
		.remote_shm_addr = REMOTE_SHM_ADDR,
		.inter_core_tx_irq = IPC_DEFAULT_INTER_CORE_IRQ,
		.inter_core_rx_irq = IPC_DEFAULT_INTER_CORE_IRQ,
		.remote_core = remote,
		.shm_size = IPC_SHM_SIZE,
		.num_channels = ARRAY_SIZE(channels),
		.channels = channels
	};

	priv.num_channels = shm_cfg.num_channels;

	return ipc_shm_init(&shm_cfg);
}

/*
 * generate_msg() - generates message from module param ipc_shm_sample_msg
 * @s:		destination buffer
 * @len:	destination buffer length
 * @msg_no:	message number
 *
 * Generate message by repeated concatenation of module param ipc_shm_sample_msg
 */
static void generate_msg(char *s, int len, int msg_no)
{
	int i, j;

	snprintf(s, len, "#%d ", msg_no);
	for (i = strlen(s), j = 0; i < len - 1; i++, j++) {
		s[i] = ipc_shm_sample_msg[j % strlen(ipc_shm_sample_msg)];
	}
	s[i] = '\0';
}

/* Completion variable used to sync send_msg func with rx cb */
static DECLARE_COMPLETION(reply_received);

/*
 * shm RX callback. Prints the data, releases the channel and signals the
 * completion variable.
 */
static void shm_sample_rx_cb(void *cb_arg, int chan_id, void *buf, size_t size)
{
	int err = 0;

	/* process the received data */
	sample_info("ch %d << %ld bytes: %s\n", chan_id, size, (char *)buf);
	memcpy(priv.last_rx_msg, buf, size);

	/* release the buffer */
	err = ipc_shm_release_buf(chan_id, buf);
	if (err) {
		sample_err("failed to free buffer for channel %d,"
			    "err code %d\n", chan_id, err);
	}

	/* signal reply received */
	complete(&reply_received);
}

/*
 * send_msg() - Sends message to the remote OS
 * @msg_len: message length
 * @msg_no: message sequence number to be written in the test message
 * @chan_id: ipc channel to be used for remote CPU communication
 *
 * It uses a completion variable for synchronization with reply callback.
 */
static int send_msg(int msg_len, int msg_no, int chan_id)
{
	int err = 0;
	char *buf = NULL;

	/* last sent and received message must match */
	if (strcmp(priv.last_rx_msg, priv.last_tx_msg) != 0) {
		sample_err("last rx msg != last tx msg\n");
		sample_err(">> %s\n", priv.last_tx_msg);
		sample_err("<< %s\n", priv.last_rx_msg);
		return -EINVAL;
	}

	buf = ipc_shm_acquire_buf(chan_id, msg_len);
	if (!buf) {
		sample_err("failed to get buffer for channel ID"
			   " %d and size %d\n", chan_id, msg_len);
		return -ENOMEM;
	}

	/* write data to buf */
	generate_msg(buf, msg_len, msg_no);
	memcpy(priv.last_tx_msg, buf, msg_len);

	sample_info("ch %d >> %d bytes: %s\n", chan_id, msg_len, buf);

	/* send data to peer */
	err = ipc_shm_tx(chan_id, buf, msg_len);
	if (err) {
		sample_err("tx failed for channel ID %d, size "
			   "%d, error code %d\n", 0, msg_len, err);
		return err;
	}

	/* wait for reply signal from rx callback */
	err = wait_for_completion_interruptible(&reply_received);
	if (err == -ERESTARTSYS) {
		sample_info("interrupted...\n");
		return err;
	}

	return 0;
}

/*
 * run_demo() - Implements the ipc-shm sample.
 *
 * This sample communicates with the remote CPU for all combinations of
 * configured message sizes, number of loops and configured channels.
 */
static int run_demo(void)
{
	int err, msg, chan_id, i;

	sample_info("starting demo...\n");

	msg = 0;
	while (msg < priv.num_msgs) {
		for (chan_id = 0; chan_id < priv.num_channels; chan_id++) {
			for (i = 0; i < msg_sizes_argc; i++) {
				err = send_msg(msg_sizes[i], msg + 1, chan_id);
				if (err)
					return err;

				if (++msg == priv.num_msgs) {
					sample_info("exit demo\n");
					return 0;
				}
			}
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

	if (strcmp(attr->attr.name, priv.ping_attr.attr.name) == 0) {
		value = priv.num_msgs;
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

	if (strcmp(attr->attr.name, priv.ping_attr.attr.name) == 0) {
		priv.num_msgs = value;
		run_demo();
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
	priv.ping_attr = ping_attr;

	/* create ipc-sample folder in sys/kernel */
	priv.ipc_kobj = kobject_create_and_add(MODULE_NAME, kernel_kobj);
	if (!priv.ipc_kobj)
		return -ENOMEM;

	/* create sysfs file for ipc sample ping command */
	err = sysfs_create_file(priv.ipc_kobj, &priv.ping_attr.attr);
	if (err) {
		sample_err("sysfs file creation failed, error code %d\n", err);
		goto err_kobj_free;
	}

	return 0;

err_kobj_free:
	kobject_put(priv.ipc_kobj);
	return err;
}

static void free_sysfs(void)
{
	kobject_put(priv.ipc_kobj);
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
