/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2025 NXP
 */
#include <linux/types.h>

#include "ipc-shm.h"
#include "ipc-os.h"
#include "ipc-hw.h"

/**
 * struct ipc_hw_priv_type - platform specific private data
 *
 * @tx_irq:         interrupt reserved for shm driver tx
 */
static struct ipc_hw_priv_type {
	int tx_irq;
} ipc_hw_priv[IPC_SHM_MAX_INSTANCES];

/**
 * ipc_hw_init() - platform specific initialization
 *
 * @instance: instance id
 * @cfg:      configuration parameters
 *
 * Return:
 * * %0		- on Success
 * * %-EINVAL	- on Error
 */
int8_t ipc_hw_init(const uint8_t instance, const struct ipc_shm_cfg *cfg)
{
	if (((IPC_IRQ_NONE != cfg->inter_core_rx_irq)
			&& (IPC_IRQ_MRU != cfg->inter_core_rx_irq))
		|| ((IPC_IRQ_NONE != cfg->inter_core_tx_irq)
			&& (IPC_IRQ_MRU != cfg->inter_core_tx_irq)))
		return -EINVAL;

	ipc_hw_priv[instance].tx_irq = cfg->inter_core_tx_irq;

	return 0;
}

/**
 * ipc_hw_free() - unmap MSCM IP block and clear irq
 */
void ipc_hw_free(const uint8_t instance)
{
	ipc_hw_irq_clear(instance);
}

/**
 * ipc_hw_irq_enable() - enable notifications from remote
 */
void ipc_hw_irq_enable(const uint8_t instance)
{
	/* Nothing to do */
}

/**
 * ipc_hw_irq_disable() - disable notifications from remote
 */
void ipc_hw_irq_disable(const uint8_t instance)
{
	/* Nothing to do */
}

/**
 * ipc_hw_irq_notify() - notify remote that data is available
 */
void ipc_hw_irq_notify(const uint8_t instance)
{
	if (IPC_IRQ_NONE != ipc_hw_priv[instance].tx_irq) {
		ipc_os_mailbox_tx(instance);
	}
}

/**
 * ipc_hw_irq_clear() - clear available data notification
 */
void ipc_hw_irq_clear(const uint8_t instance)
{
	/* Nothing to do */
}

/**
 * ipc_hw_get_rx_irq() - get interrupt index used for Rx
 *
 * Return: 0
 */
int ipc_hw_get_rx_irq(const uint8_t instance)
{
	return 0;
}
