/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2018 NXP Semiconductors
 */
#include <linux/types.h>
#include <linux/module.h>
#include <linux/io.h>

#include "ipc-hw.h"

/* Hardware IP Block Base Addresses - TODO: get them from device tree */
#define MSCM_BASE    0x40198000ul /* Miscellaneous System Control Module */

/* S32gen1 Specific Definitions */
#define DEFAULT_SHM_IRQ_ID    2u /* MSCM irq 2 = GIC irq 35 */
#define DEFAULT_REMOTE_CPU    4u /* M7 core 0 */

/* Device tree MSCM node: compatible property (search key) */
#define DT_MSCM_NODE_COMP "fsl,s32gen1-mscm"

/* S32 Processor IDs */
enum processor_idx {
	A53_0 = 0, /* ARM Cortex-A53 core 0 */
	A53_1 = 1, /* ARM Cortex-A53 core 1 */
	A53_2 = 2, /* ARM Cortex-A53 core 2 */
	A53_3 = 3, /* ARM Cortex-A53 core 3 */
	M7_0 = 4, /* ARM Cortex-M7 core 0 */
	M7_1 = 5, /* ARM Cortex-M7 core 1 */
	M7_2 = 6, /* ARM Cortex-M7 core 2 */
};

/**
 * struct mscm_regs - MSCM Peripheral Register Structure
 *
 * @CPXTYPE:        Processor x Type Register, offset 0x0
 * @CPXNUM:         Processor x Number Register, offset 0x4
 * @CPXREV:         Processor x Revision Register, offset 0x8
 * @CPXCFG[n]:      Processor x Configuration n Register, offset 0xC + 4*n
 * @CP0TYPE:        Processor 0 Type Register, offset 0x20
 * @CP0NUM:         Processor 0 Number Register, offset 0x24
 * @CP0REV:         Processor 0 Revision Register, offset 0x28
 * @CP0CFG[n]:      Processor 0 Configuration n Register, offset 0x2C + 4*n
 * @CP1TYPE:        Processor 1 Type Register, offset 0x40
 * @CP1NUM:         Processor 1 Number Register, offset 0x44
 * @CP1REV:         Processor 1 Revision Register, offset 0x48
 * @CP1CFG[n]:      Processor 1 Configuration n Register, offset 0x4C + 4*n
 * @CP2TYPE:        Processor 2 Type Register, offset 0x60
 * @CP2NUM:         Processor 2 Number Register, offset 0x64
 * @CP2REV:         Processor 2 Revision Register, offset 0x68
 * @CP2CFG[n]:      Processor 2 Configuration n Register, offset 0x6C + 4*n
 * @CP3TYPE:        Processor 3 Type Register, offset 0x80
 * @CP3NUM:         Processor 3 Number Register, offset 0x84
 * @CP3REV:         Processor 3 Revision Register, offset 0x88
 * @CP3CFG[n]:      Processor 3 Configuration n Register, offset 0x8C + 4*n
 * @CP4TYPE:        Processor 4 Type Register, offset 0xA0
 * @CP4NUM:         Processor 4 Number Register, offset 0xA4
 * @CP4REV:         Processor 4 Revision Register, offset 0xA8
 * @CP4CFG[n]:      Processor 4 Configuration n Register, offset 0xAC + 4*n
 * @CP5TYPE:        Processor 5 Type Register, offset 0xC0
 * @CP5NUM:         Processor 5 Number Register, offset 0xC4
 * @CP5REV:         Processor 5 Revision Register, offset 0xC8
 * @CP5CFG[n]:      Processor 5 Configuration n Register, offset 0xCC + 4*n
 * @CP6TYPE:        Processor 6 Type Register, offset 0xE0
 * @CP6NUM:         Processor 6 Number Register, offset 0xE4
 * @CP6REV:         Processor 6 Revision Register, offset 0xE8
 * @CP6CFG[n]:      Processor 6 Configuration n Register, offset 0xEC + 4*n
 * @IRCP0ISR0:      Interrupt Router CP0 Interrupt0 Status Register,
 *                  offset 0x200
 * @IRCP0IGR0:      Interrupt Router CP0 Interrupt0 Generation Register,
 *                  offset 0x204
 * @IRCP0ISR1:      Interrupt Router CP0 Interrupt1 Status Register,
 *                  offset 0x208
 * @IRCP0IGR1:      Interrupt Router CP0 Interrupt1 Generation Register,
 *                  offset 0x20C
 * @IRCP0ISR2:      Interrupt Router CP0 Interrupt2 Status Register,
 *                  offset 0x210
 * @IRCP0IGR2:      Interrupt Router CP0 Interrupt2 Generation Register,
 *                  offset 0x214
 * @IRCP0ISR3:      Interrupt Router CP0 InterruptX Status Register,
 *                  offset 0x218
 * @IRCP0IGR3:      Interrupt Router CP0 InterruptX Generation Register,
 *                  offset 0x21C
 * @IRCP1ISR0:      Interrupt Router CP1 Interrupt0 Status Register,
 *                  offset 0x220
 * @IRCP1IGR0:      Interrupt Router CP1 Interrupt0 Generation Register,
 *                  offset 0x224
 * @IRCP1ISR1:      Interrupt Router CP1 Interrupt1 Status Register,
 *                  offset 0x228
 * @IRCP1IGR1:      Interrupt Router CP1 Interrupt1 Generation Register,
 *                  offset 0x22C
 * @IRCP1ISR2:      Interrupt Router CP1 Interrupt2 Status Register,
 *                  offset 0x230
 * @IRCP1IGR2:      Interrupt Router CP1 Interrupt2 Generation Register,
 *                  offset 0x234
 * @IRCP1ISR3:      Interrupt Router CP1 InterruptX Status Register,
 *                  offset 0x238
 * @IRCP1IGR3:      Interrupt Router CP1 InterruptX Generation Register,
 *                  offset 0x23C
 * @IRCP2ISR0:      Interrupt Router CP2 Interrupt0 Status Register,
 *                  offset 0x240
 * @IRCP2IGR0:      Interrupt Router CP2 Interrupt0 Generation Register,
 *                  offset 0x244
 * @IRCP2ISR1:      Interrupt Router CP2 Interrupt1 Status Register,
 *                  offset 0x248
 * @IRCP2IGR1:      Interrupt Router CP2 Interrupt1 Generation Register,
 *                  offset 0x24C
 * @IRCP2ISR2:      Interrupt Router CP2 Interrupt2 Status Register,
 *                  offset 0x250
 * @IRCP2IGR2:      Interrupt Router CP2 Interrupt2 Generation Register,
 *                  offset 0x254
 * @IRCP2ISR3:      Interrupt Router CP2 InterruptX Status Register,
 *                  offset 0x258
 * @IRCP2IGR3:      Interrupt Router CP2 InterruptX Generation Register,
 *                  offset 0x25C
 * @IRCP3ISR0:      Interrupt Router CP3 Interrupt0 Status Register,
 *                  offset 0x260
 * @IRCP3IGR0:      Interrupt Router CP3 Interrupt0 Generation Register,
 *                  offset 0x264
 * @IRCP3ISR1:      Interrupt Router CP3 Interrupt1 Status Register,
 *                  offset 0x268
 * @IRCP3IGR1:      Interrupt Router CP3 Interrupt1 Generation Register,
 *                  offset 0x26C
 * @IRCP3ISR2:      Interrupt Router CP3 Interrupt2 Status Register,
 *                  offset 0x270
 * @IRCP3IGR2:      Interrupt Router CP3 Interrupt2 Generation Register,
 *                  offset 0x274
 * @IRCP3ISR3:      Interrupt Router CP3 InterruptX Status Register,
 *                  offset 0x278
 * @IRCP3IGR3:      Interrupt Router CP3 InterruptX Generation Register,
 *                  offset 0x27C
 * @IRCP4ISR0:      Interrupt Router CP4 Interrupt0 Status Register,
 *                  offset 0x280
 * @IRCP4IGR0:      Interrupt Router CP4 Interrupt0 Generation Register,
 *                  offset 0x284
 * @IRCP4ISR1:      Interrupt Router CP4 Interrupt1 Status Register,
 *                  offset 0x288
 * @IRCP4IGR1:      Interrupt Router CP4 Interrupt1 Generation Register,
 *                  offset 0x28C
 * @IRCP4ISR2:      Interrupt Router CP4 Interrupt2 Status Register,
 *                  offset 0x290
 * @IRCP4IGR2:      Interrupt Router CP4 Interrupt2 Generation Register,
 *                  offset 0x294
 * @IRCP4ISR3:      Interrupt Router CP4 InterruptX Status Register,
 *                  offset 0x298
 * @IRCP4IGR3:      Interrupt Router CP4 InterruptX Generation Register,
 *                  offset 0x29C
 * @IRCP5ISR0:      Interrupt Router CP5 Interrupt0 Status Register,
 *                  offset 0x2A0
 * @IRCP5IGR0:      Interrupt Router CP5 Interrupt0 Generation Register,
 *                  offset 0x2A4
 * @IRCP5ISR1:      Interrupt Router CP5 Interrupt1 Status Register,
 *                  offset 0x2A8
 * @IRCP5IGR1:      Interrupt Router CP5 Interrupt1 Generation Register,
 *                  offset 0x2AC
 * @IRCP5ISR2:      Interrupt Router CP5 Interrupt2 Status Register,
 *                  offset 0x2B0
 * @IRCP5IGR2:      Interrupt Router CP5 Interrupt2 Generation Register,
 *                  offset 0x2B4
 * @IRCP5ISR3:      Interrupt Router CP5 InterruptX Status Register,
 *                  offset 0x2B8
 * @IRCP5IGR3:      Interrupt Router CP5 InterruptX Generation Register,
 *                  offset 0x2BC
 * @IRCP6ISR0:      Interrupt Router CP6 Interrupt0 Status Register,
 *                  offset 0x2C0
 * @IRCP6IGR0:      Interrupt Router CP6 Interrupt0 Generation Register,
 *                  offset 0x2C4
 * @IRCP6ISR1:      Interrupt Router CP6 Interrupt1 Status Register,
 *                  offset 0x2C8
 * @IRCP6IGR1:      Interrupt Router CP6 Interrupt1 Generation Register,
 *                  offset 0x2CC
 * @IRCP6ISR2:      Interrupt Router CP6 Interrupt2 Status Register,
 *                  offset 0x2D0
 * @IRCP6IGR2:      Interrupt Router CP6 Interrupt2 Generation Register,
 *                  offset 0x2D4
 * @IRCP6ISR3:      Interrupt Router CP6 InterruptX Status Register,
 *                  offset 0x2D8
 * @IRCP6IGR3:      Interrupt Router CP6 InterruptX Generation Register,
 *                  offset 0x2DC
 * @IRCPCFG:        Interrupt Router Configuration Register,
 *                  offset 0x400
 * @IRNMIC:         Interrupt Router Non-Maskable Interrupt Control Register,
 *                  offset 0x800
 * @IRSPRC[n]:      Interrupt Router Shared Peripheral Routing Control
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

#define MSCM_IRCPnISR3_PCIE_INT0    0x0001ul /* PCIe-to-CPn Interrupt 0 Mask */
#define MSCM_IRCPnISR3_PCIE_INT1    0x0002ul /* PCIe-to-CPn Interrupt 1 Mask */
#define MSCM_IRCPnISR3_PCIE_INT2    0x0004ul /* PCIe-to-CPn Interrupt 2 Mask */
#define MSCM_IRCPnISR3_PCIE_INT3    0x0008ul /* PCIe-to-CPn Interrupt 3 Mask */
#define MSCM_IRCPnISR3_PCIE_INT4    0x0010ul /* PCIe-to-CPn Interrupt 4 Mask */
#define MSCM_IRCPnISR3_PCIE_INT5    0x0020ul /* PCIe-to-CPn Interrupt 5 Mask */
#define MSCM_IRCPnISR3_PCIE_INT6    0x0040ul /* PCIe-to-CPn Interrupt 6 Mask */
#define MSCM_IRCPnISR3_PCIE_INT7    0x0080ul /* PCIe-to-CPn Interrupt 7 Mask */
#define MSCM_IRCPnISR3_PCIE_INT8    0x0100ul /* PCIe-to-CPn Interrupt 8 Mask */
#define MSCM_IRCPnISR3_PCIE_INT9    0x0200ul /* PCIe-to-CPn Interrupt 9 Mask */
#define MSCM_IRCPnISR3_PCIE_INT10   0x0400ul /* PCIe-to-CPn Interrupt 10 Mask */
#define MSCM_IRCPnISR3_PCIE_INT11   0x0800ul /* PCIe-to-CPn Interrupt 11 Mask */
#define MSCM_IRCPnISR3_PCIE_INT12   0x1000ul /* PCIe-to-CPn Interrupt 12 Mask */
#define MSCM_IRCPnISR3_PCIE_INT13   0x2000ul /* PCIe-to-CPn Interrupt 13 Mask */
#define MSCM_IRCPnISR3_PCIE_INT14   0x4000ul /* PCIe-to-CPn Interrupt 14 Mask */
#define MSCM_IRCPnISR3_PCIE_INT15   0x8000ul /* PCIe-to-CPn Interrupt 15 Mask */

#define MSCM_IRCPCFG_LOCK    0x80000000ul /* Interrupt Router Config Lock Bit */

#define MSCM_IRCPCFG_CP0_TR    0x01ul /* CP0 Trusted Core Mask */
#define MSCM_IRCPCFG_CP1_TR    0x02ul /* CP1 Trusted Core Mask */
#define MSCM_IRCPCFG_CP2_TR    0x04ul /* CP2 Trusted Core Mask */
#define MSCM_IRCPCFG_CP3_TR    0x08ul /* CP3 Trusted Core Mask */
#define MSCM_IRCPCFG_CP4_TR    0x10ul /* CP4 Trusted Core Mask */
#define MSCM_IRCPCFG_CP5_TR    0x20ul /* CP5 Trusted Core Mask */
#define MSCM_IRCPCFG_CP6_TR    0x40ul /* CP6 Trusted Core Mask */

#define MSCM_IRNMIC_CP0_NMI_EN    0x01ul /* CP0 NMI Interrupt Steering Enable */
#define MSCM_IRNMIC_CP1_NMI_EN    0x02ul /* CP1 NMI Interrupt Steering Enable */
#define MSCM_IRNMIC_CP2_NMI_EN    0x04ul /* CP2 NMI Interrupt Steering Enable */
#define MSCM_IRNMIC_CP3_NMI_EN    0x08ul /* CP3 NMI Interrupt Steering Enable */
#define MSCM_IRNMIC_CP4_NMI_EN    0x10ul /* CP4 NMI Interrupt Steering Enable */
#define MSCM_IRNMIC_CP5_NMI_EN    0x20ul /* CP5 NMI Interrupt Steering Enable */
#define MSCM_IRNMIC_CP6_NMI_EN    0x40ul /* CP6 NMI Interrupt Steering Enable */

#define MSCM_IRSPRCn_LOCK    0x8000u /* Interrupt Routing Control Lock Bit */

#define MSCM_IRSPRCn_GIC500    0x1u /* GIC500 Interrupt Steering Enable */

#define MSCM_IRSPRCn_M7_0    0x2u /* CM7 Core 0 Interrupt Steering Enable */
#define MSCM_IRSPRCn_M7_1    0x4u /* CM7 Core 1 Interrupt Steering Enable */
#define MSCM_IRSPRCn_M7_2    0x8u /* CM7 Core 2 Interrupt Steering Enable */

/* S32 Platform Specific Implementation  */

/* pointer to memory-mapped hardware peripheral MSCM */
static struct mscm_regs *mscm;

/**
 * ipc_hw_get_dt_comp() - device tree compatible getter
 *
 * Return: MSCM compatible value string for current platform
 */
char *ipc_hw_get_dt_comp(void)
{
	return DT_MSCM_NODE_COMP;
}

/**
 * ipc_hw_get_dt_irq() - device tree compatible getter
 *
 * @shm_irq_id:     MSCM inter-core interrupt ID reserved for shm driver
 *
 * If the value PLAFORM_DEFAULT is passed as parameter, the default
 * value defined for the selected platform will be used instead.
 *
 * Return: device tree index of the MSCM interrupt used, -1 for invalid irq
 */
int ipc_hw_get_dt_irq(int shm_irq_id)
{
	/* assign default value to interrupt ID if so indicated */
	if (shm_irq_id == PLATFORM_DEFAULT) {
		shm_irq_id = DEFAULT_SHM_IRQ_ID;
	}

	if (shm_irq_id < 0 || shm_irq_id > 2) {
		shm_irq_id = -1;
	}

	return shm_irq_id;
}

/**
 * ipc_hw_init() - map MSCM IP block to proper address
 *
 * Return: 0 for success, ENOMEM for invalid interrupt ID
 */
int ipc_hw_init(void)
{
	/* map MSCM hardware peripheral block */
	mscm = (struct mscm_regs *) ioremap_nocache(
		(phys_addr_t)MSCM_BASE, sizeof(struct mscm_regs));

	if (!mscm) {
		return -ENOMEM;
	}

	return 0;
}

/**
 * ipc_hw_free() - unmap MSCM IP block
 */
void ipc_hw_free(void)
{
	/* unmap MSCM hardware peripheral block */
	iounmap(mscm);
}

/**
 * ipc_hw_irq_enable() - enable notifications from remote
 *
 * @shm_irq_id:     MSCM inter-core interrupt ID reserved for shm driver
 *
 * If the value PLAFORM_DEFAULT is passed as parameter, the default
 * value defined for the selected platform will be used instead.
 *
 * The MSCM_IRSPRCn register works with the NVIC interrupt IDs, and the NVIC ID
 * of the first MSCM inter-core interrupt is 1. In order to obtain the correct
 * index for the interrupt routing register, this value is added to shm_irq_id.
 *
 * Return: 0 for success, -EINVAL for invalid interrupt ID
 */
int ipc_hw_irq_enable(int shm_irq_id)
{
	uint16_t irsprc_mask;

	/* assign default value to interrupt ID if so indicated */
	if (shm_irq_id == PLATFORM_DEFAULT) {
		shm_irq_id = DEFAULT_SHM_IRQ_ID;
	}

	if (shm_irq_id < 0 || shm_irq_id > 2) {
		return -EINVAL;
	}

	/* enable MSCM core-to-core interrupt routing */
	irsprc_mask = readw_relaxed(&mscm->irsprc[shm_irq_id + 1]);
	writew_relaxed(irsprc_mask | MSCM_IRSPRCn_GIC500,
			&mscm->irsprc[shm_irq_id + 1]);

	return 0;
}

/**
 * ipc_hw_irq_disable() - disable notifications from remote
 *
 * @shm_irq_id:     MSCM inter-core interrupt ID reserved for shm driver
 *
 * If the value PLAFORM_DEFAULT is passed as parameter, the default
 * value defined for the selected platform will be used instead.
 *
 * The MSCM_IRSPRCn register works with the NVIC interrupt IDs, and the NVIC ID
 * of the first MSCM inter-core interrupt is 1. In order to obtain the correct
 * index for the interrupt routing register, this value is added to shm_irq_id.
 *
 * Return: 0 for success, -EINVAL for invalid interrupt ID
 */
int ipc_hw_irq_disable(int shm_irq_id)
{
	uint16_t irsprc_mask;

	/* assign default value to interrupt ID if so indicated */
	if (shm_irq_id == PLATFORM_DEFAULT) {
		shm_irq_id = DEFAULT_SHM_IRQ_ID;
	}

	if (shm_irq_id < 0 || shm_irq_id > 2) {
		return -EINVAL;
	}

	/* disable MSCM core-to-core interrupt routing */
	irsprc_mask = readw_relaxed(&mscm->irsprc[shm_irq_id + 1]);
	writew_relaxed(irsprc_mask & ~MSCM_IRSPRCn_GIC500,
			&mscm->irsprc[shm_irq_id + 1]);

	return 0;
}

/**
 * ipc_hw_irq_notify() - notify remote that data is available
 *
 * @shm_irq_id:     MSCM inter-core interrupt ID reserved for shm driver
 * @remote_cpu:     ID of the remote core to trigger the interrupt on
 *
 * If the value PLAFORM_DEFAULT is passed as either parameter, the default
 * value defined for the selected platform will be used instead.
 *
 * Return: 0 for success, -EINVAL for invalid interrupt or remote processor ID
 */
int ipc_hw_irq_notify(int shm_irq_id, int remote_cpu)
{
	int current_cpu;

	/* assign default value to interrupt and processor ID if so indicated */
	if (shm_irq_id == PLATFORM_DEFAULT) {
		shm_irq_id = DEFAULT_SHM_IRQ_ID;
	}
	if (remote_cpu == PLATFORM_DEFAULT) {
		remote_cpu = DEFAULT_REMOTE_CPU;
	}

	/* get current processor id */
	current_cpu = readl_relaxed(&mscm->cpxnum);

	/* trigger MSCM core-to-core interrupt */
	if (remote_cpu == current_cpu) {
		return -EINVAL;
	}

	switch (remote_cpu) {
	case A53_0:
		switch (shm_irq_id) {
		case 0:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp0igr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp0igr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp0igr2);
			break;
		default:
			return -EINVAL;
		}
		break;
	case A53_1:
		switch (shm_irq_id) {
		case 0:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp1igr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp1igr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp1igr2);
			break;
		default:
			return -EINVAL;
		}
		break;
	case A53_2:
		switch (shm_irq_id) {
		case 0:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp2igr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp2igr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp2igr2);
			break;
		default:
			return -EINVAL;
		}
		break;
	case A53_3:
		switch (shm_irq_id) {
		case 0:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp3igr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp3igr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp3igr2);
			break;
		default:
			return -EINVAL;
		}
		break;
	case M7_0:
		switch (shm_irq_id) {
		case 0:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp4igr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp4igr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp4igr2);
			break;
		default:
			return -EINVAL;
		}
		break;
	case M7_1:
		switch (shm_irq_id) {
		case 0:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp5igr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp5igr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp5igr2);
			break;
		default:
			return -EINVAL;
		}
		break;
	case M7_2:
		switch (shm_irq_id) {
		case 0:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp6igr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp6igr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnIGRn_INT_EN, &mscm->ircp6igr2);
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * ipc_hw_irq_clear() - clear available data notification
 *
 * @shm_irq_id:     MSCM inter-core interrupt ID reserved for shm driver
 *
 * If the value PLAFORM_DEFAULT is passed as parameter, the default
 * value defined for the selected platform will be used instead.
 *
 * Return: 0 for success, -EINVAL for invalid interrupt ID
 */
int ipc_hw_irq_clear(int shm_irq_id)
{
	int current_cpu;

	/* assign default value to interrupt ID if so indicated */
	if (shm_irq_id == PLATFORM_DEFAULT) {
		shm_irq_id = DEFAULT_SHM_IRQ_ID;
	}

	/* get current processor id */
	current_cpu = readl_relaxed(&mscm->cpxnum);

	/* clear MSCM core-to-core interrupt */
	switch (current_cpu) {
	case A53_0:
		switch (shm_irq_id) {
		case 0:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
				       &mscm->ircp0isr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
				       &mscm->ircp0isr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
				       &mscm->ircp0isr2);
			break;
		default:
			return -EINVAL;
		}
		break;
	case A53_1:
		switch (shm_irq_id) {
		case 0:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
				       &mscm->ircp1isr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
				       &mscm->ircp1isr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
				       &mscm->ircp1isr2);
			break;
		default:
			return -EINVAL;
		}
		break;
	case A53_2:
		switch (shm_irq_id) {
		case 0:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
				       &mscm->ircp2isr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
				       &mscm->ircp2isr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
				       &mscm->ircp2isr2);
			break;
		default:
			return -EINVAL;
		}
		break;
	case A53_3:
		switch (shm_irq_id) {
		case 0:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
				       &mscm->ircp3isr0);
			break;
		case 1:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
				       &mscm->ircp3isr1);
			break;
		case 2:
			writel_relaxed(MSCM_IRCPnISRn_CPx_INT,
				       &mscm->ircp3isr2);
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
