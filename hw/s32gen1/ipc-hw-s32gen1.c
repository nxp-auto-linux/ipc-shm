/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018-2019,2021,2023-2025 NXP
 */
#include <linux/io.h>

#include "ipc-shm.h"
#include "ipc-os.h"
#include "ipc-hw.h"
#include "ipc-hw-platform.h"

/**
 * struct ipc_hw_priv_type - platform specific private data
 *
 * @msi_tx_irq:     MSI index of inter-core interrupt corresponds to mscm_tx_irq
 * @msi_rx_irq:     MSI index of inter-core interrupt corresponds to mscm_rx_irq
 * @mscm_tx_irq:    MSCM inter-core interrupt reserved for shm driver tx
 * @mscm_rx_irq:    MSCM inter-core interrupt reserved for shm driver rx
 * @remote_core:    index of remote core to trigger the interrupt on
 * @local_core:     index of the local core targeted by remote
 * @spi_index       shared peripheral interrupts index
 * @ipc_mscm:       pointer to memory-mapped hardware peripheral MSCM
 */
static struct ipc_hw_priv_type {
	uint8_t msi_tx_irq;
	uint8_t msi_rx_irq;
	uint8_t spi_index;
	int mscm_tx_irq;
	int mscm_rx_irq;
	int remote_core;
	int local_core;
	struct ipc_mscm_regs *ipc_mscm;
} ipc_hw_priv[IPC_SHM_MAX_INSTANCES];

/**
 * ipc_hw_get_rx_irq() - get MSCM inter-core interrupt index [0..2] used for Rx
 *
 * Return: MSCM inter-core interrupt index used for Rx
 */
int ipc_hw_get_rx_irq(const uint8_t instance)
{
	return ipc_hw_priv[instance].mscm_rx_irq;
}

/**
 * ipc_hw_init() - platform specific initialization
 *
 * @cfg:    configuration parameters
 *
 * inter_core_tx_irq can be disabled by passing IPC_IRQ_NONE, if polling is
 * desired in transmit notification path. inter_core_tx_irq and
 * inter_core_rx_irq are not allowed to have the same value to avoid possible
 * race conditions when updating the value of the IRSPRCn register.
 * If the value IPC_CORE_DEFAULT is passed as local_core or remote_core, the
 * default value defined for the selected platform will be used instead.
 *
 * Return:
 * * %0		- on Success
 * * %-EINVAL	- either inter core interrupt invalid or invalid core
 * * %-ENOMEM	- failing to map MSCM address space
 * * %-EACCES	- failing to access MSCM registers
 */
int8_t ipc_hw_init(const uint8_t instance, const struct ipc_shm_cfg *cfg)
{
	/* map MSCM hardware peripheral block registers */
	void *addr = ipc_os_map_intc();

	return _ipc_hw_init(instance, cfg, addr);
}

/**
 * _ipc_hw_init() - platform specific initialization
 *
 * Low level variant of ipc_hw_init() used by UIO device implementation.
 */
int8_t _ipc_hw_init(const uint8_t instance,
	const struct ipc_shm_cfg *cfg, void *periph_addr)
{
	int remote_core_idx;
	int local_core_idx;
	uint32_t ircpcfg_mask;

	if (!periph_addr)
		return -EINVAL;

	ipc_hw_priv[instance].ipc_mscm = (struct ipc_mscm_regs *)periph_addr;

	switch (cfg->local_core.type) {
	case IPC_CORE_A53:
		switch (cfg->local_core.index) {
		case IPC_CORE_INDEX_0:
			local_core_idx = IPC_A53_0;
			break;
		case IPC_CORE_INDEX_1:
			local_core_idx = IPC_A53_1;
			break;
		case IPC_CORE_INDEX_2:
			local_core_idx = IPC_A53_2;
			break;
		case IPC_CORE_INDEX_3:
			local_core_idx = IPC_A53_3;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IPC_CORE_DEFAULT:
		local_core_idx = IPC_DEFAULT_LOCAL_CORE;
		break;
	default:
		return -EINVAL;
	}

	/* check trusted cores mask contains the targeted and other A53 cores */
	if ((!cfg->local_core.trusted)
		|| (cfg->local_core.trusted & ~IPC_MSCM_IRCPCFG_A53_TR)
		|| ((0x01uL << local_core_idx) & ~cfg->local_core.trusted)) {
		return -EINVAL;
	}

	switch (cfg->remote_core.type) {
	case IPC_CORE_A53:
		switch (cfg->remote_core.index) {
		case IPC_CORE_INDEX_0:
			remote_core_idx = IPC_A53_0;
			break;
		case IPC_CORE_INDEX_1:
			remote_core_idx = IPC_A53_1;
			break;
		case IPC_CORE_INDEX_2:
			remote_core_idx = IPC_A53_2;
			break;
		case IPC_CORE_INDEX_3:
			remote_core_idx = IPC_A53_3;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IPC_CORE_M7:
		switch (cfg->remote_core.index) {
		case IPC_CORE_INDEX_0:
			remote_core_idx = IPC_M7_0;
			break;
		case IPC_CORE_INDEX_1:
			remote_core_idx = IPC_M7_1;
			break;
		case IPC_CORE_INDEX_2:
			remote_core_idx = IPC_M7_2;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IPC_CORE_DEFAULT:
		remote_core_idx = IPC_DEFAULT_REMOTE_CORE;
		break;
	default:
		return -EINVAL;
	}

	if (((cfg->inter_core_tx_irq != IPC_IRQ_NONE)
			&& (cfg->inter_core_tx_irq == cfg->inter_core_rx_irq))
		|| (remote_core_idx
			== readl(
				&(ipc_hw_priv[instance].ipc_mscm->CPXNUM)))
		|| (remote_core_idx == local_core_idx)) {
		return -EINVAL;
	}

	switch (cfg->inter_core_tx_irq) {
	case IPC_IRQ_NONE:
		break;
	case 0:
		ipc_hw_priv[instance].msi_tx_irq = (uint8_t)0u;
		break;
	case 1:
		ipc_hw_priv[instance].msi_tx_irq = (uint8_t)1u;
		break;
	case 2:
		ipc_hw_priv[instance].msi_tx_irq = (uint8_t)2u;
		break;
	default:
		return -EINVAL;
	}

	switch (cfg->inter_core_rx_irq) {
	case IPC_IRQ_NONE:
		break;
	case 0:
		ipc_hw_priv[instance].spi_index = (uint8_t)1u;
		ipc_hw_priv[instance].msi_rx_irq = (uint8_t)0u;
		break;
	case 1:
		ipc_hw_priv[instance].spi_index = (uint8_t)2u;
		ipc_hw_priv[instance].msi_rx_irq = (uint8_t)1u;
		break;
	case 2:
		ipc_hw_priv[instance].spi_index = (uint8_t)3u;
		ipc_hw_priv[instance].msi_rx_irq = (uint8_t)2u;
		break;
	default:
		return -EINVAL;
	}

	ipc_hw_priv[instance].mscm_tx_irq = cfg->inter_core_tx_irq;
	ipc_hw_priv[instance].mscm_rx_irq = cfg->inter_core_rx_irq;
	ipc_hw_priv[instance].remote_core = remote_core_idx;
	ipc_hw_priv[instance].local_core = local_core_idx;
	/*
	 * disable rx irq source to avoid receiving an interrupt from remote
	 * before any of the buffer rings are initialized
	 */
	ipc_hw_irq_disable(instance);

	/*
	 * enable local trusted cores so that they can read full contents of
	 * IRCPnISRx registers
	 */
	ircpcfg_mask = readl(&ipc_hw_priv[instance].ipc_mscm->IRCPCFG);
	if (ircpcfg_mask & IPC_MSCM_IRCPCFG_LOCK)
		return -EACCES;

	writel(ircpcfg_mask | cfg->local_core.trusted,
		&(ipc_hw_priv[instance].ipc_mscm->IRCPCFG));

	return 0;
}

/**
 * ipc_hw_free() - unmap MSCM IP block and clear irq
 */
void ipc_hw_free(const uint8_t instance)
{
	ipc_hw_irq_clear(instance);

	/* unmap MSCM hardware peripheral block */
	ipc_os_unmap_intc(ipc_hw_priv[instance].ipc_mscm);
}

/**
 * ipc_hw_irq_enable() - enable notifications from remote
 *
 * The MSCM_IRSPRCn register works with the NVIC interrupt IDs, and the NVIC ID
 * of the first MSCM inter-core interrupt is 1. In order to obtain the correct
 * index for the interrupt routing register, this value is added to mscm_rx_irq.
 */
void ipc_hw_irq_enable(const uint8_t instance)
{
	uint16_t irsprc_mask;
	uint8_t spi_rx_idx;

	if (ipc_hw_priv[instance].mscm_rx_irq != IPC_IRQ_NONE) {
		spi_rx_idx = ipc_hw_priv[instance].spi_index;
		/* enable MSCM core-to-core interrupt routing */
		irsprc_mask
			= readw(
				&ipc_hw_priv[instance].ipc_mscm
					->IRSPRC[spi_rx_idx]);
		writew(irsprc_mask | IPC_MSCM_IRSPRCn_GIC500,
			&((ipc_hw_priv[instance].ipc_mscm)
				->IRSPRC[spi_rx_idx]));
	}
}

/**
 * ipc_hw_irq_disable() - disable notifications from remote
 *
 * The MSCM_IRSPRCn register works with the NVIC interrupt IDs, and the NVIC ID
 * of the first MSCM inter-core interrupt is 1. In order to obtain the correct
 * index for the interrupt routing register, this value is added to mscm_rx_irq.
 */
void ipc_hw_irq_disable(const uint8_t instance)
{
	uint16_t irsprc_mask;
	uint8_t spi_rx_idx;

	if (ipc_hw_priv[instance].mscm_rx_irq != IPC_IRQ_NONE) {
		spi_rx_idx = ipc_hw_priv[instance].spi_index;
		/* disable MSCM core-to-core interrupt routing */
		irsprc_mask
			= readw(&ipc_hw_priv[instance].ipc_mscm
				->IRSPRC[spi_rx_idx]);
		writew(irsprc_mask & ~IPC_MSCM_IRSPRCn_GIC500,
			&((ipc_hw_priv[instance].ipc_mscm)
				->IRSPRC[spi_rx_idx]));
	}

}

/**
 * ipc_hw_irq_notify() - notify remote that data is available
 */
void ipc_hw_irq_notify(const uint8_t instance)
{
	uint8_t msi_idx = ipc_hw_priv[instance].msi_tx_irq;
	int remote_core = ipc_hw_priv[instance].remote_core;

	if (ipc_hw_priv[instance].mscm_tx_irq != IPC_IRQ_NONE) {
		/* trigger MSCM core-to-core directed interrupt */
		writel(IPC_MSCM_IRCPnIGRn_INT_EN,
			&((ipc_hw_priv[instance].ipc_mscm)->
			IRCPnIRx[remote_core][msi_idx].IPC_IGR));
	}
}

/**
 * ipc_hw_irq_clear() - clear available data notification
 */
void ipc_hw_irq_clear(const uint8_t instance)
{
	uint8_t msi_idx = ipc_hw_priv[instance].msi_rx_irq;
	int local_core = ipc_hw_priv[instance].local_core;
	int remote_core = ipc_hw_priv[instance].remote_core;

	if (ipc_hw_priv[instance].mscm_rx_irq != IPC_IRQ_NONE) {
		/*
		 * clear MSCM core-to-core directed interrupt
		 * on the targeted core
		 */
		writel(1 << remote_core,
			&((ipc_hw_priv[instance].ipc_mscm)->
			IRCPnIRx[local_core][msi_idx].IPC_ISR));
	}
}
