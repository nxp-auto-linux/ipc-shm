/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2023 NXP
 */

#ifndef IPC_HW_PLATFORM_H
#define IPC_HW_PLATFORM_H

/* S32V234 Processor IDs */
enum s32v234_processor_idx {
	M4 = 0, /* ARM Cortex-M4 core */
	A53 = 1, /* ARM Cortex-A53 cluster */
};

/* S32V234 Specific Definitions */
#define DEFAULT_REMOTE_CORE    M4
#define IRQ_ID_MIN             0
#define IRQ_ID_MAX             3

/* MSCM registers count for S32V234 */
#define MSCM_CPnCFG_COUNT      4
#define MSCM_OCMDR_COUNT       4
#define MSCM_IRSPRC_COUNT      175
#define MSCM_IPCE_COUNT        4
#define MSCM_IPCIE_COUNT       4

/**
 * struct mscm_regs - MSCM Peripheral Register Structure
 *
 * @cpxtype:        Processor x Type Register, offset 0x0
 * @cpxnum:         Processor x Number Register, offset 0x4
 * @cpxmaster:      Processor x Master Number Register, offset 0x8
 * @cpxcount:       Processor x Count Register, offset 0xC
 * @cpxcfg[n]:      Processor x Configuration n Register, offset 0x10 + 4*n
 * @cp0type:        Processor 0 Type Register, offset 0x20
 * @cp0num:         Processor 0 Number Register, offset 0x24
 * @cp0master:      Processor 0 Master Number Register, offset 0x28
 * @cp0count:       Processor 0 Count Register, offset 0x2C
 * @cp0cfg[n]:      Processor 0 Configuration n Register, offset 0x30 + 4*n
 * @cp1type:        Processor 1 Type Register, offset 0x40
 * @cp1num:         Processor 1 Number Register, offset 0x44
 * @cp1master:      Processor 1 Master Number Register, offset 0x48
 * @cp1count:       Processor 1 Count Register, offset 0x4C
 * @cp1cfg[n]:      Processor 1 Configuration n Register, offset 0x50 + 4*n
 * @ocmdr[n]:       On-Chip Memory Descriptor Register n, offset 0x400 + 4*n
 * @tcmdr0:         Generic Tightly Coupled Memory Descriptor Register,
 *                  offset 0x480
 * @cpce0:          Core Parity Checking Enable Register 0, offset 0x500
 * @ircp0ir:        Interrupt Router CP0 Interrupt Register, offset 0x800
 * @ircp1ir:        Interrupt Router CP1 Interrupt Register, offset 0x804
 * @ircpgir:        Interrupt Router CPU Generate Interrupt Register,
 *                  offset 0x820
 * @irsprc[n]:      Interrupt Router Shared Peripheral Routing Control
 *                  Register n, offset 0x880 + 2*n
 * @ipcge:          Interconnect Parity Checking Global Enable Register,
 *                  offset 0xD00
 * @ipce[n]:        Interconnect Parity Checking Enable Register n,
 *                  offset 0xD10 + 4*n
 * @ipcgie:         Interconnect Parity Checking Global Injection Enable
 *                  Register, offset 0xD40
 * @ipcie[n]:       Interconnect Parity Checking Injection Enable Register n,
 *                  offset 0xD50 + 4*n
 */
struct mscm_regs {
	volatile const uint32_t cpxtype;
	volatile const uint32_t cpxnum;
	volatile const uint32_t cpxmaster;
	volatile const uint32_t cpxcount;
	volatile const uint32_t cpxcfg[MSCM_CPnCFG_COUNT];
	volatile const uint32_t cp0type;
	volatile const uint32_t cp0num;
	volatile const uint32_t cp0master;
	volatile const uint32_t cp0count;
	volatile const uint32_t cp0cfg[MSCM_CPnCFG_COUNT];
	volatile const uint32_t cp1type;
	volatile const uint32_t cp1num;
	volatile const uint32_t cp1master;
	volatile const uint32_t cp1count;
	volatile const uint32_t cp1cfg[MSCM_CPnCFG_COUNT];
	uint8_t reserved00[928]; /* 0x3A8 */
	volatile uint32_t ocmdr[MSCM_OCMDR_COUNT];
	uint8_t reserved01[112]; /* 0x70 */
	volatile uint32_t tcmdr0;
	uint8_t reserved02[124]; /* 0x7C */
	volatile uint32_t cpce0;
	uint8_t reserved03[764]; /* 0x2FC */
	volatile uint32_t ircp0ir;
	volatile uint32_t ircp1ir;
	uint8_t reserved04[24]; /* 0x18 */
	volatile uint32_t ircpgir;
	uint8_t reserved05[92]; /* 0x5C */
	volatile uint16_t irsprc[MSCM_IRSPRC_COUNT];
	uint8_t reserved06[800]; /* 0x320 */
	volatile uint32_t ipcge;
	uint8_t reserved07[12]; /* 0xC */
	volatile uint32_t ipce[MSCM_IPCE_COUNT];
	uint8_t reserved08[32]; /* 0x20 */
	volatile uint32_t ipcgie;
	uint8_t reserved09[12]; /* 0xC */
	volatile uint32_t ipcie[MSCM_IPCIE_COUNT];
};

/* MSCM Hardware Register Bit Fields Definitions */

#define MSCM_IRCPxIR_INT(n)    (1u << n) /* Interrupt Router CPx Interrupt n */

#define MSCM_IRCPGIR_TLF_MASK      0x03000000uL /* Target List Field */
#define MSCM_IRCPGIR_CPUTL_MASK    0x000F0000uL /* CPU Target List */
#define MSCM_IRCPGIR_INTID_MASK    0x00000003uL /* Interrupt MSCM ID */

#define MSCM_IRCPGIR_TLF(n)      ((n << 24u) & MSCM_IRCPGIR_TLF_MASK)
#define MSCM_IRCPGIR_CPUTL(n)    (((1u << n) << 16u) & MSCM_IRCPGIR_CPUTL_MASK)
#define MSCM_IRCPGIR_INTID(n)    ((n <<  0u) & MSCM_IRCPGIR_INTID_MASK)

#define MSCM_IRCPGIR_TLF_CPUTL    0u /* CPUTL field used to direct interrupts */
#define MSCM_IRCPGIR_TLF_OTHER    1u /* Interrupt directed to all other cores */
#define MSCM_IRCPGIR_TLF_SELF     2u /* Interrupt directed to requesting core */

#define MSCM_IRSPRCn_RO    0x8000u /* Interrupt Routing Control Lock Bit */

#define MSCM_IRSPRCn_CPxE(n)    (1u << n) /* Interrupt Steering Enable fo CPx */

/* S32V234 Platform Specific Implementation  */

#endif /* IPC_HW_PLATFORM_H */
