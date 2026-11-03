/*
 * The M profile's exception model: its exception numbers, register bits
 * and state (ArmMStateType). A target's arm_target_profile.h includes this
 * to hold the state in its core; the model is in arm_m_exception.h.
 */
#ifndef ARM_COMMON_ARM_M_TYPES_H
#define ARM_COMMON_ARM_M_TYPES_H

#include "arm_types.h"

/* Exception numbers */
#define ARM_M_EXC_RESET		1U
#define ARM_M_EXC_NMI		2U
#define ARM_M_EXC_HARDFAULT	3U
#define ARM_M_EXC_MEMMANAGE	4U
#define ARM_M_EXC_BUSFAULT	5U
#define ARM_M_EXC_USAGEFAULT	6U
#define ARM_M_EXC_SVCALL	11U
#define ARM_M_EXC_DEBUGMON	12U
#define ARM_M_EXC_PENDSV	14U
#define ARM_M_EXC_SYSTICK	15U
#define ARM_M_EXC_IRQ0		16U
#define ARM_M_EXC_NUM		512U	/* the architecture's limit (16 + 496 interrupts) */

/* CONTROL */
#define ARM_M_CONTROL_NPRIV	(1U << 0)
#define ARM_M_CONTROL_SPSEL	(1U << 1)
#define ARM_M_CONTROL_FPCA	(1U << 2)

/* UFSR (CFSR<31:16>) */
#define ARM_M_UFSR_UNDEFINSTR	(1U << 0)
#define ARM_M_UFSR_INVSTATE	(1U << 1)
#define ARM_M_UFSR_INVPC	(1U << 2)
#define ARM_M_UFSR_NOCP		(1U << 3)
#define ARM_M_UFSR_UNALIGNED	(1U << 8)
#define ARM_M_UFSR_DIVBYZERO	(1U << 9)
#define ARM_M_HFSR_FORCED	(1U << 30)

/* FPCCR */
#define ARM_M_FPCCR_LSPACT	(1U << 0)
#define ARM_M_FPCCR_USER	(1U << 1)
#define ARM_M_FPCCR_THREAD	(1U << 3)
#define ARM_M_FPCCR_HFRDY	(1U << 4)
#define ARM_M_FPCCR_MMRDY	(1U << 5)
#define ARM_M_FPCCR_BFRDY	(1U << 6)
#define ARM_M_FPCCR_MONRDY	(1U << 8)
#define ARM_M_FPCCR_LSPEN	(1U << 30)
#define ARM_M_FPCCR_ASPEN	(1U << 31)

/* CCR */
#define ARM_M_CCR_NONBASETHRDENA	(1U << 0)
#define ARM_M_CCR_DIV_0_TRP		(1U << 4)
#define ARM_M_CCR_STKALIGN		(1U << 9)

typedef struct {
	/* configuration of the implementation */
	uint32 num_irq;		/* external interrupts */
	uint32 priority_bits;	/* implemented priority bits (the top ones of 8) */
	bool have_fp;		/* the FP extension */
	/* execution state */
	bool handler_mode;	/* CurrentMode == Mode_Handler */
	uint32 sp_other;	/* the stack pointer that is not R[13] */
	uint32 control;
	uint32 primask;
	uint32 faultmask;
	uint32 basepri;
	uint32 ipsr;		/* IPSR<8:0>: the exception number */
	bool epsr_t;		/* EPSR.T: 0 makes the next instruction an INVSTATE UsageFault */
	bool event;		/* the event register (WFE, SEV) */
	bool lockup;
	bool pending_any;	/* a summary: set whenever an exception may have become pending */
	/* exceptions */
	uint8 active[ARM_M_EXC_NUM];
	uint8 pending[ARM_M_EXC_NUM];
	uint8 enabled[ARM_M_EXC_NUM];	/* NVIC_ISER for interrupts; SHCSR for MemManage, BusFault, UsageFault */
	uint8 priority[ARM_M_EXC_NUM];	/* the priority as written (unimplemented bits zero); fixed for 1..3 */
	/* system control */
	uint32 vtor;
	uint32 aircr_prigroup;
	uint32 ccr;
	uint32 scr;
	uint32 cfsr;
	uint32 hfsr;
	uint32 cpacr;
	uint32 fpccr;
	uint32 fpcar;
	uint32 fpdscr;
} ArmMStateType;

/* SysTick, part of the SCS but kept apart from the exception model. */
typedef struct {
	uint32 csr;
	uint32 rvr;
	uint32 cvr;
	uint32 calib;
} ArmMSysTickType;

/* The CPUID base register, the writable bits of CCR, the identification of the implementation. */
typedef struct {
	uint32 cpuid;
	uint32 ccr_writable;
	bool reset_requested;	/* AIRCR.SYSRESETREQ written */
} ArmMScsConfigType;

#endif /* ARM_COMMON_ARM_M_TYPES_H */
