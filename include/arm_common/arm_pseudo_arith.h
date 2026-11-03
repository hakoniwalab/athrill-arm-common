/*
 * Pseudo-code of the arithmetic instructions.
 */
#ifndef ARM_COMMON_ARM_PSEUDO_ARITH_H
#define ARM_COMMON_ARM_PSEUDO_ARITH_H

#include "arm_profile.h"

/*
 * ADD (immediate)
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeUpdateStatusFlagType setflags;
	PseudoCodeRegisterDataType Rd;
	PseudoCodeRegisterDataType Rn;
	PseudoCodeImmediateData32Type imm32;
} arm_add_imm_input_type;

typedef struct {
	bool passed;
	PseudoCodeRegisterDataType Rd;
	PseudoCodeStatusFlagType status_flag;
} arm_add_imm_output_type;

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     (result, carry, overflow) = AddWithCarry(R[n], imm32, '0');
 *     if d == 15 then          // Can only occur for ARM encoding
 *         ALUWritePC(result);  // setflags is always FALSE here
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>;
 *             APSR.Z = IsZeroBit(result);
 *             APSR.C = carry;
 *             APSR.V = overflow;
 */
static inline Std_ReturnType arm_pseudo_add_imm(TargetCoreType *core, const arm_add_imm_input_type *in, arm_add_imm_output_type *out)
{
	uint32 result;

	out->Rd = in->Rd;
	out->status_flag = ApsrFlags(ArmAPSR(core));
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	if (!out->passed) {
		return STD_E_OK;
	}
	result = AddWithCarry(in->Rn.regData, in->imm32, FALSE, &out->status_flag);
	out->Rd.regData = result;
	if (in->Rd.regId == ARM_REG_PC) {
		return ArmALUWritePC(core, result);
	}
	ArmRWrite(core, in->Rd.regId, result);
	if (in->setflags) {
		ArmAPSRWrite(core, ApsrUpdateNZCV(ArmAPSR(core), result, &out->status_flag));
	}
	return STD_E_OK;
}

#endif /* ARM_COMMON_ARM_PSEUDO_ARITH_H */
