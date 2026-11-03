/*
 * Pseudo-code of the move instructions, including the shifts, which are
 * MOV with a shifted register operand (LSL, LSR, ASR, ROR, RRX by an
 * immediate are MOV (register); by a register, MOV (register-shifted
 * register)).
 */
#ifndef ARM_COMMON_ARM_PSEUDO_MOVE_H
#define ARM_COMMON_ARM_PSEUDO_MOVE_H

#include "arm_pseudo_dp.h"

/*
 * MOV (immediate)
 *     result = imm32;
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *             // APSR.V unchanged
 */
static inline Std_ReturnType arm_pseudo_mov_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	out->status_flag.carry = in->carry;
	return ArmDataProcessingWrite(core, in->Rd.regId, in->imm32, in->setflags, out);
}

/*
 * MOV (register), with the shift of LSL/LSR/ASR/ROR/RRX (immediate):
 *     (result, carry) = Shift_C(R[m], shift_t, shift_n, APSR.C);
 *     if d == 15 then ALUWritePC(result);   // setflags is always FALSE here
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *             // APSR.V unchanged
 * For a plain MOV, shift_n is 0 and carry is APSR.C.
 */
static inline Std_ReturnType arm_pseudo_mov_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = Shift_C(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry, &out->status_flag.carry);
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * MOV (register-shifted register): LSL, LSR, ASR, ROR (register)
 *     shift_n = UInt(R[s]<7:0>);
 *     (result, carry) = Shift_C(R[m], shift_t, shift_n, APSR.C);
 *     R[d] = result;
 *     if setflags then
 *         APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *         // APSR.V unchanged
 */
static inline Std_ReturnType arm_pseudo_mov_rsr(TargetCoreType *core, const arm_dp_rsr_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = Shift_C(in->Rm.regData, in->shift_t, ArmShiftAmountFromRegister(in), out->status_flag.carry, &out->status_flag.carry);
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * MVN (immediate)
 *     result = NOT(imm32);
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *             // APSR.V unchanged
 * (carry is from the immediate's expansion.)
 */
static inline Std_ReturnType arm_pseudo_mvn_imm(TargetCoreType *core, const arm_dp_imm_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = ~in->imm32;
	out->status_flag.carry = in->carry;
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

/*
 * MVN (register)
 *     (shifted, carry) = Shift_C(R[m], shift_t, shift_n, APSR.C);
 *     result = NOT(shifted);
 *     if d == 15 then ALUWritePC(result);   // Can only occur for ARM encoding
 *     else
 *         R[d] = result;
 *         if setflags then
 *             APSR.N = result<31>; APSR.Z = IsZeroBit(result); APSR.C = carry;
 *             // APSR.V unchanged
 */
static inline Std_ReturnType arm_pseudo_mvn_reg(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out)
{
	uint32 shifted;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	shifted = Shift_C(in->Rm.regData, in->shift_t, in->shift_n, out->status_flag.carry, &out->status_flag.carry);
	result = ~shifted;
	return ArmDataProcessingWrite(core, in->Rd.regId, result, in->setflags, out);
}

#endif /* ARM_COMMON_ARM_PSEUDO_MOVE_H */
