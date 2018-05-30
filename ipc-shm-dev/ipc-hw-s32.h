/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2018 NXP Semiconductors
 */
#ifndef IPC_HW_S32_H_
#define IPC_HW_S32_H_

/* MSCM CPU2CPU Interrupt ID (there are 3 CPU2CPU interrupts on S32G) */
#define MSCM_IRQ_ID    0 /* GIC interrupt 33 */

/* Device tree MSCM node: compatible property (search key) */
#define DT_MSCM_NODE_COMP "fsl,s32gen1-mscm"

/*
 * Device tree MSCM CPU2CPU interrupt index.
 * DT intr index is offset by 1 from MSCM_IRQ_ID because the first interrupt in
 * MSCM is PCIe1 MSI interrupt.
 */
#define DT_MSCM_CPU2CPU_INTR (MSCM_IRQ_ID + 1)

/* MSCM Peripheral Register Structure */
struct mscm_memmap {
	volatile const uint32_t cpxtype; /* Processor x Type Register,
					  * offset 0x0 */
	volatile const uint32_t cpxnum; /* Processor x Number Register,
					 * offset 0x4 */
	volatile const uint32_t cpxrev; /* Processor x Revision Register,
					 * offset 0x8 */
	volatile const uint32_t cpxcfg[4]; /* Processor x Configuration n
					    * Register, offset 0xC + 4*n */
	uint8_t reserved00[4];
	volatile const uint32_t cp0type; /* Processor 0 Type Register,
					  * offset 0x20 */
	volatile const uint32_t cp0num; /* Processor 0 Number Register,
					 * offset 0x24 */
	volatile const uint32_t cp0rev; /* Processor 0 Revision Register,
					 * offset 0x28 */
	volatile const uint32_t cp0cfg[4]; /* Processor 0 Configuration n
					    * Register, offset 0x2C + 4*n */
	uint8_t reserved01[4];
	volatile const uint32_t cp1type; /* Processor 1 Type Register,
					  * offset 0x40 */
	volatile const uint32_t cp1num; /* Processor 1 Number Register,
					 * offset 0x44 */
	volatile const uint32_t cp1rev; /* Processor 1 Revision Register,
					 * offset 0x48 */
	volatile const uint32_t cp1cfg[4]; /* Processor 1 Configuration n
					    * Register, offset 0x4C + 4*n */
	uint8_t reserved02[4];
	volatile const uint32_t cp2type; /* Processor 2 Type Register,
					  * offset 0x60 */
	volatile const uint32_t cp2num; /* Processor 2 Number Register,
					 * offset 0x64 */
	volatile const uint32_t cp2rev; /* Processor 2 Revision Register,
					 * offset 0x68 */
	volatile const uint32_t cp2cfg[4]; /* Processor 2 Configuration n
					    * Register, offset 0x6C + 4*n */
	uint8_t reserved03[4];
	volatile const uint32_t cp3type; /* Processor 3 Type Register,
					  * offset 0x80 */
	volatile const uint32_t cp3num; /* Processor 3 Number Register,
					 * offset 0x84 */
	volatile const uint32_t cp3rev; /* Processor 3 Revision Register,
					 * offset 0x88 */
	volatile const uint32_t cp3cfg[4]; /* Processor 3 Configuration n
					    * Register, offset 0x8C + 4*n */
	uint8_t reserved04[4];
	volatile const uint32_t cp4type; /* Processor 4 Type Register,
					  * offset 0xA0 */
	volatile const uint32_t cp4num; /* Processor 4 Number Register,
					 * offset 0xA4 */
	volatile const uint32_t cp4rev; /* Processor 4 Revision Register,
					 * offset 0xA8 */
	volatile const uint32_t cp4cfg[4]; /* Processor 4 Configuration n
					    * Register, offset 0xAC + 4*n */
	uint8_t reserved05[4];
	volatile const uint32_t cp5type; /* Processor 5 Type Register,
					  * offset 0xC0 */
	volatile const uint32_t cp5num; /* Processor 5 Number Register,
					 * offset 0xC4 */
	volatile const uint32_t cp5rev; /* Processor 5 Revision Register,
					 * offset 0xC8 */
	volatile const uint32_t cp5cfg[4]; /* Processor 5 Configuration n
					    * Register, offset 0xCC + 4*n */
	uint8_t reserved06[4];
	volatile const uint32_t cp6type; /* Processor 6 Type Register,
					  * offset 0xE0 */
	volatile const uint32_t cp6num; /* Processor 6 Number Register,
					 * offset 0xE4 */
	volatile const uint32_t cp6rev; /* Processor 6 Revision Register,
					 * offset 0xE8 */
	volatile const uint32_t cp6cfg[4]; /* Processor 6 Configuration n
					    * Register, offset 0xEC + 4*n */
	uint8_t reserved07[260];
	volatile uint32_t ircp0isr0; /* Interrupt Router CP0 Interrupt0 Status
				      * Register, offset 0x200 */
	volatile uint32_t ircp0igr0; /* Interrupt Router CP0 Interrupt0
				      * Generation Register, offset 0x204 */
	volatile uint32_t ircp0isr1; /* Interrupt Router CP0 Interrupt1 Status
				      * Register, offset 0x208 */
	volatile uint32_t ircp0igr1; /* Interrupt Router CP0 Interrupt1
				      * Generation Register, offset 0x20C */
	volatile uint32_t ircp0isr2; /* Interrupt Router CP0 Interrupt2 Status
				      * Register, offset 0x210 */
	volatile uint32_t ircp0igr2; /* Interrupt Router CP0 Interrupt2
				      * Generation Register, offset 0x214 */
	volatile uint32_t ircp0isr3; /* Interrupt Router CP0 InterruptX Status
				      * Register, offset 0x218 */
	volatile uint32_t ircp0igr3; /* Interrupt Router CP0 InterruptX
				      * Generation Register, offset 0x21C */
	volatile uint32_t ircp1isr0; /* Interrupt Router CP1 Interrupt0 Status
				      * Register, offset 0x220 */
	volatile uint32_t ircp1igr0; /* Interrupt Router CP1 Interrupt0
				      * Generation Register, offset 0x224 */
	volatile uint32_t ircp1isr1; /* Interrupt Router CP1 Interrupt1 Status
				      * Register, offset 0x228 */
	volatile uint32_t ircp1igr1; /* Interrupt Router CP1 Interrupt1
				      * Generation Register, offset 0x22C */
	volatile uint32_t ircp1isr2; /* Interrupt Router CP1 Interrupt2 Status
				      * Register, offset 0x230 */
	volatile uint32_t ircp1igr2; /* Interrupt Router CP1 Interrupt2
				      * Generation Register, offset 0x234 */
	volatile uint32_t ircp1isr3; /* Interrupt Router CP1 InterruptX Status
				      * Register, offset 0x238 */
	volatile uint32_t ircp1igr3; /* Interrupt Router CP1 InterruptX
				      * Generation Register, offset 0x23C */
	volatile uint32_t ircp2isr0; /* Interrupt Router CP2 Interrupt0 Status
				      * Register, offset 0x240 */
	volatile uint32_t ircp2igr0; /* Interrupt Router CP2 Interrupt0
				      * Generation Register, offset 0x244 */
	volatile uint32_t ircp2isr1; /* Interrupt Router CP2 Interrupt1 Status
				      * Register, offset 0x248 */
	volatile uint32_t ircp2igr1; /* Interrupt Router CP2 Interrupt1
				      * Generation Register, offset 0x24C */
	volatile uint32_t ircp2isr2; /* Interrupt Router CP2 Interrupt2 Status
				      * Register, offset 0x250 */
	volatile uint32_t ircp2igr2; /* Interrupt Router CP2 Interrupt2
				      * Generation Register, offset 0x254 */
	volatile uint32_t ircp2isr3; /* Interrupt Router CP2 InterruptX Status
				      * Register, offset 0x258 */
	volatile uint32_t ircp2igr3; /* Interrupt Router CP2 InterruptX
				      * Generation Register, offset 0x25C */
	volatile uint32_t ircp3isr0; /* Interrupt Router CP3 Interrupt0 Status
				      * Register, offset 0x260 */
	volatile uint32_t ircp3igr0; /* Interrupt Router CP3 Interrupt0
				      * Generation Register, offset 0x264 */
	volatile uint32_t ircp3isr1; /* Interrupt Router CP3 Interrupt1 Status
				      * Register, offset 0x268 */
	volatile uint32_t ircp3igr1; /* Interrupt Router CP3 Interrupt1
				      * Generation Register, offset 0x26C */
	volatile uint32_t ircp3isr2; /* Interrupt Router CP3 Interrupt2 Status
				      * Register, offset 0x270 */
	volatile uint32_t ircp3igr2; /* Interrupt Router CP3 Interrupt2
				      * Generation Register, offset 0x274 */
	volatile uint32_t ircp3isr3; /* Interrupt Router CP3 InterruptX Status
				      * Register, offset 0x278 */
	volatile uint32_t ircp3igr3; /* Interrupt Router CP3 InterruptX
				      * Generation Register, offset 0x27C */
	volatile uint32_t ircp4isr0; /* Interrupt Router CP4 Interrupt0 Status
				      * Register, offset 0x280 */
	volatile uint32_t ircp4igr0; /* Interrupt Router CP4 Interrupt0
				      * Generation Register, offset 0x284 */
	volatile uint32_t ircp4isr1; /* Interrupt Router CP4 Interrupt1 Status
				      * Register, offset 0x288 */
	volatile uint32_t ircp4igr1; /* Interrupt Router CP4 Interrupt1
				      * Generation Register, offset 0x28C */
	volatile uint32_t ircp4isr2; /* Interrupt Router CP4 Interrupt2 Status
				      * Register, offset 0x290 */
	volatile uint32_t ircp4igr2; /* Interrupt Router CP4 Interrupt2
				      * Generation Register, offset 0x294 */
	volatile uint32_t ircp4isr3; /* Interrupt Router CP4 InterruptX Status
				      * Register, offset 0x298 */
	volatile uint32_t ircp4igr3; /* Interrupt Router CP4 InterruptX
				      * Generation Register, offset 0x29C */
	volatile uint32_t ircp5isr0; /* Interrupt Router CP5 Interrupt0 Status
				      * Register, offset 0x2A0 */
	volatile uint32_t ircp5igr0; /* Interrupt Router CP5 Interrupt0
				      * Generation Register, offset 0x2A4 */
	volatile uint32_t ircp5isr1; /* Interrupt Router CP5 Interrupt1 Status
				      * Register, offset 0x2A8 */
	volatile uint32_t ircp5igr1; /* Interrupt Router CP5 Interrupt1
				      * Generation Register, offset 0x2AC */
	volatile uint32_t ircp5isr2; /* Interrupt Router CP5 Interrupt2 Status
				      * Register, offset 0x2B0 */
	volatile uint32_t ircp5igr2; /* Interrupt Router CP5 Interrupt2
				      * Generation Register, offset 0x2B4 */
	volatile uint32_t ircp5isr3; /* Interrupt Router CP5 InterruptX Status
				      * Register, offset 0x2B8 */
	volatile uint32_t ircp5igr3; /* Interrupt Router CP5 InterruptX
				      * Generation Register, offset 0x2BC */
	volatile uint32_t ircp6isr0; /* Interrupt Router CP6 Interrupt0 Status
				      * Register, offset 0x2C0 */
	volatile uint32_t ircp6igr0; /* Interrupt Router CP6 Interrupt0
				      * Generation Register, offset 0x2C4 */
	volatile uint32_t ircp6isr1; /* Interrupt Router CP6 Interrupt1 Status
				      * Register, offset 0x2C8 */
	volatile uint32_t ircp6igr1; /* Interrupt Router CP6 Interrupt1
				      * Generation Register, offset 0x2CC */
	volatile uint32_t ircp6isr2; /* Interrupt Router CP6 Interrupt2 Status
				      * Register, offset 0x2D0 */
	volatile uint32_t ircp6igr2; /* Interrupt Router CP6 Interrupt2
				      * Generation Register, offset 0x2D4 */
	volatile uint32_t ircp6isr3; /* Interrupt Router CP6 InterruptX Status
				      * Register, offset 0x2D8 */
	volatile uint32_t ircp6igr3; /* Interrupt Router CP6 InterruptX
				      * Generation Register, offset 0x2DC */
	uint8_t reserved08[288];
	volatile uint32_t ircpcfg; /* Interrupt Router Configuration Register,
				    * offset 0x400 */
	uint8_t reserved09[1020];
	volatile uint32_t irnmic; /* Interrupt Router Non-Maskable Interrupt
				   * Control Register, offset 0x800 */
	uint8_t reserved10[124];
	volatile uint16_t irsprc[240]; /* Interrupt Router Shared Peripheral
					* Routing Control Register n,
					* offset 0x880 + 2*n */
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

/* MSCM Hardware Register Access Macros */

#define CONCAT(cpu, type, irq)    ircp ## cpu ## type ## irq /* Concatenation */

#define ISR(cpu, irq)    CONCAT(cpu, isr, irq) /* Interrupt Status Macro */
#define IGR(cpu, irq)    CONCAT(cpu, igr, irq) /* Interrupt Generation Macro */

/* S32 Common Chassis Specific Functions */

int ipc_hw_init(void);

void ipc_hw_irq_enable(void);

void ipc_hw_irq_disable(void);

void ipc_hw_irq_notify(void);

void ipc_hw_irq_clear(void);

#endif /* IPC_HW_S32_H_ */
