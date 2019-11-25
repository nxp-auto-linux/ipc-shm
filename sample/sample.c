/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018-2019 NXP
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

/* IPC SHM configuration defines */
#if defined(CONFIG_SOC_S32GEN1)
	#define LOCAL_SHM_ADDR 0x34100000
#elif defined(CONFIG_SOC_S32V234)
	#define LOCAL_SHM_ADDR 0x3E900000
#else
	#error "Platform not supported"
#endif
#define IPC_SHM_SIZE 0x100000 /* 1M local shm, 1M remote shm */
#define REMOTE_SHM_ADDR (LOCAL_SHM_ADDR + IPC_SHM_SIZE)
#define INTER_CORE_TX_IRQ 2u
#define INTER_CORE_RX_IRQ 1u
#define S_BUF_LEN 16
#define M_BUF_LEN 256
#define L_BUF_LEN 4096
#define CTRL_CHAN_ID 0
#define CTRL_CHAN_SIZE 64

/* convenience wrappers for printing messages */
#define sample_fmt(fmt) MODULE_NAME": %s(): "fmt
#define sample_err(fmt, ...) pr_err(sample_fmt(fmt), __func__, ##__VA_ARGS__)
#define sample_warn(fmt, ...) pr_warn(sample_fmt(fmt), __func__, ##__VA_ARGS__)
#define sample_info(fmt, ...) pr_info(MODULE_NAME": "fmt, ##__VA_ARGS__)
#define sample_dbg(fmt, ...) pr_debug(sample_fmt(fmt), __func__, ##__VA_ARGS__)

static int msg_sizes[IPC_SHM_MAX_POOLS] = {S_BUF_LEN};
static int msg_sizes_argc = 1;
module_param_array(msg_sizes, int, &msg_sizes_argc, 0000);
MODULE_PARM_DESC(msg_sizes, "Sample message sizes");

/**
 * struct ipc_sample_app - sample app private data
 * @num_channels:	number of channels configured in this sample
 * @num_msgs:		number of messages to be sent to remote app
 * @ctrl_shm:		control channel local shared memory
 * @last_tx_msg:	last transmitted message
 * @last_rx_msg:	last received message
 * @ipc_kobj:		sysfs kernel object
 * @ping_attr:		sysfs ping command attributes
 */
static struct ipc_sample_app {
	int num_channels;
	int num_msgs;
	char *ctrl_shm;
	char last_tx_msg[L_BUF_LEN];
	char last_rx_msg[L_BUF_LEN];
	struct kobject *ipc_kobj;
	struct kobj_attribute ping_attr;
} app;

/* Completion variable used to sync send_msg func with shm_rx_cb */
static DECLARE_COMPLETION(reply_received);

static void data_chan_rx_cb(void *cb_arg, int chan_id, void *buf, size_t size);
static void ctrl_chan_rx_cb(void *cb_arg, int chan_id, void *mem);

/* Init IPC shared memory driver (see ipc-shm.h for API) */
static int init_ipc_shm(void)
{
	int err;

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

	/* data channel configuration */
	struct ipc_shm_channel_cfg data_chan_cfg = {
		.type = IPC_SHM_MANAGED,
		.ch = {
			.managed = {
				.num_pools = ARRAY_SIZE(buf_pools),
				.pools = buf_pools,
				.rx_cb = data_chan_rx_cb,
				.cb_arg = &app,
			},
		}
	};

	/* control channel configuration */
	struct ipc_shm_channel_cfg ctrl_chan_cfg = {
		.type = IPC_SHM_UNMANAGED,
		.ch = {
			.unmanaged = {
				.size = CTRL_CHAN_SIZE,
				.rx_cb = ctrl_chan_rx_cb,
				.cb_arg = &app,
			},
		}
	};

	/* use same configuration for all data channels */
	struct ipc_shm_channel_cfg channels[] = {ctrl_chan_cfg,
						 data_chan_cfg, data_chan_cfg};

	/* ipc shm configuration */
	struct ipc_shm_cfg shm_cfg = {
		.local_shm_addr = LOCAL_SHM_ADDR,
		.remote_shm_addr = REMOTE_SHM_ADDR,
		.shm_size = IPC_SHM_SIZE,
		.inter_core_tx_irq = INTER_CORE_TX_IRQ,
		.inter_core_rx_irq = INTER_CORE_RX_IRQ,
		.remote_core = {
			.type = IPC_CORE_DEFAULT,
			.index = 0,
		},
		.num_channels = ARRAY_SIZE(channels),
		.channels = channels
	};

	err = ipc_shm_init(&shm_cfg);
	if (err)
		return err;

	app.num_channels = shm_cfg.num_channels;

	/* acquire control channel memory once */
	app.ctrl_shm = ipc_shm_unmanaged_acquire(CTRL_CHAN_ID);
	if (!app.ctrl_shm) {
		sample_err("failed to get memory of control channel");
		return -ENOMEM;
	}

	return 0;
}

/*
 * data channel Rx callback: print message, release buffer and signal the
 * completion variable.
 */
static void data_chan_rx_cb(void *arg, int chan_id, void *buf, size_t size)
{
	int err = 0;

	WARN_ON(arg != &app);
	WARN_ON(size > L_BUF_LEN);

	/* process the received data */
	sample_info("ch %d << %ld bytes: %s\n", chan_id, size, (char *)buf);
	memcpy(app.last_rx_msg, buf, size);

	/* release the buffer */
	err = ipc_shm_release_buf(chan_id, buf);
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
static void ctrl_chan_rx_cb(void *arg, int chan_id, void *mem)
{
	WARN_ON(arg != &app);
	WARN_ON(chan_id != CTRL_CHAN_ID);
	WARN_ON(strlen(mem) > L_BUF_LEN);

	sample_info("ch %d << %ld bytes: %s\n",
		    chan_id, strlen(mem), (char *)mem);
}

/* send control message with number of data messages to be sent */
static int send_ctrl_msg(void)
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
	strcpy(app.ctrl_shm, tmp);

	sample_info("ch %d >> %ld bytes: %s\n", chan_id, strlen(tmp), tmp);

	/* notify remote */
	err = ipc_shm_unmanaged_tx(chan_id);
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
	static char *msg_pattern = "Hello world! ";
	int i, j;

	snprintf(s, len, "#%d ", msg_no);
	for (i = strlen(s), j = 0; i < len - 1; i++, j++) {
		s[i] = msg_pattern[j % strlen(msg_pattern)];
	}
	s[i] = '\0';
}

/**
 * send_data_msg() - Send generated data message to remote peer
 * @msg_len: message length
 * @msg_no: message sequence number to be written in the test message
 * @chan_id: ipc channel to be used for remote CPU communication
 *
 * It uses a completion variable for synchronization with reply callback.
 */
static int send_data_msg(int msg_len, int msg_no, int chan_id)
{
	int err = 0;
	char *buf = NULL;

	buf = ipc_shm_acquire_buf(chan_id, msg_len);
	if (!buf) {
		sample_err("failed to get buffer for channel ID"
			   " %d and size %d\n", chan_id, msg_len);
		return -ENOMEM;
	}

	/* write data to acquired buffer */
	generate_msg(buf, msg_len, msg_no);

	/* save data for comparison with echo reply */
	strcpy(app.last_tx_msg, buf);

	sample_info("ch %d >> %d bytes: %s\n", chan_id, msg_len, buf);

	/* send data to remote peer */
	err = ipc_shm_tx(chan_id, buf, msg_len);
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

	/* check if received message match with sent message */
	if (strcmp(app.last_rx_msg, app.last_tx_msg) != 0) {
		sample_err("last rx msg != last tx msg\n");
		sample_err(">> %s\n", app.last_tx_msg);
		sample_err("<< %s\n", app.last_rx_msg);
		return -EINVAL;
	}

	return 0;
}

/*
 * Send requested number of messages to remote peer, cycling through all data
 * channels and wait for an echo reply after each sent message.
 */
static int run_demo(int num_msgs)
{
	int err, msg, ch, i;

	sample_info("starting demo...\n");

	/* signal number of messages to remote via control channel */
	err = send_ctrl_msg();
	if (err)
		return err;

	/* send generated data messages */
	msg = 0;
	while (msg < num_msgs) {
		for (ch = CTRL_CHAN_ID + 1; ch < app.num_channels; ch++) {
			for (i = 0; i < msg_sizes_argc; i++) {
				err = send_data_msg(msg_sizes[i], msg + 1, ch);
				if (err)
					return err;

				if (++msg == num_msgs) {
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

	if (strcmp(attr->attr.name, app.ping_attr.attr.name) == 0) {
		value = app.num_msgs;
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
	if (!app.ipc_kobj)
		return -ENOMEM;

	/* create sysfs file for ipc sample ping command */
	err = sysfs_create_file(app.ipc_kobj, &app.ping_attr.attr);
	if (err) {
		sample_err("sysfs file creation failed, error code %d\n", err);
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
