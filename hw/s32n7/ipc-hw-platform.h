/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2025 NXP
 */

#ifndef IPC_HW_PLATFORM_H
#define IPC_HW_PLATFORM_H

/* APS_MRU - Size of Registers Arrays */
#define IPC_APS_MRU_CHCONFIG_COUNT     3u
#define IPC_APS_MRU_CHxMB_COUNT        3u
#define IPC_APS_MRU_NOTIFY_COUNT       4u

/**
 * APS_MRU - Register Layout Typedef
 *
 * @CH_CFG0:      Channel (x) Configuration 0, array offset: 0x0
 * @CH_CFG1:      Channel (x) Configuration 1, array offset: 0x4
 * @CH_MBSTAT:    Channel (x) Mailbox Status, array offset: 0x8
 * @NOTIFY:       Notification, array offset: 0x200
 * @RSTSTAT:      Channel Reset Status, offset: 0x240
 * @MB:           Channel (x) Mailbox (n), offset: 0x10000
 * @CH_MBSTAT:    Channel (x) Reset, offset: 0x10100
 */
typedef struct {
	struct IPC_APS_MRU_CHXCONFIG {
		volatile uint32_t CH_CFG0;
		volatile uint32_t CH_CFG1;
		volatile uint32_t CH_MBSTAT;
		uint8_t RESERVED_0[4];
	} CHXCONFIG[IPC_APS_MRU_CHCONFIG_COUNT];

	uint8_t RESERVED_0[464];
	volatile uint32_t NOTIFY[IPC_APS_MRU_NOTIFY_COUNT];
	uint8_t RESERVED_1[48];
	volatile uint32_t RSTSTAT;
	uint8_t RESERVED_2[64956];

	struct IPC_APS_MRU_CHX_MBY {
		volatile uint32_t MB[IPC_APS_MRU_CHxMB_COUNT];
		uint8_t RESERVED_3[232];
		volatile uint32_t RST;
		uint8_t RESERVED_4[65276];
	} CH[IPC_APS_MRU_CHCONFIG_COUNT];
} IPC_APS_MRU_Type;


/* RTU_MRU - Size of Registers Arrays */
#define IPC_RTU_MRU_CHXCONFIG_COUNT    12u
#define IPC_RTU_MRU_CHxMB_COUNT        8u
#define IPC_RTU_MRU_NOTIFY_COUNT       4u

/**
 * RTU_MRU - Register Layout Typedef
 *
 * @CH_CFG0:      Channel (x) Configuration 0, array offset: 0x0
 * @CH_CFG1:      Channel (x) Configuration 1, array offset: 0x4
 * @CH_MBSTAT:    Channel (x) Mailbox Status, array offset: 0x8
 * @NOTIFY:       Notification, array offset: 0x200
 * @RSTSTAT:      Channel Reset Status, offset: 0x240
 * @MB:           Channel (x) Mailbox (n), offset: 0x10000
 * @CH_MBSTAT:    Channel (x) Reset, offset: 0x10100
 */
typedef struct {
	struct RTU_MRU_CHXCONFIG {
		volatile uint32_t CH_CFG0;
		volatile uint32_t CH_CFG1;
		volatile uint32_t CH_MBSTAT;
		uint8_t RESERVED_0[4];
	} CHXCONFIG[IPC_RTU_MRU_CHXCONFIG_COUNT];

	uint8_t RESERVED_0[320];
	volatile uint32_t NOTIFY[IPC_RTU_MRU_NOTIFY_COUNT];
	uint8_t RESERVED_1[48];
	volatile uint32_t RSTSTAT;
	uint8_t RESERVED_2[64956];

	struct RTU_MRU_CHX_MBY {
		volatile uint32_t MB[IPC_RTU_MRU_CHxMB_COUNT];
		uint8_t RESERVED_3[224];
		volatile uint32_t RST;
		uint8_t RESERVED_4[65276];
	} CH[IPC_RTU_MRU_CHXCONFIG_COUNT];
} IPC_RTU_MRU_Type;

/* RCU_MRU_C - Size of Registers Arrays */
#define IPC_RCU_MRU_C_CHXCONFIG_COUNT    12u
#define IPC_RCU_MRU_C_CHxMB_COUNT        8u
#define IPC_RCU_MRU_C_NOTIFY_COUNT       4u

/**
 * RCU_MRU_C - Register Layout Typedef
 *
 * @CH_CFG0:      Channel (x) Configuration 0, array offset: 0x0
 * @CH_CFG1:      Channel (x) Configuration 1, array offset: 0x4
 * @CH_MBSTAT:    Channel (x) Mailbox Status, array offset: 0x8
 * @NOTIFY:       Notification, array offset: 0x200
 * @RSTSTAT:      Channel Reset Status, offset: 0x240
 * @MB:           Channel (x) Mailbox (n), offset: 0x10000
 * @CH_MBSTAT:    Channel (x) Reset, offset: 0x10100
 */
typedef struct {
	struct RCU_MRU_C_CHXCONFIG {
		volatile uint32_t CH_CFG0;
		volatile uint32_t CH_CFG1;
		volatile uint32_t CH_MBSTAT;
		uint8_t RESERVED_0[4];
	} CHXCONFIG[IPC_RCU_MRU_C_CHXCONFIG_COUNT];

	uint8_t RESERVED_0[320];
	volatile uint32_t NOTIFY[IPC_RCU_MRU_C_NOTIFY_COUNT];
	uint8_t RESERVED_1[48];
	volatile uint32_t RSTSTAT;
	uint8_t RESERVED_2[64956];

	struct RCU_MRU_C_CHX_MBY {
		volatile uint32_t MB[IPC_RCU_MRU_C_CHxMB_COUNT];
		uint8_t RESERVED_3[224];
		volatile uint32_t RST;
		uint8_t RESERVED_4[65276];
	} CH[IPC_RCU_MRU_C_CHXCONFIG_COUNT];
} IPC_RCU_MRU_C_Type;

/** RCU_MRU_HL - Size of Registers Arrays */
#define IPC_RCU_MRU_HL_CHXCONFIG_COUNT    4u
#define IPC_RCU_MRU_HL_CHxMB_COUNT        16u
#define IPC_RCU_MRU_HL_NOTIFY_COUNT       4u

/**
 * RCU_MRU_HL - Register Layout Typedef
 *
 * @CH_CFG0:      Channel (x) Configuration 0, array offset: 0x0
 * @CH_CFG1:      Channel (x) Configuration 1, array offset: 0x4
 * @CH_MBSTAT:    Channel (x) Mailbox Status, array offset: 0x8
 * @NOTIFY:       Notification, array offset: 0x200
 * @RSTSTAT:      Channel Reset Status, offset: 0x240
 * @MB:           Channel (x) Mailbox (n), offset: 0x10000
 * @CH_MBSTAT:    Channel (x) Reset, offset: 0x10100
 */
typedef struct {
	struct RCU_MRU_HL_CHXCONFIG {
		volatile uint32_t CH_CFG0;
		volatile uint32_t CH_CFG1;
		volatile uint32_t CH_MBSTAT;
		uint8_t RESERVED_0[4];
	} CHXCONFIG[IPC_RCU_MRU_HL_CHXCONFIG_COUNT];

	uint8_t RESERVED_0[448];
	volatile uint32_t NOTIFY[IPC_RCU_MRU_HL_NOTIFY_COUNT];
	uint8_t RESERVED_1[48];
	volatile uint32_t RSTSTAT;
	uint8_t RESERVED_2[69052];

	struct RCU_MRU_HL_CHX_MBY {
		volatile uint32_t MB[IPC_RCU_MRU_HL_CHxMB_COUNT];
		uint8_t RESERVED_3[192];
		volatile uint32_t RST;
		uint8_t RESERVED_4[3836];
	} CH[IPC_RCU_MRU_HL_CHXCONFIG_COUNT];
} IPC_RCU_MRU_HL_Type;


/* FSS_MRU_MB - Size of Registers Arrays */
#define IPC_FSS_MRU_CHXCONFIG_COUNT    15u
#define IPC_FSS_MRU_CHxMB_COUNT        3u
#define IPC_FSS_MRU_NOTIFY_COUNT       4u

/**
 * FSS_MRU_MB - Register Layout Typedef
 *
 * @CH_CFG0:      Channel (x) Configuration 0, array offset: 0x0
 * @CH_CFG1:      Channel (x) Configuration 1, array offset: 0x4
 * @CH_MBSTAT:    Channel (x) Mailbox Status, array offset: 0x8
 * @NOTIFY:       Notification, array offset: 0x200
 * @RSTSTAT:      Channel Reset Status, offset: 0x240
 * @MB:           Channel (x) Mailbox (n), offset: 0x10000
 * @CH_MBSTAT:    Channel (x) Reset, offset: 0x10100
 */
typedef struct {
	struct FSS_MRU_MB1_CHXCONFIG {
		volatile uint32_t CH_CFG0;
		volatile uint32_t CH_CFG1;
		volatile uint32_t CH_MBSTAT;
		uint8_t RESERVED_0[4];
	} CHXCONFIG[IPC_FSS_MRU_CHXCONFIG_COUNT];

	uint8_t RESERVED_0[272];
	volatile uint32_t NOTIFY[IPC_FSS_MRU_NOTIFY_COUNT];
	uint8_t RESERVED_1[48];
	volatile uint32_t RSTSTAT;
	uint8_t RESERVED_2[64956];


	struct FSS_MRU_CHX_MBY {
		volatile uint32_t MB[IPC_FSS_MRU_CHxMB_COUNT];
		uint8_t RESERVED_3[192];
		volatile uint32_t RST;
		uint8_t RESERVED_4[3836];
	} CH[IPC_FSS_MRU_CHXCONFIG_COUNT];
} IPC_FSS_MRU_Type;

#define IPC_MRU_CHx_CFG0_MBE0_ENABLE          (0x00010000u)
#define IPC_MRU_CHx_CFG0_CHE_ENABLE           (0x00000001u)
#define IPC_MRU_CHx_CFG0_IE_ENABLE            (0x00000004u)
#define IPC_MRU_CHx_CFG1_MBIC0_ENABLE         (0x00000001u)
#define IPC_MRU_CHx_MBSTAT0_CLEAR             (0x00010000u)

#define IPC_RTU_MRU_COUNT             (8u)
#define IPC_APS_MRU_COUNT             (8u)
#define IPC_FSS_MRU_COUNT             (11u)
#define IPC_RCU_MRU_COUNT             (32u)

#endif /* IPC_HW_PLATFORM_H */
