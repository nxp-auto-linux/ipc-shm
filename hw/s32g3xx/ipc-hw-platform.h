/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2023 NXP
 */

#ifndef IPC_HW_PLATFORM_H
#define IPC_HW_PLATFORM_H

/* S32g3xx Processor IDs */
enum ipc_s32g3xx_processor_idx {
	IPC_A53_0 = 0,  /* ARM Cortex-A53 cluster 0 core 0 */
	IPC_A53_1 = 1,  /* ARM Cortex-A53 cluster 1 core 1 */
	IPC_A53_2 = 2,  /* ARM Cortex-A53 cluster 1 core 0 */
	IPC_A53_3 = 3,  /* ARM Cortex-A53 cluster 1 core 1 */
	IPC_M7_0  = 4,   /* ARM Cortex-M7 core 0 */
	IPC_M7_1  = 5,   /* ARM Cortex-M7 core 1 */
	IPC_M7_2  = 6,   /* ARM Cortex-M7 core 2 */
	IPC_M7_3  = 7,   /* ARM Cortex-M7 core 2 */
	IPC_A53_4 = 8,  /* ARM Cortex-A53 cluster 0 core 2 */
	IPC_A53_5 = 9,  /* ARM Cortex-A53 cluster 0 core 3 */
	IPC_A53_6 = 10, /* ARM Cortex-A53 cluster 1 core 2 */
	IPC_A53_7 = 11, /* ARM Cortex-A53 cluster 1 core 3 */
};

/* S32g3xx Specific Definitions */
#define IPC_DEFAULT_REMOTE_CORE    IPC_M7_0
#define IPC_DEFAULT_LOCAL_CORE     IPC_A53_0 /* default local core */

/* MSCM registers count for S32g3xx */
#define IPC_MSCM_CPnCFG_COUNT      4
#define IPC_MSCM_CP_COUNT          12
#define IPC_MSCM_IRQ_COUNT         14
#define IPC_MSCM_IRSPRC_COUNT      240
#define IPC_MSCM_RESERVED00_COUNT  4
#define IPC_MSCM_RESERVED01_COUNT  612
#define IPC_MSCM_RESERVED02_COUNT  1020
#define IPC_MSCM_RESERVED03_COUNT  124

/**
 * struct mscm_regs - MSCM Peripheral Register Structure
 */
struct ipc_mscm_regs {
	volatile const uint32_t CPXTYPE;
	volatile const uint32_t CPXNUM;
	volatile const uint32_t CPXREV;
	volatile const uint32_t CPXCFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED00[IPC_MSCM_RESERVED00_COUNT];
	volatile const uint32_t CP0TYPE;
	volatile const uint32_t CP0NUM;
	volatile const uint32_t CP0REV;
	volatile const uint32_t CP0CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED01[IPC_MSCM_RESERVED00_COUNT];
	volatile const uint32_t CP1TYPE;
	volatile const uint32_t CP1NUM;
	volatile const uint32_t CP1REV;
	volatile const uint32_t CP1CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED02[IPC_MSCM_RESERVED00_COUNT];
	volatile const uint32_t CP2TYPE;
	volatile const uint32_t CP2NUM;
	volatile const uint32_t CP2REV;
	volatile const uint32_t CP2CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED03[IPC_MSCM_RESERVED00_COUNT];
	volatile const uint32_t CP3TYPE;
	volatile const uint32_t CP3NUM;
	volatile const uint32_t CP3REV;
	volatile const uint32_t CP3CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED04[IPC_MSCM_RESERVED00_COUNT];
	volatile const uint32_t CP4TYPE;
	volatile const uint32_t CP4NUM;
	volatile const uint32_t CP4REV;
	volatile const uint32_t CP4CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED05[IPC_MSCM_RESERVED00_COUNT];
	volatile const uint32_t CP5TYPE;
	volatile const uint32_t CP5NUM;
	volatile const uint32_t CP5REV;
	volatile const uint32_t CP5CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED06[IPC_MSCM_RESERVED00_COUNT];
	volatile const uint32_t CP6TYPE;
	volatile const uint32_t CP6NUM;
	volatile const uint32_t CP6REV;
	volatile const uint32_t CP6CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED07[IPC_MSCM_RESERVED00_COUNT];
	volatile const uint32_t CP7TYPE;
	volatile const uint32_t CP7NUM;
	volatile const uint32_t CP7REV;
	volatile const uint32_t CP7CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED08[IPC_MSCM_RESERVED00_COUNT];
	volatile const uint32_t CP8TYPE;
	volatile const uint32_t CP8NUM;
	volatile const uint32_t CP8REV;
	volatile const uint32_t CP8CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED09[IPC_MSCM_RESERVED00_COUNT];
	volatile const uint32_t CP9TYPE;
	volatile const uint32_t CP9NUM;
	volatile const uint32_t CP9REV;
	volatile const uint32_t CP9CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED010[IPC_MSCM_RESERVED00_COUNT];
	volatile const uint32_t CP10TYPE;
	volatile const uint32_t CP10NUM;
	volatile const uint32_t CP10REV;
	volatile const uint32_t CP10CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED011[IPC_MSCM_RESERVED00_COUNT];
	volatile const uint32_t CP11TYPE;
	volatile const uint32_t CP11NUM;
	volatile const uint32_t CP11REV;
	volatile const uint32_t CP11CFG[IPC_MSCM_CPnCFG_COUNT];
	uint8_t RESERVED012[IPC_MSCM_RESERVED01_COUNT];
	volatile uint32_t IRCPCFG;
	uint8_t RESERVED013[IPC_MSCM_RESERVED02_COUNT];
	volatile uint32_t IRNMIC;
	uint8_t RESERVED14[IPC_MSCM_RESERVED03_COUNT];
	volatile uint16_t IRSPRC[IPC_MSCM_IRSPRC_COUNT];
	struct {
		volatile uint32_t IPC_ISR;
		volatile uint32_t IPC_IGR;
	} IRCPnIRx[IPC_MSCM_CP_COUNT][IPC_MSCM_IRQ_COUNT];
};

/* MSCM Hardware Register Bit Fields Definitions */
#define IPC_MSCM_IRCPnISRn_CP0_INT   0x01uL /* Processor 0 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP1_INT   0x02uL /* Processor 1 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP2_INT   0x04uL /* Processor 2 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP3_INT   0x08uL /* Processor 3 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP4_INT   0x10uL /* Processor 4 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP5_INT   0x20uL /* Processor 5 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP6_INT   0x40uL /* Processor 6 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP7_INT   0x80uL /* Processor 7 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP8_INT   0x100uL /* Processor 8 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP9_INT   0x200uL /* Processor 9 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP10_INT  0x400uL /* Processor 10 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CP11_INT  0x800uL /* Processor 11 Interrupt Mask */
#define IPC_MSCM_IRCPnISRn_CPx_INT   0xFFFuL /* All Processors Interrupt Mask */

#define IPC_MSCM_IRCPnIGRn_INT_EN    0x1uL /* Interrupt Enable */

/* Interrupt Router Config Lock Bit */
#define IPC_MSCM_IRCPCFG_LOCK     0x80000000uL
#define IPC_MSCM_IRCPCFG_CP0_TR   0x01uL /* Processor 0 Trusted core */
#define IPC_MSCM_IRCPCFG_CP1_TR   0x02uL /* Processor 1 Trusted core */
#define IPC_MSCM_IRCPCFG_CP2_TR   0x04uL /* Processor 2 Trusted core */
#define IPC_MSCM_IRCPCFG_CP3_TR   0x08uL /* Processor 3 Trusted core */
#define IPC_MSCM_IRCPCFG_CP4_TR   0x10uL /* Processor 4 Trusted core */
#define IPC_MSCM_IRCPCFG_CP5_TR   0x20uL /* Processor 5 Trusted core */
#define IPC_MSCM_IRCPCFG_CP6_TR   0x40uL /* Processor 6 Trusted core */
#define IPC_MSCM_IRCPCFG_CP7_TR   0x80uL /* Processor 7 Trusted core */
#define IPC_MSCM_IRCPCFG_CP8_TR   0x80uL /* Processor 8 Trusted core */
#define IPC_MSCM_IRCPCFG_CP9_TR   0x80uL /* Processor 9 Trusted core */
#define IPC_MSCM_IRCPCFG_CP10_TR  0x80uL /* Processor 10 Trusted core */
#define IPC_MSCM_IRCPCFG_CP11_TR  0x80uL /* Processor 11 Trusted core */
#define IPC_MSCM_IRCPCFG_A53_TR   0xF0FuL /* 8 A53 cores Trusted Mask */

#define IPC_MSCM_IRSPRCn_LOCK   0x8000u /* Interrupt Routing Control Lock Bit */
#define IPC_MSCM_IRSPRCn_GIC500 0x1u /* GIC500 Interrupt Steering Enable */
#define IPC_MSCM_IRSPRCn_M7_0   0x02u /* CM7 Core 0 Interrupt Steering Enable */
#define IPC_MSCM_IRSPRCn_M7_1   0x04u /* CM7 Core 1 Interrupt Steering Enable */
#define IPC_MSCM_IRSPRCn_M7_2   0x08u /* CM7 Core 2 Interrupt Steering Enable */
#define IPC_MSCM_IRSPRCn_M7_3   0x10u /* CM7 Core 3 Interrupt Steering Enable */

#endif /* IPC_HW_PLATFORM_H */
