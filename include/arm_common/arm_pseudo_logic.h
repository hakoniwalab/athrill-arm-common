/*
 * Pseudo-code of the logical instructions: C comes from the operand's
 * expansion or shift, V is unchanged.
 */
#ifndef ARM_COMMON_ARM_PSEUDO_LOGIC_H
#define ARM_COMMON_ARM_PSEUDO_LOGIC_H

#include "arm_pseudo_dp.h"

/*
 * AND (immediate)
 *     result = R[n] AND imm32;
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *             // APSR.V unchanged
 * (carry is from the immediate's expansion.)
 */
static inline Std_ReturnType arm_pseudo_and_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = in->Rn.regData & in->imm32;
	out->status_flag.carry = in->carry;
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * AND (register)
 *     (shifted, carry) = Shift_C(R[m], shift_t, shift_n, APSR.C);
 *     result = R[n] AND shifted;
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *             // APSR.V unchanged
 */
static inline Std_ReturnType arm_pseudo_and_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 shifted;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	shifted = Shift_C(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry, &out->status_flag.carry);
	result = in->Rn.regData & shifted;
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * ORR (immediate)
 *     result = R[n] OR imm32;
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *             // APSR.V unchanged
 * (carry is from the immediate's expansion.)
 */
static inline Std_ReturnType arm_pseudo_orr_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = in->Rn.regData | in->imm32;
	out->status_flag.carry = in->carry;
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * ORR (register)
 *     (shifted, carry) = Shift_C(R[m], shift_t, shift_n, APSR.C);
 *     result = R[n] OR shifted;
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *             // APSR.V unchanged
 */
static inline Std_ReturnType arm_pseudo_orr_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 shifted;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	shifted = Shift_C(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry, &out->status_flag.carry);
	result = in->Rn.regData | shifted;
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * ORN (immediate)
 *     result = R[n] OR NOT(imm32);
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *             // APSR.V unchanged
 * (carry is from the immediate's expansion.)
 */
static inline Std_ReturnType arm_pseudo_orn_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = in->Rn.regData | ~in->imm32;
	out->status_flag.carry = in->carry;
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * ORN (register)
 *     (shifted, carry) = Shift_C(R[m], shift_t, shift_n, APSR.C);
 *     result = R[n] OR NOT(shifted);
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *             // APSR.V unchanged
 */
static inline Std_ReturnType arm_pseudo_orn_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 shifted;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	shifted = Shift_C(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry, &out->status_flag.carry);
	result = in->Rn.regData | ~shifted;
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * EOR (immediate)
 *     result = R[n] EOR imm32;
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *             // APSR.V unchanged
 * (carry is from the immediate's expansion.)
 */
static inline Std_ReturnType arm_pseudo_eor_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = in->Rn.regData ^ in->imm32;
	out->status_flag.carry = in->carry;
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * EOR (register)
 *     (shifted, carry) = Shift_C(R[m], shift_t, shift_n, APSR.C);
 *     result = R[n] EOR shifted;
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *             // APSR.V unchanged
 */
static inline Std_ReturnType arm_pseudo_eor_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 shifted;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	shifted = Shift_C(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry, &out->status_flag.carry);
	result = in->Rn.regData ^ shifted;
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * BIC (immediate)
 *     result = R[n] AND NOT(imm32);
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *             // APSR.V unchanged
 * (carry is from the immediate's expansion.)
 */
static inline Std_ReturnType arm_pseudo_bic_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = in->Rn.regData & ~in->imm32;
	out->status_flag.carry = in->carry;
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * BIC (register)
 *     (shifted, carry) = Shift_C(R[m], shift_t, shift_n, APSR.C);
 *     result = R[n] AND NOT(shifted);
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *             // APSR.V unchanged
 */
static inline Std_ReturnType arm_pseudo_bic_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 shifted;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	shifted = Shift_C(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry, &out->status_flag.carry);
	result = in->Rn.regData & ~shifted;
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * TST (immediate)
 *     result = R[n] AND imm32;
 *     APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *     // APSR.V unchanged
 * (carry is from the immediate's expansion.)
 */
static inline Std_ReturnType arm_pseudo_tst_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = in->Rn.regData & in->imm32;
	out->status_flag.carry = in->carry;
	return ArmDataProcessingFlags(core, result, out);
}

/*
 * TST (register)
 *     (shifted, carry) = Shift_C(R[m], shift_t, shift_n, APSR.C);
 *     result = R[n] AND shifted;
 *     APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *     // APSR.V unchanged
 */
static inline Std_ReturnType arm_pseudo_tst_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 shifted;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	shifted = Shift_C(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry, &out->status_flag.carry);
	result = in->Rn.regData & shifted;
	return ArmDataProcessingFlags(core, result, out);
}

/*
 * TEQ (immediate)
 *     result = R[n] EOR imm32;
 *     APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *     // APSR.V unchanged
 * (carry is from the immediate's expansion.)
 */
static inline Std_ReturnType arm_pseudo_teq_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = in->Rn.regData ^ in->imm32;
	out->status_flag.carry = in->carry;
	return ArmDataProcessingFlags(core, result, out);
}

/*
 * TEQ (register)
 *     (shifted, carry) = Shift_C(R[m], shift_t, shift_n, APSR.C);
 *     result = R[n] EOR shifted;
 *     APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *     // APSR.V unchanged
 */
static inline Std_ReturnType arm_pseudo_teq_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 shifted;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	shifted = Shift_C(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry, &out->status_flag.carry);
	result = in->Rn.regData ^ shifted;
	return ArmDataProcessingFlags(core, result, out);
}

#endif /* ARM_COMMON_ARM_PSEUDO_LOGIC_H */
