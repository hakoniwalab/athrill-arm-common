/*
 * The exception model of the M profile (Armv7-M, B1.5): the modes, the two
 * stack pointers, the special registers (PRIMASK, FAULTMASK, BASEPRI,
 * CONTROL), the exceptions' pending, active and priority state, and the
 * pseudo-code of exception entry (PushStack, ExceptionTaken) and return
 * (ExceptionReturn, PopStack), with the floating-point context and its
 * lazy preservation.
 *
 * The state is ArmMStateType, held by the target in its core and reached
 * through the profile (ArmMState). R[13] is the current stack pointer; the
 * other one is kept here and swapped in when the selection changes
 * (LookUpSP()). The system registers that hold this state (SCS: NVIC, SCB,
 * SysTick, the FPU's) are in arm_m_scs.h.
 *
 * Exceptions are taken between instructions: the target asks for the
 * exception to take before each instruction (ArmMPendingToTake), and calls
 * ArmMExceptionEntry for it, or for a synchronous exception (SVC, a fault)
 * after the instruction that raised it.
 */
#ifndef ARM_COMMON_ARM_M_EXCEPTION_H
#define ARM_COMMON_ARM_M_EXCEPTION_H

#include "arm_profile.h"
#include "arm_m_types.h"

/*
 * Reset (TakeReset): Thread mode, the main stack, privileged, nothing
 * pending or active, priorities 0; CCR.STKALIGN set as on Armv7-M's
 * Cortex-M3 r2p0 and later; FPCCR with automatic and lazy preservation.
 * The target then loads SP_main and the reset handler from the vector table.
 */
static inline void ArmMReset(ArmMStateType *m, uint32 num_irq, uint32 priority_bits, bool have_fp)
{
	uint32 i;

	m->num_irq = num_irq;
	m->priority_bits = priority_bits;
	m->have_fp = have_fp;
	m->handler_mode = FALSE;
	m->sp_other = 0U;
	m->control = 0U;
	m->primask = 0U;
	m->faultmask = 0U;
	m->basepri = 0U;
	m->ipsr = 0U;
	m->epsr_t = TRUE;
	m->event = FALSE;
	m->lockup = FALSE;
	for (i = 0U; i < ARM_M_EXC_NUM; i++) {
		m->active[i] = 0U;
		m->pending[i] = 0U;
		m->enabled[i] = 0U;
		m->priority[i] = 0U;
	}
	m->vtor = 0U;
	m->aircr_prigroup = 0U;
	m->ccr = ARM_M_CCR_STKALIGN;
	m->scr = 0U;
	m->cfsr = 0U;
	m->hfsr = 0U;
	m->cpacr = 0U;
	m->fpccr = have_fp ? (ARM_M_FPCCR_ASPEN | ARM_M_FPCCR_LSPEN) : 0U;
	m->fpcar = 0U;
	m->fpdscr = 0U;
}

/* The exceptions that exist: 1..15 by the architecture's list, then the interrupts. */
static inline bool ArmMExceptionExists(const ArmMStateType *m, uint32 n)
{
	switch (n) {
	case ARM_M_EXC_NMI:
	case ARM_M_EXC_HARDFAULT:
	case ARM_M_EXC_MEMMANAGE:
	case ARM_M_EXC_BUSFAULT:
	case ARM_M_EXC_USAGEFAULT:
	case ARM_M_EXC_SVCALL:
	case ARM_M_EXC_DEBUGMON:
	case ARM_M_EXC_PENDSV:
	case ARM_M_EXC_SYSTICK:
		return TRUE;
	default:
		return (n >= ARM_M_EXC_IRQ0) && (n < (ARM_M_EXC_IRQ0 + m->num_irq));
	}
}

/* CurrentModeIsPrivileged() = (CurrentMode == Mode_Handler || CONTROL.nPRIV == '0') */
static inline bool ArmMPrivileged(const ArmMStateType *m)
{
	return m->handler_mode || ((m->control & ARM_M_CONTROL_NPRIV) == 0U);
}

/*
 * The stack pointers. R[13] is SP_process in Thread mode with
 * CONTROL.SPSEL set, SP_main otherwise (LookUpSP()).
 */
static inline bool ArmMProcessStackSelected(const ArmMStateType *m)
{
	return !m->handler_mode && ((m->control & ARM_M_CONTROL_SPSEL) != 0U);
}

static inline uint32 ArmMSPMain(const TargetCoreType *core)
{
	const ArmMStateType *m = ArmMState(core);

	return ArmMProcessStackSelected(m) ? m->sp_other : ArmR(core, 13U);
}

static inline uint32 ArmMSPProcess(const TargetCoreType *core)
{
	const ArmMStateType *m = ArmMState(core);

	return ArmMProcessStackSelected(m) ? ArmR(core, 13U) : m->sp_other;
}

static inline void ArmMSPMainWrite(TargetCoreType *core, uint32 value)
{
	ArmMStateType *m = ArmMState(core);

	if (ArmMProcessStackSelected(m)) {
		m->sp_other = value;
	}
	else {
		ArmRWrite(core, 13U, value);
	}
}

static inline void ArmMSPProcessWrite(TargetCoreType *core, uint32 value)
{
	ArmMStateType *m = ArmMState(core);

	if (ArmMProcessStackSelected(m)) {
		ArmRWrite(core, 13U, value);
	}
	else {
		m->sp_other = value;
	}
}

/* Sets CurrentMode and CONTROL.SPSEL, keeping R[13] the selected stack pointer. */
static inline void ArmMSetModeAndSPSEL(TargetCoreType *core, bool handler_mode, bool spsel)
{
	ArmMStateType *m = ArmMState(core);
	bool before = ArmMProcessStackSelected(m);
	bool after;
	uint32 sp;

	m->handler_mode = handler_mode;
	m->control = spsel ? (m->control | ARM_M_CONTROL_SPSEL) : (m->control & ~ARM_M_CONTROL_SPSEL);
	after = ArmMProcessStackSelected(m);
	if (before != after) {
		sp = ArmR(core, 13U);
		ArmRWrite(core, 13U, m->sp_other);
		m->sp_other = sp;
	}
}

/*
 * Priorities
 */

/* The priority of exception n: -3, -2, -1 for Reset, NMI, HardFault; the programmed one otherwise. */
static inline sint32 ArmMExceptionPriority(const ArmMStateType *m, uint32 n)
{
	if (n == ARM_M_EXC_RESET) {
		return -3;
	}
	if (n == ARM_M_EXC_NMI) {
		return -2;
	}
	if (n == ARM_M_EXC_HARDFAULT) {
		return -1;
	}
	return (sint32)m->priority[n];
}

/* The group priority: the priority without its subpriority bits (AIRCR.PRIGROUP). */
static inline sint32 ArmMGroupPriority(const ArmMStateType *m, sint32 priority)
{
	sint32 groupvalue = (sint32)(2U << m->aircr_prigroup);

	if (priority < 0) {
		return priority;
	}
	return priority - (priority % groupvalue);
}

/*
 * integer ExecutionPriority()
 *     highestpri = 256; // Priority of Thread mode with no active exceptions
 *     boostedpri = 256; // Priority influence of BASEPRI, PRIMASK and FAULTMASK
 *     subgroupshift = UInt(AIRCR.PRIGROUP);
 *     groupvalue = 2 << subgroupshift;
 *     for (i=2; i<512; i=i+1)  // IPSR values of the exception handlers
 *         if ExceptionActive[i] == '1' then
 *             if ExceptionPriority[i] < highestpri then
 *                 highestpri = ExceptionPriority[i];
 *                 // Include the PRIGROUP effect
 *                 subgroupvalue = highestpri MOD groupvalue;
 *                 highestpri = highestpri - subgroupvalue;
 *     if UInt(BASEPRI<7:0>) != 0 then
 *         boostedpri = UInt(BASEPRI<7:0>);
 *         // Include the PRIGROUP effect
 *         subgroupvalue = boostedpri MOD groupvalue;
 *         boostedpri = boostedpri - subgroupvalue;
 *     if PRIMASK<0> == '1' then boostedpri = 0;
 *     if FAULTMASK<0> == '1' then boostedpri = -1;
 *     if boostedpri < highestpri then priority = boostedpri;
 *     else priority = highestpri;
 *     return priority;
 */
static inline sint32 ArmMExecutionPriority(const ArmMStateType *m)
{
	sint32 highestpri = 256;
	sint32 boostedpri = 256;
	uint32 i;

	for (i = 2U; i < ARM_M_EXC_NUM; i++) {
		if ((m->active[i] != 0U) && (ArmMExceptionPriority(m, i) < highestpri)) {
			highestpri = ArmMGroupPriority(m, ArmMExceptionPriority(m, i));
		}
	}
	if ((m->basepri & 0xFFU) != 0U) {
		boostedpri = ArmMGroupPriority(m, (sint32)(m->basepri & 0xFFU));
	}
	if ((m->primask & 1U) != 0U) {
		boostedpri = 0;
	}
	if ((m->faultmask & 1U) != 0U) {
		boostedpri = -1;
	}
	return (boostedpri < highestpri) ? boostedpri : highestpri;
}

/*
 * The pending exception to take now, or 0: the enabled pending exception
 * of the highest priority (lowest value; between equal priorities the lowest
 * number), if its group priority is higher than the execution priority.
 */
static inline uint32 ArmMPendingToTake(const ArmMStateType *m)
{
	uint32 best = 0U;
	sint32 best_priority = 512;
	uint32 i;

	for (i = 2U; i < (ARM_M_EXC_IRQ0 + m->num_irq); i++) {
		bool enabled = (i < ARM_M_EXC_IRQ0) ? ((i > ARM_M_EXC_USAGEFAULT) || (i < ARM_M_EXC_MEMMANAGE) || (m->enabled[i] != 0U))
			: (m->enabled[i] != 0U);

		if ((m->pending[i] != 0U) && enabled && (ArmMExceptionPriority(m, i) < best_priority)) {
			best = i;
			best_priority = ArmMExceptionPriority(m, i);
		}
	}
	if ((best != 0U) && (ArmMGroupPriority(m, best_priority) < ArmMExecutionPriority(m))) {
		return best;
	}
	return 0U;
}

/*
 * The xPSR as one word: APSR (N Z C V Q, GE), IPSR, EPSR (T, ICI/IT).
 * IT<1:0> are bits 26:25, IT<7:2> bits 15:10.
 */
static inline uint32 ArmMXPSR(const TargetCoreType *core)
{
	const ArmMStateType *m = ArmMState(core);
	uint32 it = ArmITState(core);

	return (ArmAPSR(core) & (ARM_APSR_N | ARM_APSR_Z | ARM_APSR_C | ARM_APSR_V | ARM_APSR_Q | ARM_APSR_GE))
		| ((it & 0x3U) << 25) | (((it >> 2) & 0x3FU) << 10)
		| (m->epsr_t ? (1U << 24) : 0U)
		| (m->ipsr & 0x1FFU);
}

/*
 * Floating-point context
 */

/*
 * PreserveFPState()
 *     // Preserve FP state using address, privilege and relative priorities recorded during
 *     // original stacking. Derived exceptions are handled by TakePreserveFPException().
 *     // The checks usually performed for stacking using ValidateAddress() are performed, with
 *     // the value of ispriv for the checks taken from FPCCR.USER.
 *     isPriv = (FPCCR.USER == '0');
 *     for i = 0 to 15
 *         MemA_with_priv[FPCAR+(4*i),4, isPriv] = S[i];
 *     MemA_with_priv[FPCAR+0x40,4, isPriv] = FPSCR;
 *     FPCCR.LSPACT = '0';
 * (Without an MPU, the privilege does not change the accesses.)
 */
static inline Std_ReturnType ArmMPreserveFPState(TargetCoreType *core)
{
	ArmMStateType *m = ArmMState(core);
	Std_ReturnType err;
	uint32 i;

	for (i = 0U; i < 16U; i++) {
		err = ArmMemA_W(core, m->fpcar + (4U * i), 4U, ArmS(core, i));
		if (err != STD_E_OK) {
			return err;
		}
	}
	err = ArmMemA_W(core, m->fpcar + 0x40U, 4U, ArmFPSCR(core));
	if (err != STD_E_OK) {
		return err;
	}
	m->fpccr &= ~ARM_M_FPCCR_LSPACT;
	return STD_E_OK;
}

/*
 * ExecuteFPCheck(), after the coprocessor access check (CPACR, not modelled:
 * the FPU is enabled):
 *     if FPCCR.LSPACT == '1' then PreserveFPState();  // the lazily stacked context
 *     if FPCCR.ASPEN == '1' && CONTROL.FPCA == '0' then  // a new floating-point context
 *         FPSCR = FPDSCR;
 *         CONTROL.FPCA = '1';
 */
static inline Std_ReturnType ArmMExecuteFPCheck(TargetCoreType *core)
{
	ArmMStateType *m = ArmMState(core);
	Std_ReturnType err;

	if ((m->fpccr & ARM_M_FPCCR_LSPACT) != 0U) {
		err = ArmMPreserveFPState(core);
		if (err != STD_E_OK) {
			return err;
		}
	}
	if (((m->fpccr & ARM_M_FPCCR_ASPEN) != 0U) && ((m->control & ARM_M_CONTROL_FPCA) == 0U)) {
		ArmFPSCRWrite(core, m->fpdscr);
		m->control |= ARM_M_CONTROL_FPCA;
	}
	return STD_E_OK;
}

/*
 * UpdateFPCCR(frameptr): the lazy preservation of a context stacked at
 * frameptr, with what the preservation will need:
 *     FPCAR.ADDRESS = (frameptr + 0x20)<31:3>;
 *     FPCCR.LSPACT = '1';
 *     FPCCR.USER = if CurrentModeIsPrivileged() then '0' else '1';
 *     FPCCR.THREAD = if CurrentMode == Mode_Thread then '1' else '0';
 *     FPCCR.HFRDY = if ExecutionPriority() > -1 then '1' else '0';
 *     FPCCR.BFRDY, MMRDY: whether those could be taken now
 *     FPCCR.MONRDY: with DEMCR.MON_EN only (no debug monitor here: 0)
 */
static inline void ArmMUpdateFPCCR(ArmMStateType *m, uint32 frameptr)
{
	sint32 execpri = ArmMExecutionPriority(m);
	uint32 fpccr = m->fpccr & ~(ARM_M_FPCCR_USER | ARM_M_FPCCR_THREAD | ARM_M_FPCCR_HFRDY | ARM_M_FPCCR_MMRDY
		| ARM_M_FPCCR_BFRDY | ARM_M_FPCCR_MONRDY);

	m->fpcar = (frameptr + 0x20U) & ~7U;
	fpccr |= ARM_M_FPCCR_LSPACT;
	if (!ArmMPrivileged(m)) {
		fpccr |= ARM_M_FPCCR_USER;
	}
	if (!m->handler_mode) {
		fpccr |= ARM_M_FPCCR_THREAD;
	}
	if (execpri > -1) {
		fpccr |= ARM_M_FPCCR_HFRDY;
	}
	if ((m->enabled[ARM_M_EXC_MEMMANAGE] != 0U) && (ArmMExceptionPriority(m, ARM_M_EXC_MEMMANAGE) < execpri)) {
		fpccr |= ARM_M_FPCCR_MMRDY;
	}
	if ((m->enabled[ARM_M_EXC_BUSFAULT] != 0U) && (ArmMExceptionPriority(m, ARM_M_EXC_BUSFAULT) < execpri)) {
		fpccr |= ARM_M_FPCCR_BFRDY;
	}
	m->fpccr = fpccr;
}

/*
 * Exception entry
 */

/*
 * PushStack(ExceptionType)
 *     if HaveFPExt() && CONTROL.FPCA == '1' then
 *         framesize = 0x68; forcealign = '1';
 *     else
 *         framesize = 0x20; forcealign = CCR.STKALIGN;
 *     spmask = NOT(ZeroExtend(forcealign:'00',32));
 *     if CONTROL.SPSEL == '1' && CurrentMode == Mode_Thread then
 *         frameptralign = SP_process<2> AND forcealign;
 *         SP_process = (SP_process - framesize) AND spmask;
 *         frameptr = SP_process;
 *     else
 *         frameptralign = SP_main<2> AND forcealign;
 *         SP_main = (SP_main - framesize) AND spmask;
 *         frameptr = SP_main;
 *     // only the stack locations, not the store order, are architected
 *     MemA[frameptr,4]      = R[0];
 *     MemA[frameptr+0x4,4]  = R[1];
 *     MemA[frameptr+0x8,4]  = R[2];
 *     MemA[frameptr+0xC,4]  = R[3];
 *     MemA[frameptr+0x10,4] = R[12];
 *     MemA[frameptr+0x14,4] = LR;
 *     MemA[frameptr+0x18,4] = ReturnAddress(ExceptionType);
 *     MemA[frameptr+0x1C,4] = (XPSR<31:10>:frameptralign:XPSR<8:0>);
 *     if HaveFPExt() && CONTROL.FPCA == '1' then
 *         if FPCCR.LSPEN == '0' then
 *             CheckVFPEnabled();
 *             for i = 0 to 15
 *                 MemA[frameptr+0x20+(4*i),4] = S[i];
 *             MemA[frameptr+0x60,4] = FPSCR;
 *         else
 *             UpdateFPCCR(frameptr);
 *     if HaveFPExt() then
 *         if CurrentMode==Mode_Handler then
 *             LR = Ones(27):NOT(CONTROL.FPCA):'0001';
 *         else
 *             LR = Ones(27):NOT(CONTROL.FPCA):'1':CONTROL.SPSEL:'01';
 *     else
 *         if CurrentMode==Mode_Handler then
 *             LR = Ones(28):'0001';
 *         else
 *             LR = Ones(29):CONTROL.SPSEL:'01';
 * (A fault while stacking would be a derived exception; it stops the run.)
 */
static inline Std_ReturnType ArmMPushStack(TargetCoreType *core, uint32 return_address)
{
	ArmMStateType *m = ArmMState(core);
	bool fp_frame = m->have_fp && ((m->control & ARM_M_CONTROL_FPCA) != 0U);
	uint32 framesize = fp_frame ? 0x68U : 0x20U;
	bool forcealign = fp_frame || ((m->ccr & ARM_M_CCR_STKALIGN) != 0U);
	uint32 spmask = forcealign ? ~4U : ~0U;
	bool process = ArmMProcessStackSelected(m);
	uint32 sp = process ? ArmMSPProcess(core) : ArmMSPMain(core);
	uint32 frameptralign = ((sp & 4U) != 0U) && forcealign ? 1U : 0U;
	uint32 frameptr = (sp - framesize) & spmask;
	uint32 values[8];
	uint32 i;
	Std_ReturnType err;

	if (process) {
		ArmMSPProcessWrite(core, frameptr);
	}
	else {
		ArmMSPMainWrite(core, frameptr);
	}
	values[0] = ArmR(core, 0U);
	values[1] = ArmR(core, 1U);
	values[2] = ArmR(core, 2U);
	values[3] = ArmR(core, 3U);
	values[4] = ArmR(core, 12U);
	values[5] = ArmR(core, 14U);
	values[6] = return_address;
	values[7] = (ArmMXPSR(core) & ~(1U << 9)) | (frameptralign << 9);
	for (i = 0U; i < 8U; i++) {
		err = ArmMemA_W(core, frameptr + (4U * i), 4U, values[i]);
		if (err != STD_E_OK) {
			return err;
		}
	}
	if (fp_frame) {
		if ((m->fpccr & ARM_M_FPCCR_LSPEN) == 0U) {
			for (i = 0U; i < 16U; i++) {
				err = ArmMemA_W(core, frameptr + 0x20U + (4U * i), 4U, ArmS(core, i));
				if (err != STD_E_OK) {
					return err;
				}
			}
			err = ArmMemA_W(core, frameptr + 0x60U, 4U, ArmFPSCR(core));
			if (err != STD_E_OK) {
				return err;
			}
		}
		else {
			ArmMUpdateFPCCR(m, frameptr);
		}
	}
	if (m->handler_mode) {
		ArmRWrite(core, 14U, 0xFFFFFFE1U | (fp_frame ? 0U : 0x10U));
	}
	else {
		ArmRWrite(core, 14U, 0xFFFFFFE9U | (fp_frame ? 0U : 0x10U) | (process ? 0x4U : 0U));
	}
	if (!m->have_fp) {
		ArmRWrite(core, 14U, ArmR(core, 14U) | 0x10U);
	}
	return STD_E_OK;
}

/*
 * ExceptionTaken(integer ExceptionNumber)
 *     bit tbit;
 *     bits(32) tmp;
 *     for i = 0 to 3
 *         R[i] = bits(32) UNKNOWN;
 *     R[12] = bits(32) UNKNOWN;
 *     tmp = MemA[VTOR+(4*ExceptionNumber),4];
 *     BranchTo(tmp AND 0xFFFFFFFE<31:0>);
 *     tbit = tmp<0>;
 *     CurrentMode = Mode_Handler;
 *     APSR = bits(32) UNKNOWN;  // Flags UNPREDICTABLE due to other activations
 *     IPSR<8:0> = ExceptionNumber<8:0>;  // ExceptionNumber set in IPSR
 *     EPSR.T = tbit;  // T-bit set from vector
 *     EPSR.IT<7:0> = Zeros(8);  // IT/ICI bits cleared
 *     // PRIMASK, FAULTMASK, BASEPRI unchanged on exception entry
 *     if HaveFPExt() then
 *         CONTROL.FPCA = '0';  // Mark Floating-point inactive
 *     CONTROL.SPSEL = '0';  // current Stack is Main, CONTROL.nPRIV unchanged
 *     // CONTROL.nPRIV unchanged
 *     ExceptionActive[ExceptionNumber] = '1';  // initiate handler activity
 *     SCS_UpdateStatusRegs();
 *     ClearExclusiveLocal();
 *     SetEventRegister();  // see WFE instruction for more details
 *     InstructionSynchronizationBarrier();
 * (The UNKNOWN registers keep their values.)
 */
static inline Std_ReturnType ArmMExceptionTaken(TargetCoreType *core, uint32 n)
{
	ArmMStateType *m = ArmMState(core);
	uint32 tmp;
	Std_ReturnType err;

	err = ArmMemA_R(core, m->vtor + (4U * n), 4U, &tmp);
	if (err != STD_E_OK) {
		return err;
	}
	(void)ArmBranchWritePC(core, tmp & ~1U);
	ArmMSetModeAndSPSEL(core, TRUE, FALSE);
	m->ipsr = n & 0x1FFU;
	m->epsr_t = (tmp & 1U) != 0U;
	ArmITStateWrite(core, 0U);
	if (m->have_fp) {
		m->control &= ~ARM_M_CONTROL_FPCA;
	}
	m->active[n] = 1U;
	m->pending[n] = 0U;
	ArmClearExclusiveLocal(core);
	m->event = TRUE;
	return STD_E_OK;
}

/*
 * ExceptionEntry: PushStack(ExceptionType); ExceptionTaken(ExceptionNumber);
 * return_address is ReturnAddress(): the next instruction to execute for an
 * interrupt, the instruction after SVC, the faulting instruction for a fault.
 */
static inline Std_ReturnType ArmMExceptionEntry(TargetCoreType *core, uint32 n, uint32 return_address)
{
	Std_ReturnType err = ArmMPushStack(core, return_address);

	if (err != STD_E_OK) {
		return err;
	}
	return ArmMExceptionTaken(core, n);
}

/*
 * A synchronous exception (SVC, a fault) n: taken if its group priority is
 * higher than the execution priority and it is enabled; otherwise it
 * escalates to HardFault (HFSR.FORCED), and if HardFault cannot be taken
 * either, the processor locks up (which stops the run).
 */
static inline Std_ReturnType ArmMTakeSynchronous(TargetCoreType *core, uint32 n, uint32 return_address)
{
	ArmMStateType *m = ArmMState(core);
	sint32 execpri = ArmMExecutionPriority(m);
	bool enabled = (n < ARM_M_EXC_MEMMANAGE) || (n > ARM_M_EXC_USAGEFAULT) || (m->enabled[n] != 0U);

	if (!enabled || (ArmMGroupPriority(m, ArmMExceptionPriority(m, n)) >= execpri)) {
		if (n == ARM_M_EXC_HARDFAULT || execpri <= -1) {
			m->lockup = TRUE;
			return STD_E_EXEC;
		}
		m->hfsr |= ARM_M_HFSR_FORCED;
		n = ARM_M_EXC_HARDFAULT;
	}
	return ArmMExceptionEntry(core, n, return_address);
}

/*
 * Exception return
 */

/* DeActivate(ReturningExceptionNumber): not active; FAULTMASK cleared but for NMI. */
static inline void ArmMDeActivate(ArmMStateType *m, uint32 n)
{
	m->active[n] = 0U;
	if (m->ipsr != ARM_M_EXC_NMI) {
		m->faultmask = 0U;
	}
}

static inline uint32 ArmMActiveCount(const ArmMStateType *m)
{
	uint32 count = 0U;
	uint32 i;

	for (i = 0U; i < ARM_M_EXC_NUM; i++) {
		count += (m->active[i] != 0U) ? 1U : 0U;
	}
	return count;
}

/*
 * PopStack(bits(32) frameptr, bits(28) EXC_RETURN)
 *     if HaveFPExt() && EXC_RETURN<4> == '0' then
 *         framesize = 0x68; forcealign = '1';
 *     else
 *         framesize = 0x20; forcealign = CCR.STKALIGN;
 *     R[0] = MemA[frameptr,4];  R[1] = MemA[frameptr+0x4,4];
 *     R[2] = MemA[frameptr+0x8,4];  R[3] = MemA[frameptr+0xC,4];
 *     R[12] = MemA[frameptr+0x10,4];  LR = MemA[frameptr+0x14,4];
 *     BranchTo(MemA[frameptr+0x18,4]);  // UNPREDICTABLE if the new PC not halfword aligned
 *     psr = MemA[frameptr+0x1C,4];
 *     if HaveFPExt() then
 *         if EXC_RETURN<4> == '0' then
 *             if FPCCR.LSPACT == '1' then
 *                 FPCCR.LSPACT = '0';  // state in FP is still valid
 *             else
 *                 CheckVFPEnabled();
 *                 for i = 0 to 15
 *                     S[i] = MemA[frameptr+0x20+(4*i),4];
 *                 FPSCR = MemA[frameptr+0x60,4];
 *         CONTROL.FPCA = NOT(EXC_RETURN<4>);
 *     spmask = ZeroExtend((psr<9> AND forcealign):'00', 32);
 *     case EXC_RETURN<3:0> of
 *         when '0001' SP_main = (SP_main + framesize) OR spmask;
 *         when '1001' SP_main = (SP_main + framesize) OR spmask;
 *         when '1101' SP_process = (SP_process + framesize) OR spmask;
 *     APSR<31:27> = psr<31:27>;  // valid APSR bits loaded from memory
 *     if HaveDSPExt() then APSR<19:16> = psr<19:16>;
 *     IPSR<8:0> = psr<8:0>;  // valid IPSR bits loaded from memory
 *     EPSR<26:24,15:10> = psr<26:24,15:10>;  // valid EPSR bits loaded from memory
 * (Called after the mode and the stack have been selected: frameptr is R[13].)
 */
static inline Std_ReturnType ArmMPopStack(TargetCoreType *core, uint32 exc_return)
{
	ArmMStateType *m = ArmMState(core);
	bool fp_frame = m->have_fp && ((exc_return & 0x10U) == 0U);
	uint32 framesize = fp_frame ? 0x68U : 0x20U;
	bool forcealign = fp_frame || ((m->ccr & ARM_M_CCR_STKALIGN) != 0U);
	uint32 frameptr = ArmR(core, 13U);
	uint32 values[8];
	uint32 psr;
	uint32 i;
	Std_ReturnType err;

	for (i = 0U; i < 8U; i++) {
		err = ArmMemA_R(core, frameptr + (4U * i), 4U, &values[i]);
		if (err != STD_E_OK) {
			return err;
		}
	}
	if (fp_frame) {
		if ((m->fpccr & ARM_M_FPCCR_LSPACT) != 0U) {
			m->fpccr &= ~ARM_M_FPCCR_LSPACT;
		}
		else {
			uint32 value;

			for (i = 0U; i < 16U; i++) {
				err = ArmMemA_R(core, frameptr + 0x20U + (4U * i), 4U, &value);
				if (err != STD_E_OK) {
					return err;
				}
				ArmSWrite(core, i, value);
			}
			err = ArmMemA_R(core, frameptr + 0x60U, 4U, &value);
			if (err != STD_E_OK) {
				return err;
			}
			ArmFPSCRWrite(core, value);
		}
	}
	if (m->have_fp) {
		m->control = fp_frame ? (m->control | ARM_M_CONTROL_FPCA) : (m->control & ~ARM_M_CONTROL_FPCA);
	}
	ArmRWrite(core, 0U, values[0]);
	ArmRWrite(core, 1U, values[1]);
	ArmRWrite(core, 2U, values[2]);
	ArmRWrite(core, 3U, values[3]);
	ArmRWrite(core, 12U, values[4]);
	ArmRWrite(core, 14U, values[5]);
	(void)ArmBranchWritePC(core, values[6]);
	psr = values[7];
	ArmRWrite(core, 13U, (frameptr + framesize) | ((((psr >> 9) & 1U) != 0U) && forcealign ? 4U : 0U));
	ArmAPSRWrite(core, psr & (ARM_APSR_N | ARM_APSR_Z | ARM_APSR_C | ARM_APSR_V | ARM_APSR_Q | ARM_APSR_GE));
	m->ipsr = psr & 0x1FFU;
	m->epsr_t = ((psr >> 24) & 1U) != 0U;
	ArmITStateWrite(core, ((psr >> 25) & 0x3U) | (((psr >> 10) & 0x3FU) << 2));
	return STD_E_OK;
}

/*
 * ExceptionReturn(bits(28) EXC_RETURN)
 *     assert CurrentMode == Mode_Handler;
 *     if HaveFPExt() then
 *         if !IsOnes(EXC_RETURN<27:5>) then UNPREDICTABLE;
 *     else
 *         if !IsOnes(EXC_RETURN<27:4>) then UNPREDICTABLE;
 *     integer ReturningExceptionNumber = UInt(IPSR<8:0>);
 *     integer NestedActivation;  // used for Handler => Thread check when value == 1
 *     NestedActivation = ExceptionActiveBitCount();  // Number of active exceptions
 *     if ExceptionActive[ReturningExceptionNumber] == '0' then
 *         DeActivate(ReturningExceptionNumber);
 *         UFSR.INVPC = '1';
 *         LR = '1111':EXC_RETURN;
 *         ExceptionTaken(UsageFault);  // returning from an inactive handler
 *         return;
 *     else
 *         case EXC_RETURN<3:0> of
 *             when '0001'  // return to Handler
 *                 if NestedActivation == 1 then
 *                     DeActivate(ReturningExceptionNumber); UFSR.INVPC = '1';
 *                     LR = '1111':EXC_RETURN;
 *                     ExceptionTaken(UsageFault);  // return to Handler exception mismatch
 *                     return;
 *                 else
 *                     frameptr = SP_main; CurrentMode = Mode_Handler; CONTROL.SPSEL = '0';
 *             when '1001'  // returning to Thread using Main stack
 *                 if NestedActivation != 1 && CCR.NONBASETHRDENA == '0' then
 *                     (as above: INVPC)
 *                 else
 *                     frameptr = SP_main; CurrentMode = Mode_Thread; CONTROL.SPSEL = '0';
 *             when '1101'  // returning to Thread using Process stack
 *                 if NestedActivation != 1 && CCR.NONBASETHRDENA == '0' then
 *                     (as above: INVPC)
 *                 else
 *                     frameptr = SP_process; CurrentMode = Mode_Thread; CONTROL.SPSEL = '1';
 *             otherwise
 *                 DeActivate(ReturningExceptionNumber); UFSR.INVPC = '1';
 *                 LR = '1111':EXC_RETURN;
 *                 ExceptionTaken(UsageFault);  // illegal EXC_RETURN
 *                 return;
 *     DeActivate(ReturningExceptionNumber);
 *     PopStack(frameptr, EXC_RETURN);
 *     if CurrentMode==Mode_Handler AND IPSR<8:0> == '000000000' then
 *         UFSR.INVPC = '1';
 *         PushStack(UsageFault);  // to negate PopStack()
 *         LR = '1111':EXC_RETURN;
 *         ExceptionTaken(UsageFault);  // return IPSR is inconsistent
 *         return;
 *     if CurrentMode==Mode_Thread AND IPSR<8:0> != '000000000' then
 *         (as above)
 *     ClearExclusiveLocal();
 *     SetEventRegister();  // see WFE instruction for more details
 *     InstructionSynchronizationBarrier();
 *     if CurrentMode==Mode_Thread AND NestedActivation == 1 AND SCR.SLEEPONEXIT == '1' then
 *         SleepOnExit();  // IMPLEMENTATION DEFINED
 * The UsageFaults are taken here with ExceptionTaken() only, as the
 * pseudo-code does (the exception frame of the returning handler is kept).
 * Returns the error of a fault that stops the run.
 */
static inline Std_ReturnType ArmMExceptionReturn(TargetCoreType *core, uint32 exc_return)
{
	ArmMStateType *m = ArmMState(core);
	uint32 returning = m->ipsr & 0x1FFU;
	uint32 nested = ArmMActiveCount(m);
	uint32 ones_mask = m->have_fp ? 0x0FFFFFE0U : 0x0FFFFFF0U;
	bool to_handler = FALSE;
	bool spsel = FALSE;
	bool invalid = FALSE;
	Std_ReturnType err;

	if ((exc_return & ones_mask) != ones_mask) {
		return STD_E_INVALID;	/* UNPREDICTABLE */
	}
	if (m->active[returning] == 0U) {
		invalid = TRUE;
	}
	else {
		switch (exc_return & 0xFU) {
		case 0x1U:
			invalid = (nested == 1U);
			to_handler = TRUE;
			break;
		case 0x9U:
			invalid = (nested != 1U) && ((m->ccr & ARM_M_CCR_NONBASETHRDENA) == 0U);
			break;
		case 0xDU:
			invalid = (nested != 1U) && ((m->ccr & ARM_M_CCR_NONBASETHRDENA) == 0U);
			spsel = TRUE;
			break;
		default:
			invalid = TRUE;
			break;
		}
	}
	if (invalid) {
		ArmMDeActivate(m, returning);
		m->cfsr |= ARM_M_UFSR_INVPC << 16;
		ArmRWrite(core, 14U, 0xF0000000U | (exc_return & 0x0FFFFFFFU));
		return ArmMExceptionTaken(core, ARM_M_EXC_USAGEFAULT);
	}
	ArmMDeActivate(m, returning);
	ArmMSetModeAndSPSEL(core, to_handler, spsel);
	err = ArmMPopStack(core, exc_return);
	if (err != STD_E_OK) {
		return err;
	}
	if ((m->handler_mode && (m->ipsr == 0U)) || (!m->handler_mode && (m->ipsr != 0U))) {
		m->cfsr |= ARM_M_UFSR_INVPC << 16;
		err = ArmMPushStack(core, ArmNextPC(core));
		if (err != STD_E_OK) {
			return err;
		}
		ArmRWrite(core, 14U, 0xF0000000U | (exc_return & 0x0FFFFFFFU));
		return ArmMExceptionTaken(core, ARM_M_EXC_USAGEFAULT);
	}
	ArmClearExclusiveLocal(core);
	m->event = TRUE;
	return STD_E_OK;
}

#endif /* ARM_COMMON_ARM_M_EXCEPTION_H */
