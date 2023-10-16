/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2020-2023 NXP
 */

#ifndef IPCF_IP_CFG_H
#define IPCF_IP_CFG_H

#include <linux/module.h>
#include "ipc-shm.h"
#include "ipc-hw-platform.h"

#define INSTANCE_ID0		0
#define INSTANCE_ID1		1
#define INSTANCE_ID2		2

#define SAMPLE_NUM_INST		3

/* callbacks for channels  - must be implemented by application*/
void ctrl_chan_rx_cb(void *arg, const uint8_t instance, int chan_id, void *mem);
void data_chan_rx_cb(void *arg, const uint8_t instance, int chan_id, void *buf,
	size_t size);

/* arguments for callbacks - must be implemented by application*/
extern const void *rx_cb_arg;

/* ipc shm configuration for all instances */
extern struct ipc_shm_instances_cfg ipcf_shm_instances_cfg;

#endif /* IPCF_IP_CFG_H */
