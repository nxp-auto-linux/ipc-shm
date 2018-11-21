/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018 NXP
 */
#include <linux/io.h>

#include "ipc-shm.h"
#include "ipc-os.h"
#include "ipc-hw.h"

/* Hardware IP Block Base Addresses - TODO: get them from device tree */
#define MSCM_BASE    0x40198000ul /* Miscellaneous System Control Module */

/* S32gen1 Processor IDs */
enum s32gen1_processor_idx {
	A53_0 = 0, /* ARM Cortex-A53 core 0 */
	A53_1 = 1, /* ARM Cortex-A53 core 1 */
	A53_2 = 2, /* ARM Cortex-A53 core 2 */
	A53_3 = 3, /* ARM Cortex-A53 core 3 */
	M7_0 = 4, /* ARM Cortex-M7 core 0 */
	M7_1 = 5, /* ARM Cortex-M7 core 1 */
	M7_2 = 6, /* ARM Cortex-M7 core 2 */
};

/* S32gen1 Specific Definitions */
#define DEFAULT_MSCM_IRQ_ID    2u /* MSCM irq 2 = GIC irq 35 */
#define DEFAULT_REMOTE_CORE    M7_0

/**
 * struct mscm_regs - MSCM Peripheral Register Structure
 *
 * @cpxtype:        Processor x Type Register, offset 0x0
 * @cpxnum:         Processor x Number Register, offset 0x4
 * @cpxrev:         Processor x Revision Register, offset 0x8
 * @cpxcfg[n]:      Processor x Configuration n Register, offset 0xC + 4*n
 * @cp0type:        Processor 0 Type Register, offset 0x20
 * @cp0num:         Processor 0 Number Register, offset 0x24
 * @cp0rev:         Processor 0 Revision Register, offset 0x28
 * @cp0cfg[n]:      Processor 0 Configuration n Register, offset 0x2C + 4*n
 * @cp1type:        Processor 1 Type Register, offset 0x40
 * @cp1num:         Processor 1 Number Register, offset 0x44
 * @cp1rev:         Processor 1 Revision Register, offset 0x48
 * @cp1cfg[n]:      Processor 1 Configuration n Register, offset 0x4C + 4*n
 * @cp2type:        Processor 2 Type Register, offset 0x60
 * @cp2num:         Processor 2 Number Register, offset 0x64
 * @cp2rev:         Processor 2 Revision Register, offset 0x68
 * @cp2cfg[n]:      Processor 2 Configuration n Register, offset 0x6C + 4*n
 * @cp3type:        Processor 3 Type Register, offset 0x80
 * @cp3num:         Processor 3 Number Register, offset 0x84
 * @cp3rev:         Processor 3 Revision Register, offset 0x88
 * @cp3cfg[n]:      Processor 3 Configuration n Register, offset 0x8C + 4*n
 * @cp4type:        Processor 4 Type Register, offset 0xA0
 * @cp4num:         Processor 4 Number Register, offset 0xA4
 * @cp4rev:         Processor 4 Revision Register, offset 0xA8
 * @cp4cfg[n]:      Processor 4 Configuration n Register, offset 0xAC + 4*n
 * @cp5type:        Processor 5 Type Register, offset 0xC0
 * @cp5num:         Processor 5 Number Register, offset 0xC4
 * @cp5rev:         Processor 5 Revision Register, offset 0xC8
 * @cp5cfg[n]:      Processor 5 Configuration n Register, offset 0xCC + 4*n
 * @cp6type:        Processor 6 Type Register, offset 0xE0
 * @cp6num:         Processor 6 Number Register, offset 0xE4
 * @cp6rev:         Processor 6 Revision Register, offset 0xE8
 * @cp6cfg[n]:      Processor 6 Configuration n Register, offset 0xEC + 4*n
 * @ircp0isr0:      Interrupt Router CP0 Interrupt0 Status Register,
 *                  offset 0x200
 * @ircp0igr0:      Interrupt Router CP0 Interrupt0 Generation Register,
 *                  offset 0x204
 * @ircp0isr1:      Interrupt Router CP0 Interrupt1 Status Register,
 *                  offset 0x208
 * @ircp0igr1:      Interrupt Router CP0 Interrupt1 Generation Register,
 *                  offset 0x20C
 * @ircp0isr2:      Interrupt Router CP0 Interrupt2 Status Register,
 *                  offset 0x210
 * @ircp0igr2:      Interrupt Router CP0 Interrupt2 Generation Register,
 *                  offset 0x214
 * @ircp0isr3:      Interrupt Router CP0 InterruptX Status Register,
 *                  offset 0x218
 * @ircp0igr3:      Interrupt Router CP0 InterruptX Generation Register,
 *                  offset 0x21C
 * @ircp1isr0:      Interrupt Router CP1 Interrupt0 Status Register,
 *                  offset 0x220
 * @ircp1igr0:      Interrupt Router CP1 Interrupt0 Generation Register,
 *                  offset 0x224
 * @ircp1isr1:      Interrupt Router CP1 Interrupt1 Status Register,
 *                  offset 0x228
 * @ircp1igr1:      Interrupt Router CP1 Interrupt1 Generation Register,
 *                  offset 0x22C
 * @ircp1isr2:      Interrupt Router CP1 Interrupt2 Status Register,
 *                  offset 0x230
 * @ircp1igr2:      Interrupt Router CP1 Interrupt2 Generation Register,
 *                  offset 0x234
 * @ircp1isr3:      Interrupt Router CP1 InterruptX Status Register,
 *                  offset 0x238
 * @ircp1igr3:      Interrupt Router CP1 InterruptX Generation Register,
 *                  offset 0x23C
 * @ircp2isr0:      Interrupt Router CP2 Interrupt0 Status Register,
 *                  offset 0x240
 * @ircp2igr0:      Interrupt Router CP2 Interrupt0 Generation Register,
 *                  offset 0x244
 * @ircp2isr1:      Interrupt Router CP2 Interrupt1 Status Register,
 *                  offset 0x248
 * @ircp2igr1:      Interrupt Router CP2 Interrupt1 Generation Register,
 *                  offset 0x24C
 * @ircp2isr2:      Interrupt Router CP2 Interrupt2 Status Register,
 *                  offset 0x250
 * @ircp2igr2:      Interrupt Router CP2 Interrupt2 Generation Register,
 *                  offset 0x254
 * @ircp2isr3:      Interrupt Router CP2 InterruptX Status Register,
 *                  offset 0x258
 * @ircp2igr3:      Interrupt Router CP2 InterruptX Generation Register,
 *                  offset 0x25C
 * @ircp3isr0:      Interrupt Router CP3 Interrupt0 Status Register,
 *                  offset 0x260
 * @ircp3igr0:      Interrupt Router CP3 Interrupt0 Generation Register,
 *                  offset 0x264
 * @ircp3isr1:      Interrupt Router CP3 Interrupt1 Status Register,
 *                  offset 0x268
 * @ircp3igr1:      Interrupt Router CP3 Interrupt1 Generation Register,
 *                  offset 0x26C
 * @ircp3isr2:      Interrupt Router CP3 Interrupt2 Status Register,
 *                  offset 0x270
 * @ircp3igr2:      Interrupt Router CP3 Interrupt2 Generation Register,
 *                  offset 0x274
 * @ircp3isr3:      Interrupt Router CP3 InterruptX Status Register,
 *                  offset 0x278
 * @ircp3igr3:      Interrupt Router CP3 InterruptX Generation Register,
 *                  offset 0x27C
 * @ircp4isr0:      Interrupt Router CP4 Interrupt0 Status Register,
 *                  offset 0x280
 * @ircp4igr0:      Interrupt Router CP4 Interrupt0 Generation Register,
 *                  offset 0x284
 * @ircp4isr1:      Interrupt Router CP4 Interrupt1 Status Register,
 *                  offset 0x288
 * @ircp4igr1:      Interrupt Router CP4 Interrupt1 Generation Register,
 *                  offset 0x28C
 * @ircp4isr2:      Interrupt Router CP4 Interrupt2 Status Register,
 *                  offset 0x290
 * @ircp4igr2:      Interrupt Router CP4 Interrupt2 Generation Register,
 *                  offset 0x294
 * @ircp4isr3:      Interrupt Router CP4 InterruptX Status Register,
 *                  offset 0x298
 * @ircp4igr3:      Interrupt Router CP4 InterruptX Generation Register,
 *                  offset 0x29C
 * @ircp5isr0:      Interrupt Router CP5 Interrupt0 Status Register,
 *                  offset 0x2A0
 * @ircp5igr0:      Interrupt Router CP5 Interrupt0 Generation Register,
 *                  offset 0x2A4
 * @ircp5isr1:      Interrupt Router CP5 Interrupt1 Status Register,
 *                  offset 0x2A8
 * @ircp5igr1:      Interrupt Router CP5 Interrupt1 Generation Register,
 *                  offset 0x2AC
 * @ircp5isr2:      Interrupt Router CP5 Interrupt2 Status Register,
 *                  offset 0x2B0
 * @ircp5igr2:      Interrupt Router CP5 Interrupt2 Generation Register,
 *                  offset 0x2B4
 * @ircp5isr3:      Interrupt Router CP5 InterruptX Status Register,
 *                  offset 0x2B8
 * @ircp5igr3:      Interrupt Router CP5 InterruptX Generation Register,
 *                  offset 0x2BC
 * @ircp6isr0:      Interrupt Router CP6 Interrupt0 Status Register,
 *                  offset 0x2C0
 * @ircp6igr0:      Interrupt Router CP6 Interrupt0 Generation Register,
 *                  offset 0x2C4
 * @ircp6isr1:      Interrupt Router CP6 Interrupt1 Status Register,
 *                  offset 0x2C8
 * @ircp6igr1:      Interrupt Router CP6 Interrupt1 Generation Register,
 *                  offset 0x2CC
 * @ircp6isr2:      Interrupt Router CP6 Interrupt2 Status Register,
 *                  offset 0x2D0
 * @ircp6igr2:      Interrupt Router CP6 Interrupt2 Generation Register,
 *                  offset 0x2D4
 * @ircp6isr3:      Interrupt Router CP6 InterruptX Status Register,
 *                  offset 0x2D8
 * @ircp6igr3:      Interrupt Router CP6 InterruptX Generation Register,
 *                  offset 0x2DC
 * @ircpcfg:        Interrupt Router Configuration Register,
 *                  offset 0x400
 * @irnmic:         Interrupt Router Non-Maskable Interrupt Control Register,
 *                  offset 0x800
 * @irsprc[n]:      Interrupt Router Shared Peripheral Routing Control
 *                  Register n, offset 0x880 + 2*n
 */
struct mscm_regs {
	volatile const uint32_t cpxtype;
	volatile const uint32_t cpxnum;
	volatile const uint32_t cpxrev;
	volatile const uint32_t cpxcfg[4];
	uint8_t reserved00[4]; /* 0x4 */
	volatile const uint32_t cp0type;
	volatile const uint32_t cp0num;
	volatile const uint32_t cp0rev;
	volatile const uint32_t cp0cfg[4];
	uint8_t reserved01[4]; /* 0x4 */
	volatile const uint32_t cp1type;
	volatile const uint32_t cp1num;
	volatile const uint32_t cp1rev;
	volatile const uint32_t cp1cfg[4];
	uint8_t reserved02[4]; /* 0x4 */
	volatile const uint32_t cp2type;
	volatile const uint32_t cp2num;
	volatile const uint32_t cp2rev;
	volatile const uint32_t cp2cfg[4];
	uint8_t reserved03[4]; /* 0x4 */
	volatile const uint32_t cp3type;
	volatile const uint32_t cp3num;
	volatile const uint32_t cp3rev;
	volatile const uint32_t cp3cfg[4];
	uint8_t reserved04[4]; /* 0x4 */
	volatile const uint32_t cp4type;
	volatile const uint32_t cp4num;
	volatile const uint32_t cp4rev;
	volatile const uint32_t cp4cfg[4];
	uint8_t reserved05[4]; /* 0x4 */
	volatile const uint32_t cp5type;
	volatile const uint32_t cp5num;
	volatile const uint32_t cp5rev;
	volatile const uint32_t cp5cfg[4];
	uint8_t reserved06[4]; /* 0x4 */
	volatile const uint32_t cp6type;
	volatile const uint32_t cp6num;
	volatile const uint32_t cp6rev;
	volatile const uint32_t cp6cfg[4];
	uint8_t reserved07[260]; /* 0x104 */
	volatile uint32_t ircp0isr0;
	volatile uint32_t ircp0igr0;
	volatile uint32_t ircp0isr1;
	volatile uint32_t ircp0igr1;
	volatile uint32_t ircp0isr2;
	volatile uint32_t ircp0igr2;
	volatile uint32_t ircp0isr3;
	volatile uint32_t ircp0igr3;
	volatile uint32_t ircp1isr0;
	volatile uint32_t ircp1igr0;
	volatile uint32_t ircp1isr1;
	volatile uint32_t ircp1igr1;
	volatile uint32_t ircp1isr2;
	volatile uint32_t ircp1igr2;
	volatile uint32_t ircp1isr3;
	volatile uint32_t ircp1igr3;
	volatile uint32_t ircp2isr0;
	volatile uint32_t ircp2igr0;
	volatile uint32_t ircp2isr1;
	volatile uint32_t ircp2igr1;
	volatile uint32_t ircp2isr2;
	volatile uint32_t ircp2igr2;
	volatile uint32_t ircp2isr3;
	volatile uint32_t ircp2igr3;
	volatile uint32_t ircp3isr0;
	volatile uint32_t ircp3igr0;
	volatile uint32_t ircp3isr1;
	volatile uint32_t ircp3igr1;
	volatile uint32_t ircp3isr2;
	volatile uint32_t ircp3igr2;
	volatile uint32_t ircp3isr3;
	volatile uint32_t ircp3igr3;
	volatile uint32_t ircp4isr0;
	volatile uint32_t ircp4igr0;
	volatile uint32_t ircp4isr1;
	volatile uint32_t ircp4igr1;
	volatile uint32_t ircp4isr2;
	volatile uint32_t ircp4igr2;
	volatile uint32_t ircp4isr3;
	volatile uint32_t ircp4igr3;
	volatile uint32_t ircp5isr0;
	volatile uint32_t ircp5igr0;
	volatile uint32_t ircp5isr1;
	volatile uint32_t ircp5igr1;
	volatile uint32_t ircp5isr2;
	volatile uint32_t ircp5igr2;
	volatile uint32_t ircp5isr3;
	volatile uint32_t ircp5igr3;
	volatile uint32_t ircp6isr0;
	volatile uint32_t ircp6igr0;
	volatile uint32_t ircp6isr1;
	volatile uint32_t ircp6igr1;
	volatile uint32_t ircp6isr2;
	volatile uint32_t ircp6igr2;
	volatile uint32_t ircp6isr3;
	volatile uint32_t ircp6igr3;
	uint8_t reserved08[288]; /* 0x120 */
	volatile uint32_t ircpcfg;
	uint8_t reserved09[1020]; /* 0x3FC */
	volatile uint32_t irnmic;
	uint8_t reserved10[124]; /* 0x7C */
	volatile uint16_t irsprc[240];
};

/* MSCM Hardware Register Bit Fields Definitions */

#define MSCM_IRCPnISRn_CP0_INT    0x01ul /* Processor 0 Interrupt Mask */
#define MSCM_IRCPnISRn_CP1_INT    0x02ul /* Processor 1 Interrupt Mask */
#define MSCM_IRCPnISRn_CP2_INT    0x04ul /* Processor 2 Interrupt Mask */
#define MSCM_IRCPnISRn_CP3_INT    0x08ul /* Processor 3 Interrupt Mask */
#define MSCM_IRCPnISRn_CP4_INT    0x10ul /* Processor 4 Interrupt Mask */
#define MSCM_IRCPnISRn_CP5_INT    0x20ul /* Processor 5 Interrupt Mask */
#define MSCM_IRCPnISRn_CP6_INT    0x40ul /* Processor 6 Interrupt Mask */
#define MSCM_IRCPnISRn_CPx_INT    0x7Ful /* All Processors Interrupt Mask */

#define MSCM_IRCPnIGRn_INT_EN    0x1ul /* Interrupt Enable */

#define MSCM_IRCPCFG_LOCK    0x80000000ul /* Interrupt Router Config Lock Bit */

#define MSCM_IRSPRCn_LOCK    0x8000u /* Interrupt Routing Control Lock Bit */

#define MSCM_IRSPRCn_GIC500    0x1u /* GIC500 Interrupt Steering Enable */

#define MSCM_IRSPRCn_M7_0    0x2u /* CM7 Core 0 Interrupt Steering Enable */
#define MSCM_IRSPRCn_M7_1    0x4u /* CM7 Core 1 Interrupt Steering Enable */
#define MSCM_IRSPRCn_M7_2    0x8u /* CM7 Core 2 Interrupt Steering Enable */

/* S32 Platform Specific Implementation  */

/**
 * struct ipc_hw_priv - platform specific private data
 *
 * @mscm_tx_irq:    MSCM inter-core interrupt reserved for shm driver tx
 * @mscm_rx_irq:    MSCM inter-core interrupt reserved for shm driver rx
 * @remote_core:    remote core to trigger the interrupt on
 * @mscm:           pointer to memory-mapped hardware peripheral MSCM
 */
static struct ipc_hw_priv {
	int mscm_tx_irq;
	int mscm_rx_irq;
	int remote_core;
	struct mscm_regs *mscm;
} priv;

/**
 * ipc_hw_get_rx_irq() - get MSCM inter-core interrupt index [0..2] used for Rx
 *
 * Return: MSCM inter-core interrupt index used for Rx
 */
int ipc_hw_get_rx_irq(void)
{
	return priv.mscm_rx_irq;
}

/**
 * ipc_hw_init() - platform specific initialization
 *
 * @cfg:    configuration parameters
 *
 * If the value IPC_DEFAULT_INTER_CORE_IRQ is passed as either inter_core_tx_irq
 * or inter_core_rx_irq or both, the default irq value defined for the selected
 * platform will be used instead; inter_core_tx_irq and inter_core_rx_irq are
 * allowed to have the same value since they are configured by the driver as
 * directed interrupts. If the value IPC_CORE_DEFAULT is passed as remote_core,
 * the default value defined for the selected platform will be used instead.
 *
 * Return: 0 for success, -EINVAL for either inter core interrupt invalid or
 *         invalid remote core, -ENOMEM for failing to map MSCM address space
 */
int ipc_hw_init(const struct ipc_shm_cfg *cfg)
{
	/* map MSCM hardware peripheral block registers */
	priv.mscm = (struct mscm_regs *)ipc_os_map_intc();
	if (!priv.mscm) {
		return -ENOMEM;
	}

	switch (cfg->remote_core.type) {
	case IPC_CORE_A53:
		switch (cfg->remote_core.index) {
		case 0:
			priv.remote_core = A53_0;
			break;
		case 1:
			priv.remote_core = A53_1;
			break;
		case 2:
			priv.remote_core = A53_2;
			break;
		case 3:
			priv.remote_core = A53_3;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IPC_CORE_M7:
		switch (cfg->remote_core.index) {
		case 0:
			priv.remote_core = M7_0;
			break;
		case 1:
			priv.remote_core = M7_1;
			break;
		case 2:
			priv.remote_core = M7_2;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IPC_CORE_DEFAULT:
		priv.remote_core = DEFAULT_REMOTE_CORE;
		break;
	default:
		return -EINVAL;
	}

	if (cfg->inter_core_tx_irq != IPC_DEFAULT_INTER_CORE_IRQ) {
		priv.mscm_tx_irq = cfg->inter_core_tx_irq;
	} else {
		priv.mscm_tx_irq = DEFAULT_MSCM_IRQ_ID;
	}

	if (cfg->inter_core_rx_irq != IPC_DEFAULT_INTER_CORE_IRQ) {
		priv.mscm_rx_irq = cfg->inter_core_rx_irq;
	} else {
		priv.mscm_rx_irq = DEFAULT_MSCM_IRQ_ID;
	}

	if (priv.mscm_tx_irq < 0 || priv.mscm_tx_irq > 2
		|| priv.mscm_rx_irq < 0 || priv.mscm_rx_irq > 2
		|| priv.remote_core == readl_relaxed(&priv.mscm->cpxnum)) {
		return -EINVAL;
	}

	return 0;
}

/**
 * ipc_hw_free() - unmap MSCM IP block and clear irq
 */
void ipc_hw_free(void)
{
	ipc_hw_irq_clear();

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
void ipc_hw_irq_enable(void)
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
void ipc_hw_irq_disable(void)
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
void ipc_hw_irq_notify(void)
{
	/* trigger MSCM core-to-core directed interrupt */
	switch (priv.remote_core) {
	case A53_0:
		switch (priv.mscm_tx_irq) {
		case 0:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp0igr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp0igr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp0igr2);
			break;
		default:
			return;
		}
		break;
	case A53_1:
		switch (priv.mscm_tx_irq) {
		case 0:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp1igr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp1igr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp1igr2);
			break;
		default:
			return;
		}
		break;
	case A53_2:
		switch (priv.mscm_tx_irq) {
		case 0:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp2igr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp2igr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp2igr2);
			break;
		default:
			return;
		}
		break;
	case A53_3:
		switch (priv.mscm_tx_irq) {
		case 0:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp3igr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp3igr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp3igr2);
			break;
		default:
			return;
		}
		break;
	case M7_0:
		switch (priv.mscm_tx_irq) {
		case 0:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp4igr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp4igr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp4igr2);
			break;
		default:
			return;
		}
		break;
	case M7_1:
		switch (priv.mscm_tx_irq) {
		case 0:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp5igr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp5igr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp5igr2);
			break;
		default:
			return;
		}
		break;
	case M7_2:
		switch (priv.mscm_tx_irq) {
		case 0:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp6igr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp6igr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN,
					&priv.mscm->ircp6igr2);
			break;
		default:
			return;
		}
		break;
	default:
		return;
	}
}

/**
 * ipc_hw_irq_clear() - clear available data notification
 */
void ipc_hw_irq_clear(void)
{
	int current_cpu;

	/* get current processor id */
	current_cpu = readl_relaxed(&priv.mscm->cpxnum);

	/* clear MSCM core-to-core directed interrupt */
	switch (current_cpu) {
	case A53_0:
		switch (priv.mscm_rx_irq) {
		case 0:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
					&priv.mscm->ircp0isr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
					&priv.mscm->ircp0isr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
					&priv.mscm->ircp0isr2);
			break;
		default:
			return;
		}
		break;
	case A53_1:
		switch (priv.mscm_rx_irq) {
		case 0:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
					&priv.mscm->ircp1isr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
					&priv.mscm->ircp1isr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
					&priv.mscm->ircp1isr2);
			break;
		default:
			return;
		}
		break;
	case A53_2:
		switch (priv.mscm_rx_irq) {
		case 0:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
					&priv.mscm->ircp2isr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
					&priv.mscm->ircp2isr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
					&priv.mscm->ircp2isr2);
			break;
		default:
			return;
		}
		break;
	case A53_3:
		switch (priv.mscm_rx_irq) {
		case 0:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
					&priv.mscm->ircp3isr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
					&priv.mscm->ircp3isr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
					&priv.mscm->ircp3isr2);
			break;
		default:
			return;
		}
		break;
	default:
		return;
	}
}
