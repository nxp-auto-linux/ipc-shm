/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2025 NXP
 */
#include <linux/io.h>

#include "ipc-shm.h"
#include "ipc-os.h"
#include "ipc-hw.h"

/* s32r47 Processor IDs */
enum ipc_s32r47_processor_idx {
	IPC_A53_0 = 0, /* ARM Cortex-A53 core 0 */
	IPC_A53_1 = 1, /* ARM Cortex-A53 core 1 */
	IPC_A53_2 = 2, /* ARM Cortex-A53 core 2 */
	IPC_A53_3 = 3, /* ARM Cortex-A53 core 3 */
	IPC_M7_0  = 4, /* ARM Cortex-M7 core 0 */
	IPC_M7_1  = 5, /* ARM Cortex-M7 core 1 */
	IPC_M7_2  = 6, /* ARM Cortex-M7 core 2 */
	IPC_BBE32_0  = 7, /* DSP BBE32 core 0 */
	IPC_BBE32_1  = 8, /* DSP BBE32 core 1 */
	IPC_KQ8_0  = 9, /* DSP KQ8 core 0 */
	IPC_KQ8_1  = 10, /* DSP KQ8 core 1 */
};

/* s32r47 MSCM core to core interrupt IDs */
enum {
	MSCM_INT0_IRQn  = 0U,  /**< CPU to CPU interrupt#0 */
	MSCM_INT1_IRQn  = 1U,  /**< CPU to CPU interrupt#1 */
	MSCM_INT2_IRQn  = 2U,  /**< CPU to CPU interrupt#2 */
	MSCM_INT3_IRQn  = 3U,  /**< CPU to CPU interrupt#3 */
	MSCM_INT4_IRQn  = 4U,  /**< CPU to CPU interrupt#4 */
	MSCM_INT5_IRQn  = 5U,  /**< CPU to CPU interrupt#5 */
	MSCM_INT6_IRQn  = 6U,  /**< CPU to CPU interrupt#6 */
	MSCM_INT7_IRQn  = 7U,  /**< CPU to CPU interrupt#7 */
	MSCM_INT8_IRQn  = 8U,  /**< CPU to CPU interrupt#8 */
	MSCM_INT9_IRQn  = 9U,  /**< CPU to CPU interrupt#9 */
	MSCM_INT10_IRQn = 10U  /**< CPU to CPU interrupt#10 */
};

/* default remote core */
#define IPC_DEFAULT_REMOTE_CORE    IPC_M7_0
/* default local core */
#define IPC_DEFAULT_LOCAL_CORE     IPC_A53_0

/* MSCM registers count for s32r47 */
#define IPC_MSCM_CPXCFG_COUNT      (4U)
#define IPC_MSCM_CPnCFG_COUNT      (4U)
#define IPC_MSCM_CPnINFO_COUNT     (11U)
#define IPC_MSCM_ENEDC_COUNT       (3U)
#define IPC_MSCM_IAHBCFGREG_COUNT  (3U)
#define IPC_MSCM_IRSPRC_COUNT      (240U)

#define IPC_MSCM_RESERVED00_COUNT  (4U)
#define IPC_MSCM_RESERVED01_COUNT  (644U)
#define IPC_MSCM_RESERVED02_COUNT  (1020U)
#define IPC_MSCM_RESERVED03_COUNT  (124U)
#define IPC_MSCM_RESERVED04_COUNT  (32U)
#define IPC_MSCM_RESERVED05_COUNT  (184U)

#define IPC_MSCM_CP_COUNT               (11U)
#define IPC_MSCM_IRQ_COUNT              (13U)
#define IPC_MSCM_IRCPnIGRx_INT_EN       (0x1U)
#define IPC_MSCM_IRCPnISRx_CLEAR_A53    (0x000FU)
#define IPC_MSCM_IRSPRCn_GIC500         (0x01U)
#define IPC_MSCM_CPXNUM_CPN_MASK        (0xFFU)

/* Interrupt Router Config Lock Bit */
#define IPC_MSCM_IRCPCFG_LOCK    0x80000000uL
/* Processors 0 to 3 Trusted cores */
#define IPC_MSCM_IRCPCFG_A53_TR  0x0FuL

/**
 * MSCM - Register Layout Typedef
 * CPXTYPE:   Processor X Type Register
 * CPXNUM:    Processor X Number Register
 * CPXREV:    Processor X Revision Register
 * CPXCFG:    Processor X Configuration 0-3 Register
 * CPnTYPE:   Processor n Type Register (n = 0 to 10)
 * CPnNUM:    Processor n Number Register (n = 0 to 10)
 * CPnREV:    Processor n Count Register (n = 0 to 10)
 * CPnCFG:    Processor n Configuration 0-3 Register (n = 0 to 10)
 * IRCPCFG:   Interrupt Router Configuration Register
 * IRNMIC:    Interrupt Router Non-Maskable Interrupt Control Register
 * IRSPRC:    Interrupt Router Shared Peripheral Routing Control Register
 * ISR:       Interrupt Router CPn Interrupt0 Status Register
 * IGR:       Interrupt Router CPn Interrupt0 Generation Register
 * ENEDC:     Read/Write Data/Address Check Enable
 * IAHBCFGREG: AHB Gasket Configuration 0-2
 */

struct ipc_mscm_regs {
	volatile uint32_t CPXTYPE;
	volatile uint32_t CPXNUM;
	volatile uint32_t CPXREV;
	volatile uint32_t CPXCFG[IPC_MSCM_CPXCFG_COUNT];
	struct {
		uint8_t RESERVED_0[IPC_MSCM_RESERVED00_COUNT];
		volatile uint32_t CPnTYPE;
		volatile uint32_t CPnNUM;
		volatile uint32_t CPnREV;
		volatile uint32_t CPnCFG[IPC_MSCM_CPnCFG_COUNT];
	} CPnINFO[IPC_MSCM_CPnINFO_COUNT];
	uint8_t RESERVED_11[IPC_MSCM_RESERVED01_COUNT];
	volatile uint32_t IRCPCFG;
	uint8_t RESERVED_12[IPC_MSCM_RESERVED02_COUNT];
	volatile uint32_t IRNMIC;
	uint8_t RESERVED_13[IPC_MSCM_RESERVED03_COUNT];
	volatile uint16_t IRSPRC[IPC_MSCM_IRSPRC_COUNT];
	uint8_t RESERVED_14[IPC_MSCM_RESERVED04_COUNT];
	struct {
		volatile uint32_t ISR;
		volatile uint32_t IGR;
	} IRCPnIRx[IPC_MSCM_CP_COUNT][IPC_MSCM_IRQ_COUNT];
	uint8_t RESERVED_15[IPC_MSCM_RESERVED05_COUNT];
	volatile uint32_t ENEDC[IPC_MSCM_ENEDC_COUNT];
	uint8_t RESERVED_16[IPC_MSCM_RESERVED00_COUNT];
	volatile uint32_t IAHBCFGREG[IPC_MSCM_IAHBCFGREG_COUNT];
};

/* S32 Platform Specific Implementation  */

/**
 * struct ipc_hw_priv_type - platform specific private data
 *
 * @mscm_tx_irq:    MSCM inter-core interrupt reserved for shm driver tx
 * @mscm_rx_irq:    MSCM inter-core interrupt reserved for shm driver rx
 * @remote_core:    index of remote core to trigger the interrupt on
 * @local_core:     index of the local core targeted by remote
 * @ipc_mscm:       pointer to memory-mapped hardware peripheral MSCM
 */
static struct ipc_hw_priv_type {
	int16_t mscm_tx_irq;
	int16_t mscm_rx_irq;
	uint32_t shm_size;
	enum ipc_s32r47_processor_idx remote_core;
	enum ipc_s32r47_processor_idx local_core;
	struct ipc_mscm_regs *ipc_mscm;
} ipc_hw_priv[IPC_SHM_MAX_INSTANCES];

static int8_t ipc_hw_local_core_init(const uint8_t instance,
		const struct ipc_shm_cfg *cfg)
{
	switch (cfg->local_core.type) {
	case IPC_CORE_A53:
		switch (cfg->local_core.index) {
		case IPC_CORE_INDEX_0:
			ipc_hw_priv[instance].local_core = IPC_A53_0;
			break;
		case IPC_CORE_INDEX_1:
			ipc_hw_priv[instance].local_core = IPC_A53_1;
			break;
		case IPC_CORE_INDEX_2:
			ipc_hw_priv[instance].local_core = IPC_A53_2;
			break;
		case IPC_CORE_INDEX_3:
			ipc_hw_priv[instance].local_core = IPC_A53_3;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IPC_CORE_DEFAULT:
		ipc_hw_priv[instance].local_core = IPC_DEFAULT_LOCAL_CORE;
		break;
	default:
		return -EINVAL;
	}

	/* check trusted cores mask contains the targeted and other A53 cores */
	if ((!cfg->local_core.trusted)
		|| (cfg->local_core.trusted & ~IPC_MSCM_IRCPCFG_A53_TR)
		|| ((0x01uL << ipc_hw_priv[instance].local_core)
			& ~cfg->local_core.trusted)) {
		return -EINVAL;
	}

	return 0;
}

static int8_t ipc_hw_remote_core_init(const uint8_t instance,
		const struct ipc_shm_cfg *cfg)
{
	switch (cfg->remote_core.type) {
	case IPC_CORE_A53:
		switch (cfg->remote_core.index) {
		case IPC_CORE_INDEX_0:
			ipc_hw_priv[instance].remote_core = IPC_A53_0;
			break;
		case IPC_CORE_INDEX_1:
			ipc_hw_priv[instance].remote_core = IPC_A53_1;
			break;
		case IPC_CORE_INDEX_2:
			ipc_hw_priv[instance].remote_core = IPC_A53_2;
			break;
		case IPC_CORE_INDEX_3:
			ipc_hw_priv[instance].remote_core = IPC_A53_3;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IPC_CORE_M7:
		switch (cfg->remote_core.index) {
		case IPC_CORE_INDEX_0:
			ipc_hw_priv[instance].remote_core = IPC_M7_0;
			break;
		case IPC_CORE_INDEX_1:
			ipc_hw_priv[instance].remote_core = IPC_M7_1;
			break;
		case IPC_CORE_INDEX_2:
			ipc_hw_priv[instance].remote_core = IPC_M7_2;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IPC_CORE_KQ8:
		switch (cfg->remote_core.index) {
		case IPC_CORE_INDEX_0:
			ipc_hw_priv[instance].remote_core = IPC_KQ8_0;
			break;
		case IPC_CORE_INDEX_1:
			ipc_hw_priv[instance].remote_core = IPC_KQ8_1;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IPC_CORE_BBE32:
		switch (cfg->remote_core.index) {
		case IPC_CORE_INDEX_0:
			ipc_hw_priv[instance].remote_core = IPC_BBE32_0;
			break;
		case IPC_CORE_INDEX_1:
			ipc_hw_priv[instance].remote_core = IPC_BBE32_1;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IPC_CORE_DEFAULT:
		ipc_hw_priv[instance].remote_core = IPC_DEFAULT_REMOTE_CORE;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * ipc_hw_get_rx_irq() - get MSCM inter-core interrupt index [0..10] used for Rx
 *
 * Return: MSCM inter-core interrupt index used for Rx
 */
int ipc_hw_get_rx_irq(const uint8_t instance)
{
	return ipc_hw_priv[instance].mscm_rx_irq;
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
 * Return:
 * * %0		- on Success
 * * %-EINVAL	- either inter core interrupt invalid or invalid core
 * * %-ENOMEM	- failing to map MSCM address space
 * * %-EACCES	- failing to access MSCM registers
 */
int8_t ipc_hw_init(const uint8_t instance, const struct ipc_shm_cfg *cfg)
{
	/* map MSCM hardware peripheral block registers */
	void *addr = ipc_os_map_intc();

	return _ipc_hw_init(instance, cfg, addr);
}

/**
 * _ipc_hw_init() - platform specific initialization
 *
 * Low level variant of ipc_hw_init() used by UIO device implementation.
 */
int8_t _ipc_hw_init(const uint8_t instance,
	const struct ipc_shm_cfg *cfg, void *periph_addr)
{
	uint32_t ircpcfg_mask;

	if (!periph_addr)
		return -EINVAL;

	ipc_hw_priv[instance].ipc_mscm = (struct ipc_mscm_regs *)periph_addr;

	/* Initialize local and remote core */
	if (0 != ipc_hw_local_core_init(instance, cfg))
		return -EINVAL;

	if (0 != ipc_hw_remote_core_init(instance, cfg))
		return -EINVAL;

	if (((cfg->inter_core_tx_irq != IPC_IRQ_NONE)
			&& (cfg->inter_core_tx_irq == cfg->inter_core_rx_irq))
		|| (ipc_hw_priv[instance].remote_core
			== readl(
				&(ipc_hw_priv[instance].ipc_mscm->CPXNUM)))
		|| (ipc_hw_priv[instance].remote_core
			== ipc_hw_priv[instance].local_core)) {
		return -EINVAL;
	}

	/* Initialize tx and rx irq */
	if (IPC_IRQ_NONE != cfg->inter_core_tx_irq) {
		if ((MSCM_INT0_IRQn > cfg->inter_core_tx_irq)
				|| (MSCM_INT10_IRQn < cfg->inter_core_tx_irq))
			return -EINVAL;
	}

	if (IPC_IRQ_NONE != cfg->inter_core_rx_irq) {
		if ((MSCM_INT0_IRQn > cfg->inter_core_rx_irq)
				|| (MSCM_INT10_IRQn < cfg->inter_core_rx_irq))
			return -EINVAL;
		if ((IPC_BBE32_0 <= ipc_hw_priv[instance].remote_core)
			&& (IPC_KQ8_1 >= ipc_hw_priv[instance].remote_core)
			&& (cfg->inter_core_rx_irq != MSCM_INT0_IRQn)
				&& (cfg->inter_core_rx_irq != MSCM_INT1_IRQn))
			return -EINVAL;
	}


	ipc_hw_priv[instance].mscm_tx_irq = cfg->inter_core_tx_irq;
	ipc_hw_priv[instance].mscm_rx_irq = cfg->inter_core_rx_irq;
	/*
	 * disable rx irq source to avoid receiving an interrupt from remote
	 * before any of the buffer rings are initialized
	 */
	ipc_hw_irq_disable(instance);

	/*
	 * enable local trusted cores so that they can read full contents of
	 * IRCPnISRx registers
	 */
	ircpcfg_mask = readl(&ipc_hw_priv[instance].ipc_mscm->IRCPCFG);
	if (ircpcfg_mask & IPC_MSCM_IRCPCFG_LOCK)
		return -EACCES;

	writel(ircpcfg_mask | cfg->local_core.trusted,
		&(ipc_hw_priv[instance].ipc_mscm->IRCPCFG));

	return 0;
}

/**
 * ipc_hw_free() - unmap MSCM IP block and clear irq
 */
void ipc_hw_free(const uint8_t instance)
{
	ipc_hw_irq_clear(instance);

	/* unmap MSCM hardware peripheral block */
	ipc_os_unmap_intc(ipc_hw_priv[instance].ipc_mscm);
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
	int16_t mscm_rx_irq;

	if (ipc_hw_priv[instance].mscm_rx_irq != IPC_IRQ_NONE) {
		mscm_rx_irq = ipc_hw_priv[instance].mscm_rx_irq;
		/* enable MSCM core-to-core interrupt routing */
		irsprc_mask = readw(&ipc_hw_priv[instance].ipc_mscm
					->IRSPRC[mscm_rx_irq]);
		writew(irsprc_mask | IPC_MSCM_IRSPRCn_GIC500,
			&((ipc_hw_priv[instance].ipc_mscm)
				->IRSPRC[mscm_rx_irq]));
	}
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
	int16_t mscm_rx_irq;

	if (ipc_hw_priv[instance].mscm_rx_irq != IPC_IRQ_NONE) {
		mscm_rx_irq = ipc_hw_priv[instance].mscm_rx_irq;
		/* disable MSCM core-to-core interrupt routing */
		irsprc_mask = readw(&ipc_hw_priv[instance].ipc_mscm
				->IRSPRC[mscm_rx_irq]);
		writew(irsprc_mask & ~IPC_MSCM_IRSPRCn_GIC500,
			&((ipc_hw_priv[instance].ipc_mscm)
				->IRSPRC[mscm_rx_irq]));
	}

}

/**
 * ipc_hw_irq_notify() - notify remote that data is available
 */
void ipc_hw_irq_notify(const uint8_t instance)
{
	int16_t mscm_tx_irq = ipc_hw_priv[instance].mscm_tx_irq;
	int remote_core = ipc_hw_priv[instance].remote_core;

	if (ipc_hw_priv[instance].mscm_tx_irq != IPC_IRQ_NONE) {
		/* trigger MSCM core-to-core directed interrupt */
		writel(IPC_MSCM_IRCPnIGRx_INT_EN,
			&((ipc_hw_priv[instance].ipc_mscm)->
			IRCPnIRx[remote_core][mscm_tx_irq].IGR));
	}
}

/**
 * ipc_hw_irq_clear() - clear available data notification
 */
void ipc_hw_irq_clear(const uint8_t instance)
{
	int16_t mscm_rx_irq = ipc_hw_priv[instance].mscm_rx_irq;
	int local_core = ipc_hw_priv[instance].local_core;
	int remote_core = ipc_hw_priv[instance].remote_core;

	if (mscm_rx_irq != IPC_IRQ_NONE) {
		/*
		 * clear MSCM core-to-core directed interrupt
		 * on the targeted core
		 */
		writel(1 << remote_core,
			&((ipc_hw_priv[instance].ipc_mscm)->
			IRCPnIRx[local_core][mscm_rx_irq].ISR));
	}
}
