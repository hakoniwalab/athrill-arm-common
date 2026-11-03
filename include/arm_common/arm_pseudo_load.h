/*
 * Pseudo-code of the load instructions.
 */
#ifndef ARM_COMMON_ARM_PSEUDO_LOAD_H
#define ARM_COMMON_ARM_PSEUDO_LOAD_H

#include "arm_profile.h"

/*
 * LDR (immediate)
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rt;
	PseudoCodeRegisterDataType Rn;
	PseudoCodeImmediateData32Type imm32;
	bool index;
	bool add;
	bool wback;
} arm_ldr_imm_input_type;

typedef struct {
	bool passed;
	uint32 address;
	PseudoCodeRegisterDataType Rt;
	PseudoCodeRegisterDataType Rn;
} arm_ldr_imm_output_type;

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     data = MemU[address,4];
 *     if wback then R[n] = offset_addr;
 *     if t == 15 then
 *         if address<1:0> == '00' then LoadWritePC(data); else UNPREDICTABLE;
 *     elsif UnalignedSupport() || address<1:0> = '00' then
 *         R[t] = data;
 *     else // Can only apply before ARMv7
 *         R[t] = ROR(data, 8*UInt(address<1:0>));
 *
 * Armv7 always has UnalignedSupport().
 */
static inline Std_ReturnType arm_pseudo_ldr_imm(TargetCoreType *core, const arm_ldr_imm_input_type *in, arm_ldr_imm_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	out->Rt = in->Rt;
	out->Rn = in->Rn;
	out->address = 0U;
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	if (!out->passed) {
		return STD_E_OK;
	}
	offset_addr = in->add ? (in->Rn.regData + in->imm32) : (in->Rn.regData - in->imm32);
	out->address = in->index ? offset_addr : in->Rn.regData;
	err = ArmMemU_R(core, out->address, 4U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	if (in->wback) {
		ArmRWrite(core, in->Rn.regId, offset_addr);
		out->Rn.regData = offset_addr;
	}
	out->Rt.regData = data;
	if (in->Rt.regId == ARM_REG_PC) {
		if ((out->address & 0x3U) != 0U) {
			return STD_E_INVALID;
		}
		return ArmLoadWritePC(core, data);
	}
	ArmRWrite(core, in->Rt.regId, data);
	return STD_E_OK;
}

#endif /* ARM_COMMON_ARM_PSEUDO_LOAD_H */
