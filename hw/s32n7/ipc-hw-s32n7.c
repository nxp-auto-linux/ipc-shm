/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2025 NXP
 */
#include <linux/io.h>

#include "ipc-shm.h"
#include "ipc-os.h"
#include "ipc-hw.h"
#include "ipc-hw-platform.h"

/* magic number to indicate the driver is sending */
static uint32_t IPC_SHM_SENDING = 0x49504346;

/**
 * struct IPC_MRU_TX_REG - ipc-shm MRU struct for transmitting
 *
 * @tx_CHx_MB0: MB0 for transmitting
 */
typedef struct {
	volatile uint32_t *tx_CHx_MB0;
} IPC_MRU_TX_REG;

/**
 * struct IPC_MRU_TX_REG - ipc-shm MRU struct for receiving
 *
 * @rx_CHx_CFG0:   MRU channel configuration 0
 * @rx_CHx_CFG1:   MRU channel configuration 1
 * @rx_CHx_MBSTAT: MRU mailbox status
 * @rx_CHx_MB0:    MB0 for receiving
 */
typedef struct {
	volatile uint32_t *rx_CHx_CFG0;
	volatile uint32_t *rx_CHx_CFG1;
	volatile uint32_t *rx_CHx_MBSTAT;
	volatile uint32_t *rx_CHx_MB0;
} IPC_MRU_RX_REG;

/**
 * struct ipc_hw_priv_type - platform specific private data
 *
 * @tx_irq:         interrupt reserved for shm driver tx
 * @rx_irq:         interrupt reserved for shm driver rx
 * @tx_mapped_addr: pointer to memory-mapped of transmitting MRU
 * @rx_mapped_addr: pointer to memory-mapped of receiving MRU
 * @IPC_MRU_TX:     MRU struct for transmitting
 * @IPC_MRU_RX:     MRU struct for receiving
 */
static struct ipc_hw_priv_type {
	int tx_irq;
	int rx_irq;
	void *tx_mapped_addr;
	void *rx_mapped_addr;
	IPC_MRU_TX_REG IPC_MRU_TX;
	IPC_MRU_RX_REG IPC_MRU_RX;
} ipc_hw_priv[IPC_SHM_MAX_INSTANCES];

/**
 * ipc_hw_get_rx_irq() - get interrupt index used for Rx
 * @instance: instance id
 *
 * Return: always return 0 as each node only contains 1 interrupt
 */
int ipc_hw_get_rx_irq(const uint8_t instance)
{
	return 0;
}

/**
 * ipc_hw_rtu_tx_mru() - initialize transmitting interrupt for ipc-shm driver rx
 *                       when target subsystem is RTU
 * Each RTU subsystem contains 8 MRUs, each MRU contains 8 or 12 channels
 * depends on MRU index:
 * - From MRU0 to MRU3, there are 12 channels
 * - From MRU4 to MRU7, there are 8 channels
 *
 * @instance: instance id
 * @cfg:      configuration parameters
 *
 * Return:
 * * %0		- on Success
 * * %-EINVAL	- on Error
 */
static int8_t ipc_hw_rtu_tx_mru(const uint8_t instance,
	const struct ipc_shm_cfg *cfg)
{
	IPC_SUBSYSTEM_INDEX_ENUM_Type subsystem_id = cfg->mru_tx.subsystem_id;
	IPC_MRU_INDEX_ENUM_Type mru_id = cfg->mru_tx.mru_id;
	IPC_MRU_CHANNEL_ENUM_Type mru_channel_id = cfg->mru_tx.mru_channel_id;
	IPC_MRU_INSTANCE_ENUM_Type mru_instance = cfg->mru_tx.mru_instance;
	IPC_RTU_MRU_Type *mru_addr = NULL;

	/* Checking subsystem and mru id of target RTU */
	if ((subsystem_id < IPC_SUBSYSTEM_0)
		|| (subsystem_id > IPC_SUBSYSTEM_2)
			|| (mru_id < IPC_MRU_0) || (mru_id > IPC_MRU_7)) {
		return -EINVAL;
	}

	/* Checking MRU channel id depends on MRU id */
	if (mru_channel_id < IPC_MRU_CHANNEL_0) {
		return -EINVAL;
	} else if (mru_id < IPC_MRU_4) {
		/* MRU0-3 have 12 channels */
		if (mru_channel_id > IPC_MRU_CHANNEL_11) {
			return -EINVAL;
		}
	} else {
		/* MRU4-7 have 8 channels */
		if (mru_channel_id > IPC_MRU_CHANNEL_7) {
			return -EINVAL;
		}
	}

	/* Get MRU address from device tree */
	ipc_hw_priv[instance].rx_mapped_addr = ipc_os_mru_map_intc(
			mru_instance, subsystem_id, mru_id);
	mru_addr = (IPC_RTU_MRU_Type *)ipc_hw_priv[instance].rx_mapped_addr;
	if (mru_addr == NULL) {
		return -EINVAL;
	}

	/* Get MRU mailbox 0 address */
	ipc_hw_priv[instance].IPC_MRU_TX.tx_CHx_MB0
		= &(mru_addr->CH[mru_channel_id].MB[0]);

	return 0;
}

/**
 * ipc_hw_aps_tx_mru() - initialize transmitting interrupt for ipc-shm driver rx
 *                       when target subsystem is APS
 * Each APS subsystem contains 8 MRUs, each MRU contains 3 channels
 *
 * @instance: instance id
 * @cfg:      configuration parameters
 *
 * Return:
 * * %0		- on Success
 * * %-EINVAL	- on Error
 */
static int8_t ipc_hw_aps_tx_mru(const uint8_t instance,
	const struct ipc_shm_cfg *cfg)
{
	IPC_SUBSYSTEM_INDEX_ENUM_Type subsystem_id = cfg->mru_tx.subsystem_id;
	IPC_MRU_INDEX_ENUM_Type mru_id = cfg->mru_tx.mru_id;
	IPC_MRU_CHANNEL_ENUM_Type mru_channel_id = cfg->mru_tx.mru_channel_id;
	IPC_MRU_INSTANCE_ENUM_Type mru_instance = cfg->mru_tx.mru_instance;
	IPC_APS_MRU_Type *mru_addr = NULL;

	/* Checking subsystem, mru id, mru channel id of target APS */
	if ((subsystem_id < IPC_SUBSYSTEM_0)
		|| (subsystem_id > IPC_SUBSYSTEM_2)
			|| (mru_id < IPC_MRU_0) || (mru_id > IPC_MRU_7)
			|| (mru_channel_id < IPC_MRU_CHANNEL_0)
				|| (mru_channel_id > IPC_MRU_CHANNEL_2)) {
		return -EINVAL;
	}

	/* Get MRU address from device tree */
	ipc_hw_priv[instance].rx_mapped_addr = ipc_os_mru_map_intc(
			mru_instance, subsystem_id, mru_id);
	mru_addr = (IPC_APS_MRU_Type *)ipc_hw_priv[instance].rx_mapped_addr;
	if (mru_addr == NULL) {
		return -EINVAL;
	}

	/* Get MRU mailbox 0 address */
	ipc_hw_priv[instance].IPC_MRU_TX.tx_CHx_MB0
		= &(mru_addr->CH[mru_channel_id].MB[0]);

	return 0;
}

/**
 * ipc_hw_fss_tx_mru() - initialize transmitting interrupt for ipc-shm driver tx
 *                       when target subsystem is FSS
 * Each FSS subsystem contains 11 MRUs, each MRU contains 1 ,8 or 16 channels
 * depends on MRU index:
 * - From MRU0 to MRU1, there are 16 channels
 * - From MRU2 to MRU8, there are 1 channel
 * -      MRU9        , there are 8 channels
 * -      MRU10       , there are 1 channel
 *
 * @instance: instance id
 * @cfg:      configuration parameters
 *
 * Return:
 * * %0		- on Success
 * * %-EINVAL	- on Error
 */
static int8_t ipc_hw_fss_tx_mru(const uint8_t instance,
	const struct ipc_shm_cfg *cfg)
{
	IPC_SUBSYSTEM_INDEX_ENUM_Type subsystem_id = cfg->mru_tx.subsystem_id;
	IPC_MRU_INDEX_ENUM_Type mru_id = cfg->mru_tx.mru_id;
	IPC_MRU_CHANNEL_ENUM_Type mru_channel_id = cfg->mru_tx.mru_channel_id;
	IPC_MRU_INSTANCE_ENUM_Type mru_instance = cfg->mru_tx.mru_instance;
	IPC_FSS_MRU_Type *mru_addr = NULL;

	/* Checking subsystem and mru id of target FSS */
	if ((subsystem_id != IPC_SUBSYSTEM_0)
			|| (mru_id < IPC_MRU_0) || (mru_id > IPC_MRU_10)) {
		return -EINVAL;
	}

	/* Checking MRU channel id depends on MRU id */
	if (mru_channel_id < IPC_MRU_CHANNEL_0) {
		return -EINVAL;
	} else if (mru_id < IPC_MRU_2) {
		/* MRU0-1 have 16 channels */
		if (mru_channel_id > IPC_MRU_CHANNEL_15) {
			return -EINVAL;
		}
	} else if (mru_id != IPC_MRU_9) {
		/* MRU2-8 and MRU10 have 1 channel */
		if (mru_channel_id != IPC_MRU_CHANNEL_0) {
			return -EINVAL;
		}
	} else {
		/* MRU9 and MRU10 have 8 channels */
		if (mru_channel_id > IPC_MRU_CHANNEL_7) {
			return -EINVAL;
		}
	}

	/* Get MRU address from device tree */
	ipc_hw_priv[instance].rx_mapped_addr = ipc_os_mru_map_intc(
			mru_instance, subsystem_id, mru_id);
	mru_addr = (IPC_FSS_MRU_Type *)ipc_hw_priv[instance].rx_mapped_addr;
	if (mru_addr == NULL) {
		return -EINVAL;
	}

	/* Get MRU mailbox 0 address */
	ipc_hw_priv[instance].IPC_MRU_TX.tx_CHx_MB0
		= &(mru_addr->CH[mru_channel_id].MB[0]);

	return 0;
}


/**
 * ipc_hw_rcu_tx_mru() - initialize transmitting interrupt for ipc-shm driver tx
 *                       when target subsystem is RCU
 *
 * Each RCU subsystem contains 32 MRUs, each MRU contains 1 to 12 channels
 * depends on MRU index:
 * - From MRU_C0-MRU_C3, there are 12 channels
 * - From MRU_C4-MRU_C7, there are 8 channels
 * - From MRU_H0-MRU_H3, there are 4 channels
 * - From MRU_L0-MRU_L3, there are 4 channels
 *
 * The MRU idx will follow:
 * - [MRU_C0-MRU_C7] is [0-7]
 * - [MRU_H0-MRU_H3] is [8-11]
 * - [MRU_L0-MRU_L3] is [12-15]
 *
 * @instance: instance id
 * @cfg:      configuration parameters
 *
 * Return:
 * * %0		- on Success
 * * %-EINVAL	- on Error
 */
static int8_t ipc_hw_rcu_tx_mru(const uint8_t instance,
	const struct ipc_shm_cfg *cfg)
{
	IPC_SUBSYSTEM_INDEX_ENUM_Type subsystem_id = cfg->mru_tx.subsystem_id;
	IPC_MRU_INDEX_ENUM_Type mru_id = cfg->mru_tx.mru_id;
	IPC_MRU_CHANNEL_ENUM_Type mru_channel_id = cfg->mru_tx.mru_channel_id;
	IPC_MRU_INSTANCE_ENUM_Type mru_instance = cfg->mru_tx.mru_instance;
	IPC_RCU_MRU_C_Type *mru_addr = NULL;
	IPC_RCU_MRU_HL_Type *mru_hl_addr = NULL;

	/* Checking subsystem and mru id of target FSS */
	if ((subsystem_id != IPC_SUBSYSTEM_0)
		|| (mru_id < IPC_MRU_0) || (mru_id > IPC_MRU_15)) {
		return -EINVAL;
	}

	/* Checking MRU channel id depends on MRU id */
	if (mru_channel_id < IPC_MRU_CHANNEL_0) {
		return -EINVAL;
	} else if (mru_id < IPC_MRU_4) {
		/* MRU0-3 have 12 channels */
		if (mru_channel_id > IPC_MRU_CHANNEL_11) {
			return -EINVAL;
		}
	} else if (mru_id < IPC_MRU_8) {
		/* MRU4-7 have 8 channels */
		if (mru_channel_id > IPC_MRU_CHANNEL_7) {
			return -EINVAL;
		}
	} else {
		/* MRU8-15 have 4 channels */
		if (mru_channel_id > IPC_MRU_CHANNEL_3) {
			return -EINVAL;
		}
	}

	/* Get MRU address from device tree */
	ipc_hw_priv[instance].rx_mapped_addr = ipc_os_mru_map_intc(
			mru_instance, subsystem_id, mru_id);

	if (ipc_hw_priv[instance].rx_mapped_addr == NULL) {
		return -EINVAL;
	}

	if (mru_instance == IPC_MRU_RCU_C_INSTANCE) {
		mru_addr = (IPC_RCU_MRU_C_Type *)
			ipc_hw_priv[instance].rx_mapped_addr;
		/* Get MRU mailbox 0 address */
		ipc_hw_priv[instance].IPC_MRU_TX.tx_CHx_MB0
			= &(mru_addr->CH[mru_channel_id].MB[0]);
	} else {
		mru_hl_addr = (IPC_RCU_MRU_HL_Type *)
				ipc_hw_priv[instance].rx_mapped_addr;
		/* Get MRU mailbox 0 address */
		ipc_hw_priv[instance].IPC_MRU_TX.tx_CHx_MB0
			= &(mru_addr->CH[mru_channel_id].MB[0]);
	}


	return 0;
}

/**
 * ipc_hw_rx_mru_init() - initialize transmitting interrupt for ipc-shm
 *                        driver tx
 *
 * @instance: instance id
 * @cfg:      configuration parameters
 *
 * Return:
 * * %0		- on Success
 * * %-EINVAL	- on Error
 */
static int8_t ipc_hw_tx_mru_init(const uint8_t instance,
	const struct ipc_shm_cfg *cfg)
{
	int8_t err = 0;
	IPC_MRU_INSTANCE_ENUM_Type mru_instance = cfg->mru_tx.mru_instance;

	if ((cfg->inter_core_tx_irq != IPC_IRQ_NONE)
			&& (cfg->inter_core_tx_irq != IPC_IRQ_MRU)) {
		return -EINVAL;
	}


	/* get DT node depends on the MRU type */
	switch (mru_instance) {
	case IPC_MRU_RTU_INSTANCE:
		err = ipc_hw_rtu_tx_mru(instance, cfg);
		break;
	case IPC_MRU_APS_INSTANCE:
		err = ipc_hw_aps_tx_mru(instance, cfg);
		break;
	case IPC_MRU_FSS_INSTANCE:
		err = ipc_hw_fss_tx_mru(instance, cfg);
		break;
	case IPC_MRU_RCU_C_INSTANCE:
		err = ipc_hw_rcu_tx_mru(instance, cfg);
		break;
	case IPC_MRU_RCU_L_INSTANCE:
		err = ipc_hw_rcu_tx_mru(instance, cfg);
		break;
	case IPC_MRU_RCU_H_INSTANCE:
		err = ipc_hw_rcu_tx_mru(instance, cfg);
		break;
	default:
		err = -EINVAL;
		break;
	}

	if (err != 0) {
		return -EINVAL;
	}

	ipc_hw_priv[instance].tx_irq = cfg->inter_core_tx_irq;


	return 0;
}

/**
 * ipc_hw_rx_mru_init() - initialize receiving interrupt for ipc-shm driver rx
 *
 * Only support interrupt configuration for interrupt from APS.MRU
 * All the interrupt group is ORED, the rx interrupt is calculated from the APS
 * subsystem and MRU index
 *
 * @instance: instance id
 * @cfg:      configuration parameters
 *
 * Return:
 * * %0		- on Success
 * * %-EINVAL	- on Error
 */
static int8_t ipc_hw_rx_mru_init(const uint8_t instance,
	const struct ipc_shm_cfg *cfg)
{
	IPC_SUBSYSTEM_INDEX_ENUM_Type subsystem_id = cfg->mru_rx.subsystem_id;
	IPC_MRU_INSTANCE_ENUM_Type mru_instance = cfg->mru_rx.mru_instance;
	IPC_MRU_INDEX_ENUM_Type mru_id = cfg->mru_rx.mru_id;
	IPC_MRU_CHANNEL_ENUM_Type mru_channel_id = cfg->mru_rx.mru_channel_id;
	IPC_APS_MRU_Type *mru_addr = NULL;

	/* Checking for using polling */
	if (cfg->inter_core_rx_irq == IPC_IRQ_NONE) {
		ipc_hw_priv[instance].rx_irq = cfg->inter_core_rx_irq;
		return 0;
	}

	/* Checking mru instance is configured, APS subsystem id is correct */
	if ((cfg->inter_core_rx_irq != IPC_IRQ_MRU)
		|| (mru_instance != IPC_MRU_APS_INSTANCE)
			|| (subsystem_id < IPC_SUBSYSTEM_0)
			|| (subsystem_id > IPC_SUBSYSTEM_3)) {
		return -EINVAL;
	}

	/* Mapping address of rx MRU */
	ipc_hw_priv[instance].rx_mapped_addr
		= ipc_os_mru_map_intc(mru_instance, subsystem_id, mru_id);
	mru_addr = (IPC_APS_MRU_Type *)ipc_hw_priv[instance].rx_mapped_addr;

	if (ipc_hw_priv[instance].rx_mapped_addr == NULL) {
		return -EINVAL;
	}

	/* Calculating interrupt index */
	ipc_hw_priv[instance].rx_irq = cfg->inter_core_rx_irq;

	ipc_hw_priv[instance].IPC_MRU_RX.rx_CHx_CFG0
		= &(mru_addr->CHXCONFIG[mru_channel_id].CH_CFG0);
	ipc_hw_priv[instance].IPC_MRU_RX.rx_CHx_CFG1
		= &(mru_addr->CHXCONFIG[mru_channel_id].CH_CFG1);
	ipc_hw_priv[instance].IPC_MRU_RX.rx_CHx_MBSTAT
		= &(mru_addr->CHXCONFIG[mru_channel_id].CH_MBSTAT);
	ipc_hw_priv[instance].IPC_MRU_RX.rx_CHx_MB0
		= &(mru_addr->CH[mru_channel_id].MB[0]);

	return 0;
}

/**
 * ipc_hw_init() - platform specific initialization
 *
 * MRU register order from Devicetree:
 * - RTU Subsystem: RTU0.MRU[0-7], RTU1.MRU[0-7], ...
 * - APS Subsystem: APS0.MRU[0-7], APS1.MRU[0-7], ...
 * - FSS Subsystem: FSS.MRU[0-10]
 * - RCU Subsystem: RCU.MRU_C[0-7], RCU_MRU_H[0-11], RCU_MRU_L[0-11]
 *
 * MRU Rx Interrupt from Devicetree:
 * - Only support interrupt from APS.MRU
 * - Interrupt:
 * -- 0-7:   Interrupt from APS0.MRU[0-7]
 * -- 8-15:  Interrupt from APS1.MRU[0-7]
 * -- 16-23: Interrupt from APS2.MRU[0-7]
 * -- 24-31: Interrupt from APS3.MRU[0-7]
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
	uint32_t ipc_chx_cfg;
	int8_t err = 0;

	/* Configuring tx interrupt for ipc-shm driver */
	err = ipc_hw_tx_mru_init(instance, cfg);
	if (err != 0) {
		return err;
	}

	/* Configuring rx interrupt for ipc-shm driver */
	err = ipc_hw_rx_mru_init(instance, cfg);
	if (err != 0) {
		return err;
	}

	if (cfg->inter_core_rx_irq == IPC_IRQ_NONE) {
		return 0;
	}

	/* Enable rx channel CHE */
	ipc_chx_cfg = readl(ipc_hw_priv[instance].IPC_MRU_RX.rx_CHx_CFG0);
	writel(ipc_chx_cfg | IPC_MRU_CHx_CFG0_CHE_ENABLE,
		ipc_hw_priv[instance].IPC_MRU_RX.rx_CHx_CFG0);

	/*
	 * disable rx irq source to avoid receiving an interrupt from remote
	 * before any of the buffer rings are initialized
	 */
	ipc_hw_irq_disable(instance);

	/* Enable rx mailbox MBEn */
	ipc_chx_cfg = readl(ipc_hw_priv[instance].IPC_MRU_RX.rx_CHx_CFG0);
	writel(ipc_chx_cfg | IPC_MRU_CHx_CFG0_MBE0_ENABLE,
		ipc_hw_priv[instance].IPC_MRU_RX.rx_CHx_CFG0);
	/*
	 * Using irq group 0 for MBICn, it doesn't matter which group we used
	 * Because all irq group from a MRU is ORED
	 */

	return 0;
}

/**
 * ipc_hw_free() - unmap MSCM IP block and clear irq
 */
void ipc_hw_free(const uint8_t instance)
{
	ipc_hw_irq_clear(instance);

	/* unmap MSCM hardware peripheral block */
	ipc_os_unmap_intc(ipc_hw_priv[instance].tx_mapped_addr);
	ipc_os_unmap_intc(ipc_hw_priv[instance].rx_mapped_addr);
}

/**
 * ipc_hw_irq_enable() - enable notifications from remote
 */
void ipc_hw_irq_enable(const uint8_t instance)
{
	uint32_t ipc_chx_cfg0;

	/* Enable rx irq mailbox IE */
	if (ipc_hw_priv[instance].rx_irq != IPC_IRQ_NONE) {
		ipc_chx_cfg0 = readl(
			ipc_hw_priv[instance].IPC_MRU_RX.rx_CHx_CFG0);

		writel(ipc_chx_cfg0 | IPC_MRU_CHx_CFG0_IE_ENABLE,
			ipc_hw_priv[instance].IPC_MRU_RX.rx_CHx_CFG0);
	}
}

/**
 * ipc_hw_irq_disable() - disable notifications from remote
 */
void ipc_hw_irq_disable(const uint8_t instance)
{
	uint32_t ipc_chx_cfg0;

	/* Disable rx irq mailbox IE */
	if (ipc_hw_priv[instance].rx_irq != IPC_IRQ_NONE) {
		ipc_chx_cfg0 = readl(
			ipc_hw_priv[instance].IPC_MRU_RX.rx_CHx_CFG0);

		writel(ipc_chx_cfg0 & ~IPC_MRU_CHx_CFG0_IE_ENABLE,
			ipc_hw_priv[instance].IPC_MRU_RX.rx_CHx_CFG0);
	}

}

/**
 * ipc_hw_irq_notify() - notify remote that data is available
 */
void ipc_hw_irq_notify(const uint8_t instance)
{
	/* Writing IPCF_SENDING to TX mailbox */
	if (ipc_hw_priv[instance].tx_irq != IPC_IRQ_NONE) {
		writel(IPC_SHM_SENDING,
			ipc_hw_priv[instance].IPC_MRU_TX.tx_CHx_MB0);
	}

}

/**
 * ipc_hw_irq_clear() - clear available data notification
 */
void ipc_hw_irq_clear(const uint8_t instance)
{
	/* Writing 1 to MBSn */
	if (ipc_hw_priv[instance].rx_irq != IPC_IRQ_NONE) {
		writel(IPC_MRU_CHx_MBSTAT0_CLEAR,
			ipc_hw_priv[instance].IPC_MRU_RX.rx_CHx_MBSTAT);
	}
}
