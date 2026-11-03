/*
 * The profile interface: how the pseudo-code reaches the processor.
 *
 * A target provides "arm_target_profile.h" on its include path, which
 * defines TargetCoreType and the operations below (static inline, so they
 * fold into the pseudo-code). Everything that differs between the A and M
 * profiles, or between the ARM and Thumb states, is behind them.
 *
 *   uint32 ArmR(const TargetCoreType *core, uint32 n)
 *       R[n] for n = 0..14 (SP is the profile's current stack pointer).
 *   void ArmRWrite(TargetCoreType *core, uint32 n, uint32 value)
 *       R[n] = value for n = 0..14.
 *   uint32 ArmPC(const TargetCoreType *core)
 *       The value an instruction reads from PC: its address + 8 in ARM
 *       state, + 4 in Thumb state.
 *   ArmInstrSet ArmCurrentInstrSet(const TargetCoreType *core)
 *       CurrentInstrSet(): InstrSet_ARM or InstrSet_Thumb (always Thumb in
 *       the M profile).
 *   uint32 ArmAPSR(const TargetCoreType *core)
 *   void ArmAPSRWrite(TargetCoreType *core, uint32 apsr)
 *       The APSR flags (N, Z, C, V, Q, GE at their architectural positions;
 *       other bits are ignored on write).
 *   Std_ReturnType ArmBranchWritePC(TargetCoreType *core, uint32 address)
 *   Std_ReturnType ArmBXWritePC(TargetCoreType *core, uint32 address)
 *   Std_ReturnType ArmBLXWritePC(TargetCoreType *core, uint32 address)
 *   Std_ReturnType ArmLoadWritePC(TargetCoreType *core, uint32 address)
 *   Std_ReturnType ArmALUWritePC(TargetCoreType *core, uint32 address)
 *       The pseudo-code's writes to PC: the address of the next instruction.
 *       Without one, the next instruction is the following one.
 *   Std_ReturnType ArmMemU_R(TargetCoreType *core, uint32 address, uint32 size, uint32 *value)
 *   Std_ReturnType ArmMemU_W(TargetCoreType *core, uint32 address, uint32 size, uint32 value)
 *   Std_ReturnType ArmMemA_R(TargetCoreType *core, uint32 address, uint32 size, uint32 *value)
 *   Std_ReturnType ArmMemA_W(TargetCoreType *core, uint32 address, uint32 size, uint32 value)
 *       MemU[] and MemA[] for size 1, 2 or 4 at the current privilege
 *       (alignment checks, MPU, bus).
 *   Std_ReturnType ArmMemU_unpriv_R(TargetCoreType *core, uint32 address, uint32 size, uint32 *value)
 *   Std_ReturnType ArmMemU_unpriv_W(TargetCoreType *core, uint32 address, uint32 size, uint32 value)
 *       MemU_unpriv[]: as an unprivileged access (LDRT, STRT, ...).
 *   void ArmSetExclusiveMonitors(TargetCoreType *core, uint32 address, uint32 size)
 *   Std_ReturnType ArmExclusiveMonitorsPass(TargetCoreType *core, uint32 address, uint32 size, bool *pass)
 *   void ArmClearExclusiveLocal(TargetCoreType *core)
 *       The exclusive monitors of LDREX/STREX/CLREX. ExclusiveMonitorsPass
 *       takes the alignment fault of an unaligned address (an error) and
 *       clears the local monitor when it passes.
 *
 *   bool ArmIntegerZeroDivideTrappingEnabled(const TargetCoreType *core)
 *   Std_ReturnType ArmGenerateIntegerZeroDivide(TargetCoreType *core)
 *       SDIV and UDIV by zero: whether it traps (the M profile's
 *       CCR.DIV_0_TRP), and the trap (a UsageFault DIVBYZERO; returns its
 *       error).
 *
 *   void ArmITStateWrite(TargetCoreType *core, uint32 itstate)
 *       ITSTATE.IT<7:0> = itstate (IT). The target advances the IT state
 *       after every other instruction (ArmITAdvance) and takes the condition
 *       of an instruction in an IT block from it (ArmITCondition).
 *   uint32 ArmITState(const TargetCoreType *core)
 *       ITSTATE.IT<7:0> (exception entry stacks it).
 *   uint32 ArmNextPC(const TargetCoreType *core)
 *       The address of the next instruction as written so far (the
 *       instruction's own branch, or the one after it).
 *   ArmMStateType *ArmMState(const TargetCoreType *core)
 *       M profile: the exception model's state (arm_m_exception.h), held by
 *       the target.
 *   void ArmCallSupervisor(TargetCoreType *core)
 *       CallSupervisor(): SVC's exception, taken by the target when the
 *       instruction completes (M profile: ArmMTakeSynchronous(SVCall)).
 *   uint32 ArmIPSR(const TargetCoreType *core)
 *   bool ArmHaveDSPExt(const TargetCoreType *core)
 *       The M profile's IPSR (the exception number, 0 in Thread mode) and
 *       whether the DSP extension is implemented (MRS, MSR).
 *   uint32 ArmS(const TargetCoreType *core, uint32 n)
 *   void ArmSWrite(TargetCoreType *core, uint32 n, uint32 value)
 *   uint64 ArmD(const TargetCoreType *core, uint32 n)
 *   void ArmDWrite(TargetCoreType *core, uint32 n, uint64 value)
 *       The floating-point registers S0-S31 and D0-D15 (D0-D31 with 32
 *       double registers); with 16 of them, D[n] is S[2n+1]:S[2n].
 *   Std_ReturnType ArmExecuteFPCheck(TargetCoreType *core)
 *       ExecuteFPCheck() of a floating-point instruction whose condition
 *       passed: the coprocessor access check and, in the M profile, the
 *       lazy context preservation (ArmMExecuteFPCheck); returns its fault.
 *   uint32 ArmFPSCR(const TargetCoreType *core)
 *   void ArmFPSCRWrite(TargetCoreType *core, uint32 value)
 *       FPSCR; the write keeps only the bits the profile implements.
 *
 * The operations return STD_E_OK, or an error for a fault the profile
 * raises (a memory fault, an invalid state); the pseudo-code then stops and
 * returns it. The pseudo-code returns STD_E_INVALID for an UNPREDICTABLE case
 * it detects.
 *
 * The condition of an instruction (ConditionPassed()) is given by the
 * encoding glue in the input structure: the cond field in ARM state, the IT
 * state or the cond field of a conditional branch in Thumb state.
 */
#ifndef ARM_COMMON_ARM_PROFILE_H
#define ARM_COMMON_ARM_PROFILE_H

#include "std_errno.h"
#include "arm_helpers.h"
#include "arm_target_profile.h"

#define ARM_REG_PC	15U

/* R[n] as an instruction reads it: for n = 15, the value read from PC. */
static inline uint32 ArmReadReg(const TargetCoreType *core, uint32 n)
{
	if (n == ARM_REG_PC) {
		return ArmPC(core);
	}
	return ArmR(core, n);
}

/* Sets a register operand of an input structure: its name, number and value. */
#define ARM_SET_REG(core, arg, reg_name, n) \
	do { \
		(arg)->reg_name.name = #reg_name; \
		(arg)->reg_name.regId = (n); \
		(arg)->reg_name.regData = ArmReadReg((core), (n)); \
	} while (0)

#endif /* ARM_COMMON_ARM_PROFILE_H */
