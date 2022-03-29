/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2020-2022 NXP
 */

#ifndef IPCF_IP_CFG_H
#define IPCF_IP_CFG_H

#include <linux/module.h>
#include "../ipc-shm.h"

/* callbacks for channels  - must be implemented by application*/
/* arguments for callbacks - must be implemented by application*/

void ctrl_chan_rx_cb(void *arg, const uint8_t instance, int chan_id, void *mem);
void data_chan_rx_cb(void *arg, const uint8_t instance, int chan_id, void *buf, size_t size);

extern const void *rx_cb_arg;

/* ipc shm configuration for all instances */
extern struct ipc_shm_instances_cfg ipcf_shm_instances_cfg;

#endif /* IPCF_IP_CFG_H */

