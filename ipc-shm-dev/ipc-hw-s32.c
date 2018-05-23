/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2018 NXP Semiconductors
 */
#include <linux/types.h>
#include "ipc-hw-s32.h"
#include <linux/module.h>
#include <linux/io.h>

/* Hardware IP Block Base Addresses - TODO: get them from device tree */
#define MSCM_BASE    0x40198000ul /* Miscellaneous System Control Module */

/* S32Gxx Specific Definitions */
#define RTOS_SIGNAL_CPU    4 /* M7 core 0 */
#define GPOS_SIGNAL_CPU    0 /* A53 core 0 */

/* pointer to memory-mapped hardware peripheral MSCM */
static volatile struct mscm_memmap *mscm;

/**
 * ipc_hw_init() - map MSCM IP block to proper address
 */
int ipc_hw_init(void)
{
	/* map MSCM hardware peripheral block */
	mscm = (struct mscm_memmap *) ioremap_nocache(
		(phys_addr_t)MSCM_BASE, sizeof(struct mscm_memmap));
	if (!mscm)
		return -ENOMEM;

	return 0;
}

/**
 * ipc_hw_irq_enable() - enable notifications from remote
 */
void ipc_hw_irq_enable(void)
{
	/* enable MSCM core-to-core interrupt routing */
	mscm->irsprc[MSCM_IRQ_ID] |= MSCM_IRSPRCn_GIC500;
}

/**
 * ipc_hw_irq_disable() - disable notifications from remote
 */
void ipc_hw_irq_disable(void)
{
	/* disable MSCM core-to-core interrupt routing */
	mscm->irsprc[MSCM_IRQ_ID] &= ~MSCM_IRSPRCn_GIC500;
}

/**
 * ipc_hw_irq_notify() - notify remote that data is available
 */
void ipc_hw_irq_notify(void)
{
	/* trigger MSCM core-to-core interrupt */
	mscm->IGR(RTOS_SIGNAL_CPU, MSCM_IRQ_ID) = MSCM_IRCPnIGRn_INT_EN;
}

/**
 * ipc_hw_irq_clear() - clear available data notification
 */
void ipc_hw_irq_clear(void)
{
	/* clear MSCM core-to-core interrupt */
	mscm->ISR(GPOS_SIGNAL_CPU, MSCM_IRQ_ID) = MSCM_IRCPnISRn_CPx_INT;
}

