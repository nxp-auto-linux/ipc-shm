/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2023 NXP
 */

#ifndef IPC_HW_PLATFORM_H
#define IPC_HW_PLATFORM_H

/* S32gen1 Processor IDs */
enum ipc_s32gen1_processor_idx {
	IPC_A53_0 = 0, /* ARM Cortex-A53 core 0 */
	IPC_A53_1 = 1, /* ARM Cortex-A53 core 1 */
	IPC_A53_2 = 2, /* ARM Cortex-A53 core 2 */
	IPC_A53_3 = 3, /* ARM Cortex-A53 core 3 */
	IPC_M7_0  = 4, /* ARM Cortex-M7 core 0 */
	IPC_M7_1  = 5, /* ARM Cortex-M7 core 1 */
	IPC_M7_2  = 6, /* ARM Cortex-M7 core 2 */
};

/* S32gen1 Specific Definitions */
#define IPC_DEFAULT_REMOTE_CORE    IPC_M7_0
#define IPC_DEFAULT_LOCAL_CORE     IPC_A53_0  /* default local core */

/* MSCM registers count for S32gen1 */
#define IPC_MSCM_CPnCFG_COUNT      4
#define IPC_MSCM_CP_COUNT          7
#define IPC_MSCM_IRQ_COUNT         4
#define IPC_MSCM_IRSPRC_COUNT      240

#define IPC_MSCM_RESERVED00_COUNT  4
#define IPC_MSCM_RESERVED01_COUNT  260
#define IPC_MSCM_RESERVED02_COUNT  288
#define IPC_MSCM_RESERVED03_COUNT  1020
#define IPC_MSCM_RESERVED04_COUNT  124
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
struct ipc_mscm_regs {
	volatile const uint32_t CPXTYPE;
	volatile const uint32_t CPXNUM;
	volatile const uint32_t CPXREV;
	volatile const uint32_t CPXCFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED00[IPC_MSCM_RESERVED00_COUNT]; /* 0x4 */
	volatile const uint32_t CP0TYPE;
	volatile const uint32_t CP0NUM;
	volatile const uint32_t CP0REV;
	volatile const uint32_t CP0CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED01[IPC_MSCM_RESERVED00_COUNT]; /* 0x4 */
	volatile const uint32_t CP1TYPE;
	volatile const uint32_t CP1NUM;
	volatile const uint32_t CP1REV;
	volatile const uint32_t CP1CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED02[IPC_MSCM_RESERVED00_COUNT]; /* 0x4 */
	volatile const uint32_t CP2TYPE;
	volatile const uint32_t CP2NUM;
	volatile const uint32_t CP2REV;
	volatile const uint32_t CP2CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED03[IPC_MSCM_RESERVED00_COUNT]; /* 0x4 */
	volatile const uint32_t CP3TYPE;
	volatile const uint32_t CP3NUM;
	volatile const uint32_t CP3REV;
	volatile const uint32_t CP3CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED04[IPC_MSCM_RESERVED00_COUNT]; /* 0x4 */
	volatile const uint32_t CP4TYPE;
	volatile const uint32_t CP4NUM;
	volatile const uint32_t CP4REV;
	volatile const uint32_t CP4CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED05[IPC_MSCM_RESERVED00_COUNT]; /* 0x4 */
	volatile const uint32_t CP5TYPE;
	volatile const uint32_t CP5NUM;
	volatile const uint32_t CP5REV;
	volatile const uint32_t CP5CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED06[IPC_MSCM_RESERVED00_COUNT]; /* 0x4 */
	volatile const uint32_t CP6TYPE;
	volatile const uint32_t CP6NUM;
	volatile const uint32_t CP6REV;
	volatile const uint32_t CP6CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED07[IPC_MSCM_RESERVED01_COUNT]; /* 0x104 */
	struct {
		volatile uint32_t IPC_ISR;
		volatile uint32_t IPC_IGR;
	} IRCPnIRx[IPC_MSCM_CP_COUNT][IPC_MSCM_IRQ_COUNT];
	uint8_t RESERVED08[IPC_MSCM_RESERVED02_COUNT]; /* 0x120 */
	volatile uint32_t IRCPCFG;
	uint8_t RESERVED09[IPC_MSCM_RESERVED03_COUNT]; /* 0x3FC */
	volatile uint32_t IRNMIC;
	uint8_t RESERVED10[IPC_MSCM_RESERVED04_COUNT]; /* 0x7C */
	volatile uint16_t IRSPRC[IPC_MSCM_IRSPRC_COUNT];
};

/* MSCM Hardware Register Bit Fields Definitions */

#define IPC_MSCM_IRCPnISRn_CP0_INT    0x01uL /* Processor 0 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP1_INT    0x02uL /* Processor 1 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP2_INT    0x04uL /* Processor 2 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP3_INT    0x08uL /* Processor 3 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP4_INT    0x10uL /* Processor 4 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP5_INT    0x20uL /* Processor 5 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP6_INT    0x40uL /* Processor 6 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CPx_INT    0x7FuL /* All Processors Interrupt Mask */

#define IPC_MSCM_IRCPnIGRn_INT_EN    0x1uL /* Interrupt Enable */

/* Interrupt Router Config Lock Bit */
#define IPC_MSCM_IRCPCFG_LOCK    0x80000000uL
#define IPC_MSCM_IRCPCFG_CP0_TR  0x01uL /* Processor 0 Trusted core */
#define IPC_MSCM_IRCPCFG_CP1_TR  0x02uL /* Processor 1 Trusted core */
#define IPC_MSCM_IRCPCFG_CP2_TR  0x04uL /* Processor 2 Trusted core */
#define IPC_MSCM_IRCPCFG_CP3_TR  0x08uL /* Processor 3 Trusted core */
#define IPC_MSCM_IRCPCFG_CP4_TR  0x10uL /* Processor 4 Trusted core */
#define IPC_MSCM_IRCPCFG_CP5_TR  0x20uL /* Processor 5 Trusted core */
#define IPC_MSCM_IRCPCFG_CP6_TR  0x40uL /* Processor 6 Trusted core */
#define IPC_MSCM_IRCPCFG_A53_TR  0x0FuL /* Processors 0 to 3 Trusted cores */

#define IPC_MSCM_IRSPRCn_LOCK   0x8000u /* Interrupt Routing Control Lock Bit */
#define IPC_MSCM_IRSPRCn_GIC500 0x1u /* GIC500 Interrupt Steering Enable */
#define IPC_MSCM_IRSPRCn_M7_0   0x2u /* CM7 Core 0 Interrupt Steering Enable */
#define IPC_MSCM_IRSPRCn_M7_1   0x4u /* CM7 Core 1 Interrupt Steering Enable */
#define IPC_MSCM_IRSPRCn_M7_2   0x8u /* CM7 Core 2 Interrupt Steering Enable */

/* S32 Platform Specific Implementation  */

#endif /* IPC_HW_PLATFORM_H */
