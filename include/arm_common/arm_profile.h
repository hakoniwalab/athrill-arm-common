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
 *   uint32 ArmAPSR(const TargetCoreType *core)
 *   void ArmAPSRWrite(TargetCoreType *core, uint32 apsr)
 *       The APSR flags (N, Z, C, V, Q, GE at their architectural positions;
 *       other bits are ignored on write).
 *   Std_ReturnType ArmBranchWritePC(TargetCoreType *core, uint32 address)
 *   Std_ReturnType ArmBXWritePC(TargetCoreType *core, uint32 address)
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
