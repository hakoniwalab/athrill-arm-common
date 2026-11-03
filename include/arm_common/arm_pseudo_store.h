/*
 * Pseudo-code of the single store instructions (STR, STRB, STRH; immediate
 * and register; and their unprivileged forms).
 */
#ifndef ARM_COMMON_ARM_PSEUDO_STORE_H
#define ARM_COMMON_ARM_PSEUDO_STORE_H

#include "arm_pseudo_ls.h"

/*
 * STR (immediate)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     MemU[address,4] = R[t];
 *     if wback then R[n] = offset_addr;
 * (R[t] of PC is the value an instruction reads from it, PCStoreValue() in ARM state.)
 */
static inline Std_ReturnType arm_pseudo_str_imm(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_W(core, out->address, 4U, in->Rt.regData);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return STD_E_OK;
}

/*
 * STRB (immediate)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     MemU[address,1] = R[t]<7:0>;
 *     if wback then R[n] = offset_addr;
 */
static inline Std_ReturnType arm_pseudo_strb_imm(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_W(core, out->address, 1U, in->Rt.regData);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return STD_E_OK;
}

/*
 * STRH (immediate)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     MemU[address,2] = R[t]<15:0>;
 *     if wback then R[n] = offset_addr;
 */
static inline Std_ReturnType arm_pseudo_strh_imm(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_W(core, out->address, 2U, in->Rt.regData);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return STD_E_OK;
}

/*
 * STR (register)
 *     if ConditionPassed() then
 *     offset = Shift(R[m], shift_t, shift_n, APSR.C);
 *     offset_addr = if add then (R[n] + offset) else (R[n] - offset);
 *     address = if index then offset_addr else R[n];
 *     MemU[address,4] = R[t];
 *     if wback then R[n] = offset_addr;
 * (R[t] of PC is the value an instruction reads from it, PCStoreValue() in ARM state.)
 */
static inline Std_ReturnType arm_pseudo_str_reg(TargetCoreType *core, const arm_ls_reg_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, ArmRegisterOffset(core, in), in->index, in->add, out);
	err = ArmMemU_W(core, out->address, 4U, in->Rt.regData);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return STD_E_OK;
}

/*
 * STRB (register)
 *     if ConditionPassed() then
 *     offset = Shift(R[m], shift_t, shift_n, APSR.C);
 *     offset_addr = if add then (R[n] + offset) else (R[n] - offset);
 *     address = if index then offset_addr else R[n];
 *     MemU[address,1] = R[t]<7:0>;
 *     if wback then R[n] = offset_addr;
 */
static inline Std_ReturnType arm_pseudo_strb_reg(TargetCoreType *core, const arm_ls_reg_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, ArmRegisterOffset(core, in), in->index, in->add, out);
	err = ArmMemU_W(core, out->address, 1U, in->Rt.regData);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return STD_E_OK;
}

/*
 * STRH (register)
 *     if ConditionPassed() then
 *     offset = Shift(R[m], shift_t, shift_n, APSR.C);
 *     offset_addr = if add then (R[n] + offset) else (R[n] - offset);
 *     address = if index then offset_addr else R[n];
 *     MemU[address,2] = R[t]<15:0>;
 *     if wback then R[n] = offset_addr;
 */
static inline Std_ReturnType arm_pseudo_strh_reg(TargetCoreType *core, const arm_ls_reg_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, ArmRegisterOffset(core, in), in->index, in->add, out);
	err = ArmMemU_W(core, out->address, 2U, in->Rt.regData);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return STD_E_OK;
}

/*
 * STRT (unprivileged)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     MemU_unpriv[address,4] = R[t];
 *     if wback then R[n] = offset_addr;
 */
static inline Std_ReturnType arm_pseudo_strt(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_unpriv_W(core, out->address, 4U, in->Rt.regData);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return STD_E_OK;
}

/*
 * STRBT (unprivileged)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     MemU_unpriv[address,1] = R[t]<7:0>;
 *     if wback then R[n] = offset_addr;
 */
static inline Std_ReturnType arm_pseudo_strbt(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_unpriv_W(core, out->address, 1U, in->Rt.regData);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return STD_E_OK;
}

/*
 * STRHT (unprivileged)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     MemU_unpriv[address,2] = R[t]<15:0>;
 *     if wback then R[n] = offset_addr;
 */
static inline Std_ReturnType arm_pseudo_strht(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_unpriv_W(core, out->address, 2U, in->Rt.regData);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return STD_E_OK;
}

#endif /* ARM_COMMON_ARM_PSEUDO_STORE_H */
