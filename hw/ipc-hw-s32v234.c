/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018-2019,2021 NXP
 */
#include <linux/io.h>

#include "ipc-shm.h"
#include "ipc-os.h"
#include "ipc-hw.h"

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
 * ipc_hw_get_rx_irq() - get MSCM inter-core interrupt index [0..3] used for Rx
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
 * inter_core_tx_irq can be disabled by passing IPC_IRQ_NONE, if polling is
 * desired in transmit notification path. inter_core_tx_irq and
 * inter_core_rx_irq are not allowed to have the same value to avoid possible
 * race conditions when updating the value of the IRSPRCn register.
 * If the value IPC_CORE_DEFAULT is passed as remote_core, the default value
 * defined for the selected platform will be used instead.
 *
 * Return: 0 for success, -EINVAL for either inter core interrupt invalid or
 *         invalid remote core, -ENOMEM for failing to map MSCM address space
 */
int ipc_hw_init(const struct ipc_shm_cfg *cfg)
{
	/* map MSCM hardware peripheral block */
	void *addr = ipc_os_map_intc();

	return _ipc_hw_init(cfg->inter_core_tx_irq, cfg->inter_core_rx_irq,
			    &cfg->remote_core, addr);
}

/**
 * _ipc_hw_init() - platform specific initialization
 *
 * Low level variant of ipc_hw_init() used by UIO device implementation.
 */
int _ipc_hw_init(int tx_irq, int rx_irq,
		 const struct ipc_shm_remote_core *remote_core,
		 void *mscm_addr)
{
	if (!mscm_addr)
		return -EINVAL;

	priv.mscm = (struct mscm_regs *)mscm_addr;

	/* only M4 core is supported */
	if (remote_core->type != IPC_CORE_DEFAULT
		&& remote_core->type != IPC_CORE_M4) {
		return -EINVAL;
	}

	if (((tx_irq != IPC_IRQ_NONE)
			&& ((tx_irq < IRQ_ID_MIN) || (tx_irq > IRQ_ID_MAX)))
		|| (rx_irq < IRQ_ID_MIN) || (rx_irq > IRQ_ID_MAX)
		|| (rx_irq == tx_irq)) {
		return -EINVAL;
	}

	priv.mscm_tx_irq = tx_irq;
	priv.mscm_rx_irq = rx_irq;
	priv.remote_core = DEFAULT_REMOTE_CORE;

	/*
	 * disable rx irq source to avoid receiving an interrupt from remote
	 * before any of the buffer rings are initialized
	 */
	ipc_hw_irq_disable();

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
 */
void ipc_hw_irq_enable(void)
{
	uint16_t irsprc_mask;

	/* enable MSCM core-to-core interrupt routing */
	irsprc_mask = readw_relaxed(&priv.mscm->irsprc[priv.mscm_rx_irq]);
	writew_relaxed(irsprc_mask | MSCM_IRSPRCn_CPxE(A53),
			&priv.mscm->irsprc[priv.mscm_rx_irq]);
}

/**
 * ipc_hw_irq_disable() - disable notifications from remote
 */
void ipc_hw_irq_disable(void)
{
	uint16_t irsprc_mask;

	/* disable MSCM core-to-core interrupt routing */
	irsprc_mask = readw_relaxed(&priv.mscm->irsprc[priv.mscm_rx_irq]);
	writew_relaxed(irsprc_mask & ~MSCM_IRSPRCn_CPxE(A53),
			&priv.mscm->irsprc[priv.mscm_rx_irq]);
}

/**
 * ipc_hw_irq_notify() - notify remote that data is available
 */
void ipc_hw_irq_notify(void)
{
	if (priv.mscm_tx_irq == IPC_IRQ_NONE)
		return;

	/* trigger MSCM core-to-core directed interrupt */
	writel_relaxed(MSCM_IRCPGIR_TLF(MSCM_IRCPGIR_TLF_CPUTL) |
			MSCM_IRCPGIR_CPUTL(priv.remote_core) |
			MSCM_IRCPGIR_INTID(priv.mscm_tx_irq),
			&priv.mscm->ircpgir);
}

/**
 * ipc_hw_irq_clear() - clear available data notification
 */
void ipc_hw_irq_clear(void)
{
	/* clear MSCM core-to-core directed interrupt */
	writel_relaxed(MSCM_IRCPxIR_INT(priv.mscm_rx_irq),
		       &priv.mscm->ircp1ir);
}
