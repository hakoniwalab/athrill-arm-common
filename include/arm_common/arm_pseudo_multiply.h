/*
 * Pseudo-code of the multiply and divide instructions: MUL, MLA, MLS,
 * SMULL, UMULL, SMLAL, UMLAL, SDIV, UDIV.
 *
 * setflags is used by the ARM encodings and by MULS (Thumb); the other
 * Thumb encodings set it FALSE.
 */
#ifndef ARM_COMMON_ARM_PSEUDO_MULTIPLY_H
#define ARM_COMMON_ARM_PSEUDO_MULTIPLY_H

#include "arm_pseudo_dp.h"

/*
 * MUL, MLA, MLS, SDIV, UDIV (MUL, SDIV and UDIV leave Ra unset)
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeUpdateStatusFlagType setflags;
	PseudoCodeRegisterDataType Rd;
	PseudoCodeRegisterDataType Rn;
	PseudoCodeRegisterDataType Rm;
	PseudoCodeRegisterDataType Ra;
} arm_mul_input_type;

/*
 * R[d] = result<31:0>;
 * if setflags then
 *     APSR.N = result<31>;
 *     APSR.Z = IsZeroBit(result<31:0>);
 *     // APSR.C, APSR.V unchanged
 * (out->status_flag holds the C and V of the APSR.)
 */
static inline Std_ReturnType ArmMultiplyWrite(TargetCoreType *core, const arm_mul_input_type *in, uint32 result, arm_dp_output_type *out)
{
	Std_ReturnType err = ArmResultWrite(core, in->Rd.regId, result, out);

	if ((err != STD_E_OK) || !in->setflags) {
		return err;
	}
	ArmAPSRWrite(core, ApsrUpdateNZCV(ArmAPSR(core), result, &out->status_flag));
	return STD_E_OK;
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     operand1 = SInt(R[n]);   // operand1 = UInt(R[n]) produces the same final results
 *     operand2 = SInt(R[m]);   // operand2 = UInt(R[m]) produces the same final results
 *     result = operand1 * operand2;
 *     R[d] = result<31:0>;
 *     if setflags then
 *         APSR.N = result<31>;
 *         APSR.Z = IsZeroBit(result<31:0>);
 *         // APSR.C, APSR.V unchanged
 */
static inline Std_ReturnType arm_pseudo_mul(TargetCoreType *core, const arm_mul_input_type *in, arm_dp_output_type *out)
{
	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	return ArmMultiplyWrite(core, in, in->Rn.regData * in->Rm.regData, out);
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     operand1 = SInt(R[n]);   // operand1 = UInt(R[n]) produces the same final results
 *     operand2 = SInt(R[m]);   // operand2 = UInt(R[m]) produces the same final results
 *     addend   = SInt(R[a]);   // addend   = UInt(R[a]) produces the same final results
 *     result = operand1 * operand2 + addend;
 *     R[d] = result<31:0>;
 *     if setflags then
 *         APSR.N = result<31>;
 *         APSR.Z = IsZeroBit(result<31:0>);
 *         // APSR.C, APSR.V unchanged
 */
static inline Std_ReturnType arm_pseudo_mla(TargetCoreType *core, const arm_mul_input_type *in, arm_dp_output_type *out)
{
	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	return ArmMultiplyWrite(core, in, (in->Rn.regData * in->Rm.regData) + in->Ra.regData, out);
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     operand1 = SInt(R[n]);   // operand1 = UInt(R[n]) produces the same final results
 *     operand2 = SInt(R[m]);   // operand2 = UInt(R[m]) produces the same final results
 *     addend   = SInt(R[a]);   // addend   = UInt(R[a]) produces the same final results
 *     result = addend - operand1 * operand2;
 *     R[d] = result<31:0>;
 * (setflags is FALSE: MLS has no flag-setting form.)
 */
static inline Std_ReturnType arm_pseudo_mls(TargetCoreType *core, const arm_mul_input_type *in, arm_dp_output_type *out)
{
	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	return ArmMultiplyWrite(core, in, in->Ra.regData - (in->Rn.regData * in->Rm.regData), out);
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     if SInt(R[m]) == 0 then
 *         if IntegerZeroDivideTrappingEnabled() then
 *             GenerateIntegerZeroDivide();
 *         else
 *             result = 0;
 *     else
 *         result = RoundTowardsZero(SInt(R[n]) / SInt(R[m]));
 *     R[d] = result<31:0>;
 */
static inline Std_ReturnType arm_pseudo_sdiv(TargetCoreType *core, const arm_mul_input_type *in, arm_dp_output_type *out)
{
	sint64 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	if (in->Rm.regData == 0U) {
		if (ArmIntegerZeroDivideTrappingEnabled(core)) {
			return ArmGenerateIntegerZeroDivide(core);
		}
		result = 0;
	}
	else {
		/* C's division of integers rounds towards zero; in 64 bits,
		 * -2^31 / -1 = 2^31 does not overflow. */
		result = (sint64)(sint32)in->Rn.regData / (sint64)(sint32)in->Rm.regData;
	}
	return ArmResultWrite(core, in->Rd.regId, (uint32)result, out);
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     if UInt(R[m]) == 0 then
 *         if IntegerZeroDivideTrappingEnabled() then
 *             GenerateIntegerZeroDivide();
 *         else
 *             result = 0;
 *     else
 *         result = RoundTowardsZero(UInt(R[n]) / UInt(R[m]));
 *     R[d] = result<31:0>;
 */
static inline Std_ReturnType arm_pseudo_udiv(TargetCoreType *core, const arm_mul_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	if (in->Rm.regData == 0U) {
		if (ArmIntegerZeroDivideTrappingEnabled(core)) {
			return ArmGenerateIntegerZeroDivide(core);
		}
		result = 0U;
	}
	else {
		result = in->Rn.regData / in->Rm.regData;
	}
	return ArmResultWrite(core, in->Rd.regId, result, out);
}

/*
 * SMULL, UMULL, SMLAL, UMLAL
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeUpdateStatusFlagType setflags;
	PseudoCodeRegisterDataType RdLo;
	PseudoCodeRegisterDataType RdHi;
	PseudoCodeRegisterDataType Rn;
	PseudoCodeRegisterDataType Rm;
} arm_mul_long_input_type;

typedef struct {
	bool passed;
	uint64 result;
} arm_mul_long_output_type;

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 * Returns whether the condition passed.
 */
static inline bool ArmMultiplyLongBegin(const TargetCoreType *core, const arm_mul_long_input_type *in, arm_mul_long_output_type *out)
{
	out->result = 0U;
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	return out->passed;
}

/*
 * R[dHi] = result<63:32>;
 * R[dLo] = result<31:0>;
 * if setflags then
 *     APSR.N = result<63>;
 *     APSR.Z = IsZeroBit(result<63:0>);
 *     // APSR.C, APSR.V unchanged
 * (dHi == 15, dLo == 15 or dHi == dLo is UNPREDICTABLE.)
 */
static inline Std_ReturnType ArmMultiplyLongWrite(TargetCoreType *core, const arm_mul_long_input_type *in, uint64 result,
	arm_mul_long_output_type *out)
{
	uint32 apsr;

	out->result = result;
	if ((in->RdHi.regId == ARM_REG_PC) || (in->RdLo.regId == ARM_REG_PC) || (in->RdHi.regId == in->RdLo.regId)) {
		return STD_E_INVALID;
	}
	ArmRWrite(core, in->RdHi.regId, (uint32)(result >> 32));
	ArmRWrite(core, in->RdLo.regId, (uint32)result);
	if (in->setflags) {
		apsr = ArmAPSR(core) & ~(ARM_APSR_N | ARM_APSR_Z);
		if ((result >> 63) != 0U) {
			apsr |= ARM_APSR_N;
		}
		if (result == 0U) {
			apsr |= ARM_APSR_Z;
		}
		ArmAPSRWrite(core, apsr);
	}
	return STD_E_OK;
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     result = SInt(R[n]) * SInt(R[m]);
 *     R[dHi] = result<63:32>;
 *     R[dLo] = result<31:0>;
 *     if setflags then
 *         APSR.N = result<63>;
 *         APSR.Z = IsZeroBit(result<63:0>);
 *         // APSR.C, APSR.V unchanged
 */
static inline Std_ReturnType arm_pseudo_smull(TargetCoreType *core, const arm_mul_long_input_type *in, arm_mul_long_output_type *out)
{
	sint64 result;

	if (!ArmMultiplyLongBegin(core, in, out)) {
		return STD_E_OK;
	}
	result = (sint64)(sint32)in->Rn.regData * (sint64)(sint32)in->Rm.regData;
	return ArmMultiplyLongWrite(core, in, (uint64)result, out);
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     result = UInt(R[n]) * UInt(R[m]);
 *     R[dHi] = result<63:32>;
 *     R[dLo] = result<31:0>;
 *     if setflags then
 *         APSR.N = result<63>;
 *         APSR.Z = IsZeroBit(result<63:0>);
 *         // APSR.C, APSR.V unchanged
 */
static inline Std_ReturnType arm_pseudo_umull(TargetCoreType *core, const arm_mul_long_input_type *in, arm_mul_long_output_type *out)
{
	if (!ArmMultiplyLongBegin(core, in, out)) {
		return STD_E_OK;
	}
	return ArmMultiplyLongWrite(core, in, (uint64)in->Rn.regData * (uint64)in->Rm.regData, out);
}

/* R[dHi]:R[dLo], the 64-bit accumulator of SMLAL and UMLAL. */
static inline uint64 ArmMultiplyLongAccumulator(const arm_mul_long_input_type *in)
{
	return ((uint64)in->RdHi.regData << 32) | (uint64)in->RdLo.regData;
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     result = SInt(R[n]) * SInt(R[m]) + SInt(R[dHi]:R[dLo]);
 *     R[dHi] = result<63:32>;
 *     R[dLo] = result<31:0>;
 *     if setflags then
 *         APSR.N = result<63>;
 *         APSR.Z = IsZeroBit(result<63:0>);
 *         // APSR.C, APSR.V unchanged
 * (Modulo 2^64, the signed sum is the unsigned one.)
 */
static inline Std_ReturnType arm_pseudo_smlal(TargetCoreType *core, const arm_mul_long_input_type *in, arm_mul_long_output_type *out)
{
	uint64 product;

	if (!ArmMultiplyLongBegin(core, in, out)) {
		return STD_E_OK;
	}
	product = (uint64)((sint64)(sint32)in->Rn.regData * (sint64)(sint32)in->Rm.regData);
	return ArmMultiplyLongWrite(core, in, product + ArmMultiplyLongAccumulator(in), out);
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     result = UInt(R[n]) * UInt(R[m]) + UInt(R[dHi]:R[dLo]);
 *     R[dHi] = result<63:32>;
 *     R[dLo] = result<31:0>;
 *     if setflags then
 *         APSR.N = result<63>;
 *         APSR.Z = IsZeroBit(result<63:0>);
 *         // APSR.C, APSR.V unchanged
 */
static inline Std_ReturnType arm_pseudo_umlal(TargetCoreType *core, const arm_mul_long_input_type *in, arm_mul_long_output_type *out)
{
	uint64 product;

	if (!ArmMultiplyLongBegin(core, in, out)) {
		return STD_E_OK;
	}
	product = (uint64)in->Rn.regData * (uint64)in->Rm.regData;
	return ArmMultiplyLongWrite(core, in, product + ArmMultiplyLongAccumulator(in), out);
}

#endif /* ARM_COMMON_ARM_PSEUDO_MULTIPLY_H */
