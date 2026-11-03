/*
 * Pseudo-code of the arithmetic instructions (data processing with
 * AddWithCarry).
 */
#ifndef ARM_COMMON_ARM_PSEUDO_ARITH_H
#define ARM_COMMON_ARM_PSEUDO_ARITH_H

#include "arm_pseudo_dp.h"

/*
 * ADD (immediate)
 *     (result, carry, overflow) = AddWithCarry(R[n], imm32, '0');
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else R[d] = result; if setflags then APSR.N, Z, C, V = ...
 */
static inline Std_ReturnType arm_pseudo_add_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = AddWithCarry(in->Rn.regData, in->imm32, FALSE, &out->status_flag);
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * ADD (register)
 *     shifted = Shift(R[m], shift_t, shift_n, APSR.C);
 *     (result, carry, overflow) = AddWithCarry(R[n], shifted, '0');
 *     if d == 15 then ALUWritePC(result);
 *     else R[d] = result; if setflags then APSR.N, Z, C, V = ...
 */
static inline Std_ReturnType arm_pseudo_add_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 shifted;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	shifted = Shift(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry);
	result = AddWithCarry(in->Rn.regData, shifted, FALSE, &out->status_flag);
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * SUB (immediate)
 *     (result, carry, overflow) = AddWithCarry(R[n], NOT(imm32), '1');
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else R[d] = result; if setflags then APSR.N, Z, C, V = ...
 */
static inline Std_ReturnType arm_pseudo_sub_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = AddWithCarry(in->Rn.regData, ~in->imm32, TRUE, &out->status_flag);
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * CMP (immediate)
 *     (result, carry, overflow) = AddWithCarry(R[n], NOT(imm32), '1');
 *     APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry; APSR.V = overflow;
 */
static inline Std_ReturnType arm_pseudo_cmp_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = AddWithCarry(in->Rn.regData, ~in->imm32, TRUE, &out->status_flag);
	return ArmDataProcessingFlags(core, result, out);
}

/*
 * CMP (register)
 *     shifted = Shift(R[m], shift_t, shift_n, APSR.C);
 *     (result, carry, overflow) = AddWithCarry(R[n], NOT(shifted), '1');
 *     APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry; APSR.V = overflow;
 */
static inline Std_ReturnType arm_pseudo_cmp_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 shifted;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	shifted = Shift(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry);
	result = AddWithCarry(in->Rn.regData, ~shifted, TRUE, &out->status_flag);
	return ArmDataProcessingFlags(core, result, out);
}

/*
 * ADC (immediate)
 *     (result, carry, overflow) = AddWithCarry(R[n], imm32, APSR.C);
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else R[d] = result; if setflags then APSR.N, Z, C, V = ...
 */
static inline Std_ReturnType arm_pseudo_adc_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = AddWithCarry(in->Rn.regData, in->imm32, out->status_flag.carry, &out->status_flag);
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * ADC (register)
 *     shifted = Shift(R[m], shift_t, shift_n, APSR.C);
 *     (result, carry, overflow) = AddWithCarry(R[n], shifted, APSR.C);
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else R[d] = result; if setflags then APSR.N, Z, C, V = ...
 */
static inline Std_ReturnType arm_pseudo_adc_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 shifted;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	shifted = Shift(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry);
	result = AddWithCarry(in->Rn.regData, shifted, out->status_flag.carry, &out->status_flag);
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * SBC (immediate)
 *     (result, carry, overflow) = AddWithCarry(R[n], NOT(imm32), APSR.C);
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else R[d] = result; if setflags then APSR.N, Z, C, V = ...
 */
static inline Std_ReturnType arm_pseudo_sbc_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = AddWithCarry(in->Rn.regData, ~in->imm32, out->status_flag.carry, &out->status_flag);
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * SBC (register)
 *     shifted = Shift(R[m], shift_t, shift_n, APSR.C);
 *     (result, carry, overflow) = AddWithCarry(R[n], NOT(shifted), APSR.C);
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else R[d] = result; if setflags then APSR.N, Z, C, V = ...
 */
static inline Std_ReturnType arm_pseudo_sbc_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 shifted;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	shifted = Shift(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry);
	result = AddWithCarry(in->Rn.regData, ~shifted, out->status_flag.carry, &out->status_flag);
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * RSB (immediate)
 *     (result, carry, overflow) = AddWithCarry(NOT(R[n]), imm32, '1');
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else R[d] = result; if setflags then APSR.N, Z, C, V = ...
 */
static inline Std_ReturnType arm_pseudo_rsb_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = AddWithCarry(~in->Rn.regData, in->imm32, TRUE, &out->status_flag);
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * RSB (register)
 *     shifted = Shift(R[m], shift_t, shift_n, APSR.C);
 *     (result, carry, overflow) = AddWithCarry(NOT(R[n]), shifted, '1');
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else R[d] = result; if setflags then APSR.N, Z, C, V = ...
 */
static inline Std_ReturnType arm_pseudo_rsb_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 shifted;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	shifted = Shift(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry);
	result = AddWithCarry(~in->Rn.regData, shifted, TRUE, &out->status_flag);
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * SUB (register)
 *     shifted = Shift(R[m], shift_t, shift_n, APSR.C);
 *     (result, carry, overflow) = AddWithCarry(R[n], NOT(shifted), '1');
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else R[d] = result; if setflags then APSR.N, Z, C, V = ...
 */
static inline Std_ReturnType arm_pseudo_sub_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 shifted;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	shifted = Shift(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry);
	result = AddWithCarry(in->Rn.regData, ~shifted, TRUE, &out->status_flag);
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * CMN (immediate)
 *     (result, carry, overflow) = AddWithCarry(R[n], imm32, '0');
 *     APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry; APSR.V = overflow;
 */
static inline Std_ReturnType arm_pseudo_cmn_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = AddWithCarry(in->Rn.regData, in->imm32, FALSE, &out->status_flag);
	return ArmDataProcessingFlags(core, result, out);
}

/*
 * CMN (register)
 *     shifted = Shift(R[m], shift_t, shift_n, APSR.C);
 *     (result, carry, overflow) = AddWithCarry(R[n], shifted, '0');
 *     APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry; APSR.V = overflow;
 */
static inline Std_ReturnType arm_pseudo_cmn_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 shifted;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	shifted = Shift(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry);
	result = AddWithCarry(in->Rn.regData, shifted, FALSE, &out->status_flag);
	return ArmDataProcessingFlags(core, result, out);
}

#endif /* ARM_COMMON_ARM_PSEUDO_ARITH_H */
