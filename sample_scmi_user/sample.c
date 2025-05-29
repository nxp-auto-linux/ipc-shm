/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2025 NXP
 */
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "ipc-shm.h"
#include "ipcf_Ip_Cfg.h"

#define IPC_SHM_DEV_MEM_NAME    "/dev/mem"

#define SCMI_MSG_PAYLOAD_INDEX_U32        (7u)
#define SCMI_MSG_PAYLOAD_BYTES_INDEX      (SCMI_MSG_PAYLOAD_INDEX_U32 * 4u)
#define MAX_SAMPLE_MSG_LEN                (192)

/* convenience wrappers for printing messages */
#define sample_err(fmt, ...) printf(pr_fmt(fmt), __func__, ##__VA_ARGS__)
#define sample_info(fmt, ...) printf("ipc-shm-us-app: "fmt, ##__VA_ARGS__)
#ifdef DEBUG
#define sample_dbg(fmt, ...) printf(pr_fmt(fmt), __func__, ##__VA_ARGS__)
#else
#define sample_dbg(fmt, ...)
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef IPC_MEMCPY_BYTES_ALIGNED
#define IPC_MEMCPY_BYTES_ALIGNED 8
#endif

#define IS_ALIGNED(x, a) (((x) & ((typeof(x))(a) - 1)) == 0)

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
	sem_t sema;
	uint8_t instance;
} app;

/* link with generated variables */
const void *rx_cb_arg = &app;

/*
 * ipc implementation of memcpy to IO memory space
 */
void ipc_memcpy_toio(void *dst, void *buf, size_t count)
{
	static int bytes_aligned = IPC_MEMCPY_BYTES_ALIGNED;
	/* Cast to char* as memcpy copy each bytes */
	char *dst_casted = (char *)dst;
	char *buf_casted = (char *)buf;

	while (count && !IS_ALIGNED((unsigned long)dst_casted, bytes_aligned)) {
		*dst_casted = *buf_casted;
		dst_casted += 1;
		buf_casted += 1;
		count--;
	}

	while (count >= bytes_aligned) {
		memcpy(dst_casted, buf_casted, bytes_aligned);
		dst_casted += bytes_aligned;
		buf_casted += bytes_aligned;
		count -= bytes_aligned;
	}

	while (count) {
		*dst_casted = *buf_casted;
		dst_casted += 1;
		buf_casted += 1;
		count--;
	}
}

/*
 * ipc implementation of memcpy from IO memory space
 */
void ipc_memcpy_fromio(void *dst, void *buf, size_t count)
{
	static int bytes_aligned = IPC_MEMCPY_BYTES_ALIGNED;
	/* Cast to char* as memcpy copy each bytes */
	char *dst_casted = (char *)dst;
	char *buf_casted = (char *)buf;

	while (count && !IS_ALIGNED((unsigned long)buf_casted, bytes_aligned)) {
		*dst_casted = *buf_casted;
		dst_casted += 1;
		buf_casted += 1;
		count--;
	}

	while (count >= bytes_aligned) {
		memcpy(dst_casted, buf_casted, bytes_aligned);
		dst_casted += bytes_aligned;
		buf_casted += bytes_aligned;
		count -= bytes_aligned;
	}

	while (count) {
		*dst_casted = *buf_casted;
		dst_casted += 1;
		buf_casted += 1;
		count--;
	}
}

void scmi_chan_rx_cb(void *arg, const uint8_t instance, uint8_t chan_id,
		void *buf)
{
	int8_t err;
	char *tmp_buf = buf;
	char *endptr;
	char tmp[MAX_SAMPLE_MSG_LEN];
	struct ipc_sample_app *cb_arg_sample =
		(struct ipc_sample_app *)(*((uintptr_t *)arg));

	/* process the received data */
	ipc_memcpy_fromio(tmp, (char *)tmp_buf, MAX_SAMPLE_MSG_LEN);

	cb_arg_sample->last_rx_no_msg = strtol(
		(char *)&(tmp_buf[SCMI_MSG_PAYLOAD_BYTES_INDEX])
			+ strlen("DONE #"), &endptr, 10);

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
	sem_post(&app.sema);
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
	int err, msg, ret;
	char *buf = NULL;
	char tmp[MAX_SAMPLE_MSG_LEN] = {'\0'};

	sample_info("starting demo...\n");

	buf = ipc_shm_acquire_scmi(0, 0);
	if (!buf) {
		sample_err("failed to get buffer for scmi channel");
		return -ENOMEM;
	}

	msg = 0;

	/* Write number of messages to be sent in control channel memory.
	 * Use stack temp buffer because snprintf may do unaligned memory writes
	 * in SRAM and A53 will complain about unaligned accesses.
	 */
	ret = snprintf(tmp, MAX_SAMPLE_MSG_LEN,
		"CALL #%d ", num_msgs);

	ipc_memcpy_toio(&buf[SCMI_MSG_PAYLOAD_BYTES_INDEX], tmp, ret);

	sample_info("MSG PAYLOAD: CALL #%d\n", num_msgs);

	/* send data to remote peer */
	err = ipc_shm_scmi_tx(0, 0);
	if (err) {
		sample_err("tx failed for SCMI error code %d\n", err);
		return err;
	}
	while (msg < num_msgs) {
		sem_wait(&app.sema);
		if (++msg == num_msgs) {
			return 0;
		}
	}

	return 0;
}

/*
 * interrupt signal handler for terminating the sample execution gracefully
 */
void int_handler(int signum)
{
	app.num_msgs = 0;
}

int main(int argc, char *argv[])
{
	int err = 0;
	struct sigaction sig_action;

	app.instance = 0;
	sem_init(&app.sema, 0, 0);

	/* init ipc shm driver */
	err = init_ipc_shm();
	if (err)
		return err;

	/* catch interrupt signals to terminate the execution gracefully */
	sig_action.sa_handler = int_handler;
	sigaction(SIGINT, &sig_action, NULL);

	app.num_msgs = 1;
	while (app.num_msgs) {
		printf("\nInput number of messages to send: ");
		scanf("%d", &app.num_msgs);

		if (!app.num_msgs)
			break;

		err = run_demo(app.num_msgs);
		if (err)
			break;
	}

	ipc_shm_free();

	sem_destroy(&app.sema);

	sample_info("exit\n");

	return err;
}
