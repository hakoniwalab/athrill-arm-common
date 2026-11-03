/*
 * What the data-processing instructions share: their input and output
 * structures and the write of their result.
 *
 * Three operand forms, as the pseudo-code has them:
 *   immediate                   op2 = imm32 (and, for logical instructions,
 *                               the carry of its expansion)
 *   register                    op2 = Shift(R[m], shift_t, shift_n, APSR.C)
 *   register-shifted register   op2 = Shift(R[m], shift_t, UInt(R[s]<7:0>), APSR.C)
 * An instruction that has no Rd (compare, test) or no Rn (move) leaves that
 * operand unset (ARM_NO_REG); the pseudo-code does not read it.
 */
#ifndef ARM_COMMON_ARM_PSEUDO_DP_H
#define ARM_COMMON_ARM_PSEUDO_DP_H

#include "arm_profile.h"

typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeUpdateStatusFlagType setflags;
	PseudoCodeRegisterDataType Rd;
	PseudoCodeRegisterDataType Rn;
	PseudoCodeImmediateData32Type imm32;
	bool carry;	/* carry of the immediate's expansion (ARMExpandImm_C, ThumbExpandImm_C) */
} arm_dp_imm_input_type;

typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeUpdateStatusFlagType setflags;
	PseudoCodeRegisterDataType Rd;
	PseudoCodeRegisterDataType Rn;
	PseudoCodeRegisterDataType Rm;
	PseudoCodeSRType shift_t;
	PseudoCodeShiftNType shift_n;
} arm_dp_reg_input_type;

typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeUpdateStatusFlagType setflags;
	PseudoCodeRegisterDataType Rd;
	PseudoCodeRegisterDataType Rn;
	PseudoCodeRegisterDataType Rm;
	PseudoCodeRegisterDataType Rs;
	PseudoCodeSRType shift_t;
} arm_dp_rsr_input_type;

typedef struct {
	bool passed;
	PseudoCodeRegisterDataType Rd;		/* the result (for a compare or test: not written) */
	PseudoCodeStatusFlagType status_flag;
} arm_dp_output_type;

/* An operand the instruction does not have. */
#define ARM_NO_REG(arg, reg_name) \
	do { \
		(arg)->reg_name.name = NULL; \
		(arg)->reg_name.regId = 0U; \
		(arg)->reg_name.regData = 0U; \
	} while (0)

/*
 * The start of every data-processing instruction:
 *     if ConditionPassed() then
 *         EncodingSpecificOperations();
 * Returns whether the condition passed; out holds the flags as they are.
 */
static inline bool ArmDataProcessingBegin(const TargetCoreType *core, uint32 cond, const PseudoCodeRegisterDataType *Rd, arm_dp_output_type *out)
{
	out->Rd = *Rd;
	out->status_flag = ApsrFlags(ArmAPSR(core));
	out->passed = ConditionHolds(cond, ArmAPSR(core));
	return out->passed;
}

/*
 * The end of a data-processing instruction that writes Rd:
 *     if d == 15 then
 *         ALUWritePC(result);      // setflags is always FALSE here
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>;
 *             APSR.Z = IsZeroBit(result);
 *             APSR.C = carry;
 *             APSR.V = overflow;    // logical instructions: V unchanged
 * out->status_flag holds the carry and overflow to write.
 */
static inline Std_ReturnType ArmDataProcessingWrite(TargetCoreType *core, uint32 d, uint32 result, bool setflags, arm_dp_output_type *out)
{
	out->Rd.regData = result;
	if (d == ARM_REG_PC) {
		return ArmALUWritePC(core, result);
	}
	ArmRWrite(core, d, result);
	if (setflags) {
		ArmAPSRWrite(core, ApsrUpdateNZCV(ArmAPSR(core), result, &out->status_flag));
	}
	return STD_E_OK;
}

/*
 * The end of a compare or test: only the flags.
 *     APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry; APSR.V = overflow;
 */
static inline Std_ReturnType ArmDataProcessingFlags(TargetCoreType *core, uint32 result, arm_dp_output_type *out)
{
	out->Rd.regData = result;
	ArmAPSRWrite(core, ApsrUpdateNZCV(ArmAPSR(core), result, &out->status_flag));
	return STD_E_OK;
}

/* UInt(R[s]<7:0>): the shift amount of a register-shifted register operand. */
static inline uint32 ArmShiftAmountFromRegister(const arm_dp_rsr_input_type *in)
{
	return in->Rs.regData & 0xFFU;
}

#endif /* ARM_COMMON_ARM_PSEUDO_DP_H */
