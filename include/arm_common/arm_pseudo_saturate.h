/*
 * Pseudo-code of the saturating instructions (SSAT, USAT, SSAT16, USAT16,
 * QADD, QSUB, QDADD, QDSUB) and of PKHBT/PKHTB.
 *
 * A saturation sets APSR.Q; nothing clears it.
 */
#ifndef ARM_COMMON_ARM_PSEUDO_SATURATE_H
#define ARM_COMMON_ARM_PSEUDO_SATURATE_H

#include "arm_pseudo_parallel.h"

/* APSR.Q = '1'; */
static inline void ArmSetQ(TargetCoreType *core)
{
	ArmAPSRWrite(core, ArmAPSR(core) | ARM_APSR_Q);
}

/*
 * SSAT, USAT, SSAT16, USAT16 (the 16-bit forms have no shift)
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rd;
	PseudoCodeRegisterDataType Rn;
	uint32 saturate_to;
	PseudoCodeSRType shift_t;
	PseudoCodeShiftNType shift_n;
} arm_sat_input_type;

/*
 * SSAT
 *     if ConditionPassed() then
 *         EncodingSpecificOperations();
 *         operand = Shift(R[n], shift_t, shift_n, APSR.C);  // APSR.C ignored
 *         (result, sat) = SignedSatQ(SInt(operand), saturate_to);
 *         R[d] = SignExtend(result, 32);
 *         if sat then
 *             APSR.Q = '1';
 */
static inline Std_ReturnType arm_pseudo_ssat(TargetCoreType *core, const arm_sat_input_type *in, arm_dp_output_type *out)
{
	uint32 operand;
	sint32 result;
	bool sat;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	operand = Shift(in->Rn.regData, in->shift_t, in->shift_n, out->status_flag.carry);
	result = SignedSatQ((sint64)(sint32)operand, in->saturate_to, &sat);
	if (sat) {
		ArmSetQ(core);
	}
	return ArmResultWrite(core, in->Rd.regId, (uint32)result, out);
}

/*
 * USAT
 *     if ConditionPassed() then
 *         EncodingSpecificOperations();
 *         operand = Shift(R[n], shift_t, shift_n, APSR.C);  // APSR.C ignored
 *         (result, sat) = UnsignedSatQ(SInt(operand), saturate_to);
 *         R[d] = ZeroExtend(result, 32);
 *         if sat then
 *             APSR.Q = '1';
 */
static inline Std_ReturnType arm_pseudo_usat(TargetCoreType *core, const arm_sat_input_type *in, arm_dp_output_type *out)
{
	uint32 operand;
	uint32 result;
	bool sat;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	operand = Shift(in->Rn.regData, in->shift_t, in->shift_n, out->status_flag.carry);
	result = UnsignedSatQ((sint64)(sint32)operand, in->saturate_to, &sat);
	if (sat) {
		ArmSetQ(core);
	}
	return ArmResultWrite(core, in->Rd.regId, result, out);
}

/*
 * SSAT16
 *     if ConditionPassed() then
 *         EncodingSpecificOperations();
 *         (result1, sat1) = SignedSatQ(SInt(R[n]<15:0>), saturate_to);
 *         (result2, sat2) = SignedSatQ(SInt(R[n]<31:16>), saturate_to);
 *         R[d]<15:0> = SignExtend(result1, 16);
 *         R[d]<31:16> = SignExtend(result2, 16);
 *         if sat1 || sat2 then
 *             APSR.Q = '1';
 */
static inline Std_ReturnType arm_pseudo_ssat16(TargetCoreType *core, const arm_sat_input_type *in, arm_dp_output_type *out)
{
	sint32 result1;
	sint32 result2;
	bool sat1;
	bool sat2;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result1 = SignedSatQ(ArmLane(in->Rn.regData, 0U, 16U, TRUE), in->saturate_to, &sat1);
	result2 = SignedSatQ(ArmLane(in->Rn.regData, 1U, 16U, TRUE), in->saturate_to, &sat2);
	if (sat1 || sat2) {
		ArmSetQ(core);
	}
	return ArmResultWrite(core, in->Rd.regId, ArmHalfwords((uint32)result2, (uint32)result1), out);
}

/*
 * USAT16
 *     if ConditionPassed() then
 *         EncodingSpecificOperations();
 *         (result1, sat1) = UnsignedSatQ(SInt(R[n]<15:0>), saturate_to);
 *         (result2, sat2) = UnsignedSatQ(SInt(R[n]<31:16>), saturate_to);
 *         R[d]<15:0> = ZeroExtend(result1, 16);
 *         R[d]<31:16> = ZeroExtend(result2, 16);
 *         if sat1 || sat2 then
 *             APSR.Q = '1';
 */
static inline Std_ReturnType arm_pseudo_usat16(TargetCoreType *core, const arm_sat_input_type *in, arm_dp_output_type *out)
{
	uint32 result1;
	uint32 result2;
	bool sat1;
	bool sat2;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result1 = UnsignedSatQ(ArmLane(in->Rn.regData, 0U, 16U, TRUE), in->saturate_to, &sat1);
	result2 = UnsignedSatQ(ArmLane(in->Rn.regData, 1U, 16U, TRUE), in->saturate_to, &sat2);
	if (sat1 || sat2) {
		ArmSetQ(core);
	}
	return ArmResultWrite(core, in->Rd.regId, ArmHalfwords(result2, result1), out);
}

/*
 * QADD, QSUB, QDADD, QDSUB (on arm_dsp_input_type: Rd, Rn, Rm)
 */

/* SignedSatQ(i, 32) that sets APSR.Q when it saturates. */
static inline uint32 ArmSignedSat32(TargetCoreType *core, sint64 i)
{
	bool sat;
	sint32 result = SignedSatQ(i, 32U, &sat);

	if (sat) {
		ArmSetQ(core);
	}
	return (uint32)result;
}

/*
 * QADD
 *     (R[d], sat) = SignedSatQ(SInt(R[m]) + SInt(R[n]), 32);
 *     if sat then APSR.Q = '1';
 */
static inline Std_ReturnType arm_pseudo_qadd(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = ArmSignedSat32(core, (sint64)(sint32)in->Rm.regData + (sint64)(sint32)in->Rn.regData);
	return ArmResultWrite(core, in->Rd.regId, result, out);
}

/*
 * QSUB
 *     (R[d], sat) = SignedSatQ(SInt(R[m]) - SInt(R[n]), 32);
 *     if sat then APSR.Q = '1';
 */
static inline Std_ReturnType arm_pseudo_qsub(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = ArmSignedSat32(core, (sint64)(sint32)in->Rm.regData - (sint64)(sint32)in->Rn.regData);
	return ArmResultWrite(core, in->Rd.regId, result, out);
}

/*
 * QDADD
 *     (doubled, sat1) = SignedSatQ(2 * SInt(R[n]), 32);
 *     (R[d], sat2) = SignedSatQ(SInt(R[m]) + SInt(doubled), 32);
 *     if sat1 || sat2 then APSR.Q = '1';
 */
static inline Std_ReturnType arm_pseudo_qdadd(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	uint32 doubled;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	doubled = ArmSignedSat32(core, 2 * (sint64)(sint32)in->Rn.regData);
	result = ArmSignedSat32(core, (sint64)(sint32)in->Rm.regData + (sint64)(sint32)doubled);
	return ArmResultWrite(core, in->Rd.regId, result, out);
}

/*
 * QDSUB
 *     (doubled, sat1) = SignedSatQ(2 * SInt(R[n]), 32);
 *     (R[d], sat2) = SignedSatQ(SInt(R[m]) - SInt(doubled), 32);
 *     if sat1 || sat2 then APSR.Q = '1';
 */
static inline Std_ReturnType arm_pseudo_qdsub(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	uint32 doubled;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	doubled = ArmSignedSat32(core, 2 * (sint64)(sint32)in->Rn.regData);
	result = ArmSignedSat32(core, (sint64)(sint32)in->Rm.regData - (sint64)(sint32)doubled);
	return ArmResultWrite(core, in->Rd.regId, result, out);
}

/*
 * PKHBT, PKHTB (arm_dp_reg_input_type; tbform: PKHTB)
 *     if ConditionPassed() then
 *         EncodingSpecificOperations();
 *         operand2 = Shift(R[m], shift_t, shift_n, APSR.C);  // APSR.C ignored
 *         R[d]<15:0>  = if tbform then operand2<15:0> else R[n]<15:0>;
 *         R[d]<31:16> = if tbform then R[n]<31:16>    else operand2<31:16>;
 */
static inline Std_ReturnType arm_pseudo_pkhbt(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 operand2;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	operand2 = Shift(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry);
	return ArmResultWrite(core, in->Rd.regId, (operand2 & 0xFFFF0000U) | (in->Rn.regData & 0xFFFFU), out);
}

static inline Std_ReturnType arm_pseudo_pkhtb(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 operand2;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	operand2 = Shift(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry);
	return ArmResultWrite(core, in->Rd.regId, (in->Rn.regData & 0xFFFF0000U) | (operand2 & 0xFFFFU), out);
}

#endif /* ARM_COMMON_ARM_PSEUDO_SATURATE_H */
