/*
 * The M profile's System Control Space (0xE000E000-0xE000EFFF): the
 * registers of the exception model (arm_m_exception.h) as software sees
 * them: the NVIC (ISER, ICER, ISPR, ICPR, IABR, IPR, STIR), the System
 * Control Block (CPUID, ICSR, VTOR, AIRCR, SCR, CCR, SHPR1-3, SHCSR, CFSR,
 * HFSR, CPACR), the floating-point context control (FPCCR, FPCAR, FPDSCR),
 * SysTick and ICTR.
 *
 * The target maps the region to ArmMScsRead and ArmMScsWrite (offsets from
 * 0xE000E000; an access of 1, 2 or 4 bytes is done on its word with a byte
 * mask) and clocks SysTick with ArmMSysTickClock.
 */
#ifndef ARM_COMMON_ARM_M_SCS_H
#define ARM_COMMON_ARM_M_SCS_H

#include "arm_m_exception.h"

#define ARM_M_SCS_BASE		0xE000E000U
#define ARM_M_SCS_SIZE		0x1000U

/* SysTick CSR */
#define ARM_M_SYST_ENABLE	(1U << 0)
#define ARM_M_SYST_TICKINT	(1U << 1)
#define ARM_M_SYST_CLKSOURCE	(1U << 2)
#define ARM_M_SYST_COUNTFLAG	(1U << 16)

/* ICSR */
#define ARM_M_ICSR_RETTOBASE	(1U << 11)
#define ARM_M_ICSR_ISRPENDING	(1U << 22)
#define ARM_M_ICSR_PENDSTCLR	(1U << 25)
#define ARM_M_ICSR_PENDSTSET	(1U << 26)
#define ARM_M_ICSR_PENDSVCLR	(1U << 27)
#define ARM_M_ICSR_PENDSVSET	(1U << 28)
#define ARM_M_ICSR_NMIPENDSET	(1U << 31)

/*
 * The highest-priority pending exception, enabled or not, for
 * ICSR.VECTPENDING (0 if none).
 */
static inline uint32 ArmMVectPending(const ArmMStateType *m)
{
	uint32 best = 0U;
	sint32 best_priority = 512;
	uint32 i;

	for (i = 2U; i < (ARM_M_EXC_IRQ0 + m->num_irq); i++) {
		if ((m->pending[i] != 0U) && (ArmMExceptionPriority(m, i) < best_priority)) {
			best = i;
			best_priority = ArmMExceptionPriority(m, i);
		}
	}
	return best;
}

/* The exceptions 4..15 with an SHCSR active bit, and its position. */
static inline sint32 ArmMShcsrActiveBit(uint32 n)
{
	switch (n) {
	case ARM_M_EXC_MEMMANAGE:
		return 0;
	case ARM_M_EXC_BUSFAULT:
		return 1;
	case ARM_M_EXC_USAGEFAULT:
		return 3;
	case ARM_M_EXC_SVCALL:
		return 7;
	case ARM_M_EXC_DEBUGMON:
		return 8;
	case ARM_M_EXC_PENDSV:
		return 10;
	case ARM_M_EXC_SYSTICK:
		return 11;
	default:
		return -1;
	}
}

/* The 32 interrupts of word k of the NVIC's bit registers, as a word. */
static inline uint32 ArmMIrqBits(const ArmMStateType *m, const uint8 *bits, uint32 k)
{
	uint32 value = 0U;
	uint32 i;

	for (i = 0U; i < 32U; i++) {
		uint32 irq = (32U * k) + i;

		if ((irq < m->num_irq) && (bits[ARM_M_EXC_IRQ0 + irq] != 0U)) {
			value |= 1U << i;
		}
	}
	return value;
}

/* bits[irq] = state for the set bits of value (the set and clear registers). */
static inline void ArmMIrqBitsWrite(const ArmMStateType *m, uint8 *bits, uint32 k, uint32 value, uint8 state)
{
	uint32 i;

	for (i = 0U; i < 32U; i++) {
		uint32 irq = (32U * k) + i;

		if ((irq < m->num_irq) && (((value >> i) & 1U) != 0U)) {
			bits[ARM_M_EXC_IRQ0 + irq] = state;
		}
	}
}

/* The priority byte of exception n as read (0 for those without one). */
static inline uint32 ArmMPriorityByte(const ArmMStateType *m, uint32 n)
{
	if (!ArmMExceptionExists(m, n) || (n <= ARM_M_EXC_HARDFAULT)) {
		return 0U;
	}
	return m->priority[n];
}

static inline void ArmMPriorityByteWrite(ArmMStateType *m, uint32 n, uint32 value)
{
	if (ArmMExceptionExists(m, n) && (n > ARM_M_EXC_HARDFAULT)) {
		m->priority[n] = (uint8)(value & ((0xFF00U >> m->priority_bits) & 0xFFU));
	}
}

/* The word at offset (word aligned) as read. */
static inline uint32 ArmMScsReadWord(const ArmMStateType *m, const ArmMSysTickType *st, const ArmMScsConfigType *cfg, uint32 offset)
{
	uint32 value;
	uint32 i;

	if ((offset >= 0x100U) && (offset < 0x140U)) {			/* ISER, ICER */
		return ArmMIrqBits(m, m->enabled, (offset - 0x100U) / 4U);
	}
	if ((offset >= 0x180U) && (offset < 0x1C0U)) {
		return ArmMIrqBits(m, m->enabled, (offset - 0x180U) / 4U);
	}
	if ((offset >= 0x200U) && (offset < 0x240U)) {			/* ISPR, ICPR */
		return ArmMIrqBits(m, m->pending, (offset - 0x200U) / 4U);
	}
	if ((offset >= 0x280U) && (offset < 0x2C0U)) {
		return ArmMIrqBits(m, m->pending, (offset - 0x280U) / 4U);
	}
	if ((offset >= 0x300U) && (offset < 0x340U)) {			/* IABR */
		return ArmMIrqBits(m, m->active, (offset - 0x300U) / 4U);
	}
	if ((offset >= 0x400U) && (offset < 0x5F0U)) {			/* IPR */
		value = 0U;
		for (i = 0U; i < 4U; i++) {
			value |= ArmMPriorityByte(m, ARM_M_EXC_IRQ0 + (offset - 0x400U) + i) << (8U * i);
		}
		return value;
	}
	if ((offset >= 0xD18U) && (offset < 0xD24U)) {			/* SHPR1-3: exceptions 4..15 */
		value = 0U;
		for (i = 0U; i < 4U; i++) {
			value |= ArmMPriorityByte(m, 4U + (offset - 0xD18U) + i) << (8U * i);
		}
		return value;
	}
	switch (offset) {
	case 0x004U:	/* ICTR: INTLINESNUM */
		return (m->num_irq == 0U) ? 0U : ((m->num_irq - 1U) / 32U);
	case 0x010U:
		return st->csr;
	case 0x014U:
		return st->rvr;
	case 0x018U:
		return st->cvr;
	case 0x01CU:
		return st->calib;
	case 0xD00U:
		return cfg->cpuid;
	case 0xD04U: {	/* ICSR */
		uint32 vectpending = ArmMVectPending(m);
		bool isrpending = FALSE;

		value = (m->ipsr & 0x1FFU) | ((vectpending & 0x1FFU) << 12);
		for (i = ARM_M_EXC_IRQ0; i < (ARM_M_EXC_IRQ0 + m->num_irq); i++) {
			isrpending = isrpending || (m->pending[i] != 0U);
		}
		if (isrpending) {
			value |= ARM_M_ICSR_ISRPENDING;
		}
		if (ArmMActiveCount(m) <= 1U) {
			value |= ARM_M_ICSR_RETTOBASE;
		}
		if (m->pending[ARM_M_EXC_SYSTICK] != 0U) {
			value |= ARM_M_ICSR_PENDSTSET;
		}
		if (m->pending[ARM_M_EXC_PENDSV] != 0U) {
			value |= ARM_M_ICSR_PENDSVSET;
		}
		if (m->pending[ARM_M_EXC_NMI] != 0U) {
			value |= ARM_M_ICSR_NMIPENDSET;
		}
		return value;
	}
	case 0xD08U:
		return m->vtor;
	case 0xD0CU:	/* AIRCR: VECTKEYSTAT 0xFA05, PRIGROUP */
		return 0xFA050000U | (m->aircr_prigroup << 8);
	case 0xD10U:
		return m->scr;
	case 0xD14U:
		return m->ccr;
	case 0xD24U: {	/* SHCSR */
		value = 0U;
		for (i = ARM_M_EXC_MEMMANAGE; i <= ARM_M_EXC_SYSTICK; i++) {
			sint32 bit = ArmMShcsrActiveBit(i);

			if ((bit >= 0) && (m->active[i] != 0U)) {
				value |= 1U << (uint32)bit;
			}
		}
		value |= (m->pending[ARM_M_EXC_USAGEFAULT] != 0U) ? (1U << 12) : 0U;
		value |= (m->pending[ARM_M_EXC_MEMMANAGE] != 0U) ? (1U << 13) : 0U;
		value |= (m->pending[ARM_M_EXC_BUSFAULT] != 0U) ? (1U << 14) : 0U;
		value |= (m->pending[ARM_M_EXC_SVCALL] != 0U) ? (1U << 15) : 0U;
		value |= (m->enabled[ARM_M_EXC_MEMMANAGE] != 0U) ? (1U << 16) : 0U;
		value |= (m->enabled[ARM_M_EXC_BUSFAULT] != 0U) ? (1U << 17) : 0U;
		value |= (m->enabled[ARM_M_EXC_USAGEFAULT] != 0U) ? (1U << 18) : 0U;
		return value;
	}
	case 0xD28U:
		return m->cfsr;
	case 0xD2CU:
		return m->hfsr;
	case 0xD88U:
		return m->cpacr;
	case 0xF34U:
		return m->fpccr;
	case 0xF38U:
		return m->fpcar;
	case 0xF3CU:
		return m->fpdscr;
	default:
		return 0U;	/* reserved: RAZ/WI */
	}
}

/*
 * A write of value to the bytes of mask of the word at offset (word
 * aligned): the byte-accessible registers (IPR, SHPR) take the bytes of
 * mask; the others take the bytes of mask in place of the ones read.
 */
static inline void ArmMScsWriteWord(TargetCoreType *core, ArmMStateType *m, ArmMSysTickType *st, ArmMScsConfigType *cfg,
	uint32 offset, uint32 value, uint32 mask)
{
	uint32 i;

	if ((offset >= 0x400U) && (offset < 0x5F0U)) {
		for (i = 0U; i < 4U; i++) {
			if (((mask >> (8U * i)) & 0xFFU) != 0U) {
				ArmMPriorityByteWrite(m, ARM_M_EXC_IRQ0 + (offset - 0x400U) + i, value >> (8U * i));
			}
		}
		return;
	}
	if ((offset >= 0xD18U) && (offset < 0xD24U)) {
		for (i = 0U; i < 4U; i++) {
			if (((mask >> (8U * i)) & 0xFFU) != 0U) {
				ArmMPriorityByteWrite(m, 4U + (offset - 0xD18U) + i, value >> (8U * i));
			}
		}
		return;
	}
	value &= mask;
	if ((offset >= 0x100U) && (offset < 0x140U)) {
		ArmMIrqBitsWrite(m, m->enabled, (offset - 0x100U) / 4U, value, 1U);
		return;
	}
	if ((offset >= 0x180U) && (offset < 0x1C0U)) {
		ArmMIrqBitsWrite(m, m->enabled, (offset - 0x180U) / 4U, value, 0U);
		return;
	}
	if ((offset >= 0x200U) && (offset < 0x240U)) {
		ArmMIrqBitsWrite(m, m->pending, (offset - 0x200U) / 4U, value, 1U);
		return;
	}
	if ((offset >= 0x280U) && (offset < 0x2C0U)) {
		ArmMIrqBitsWrite(m, m->pending, (offset - 0x280U) / 4U, value, 0U);
		return;
	}
	switch (offset) {
	case 0x010U:	/* SYST_CSR: COUNTFLAG is read-only */
		st->csr = (st->csr & ARM_M_SYST_COUNTFLAG) | (value & 0x7U) | (st->csr & ~mask & 0x7U);
		break;
	case 0x014U:
		st->rvr = ((st->rvr & ~mask) | value) & 0x00FFFFFFU;
		break;
	case 0x018U:	/* any write clears the counter and COUNTFLAG */
		st->cvr = 0U;
		st->csr &= ~ARM_M_SYST_COUNTFLAG;
		break;
	case 0xD04U:	/* ICSR */
		if ((value & ARM_M_ICSR_NMIPENDSET) != 0U) {
			m->pending[ARM_M_EXC_NMI] = 1U;
		}
		if ((value & ARM_M_ICSR_PENDSVSET) != 0U) {
			m->pending[ARM_M_EXC_PENDSV] = 1U;
		}
		else if ((value & ARM_M_ICSR_PENDSVCLR) != 0U) {
			m->pending[ARM_M_EXC_PENDSV] = 0U;
		}
		if ((value & ARM_M_ICSR_PENDSTSET) != 0U) {
			m->pending[ARM_M_EXC_SYSTICK] = 1U;
		}
		else if ((value & ARM_M_ICSR_PENDSTCLR) != 0U) {
			m->pending[ARM_M_EXC_SYSTICK] = 0U;
		}
		break;
	case 0xD08U:	/* VTOR: TBLOFF, bits 31:7 */
		m->vtor = ((m->vtor & ~mask) | value) & 0xFFFFFF80U;
		break;
	case 0xD0CU:	/* AIRCR: with VECTKEY 0x05FA */
		if ((mask == 0xFFFFFFFFU) && ((value >> 16) == 0x05FAU)) {
			m->aircr_prigroup = (value >> 8) & 0x7U;
			if ((value & (1U << 2)) != 0U) {
				cfg->reset_requested = TRUE;
			}
		}
		break;
	case 0xD10U:	/* SCR: SLEEPONEXIT, SLEEPDEEP, SEVONPEND */
		m->scr = ((m->scr & ~mask) | value) & 0x16U;
		break;
	case 0xD14U:
		m->ccr = ((m->ccr & ~mask) | value) & cfg->ccr_writable;
		break;
	case 0xD24U:	/* SHCSR */
		for (i = ARM_M_EXC_MEMMANAGE; i <= ARM_M_EXC_SYSTICK; i++) {
			sint32 bit = ArmMShcsrActiveBit(i);

			if ((bit >= 0) && (((mask >> (uint32)bit) & 1U) != 0U)) {
				m->active[i] = (uint8)((value >> (uint32)bit) & 1U);
			}
		}
		if ((mask & (1U << 12)) != 0U) {
			m->pending[ARM_M_EXC_USAGEFAULT] = (uint8)((value >> 12) & 1U);
		}
		if ((mask & (1U << 13)) != 0U) {
			m->pending[ARM_M_EXC_MEMMANAGE] = (uint8)((value >> 13) & 1U);
		}
		if ((mask & (1U << 14)) != 0U) {
			m->pending[ARM_M_EXC_BUSFAULT] = (uint8)((value >> 14) & 1U);
		}
		if ((mask & (1U << 15)) != 0U) {
			m->pending[ARM_M_EXC_SVCALL] = (uint8)((value >> 15) & 1U);
		}
		if ((mask & (1U << 16)) != 0U) {
			m->enabled[ARM_M_EXC_MEMMANAGE] = (uint8)((value >> 16) & 1U);
		}
		if ((mask & (1U << 17)) != 0U) {
			m->enabled[ARM_M_EXC_BUSFAULT] = (uint8)((value >> 17) & 1U);
		}
		if ((mask & (1U << 18)) != 0U) {
			m->enabled[ARM_M_EXC_USAGEFAULT] = (uint8)((value >> 18) & 1U);
		}
		break;
	case 0xD28U:	/* CFSR, HFSR: write 1 to clear */
		m->cfsr &= ~value;
		break;
	case 0xD2CU:
		m->hfsr &= ~value;
		break;
	case 0xD88U:	/* CPACR: CP10, CP11 */
		m->cpacr = ((m->cpacr & ~mask) | value) & 0x00F00000U;
		break;
	case 0xF00U:	/* STIR */
		if (((value & 0x1FFU) < m->num_irq)) {
			m->pending[ARM_M_EXC_IRQ0 + (value & 0x1FFU)] = 1U;
		}
		break;
	case 0xF34U:	/* FPCCR: ASPEN, LSPEN and the state bits */
		if (m->have_fp) {
			m->fpccr = ((m->fpccr & ~mask) | value) & 0xC000017BU;
		}
		break;
	case 0xF38U:
		if (m->have_fp) {
			m->fpcar = ((m->fpcar & ~mask) | value) & ~7U;
		}
		break;
	case 0xF3CU:	/* FPDSCR: AHP, DN, FZ, RMode */
		if (m->have_fp) {
			m->fpdscr = ((m->fpdscr & ~mask) | value) & 0x07C00000U;
		}
		break;
	default:
		break;
	}
	(void)core;
}

/* The byte mask and the shift of an access of size bytes at offset. */
static inline uint32 ArmMScsMask(uint32 offset, uint32 size)
{
	uint32 bytes = (size >= 4U) ? 0xFFFFFFFFU : ((1U << (8U * size)) - 1U);

	return bytes << (8U * (offset & 3U));
}

static inline uint32 ArmMScsRead(const ArmMStateType *m, const ArmMSysTickType *st, ArmMScsConfigType *cfg, uint32 offset, uint32 size)
{
	uint32 word = ArmMScsReadWord(m, st, cfg, offset & ~3U);

	return (word & ArmMScsMask(offset, size)) >> (8U * (offset & 3U));
}

/* A read of SYST_CSR clears COUNTFLAG: the target calls this after such a read. */
static inline void ArmMScsReadSideEffects(ArmMSysTickType *st, uint32 offset)
{
	if ((offset & ~3U) == 0x010U) {
		st->csr &= ~ARM_M_SYST_COUNTFLAG;
	}
}

static inline void ArmMScsWrite(TargetCoreType *core, ArmMStateType *m, ArmMSysTickType *st, ArmMScsConfigType *cfg,
	uint32 offset, uint32 size, uint32 value)
{
	ArmMScsWriteWord(core, m, st, cfg, offset & ~3U, value << (8U * (offset & 3U)), ArmMScsMask(offset, size));
}

/* The counter reaching 0: COUNTFLAG, and with TICKINT SysTick pending. */
static inline void ArmMSysTickZero(ArmMStateType *m, ArmMSysTickType *st)
{
	st->csr |= ARM_M_SYST_COUNTFLAG;
	if ((st->csr & ARM_M_SYST_TICKINT) != 0U) {
		m->pending[ARM_M_EXC_SYSTICK] = 1U;
	}
}

/*
 * SysTick, for ticks clocks of its source: the counter counts down; when it
 * reaches 0 it sets COUNTFLAG and, with TICKINT, makes SysTick pending; at
 * the next clock it reloads RVR. A counter of 0 with RVR 0 stays 0. Many
 * ticks at once (a skip of the halted CPU) are counted in one step.
 */
static inline void ArmMSysTickClock(ArmMStateType *m, ArmMSysTickType *st, uint64 ticks)
{
	uint64 period;
	uint64 rest;

	if (((st->csr & ARM_M_SYST_ENABLE) == 0U) || (ticks == 0U)) {
		return;
	}
	if (st->cvr == 0U) {
		/* the reload */
		st->cvr = st->rvr;
		ticks--;
		if ((ticks == 0U) || (st->cvr == 0U)) {
			return;
		}
	}
	if (ticks < st->cvr) {
		st->cvr -= (uint32)ticks;
		return;
	}
	/* down to 0, then whole periods of RVR + 1 (a reload and RVR counts) */
	ticks -= st->cvr;
	st->cvr = 0U;
	ArmMSysTickZero(m, st);
	if ((ticks == 0U) || (st->rvr == 0U)) {
		return;
	}
	period = (uint64)st->rvr + 1U;
	rest = ticks % period;
	if (rest != 0U) {
		st->cvr = st->rvr - (uint32)(rest - 1U);
	}
}

/* The ticks until the counter next reaches 0 with TICKINT set, or 0 when it will not. */
static inline uint64 ArmMSysTickTicksToInterrupt(const ArmMSysTickType *st)
{
	if (((st->csr & ARM_M_SYST_ENABLE) == 0U) || ((st->csr & ARM_M_SYST_TICKINT) == 0U)) {
		return 0U;
	}
	if (st->cvr != 0U) {
		return st->cvr;
	}
	return (st->rvr == 0U) ? 0U : ((uint64)st->rvr + 1U);
}

#endif /* ARM_COMMON_ARM_M_SCS_H */
