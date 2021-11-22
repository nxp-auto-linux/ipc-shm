/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2021 NXP
 */
#include <linux/io.h>

#include "ipc-shm.h"
#include "ipc-os.h"
#include "ipc-hw.h"

/* S32g3xx Processor IDs */
enum s32g3xx_processor_idx {
	A53_0 = 0,  /* ARM Cortex-A53 cluster 0 core 0 */
	A53_1 = 1,  /* ARM Cortex-A53 cluster 1 core 1 */
	A53_2 = 2,  /* ARM Cortex-A53 cluster 1 core 0 */
	A53_3 = 3,  /* ARM Cortex-A53 cluster 1 core 1 */
	M7_0 = 4,   /* ARM Cortex-M7 core 0 */
	M7_1 = 5,   /* ARM Cortex-M7 core 1 */
	M7_2 = 6,   /* ARM Cortex-M7 core 2 */
	M7_3 = 7,   /* ARM Cortex-M7 core 2 */
	A53_4 = 8,  /* ARM Cortex-A53 cluster 0 core 2 */
	A53_5 = 9,  /* ARM Cortex-A53 cluster 0 core 3 */
	A53_6 = 10, /* ARM Cortex-A53 cluster 1 core 2 */
	A53_7 = 11, /* ARM Cortex-A53 cluster 1 core 3 */
};

/* S32g3xx Specific Definitions */
#define DEFAULT_REMOTE_CORE    M7_0
#define DEFAULT_LOCAL_CORE     A53_0  /* default core targeted by remote */
#define IRQ_ID_MIN             0
#define IRQ_ID_MAX             12

/* MSCM registers count for S32g3xx */
#define MSCM_CPnCFG_COUNT      4
#define MSCM_CP_COUNT          12
#define MSCM_IRQ_COUNT         14
#define MSCM_IRSPRC_COUNT      240
#define MSCM_RESERVED00_COUNT  4
#define MSCM_RESERVED01_COUNT  612
#define MSCM_RESERVED02_COUNT  1020
#define MSCM_RESERVED03_COUNT  124


/**
 * struct mscm_regs - MSCM Peripheral Register Structure
 */
struct mscm_regs {
	volatile const uint32_t cpxtype;
	volatile const uint32_t cpxnum;
	volatile const uint32_t cpxrev;
	volatile const uint32_t cpxcfg[MSCM_CPnCFG_COUNT];
	uint8_t RESERVED00[MSCM_RESERVED00_COUNT];
	volatile const uint32_t cp0type;
	volatile const uint32_t cp0num;
	volatile const uint32_t cp0rev;
	volatile const uint32_t cp0cfg[MSCM_CPnCFG_COUNT];
	uint8_t RESERVED01[MSCM_RESERVED00_COUNT];
	volatile const uint32_t cp1type;
	volatile const uint32_t cp1num;
	volatile const uint32_t cp1rev;
	volatile const uint32_t cp1cfg[MSCM_CPnCFG_COUNT];
	uint8_t RESERVED02[MSCM_RESERVED00_COUNT];
	volatile const uint32_t cp2type;
	volatile const uint32_t cp2num;
	volatile const uint32_t cp2rev;
	volatile const uint32_t cp2cfg[MSCM_CPnCFG_COUNT];
	uint8_t RESERVED03[MSCM_RESERVED00_COUNT];
	volatile const uint32_t cp3type;
	volatile const uint32_t cp3num;
	volatile const uint32_t cp3rev;
	volatile const uint32_t cp3cfg[MSCM_CPnCFG_COUNT];
	uint8_t RESERVED04[MSCM_RESERVED00_COUNT];
	volatile const uint32_t cp4type;
	volatile const uint32_t cp4num;
	volatile const uint32_t cp4rev;
	volatile const uint32_t cp4cfg[MSCM_CPnCFG_COUNT];
	uint8_t RESERVED05[MSCM_RESERVED00_COUNT];
	volatile const uint32_t cp5type;
	volatile const uint32_t cp5num;
	volatile const uint32_t cp5rev;
	volatile const uint32_t cp5cfg[MSCM_CPnCFG_COUNT];
	uint8_t RESERVED06[MSCM_RESERVED00_COUNT];
	volatile const uint32_t cp6type;
	volatile const uint32_t cp6num;
	volatile const uint32_t cp6rev;
	volatile const uint32_t cp6cfg[MSCM_CPnCFG_COUNT];
	uint8_t RESERVED07[MSCM_RESERVED00_COUNT];
	volatile const uint32_t cp7type;
	volatile const uint32_t cp7num;
	volatile const uint32_t cp7rev;
	volatile const uint32_t cp7cfg[MSCM_CPnCFG_COUNT];
	uint8_t RESERVED08[MSCM_RESERVED00_COUNT];
	volatile const uint32_t cp8type;
	volatile const uint32_t cp8num;
	volatile const uint32_t cp8rev;
	volatile const uint32_t cp8cfg[MSCM_CPnCFG_COUNT];
	uint8_t RESERVED09[MSCM_RESERVED00_COUNT];
	volatile const uint32_t cp9type;
	volatile const uint32_t cp9num;
	volatile const uint32_t cp9rev;
	volatile const uint32_t cp9cfg[MSCM_CPnCFG_COUNT];
	uint8_t RESERVED010[MSCM_RESERVED00_COUNT];
	volatile const uint32_t cp10type;
	volatile const uint32_t cp10num;
	volatile const uint32_t cp10rev;
	volatile const uint32_t cp10cfg[MSCM_CPnCFG_COUNT];
	uint8_t RESERVED011[MSCM_RESERVED00_COUNT];
	volatile const uint32_t cp11type;
	volatile const uint32_t cp11num;
	volatile const uint32_t cp11rev;
	volatile const uint32_t cp11cfg[MSCM_CPnCFG_COUNT];
	uint8_t RESERVED012[MSCM_RESERVED01_COUNT];
	volatile uint32_t ircpcfg;
	uint8_t RESERVED013[MSCM_RESERVED02_COUNT];
	volatile uint32_t irnmic;
	uint8_t RESERVED14[MSCM_RESERVED03_COUNT];
	volatile uint16_t irsprc[MSCM_IRSPRC_COUNT];
	struct {
		volatile uint32_t isr;
		volatile uint32_t igr;
	} ircpnirx[MSCM_CP_COUNT][MSCM_IRQ_COUNT];
};

/* MSCM Hardware Register Bit Fields Definitions */

#define MSCM_IRCPnISRn_CP0_INT    0x01uL /* Processor 0 Interrupt Mask */
#define MSCM_IRCPnISRn_CP1_INT    0x02uL /* Processor 1 Interrupt Mask */
#define MSCM_IRCPnISRn_CP2_INT    0x04uL /* Processor 2 Interrupt Mask */
#define MSCM_IRCPnISRn_CP3_INT    0x08uL /* Processor 3 Interrupt Mask */
#define MSCM_IRCPnISRn_CP4_INT    0x10uL /* Processor 4 Interrupt Mask */
#define MSCM_IRCPnISRn_CP5_INT    0x20uL /* Processor 5 Interrupt Mask */
#define MSCM_IRCPnISRn_CP6_INT    0x40uL /* Processor 6 Interrupt Mask */
#define MSCM_IRCPnISRn_CPx_INT    0x7FuL /* All Processors Interrupt Mask */

#define MSCM_IRCPnIGRn_INT_EN    0x1uL /* Interrupt Enable */

#define MSCM_IRCPCFG_LOCK    0x80000000uL /* Interrupt Router Config Lock Bit */
#define MSCM_IRCPCFG_CP0_TR  0x01uL /* Processor 0 Trusted core */
#define MSCM_IRCPCFG_CP1_TR  0x02uL /* Processor 1 Trusted core */
#define MSCM_IRCPCFG_CP2_TR  0x04uL /* Processor 2 Trusted core */
#define MSCM_IRCPCFG_CP3_TR  0x08uL /* Processor 3 Trusted core */
#define MSCM_IRCPCFG_CP4_TR  0x10uL /* Processor 4 Trusted core */
#define MSCM_IRCPCFG_CP5_TR  0x20uL /* Processor 5 Trusted core */
#define MSCM_IRCPCFG_CP6_TR  0x40uL /* Processor 6 Trusted core */
#define MSCM_IRCPCFG_A53_TR  0x7FuL /* Processors 0 to 3 Trusted cores */

#define MSCM_IRSPRCn_LOCK    0x8000u /* Interrupt Routing Control Lock Bit */

#define MSCM_IRSPRCn_GIC500    0x1u /* GIC500 Interrupt Steering Enable */

#define MSCM_IRSPRCn_M7_0    0x02u /* CM7 Core 0 Interrupt Steering Enable */
#define MSCM_IRSPRCn_M7_1    0x04u /* CM7 Core 1 Interrupt Steering Enable */
#define MSCM_IRSPRCn_M7_2    0x08u /* CM7 Core 2 Interrupt Steering Enable */
#define MSCM_IRSPRCn_M7_3    0x10u /* CM7 Core 3 Interrupt Steering Enable */

/* S32 Platform Specific Implementation  */

/**
 * struct ipc_hw_priv - platform specific private data
 *
 * @mscm_tx_irq:    MSCM inter-core interrupt reserved for shm driver tx
 * @mscm_rx_irq:    MSCM inter-core interrupt reserved for shm driver rx
 * @remote_core:    index of remote core to trigger the interrupt on
 * @local_core:     index of the local core targeted by remote
 * @mscm:           pointer to memory-mapped hardware peripheral MSCM
 */
static struct ipc_hw_priv {
	int mscm_tx_irq;
	int mscm_rx_irq;
	int remote_core;
	int local_core;
	struct mscm_regs *mscm;
} priv;

/**
 * ipc_hw_get_rx_irq() - get MSCM inter-core interrupt index [0..2] used for Rx
 *
 * Return: MSCM inter-core interrupt index used for Rx
 */
int ipc_hw_get_rx_irq(const uint8_t instance)
{
	return priv.mscm_rx_irq;
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
 * Return: 0 for success, -EINVAL for either inter core interrupt invalid or
 *         invalid remote core, -ENOMEM for failing to map MSCM address space,
 *         -EACCES for failing to access MSCM registers
 */
int ipc_hw_init(const uint8_t instance, const struct ipc_shm_cfg *cfg)
{
	/* map MSCM hardware peripheral block registers */
	void *addr = ipc_os_map_intc();

	return _ipc_hw_init(instance, cfg->inter_core_tx_irq,
			cfg->inter_core_rx_irq, &cfg->remote_core,
			&cfg->local_core, addr);
}

/**
 * _ipc_hw_init() - platform specific initialization
 *
 * Low level variant of ipc_hw_init() used by UIO device implementation.
 */
int _ipc_hw_init(const uint8_t instance, int tx_irq, int rx_irq,
		 const struct ipc_shm_remote_core *remote_core,
		 const struct ipc_shm_local_core *local_core, void *mscm_addr)
{
	int remote_core_idx;
	int local_core_idx;
	uint32_t ircpcfg_mask;

	if (!mscm_addr)
		return -EINVAL;

	priv.mscm = (struct mscm_regs *)mscm_addr;

	switch (local_core->type) {
	case IPC_CORE_A53:
		switch (local_core->index) {
		case IPC_CORE_INDEX_0:
			local_core_idx = A53_0;
			break;
		case IPC_CORE_INDEX_1:
			local_core_idx = A53_1;
			break;
		case IPC_CORE_INDEX_2:
			local_core_idx = A53_2;
			break;
		case IPC_CORE_INDEX_3:
			local_core_idx = A53_3;
			break;
		case IPC_CORE_INDEX_4:
			local_core_idx = A53_4;
			break;
		case IPC_CORE_INDEX_5:
			local_core_idx = A53_5;
			break;
		case IPC_CORE_INDEX_6:
			local_core_idx = A53_6;
			break;
		case IPC_CORE_INDEX_7:
			local_core_idx = A53_7;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IPC_CORE_DEFAULT:
		local_core_idx = DEFAULT_LOCAL_CORE;
		break;
	default:
		return -EINVAL;
	}

	/* check trusted cores mask contains the targeted and other A53 cores */
	if ((!local_core->trusted)
		|| (local_core->trusted & ~MSCM_IRCPCFG_A53_TR)
		|| ((0x01uL << local_core_idx) & ~local_core->trusted)) {
		return -EINVAL;
	}

	switch (remote_core->type) {
	case IPC_CORE_A53:
		switch (remote_core->index) {
		case IPC_CORE_INDEX_0:
			remote_core_idx = A53_0;
			break;
		case IPC_CORE_INDEX_1:
			remote_core_idx = A53_1;
			break;
		case IPC_CORE_INDEX_2:
			remote_core_idx = A53_2;
			break;
		case IPC_CORE_INDEX_3:
			remote_core_idx = A53_3;
			break;
		case IPC_CORE_INDEX_4:
			remote_core_idx = A53_4;
			break;
		case IPC_CORE_INDEX_5:
			remote_core_idx = A53_5;
			break;
		case IPC_CORE_INDEX_6:
			remote_core_idx = A53_6;
			break;
		case IPC_CORE_INDEX_7:
			remote_core_idx = A53_7;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IPC_CORE_M7:
		switch (remote_core->index) {
		case IPC_CORE_INDEX_0:
			remote_core_idx = M7_0;
			break;
		case IPC_CORE_INDEX_1:
			remote_core_idx = M7_1;
			break;
		case IPC_CORE_INDEX_2:
			remote_core_idx = M7_2;
			break;
		case IPC_CORE_INDEX_3:
			remote_core_idx = M7_3;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IPC_CORE_DEFAULT:
		remote_core_idx = DEFAULT_REMOTE_CORE;
		break;
	default:
		return -EINVAL;
	}

	if (((tx_irq != IPC_IRQ_NONE)
			&& ((tx_irq < IRQ_ID_MIN) || (tx_irq > IRQ_ID_MAX)))
		|| (rx_irq < IRQ_ID_MIN || rx_irq > IRQ_ID_MAX)
		|| (rx_irq == tx_irq)
		|| (remote_core_idx == readl_relaxed(&priv.mscm->cpxnum))
		|| (remote_core_idx == local_core_idx)) {
		return -EINVAL;
	}

	priv.mscm_tx_irq = tx_irq;
	priv.mscm_rx_irq = rx_irq;
	priv.remote_core = remote_core_idx;
	priv.local_core = local_core_idx;

	/*
	 * disable rx irq source to avoid receiving an interrupt from remote
	 * before any of the buffer rings are initialized
	 */
	ipc_hw_irq_disable(instance);

	/*
	 * enable local trusted cores so that they can read full contents of
	 * IRCPnISRx registers
	 */
	ircpcfg_mask = readl_relaxed(&priv.mscm->ircpcfg);
	if (ircpcfg_mask & MSCM_IRCPCFG_LOCK)
		return -EACCES;

	writel_relaxed(ircpcfg_mask | local_core->trusted, &priv.mscm->ircpcfg);

	return 0;
}

/**
 * ipc_hw_free() - unmap MSCM IP block and clear irq
 */
void ipc_hw_free(const uint8_t instance)
{
	ipc_hw_irq_clear(instance);

	/* unmap MSCM hardware peripheral block */
	ipc_os_unmap_intc(priv.mscm);
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

	/* enable MSCM core-to-core interrupt routing */
	irsprc_mask = readw_relaxed(&priv.mscm->irsprc[priv.mscm_rx_irq + 1]);
	writew_relaxed(irsprc_mask | MSCM_IRSPRCn_GIC500,
			&priv.mscm->irsprc[priv.mscm_rx_irq + 1]);
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

	/* disable MSCM core-to-core interrupt routing */
	irsprc_mask = readw_relaxed(&priv.mscm->irsprc[priv.mscm_rx_irq + 1]);
	writew_relaxed(irsprc_mask & ~MSCM_IRSPRCn_GIC500,
			&priv.mscm->irsprc[priv.mscm_rx_irq + 1]);
}

/**
 * ipc_hw_irq_notify() - notify remote that data is available
 */
void ipc_hw_irq_notify(const uint8_t instance)
{
	if (priv.mscm_tx_irq == IPC_IRQ_NONE)
		return;

	/* trigger MSCM core-to-core directed interrupt */
	writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
		&priv.mscm->ircpnirx[priv.remote_core][priv.mscm_tx_irq].igr);
}

/**
 * ipc_hw_irq_clear() - clear available data notification
 */
void ipc_hw_irq_clear(const uint8_t instance)
{
	/* clear MSCM core-to-core directed interrupt on the targeted core */
	writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
		&priv.mscm->ircpnirx[priv.local_core][priv.mscm_rx_irq].isr);
}
