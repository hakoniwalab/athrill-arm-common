/*
 * Pseudo-code of the single load instructions (LDR, LDRB, LDRH, LDRSB,
 * LDRSH; immediate, literal and register; and their unprivileged forms).
 */
#ifndef ARM_COMMON_ARM_PSEUDO_LOAD_H
#define ARM_COMMON_ARM_PSEUDO_LOAD_H

#include "arm_pseudo_ls.h"

/*
 * The end of a word load:
 *     if t == 15 then
 *         if address<1:0> == '00' then LoadWritePC(data); else UNPREDICTABLE;
 *     elsif UnalignedSupport() || address<1:0> = '00' then
 *         R[t] = data;
 * Armv7 always has UnalignedSupport().
 */
static inline Std_ReturnType ArmLoadWordWrite(TargetCoreType *core, uint32 t, uint32 address, uint32 data, arm_ls_output_type *out)
{
	out->Rt.regData = data;
	if (t == ARM_REG_PC) {
		if ((address & 0x3U) != 0U) {
			return STD_E_INVALID;
		}
		return ArmLoadWritePC(core, data);
	}
	ArmRWrite(core, t, data);
	return STD_E_OK;
}

/*
 * LDR (immediate)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     data = MemU[address,4];
 *     if wback then R[n] = offset_addr;
 *     if t == 15 then ... LoadWritePC(data) ... else R[t] = data;   (ArmLoadWordWrite)
 */
static inline Std_ReturnType arm_pseudo_ldr_imm(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_R(core, out->address, 4U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return ArmLoadWordWrite(core, in->Rt.regId, out->address, data, out);
}

/*
 * LDRB (immediate)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     R[t] = ZeroExtend(MemU[address,1], 32);
 *     if wback then R[n] = offset_addr;
 */
static inline Std_ReturnType arm_pseudo_ldrb_imm(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_R(core, out->address, 1U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	out->Rt.regData = ZeroExtend(data, 8U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return STD_E_OK;
}

/*
 * LDRH (immediate)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     data = MemU[address,2];
 *     if wback then R[n] = offset_addr;
 *     R[t] = ZeroExtend(data, 32);
 */
static inline Std_ReturnType arm_pseudo_ldrh_imm(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_R(core, out->address, 2U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	out->Rt.regData = ZeroExtend(data, 16U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	return STD_E_OK;
}

/*
 * LDRSB (immediate)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     R[t] = SignExtend(MemU[address,1], 32);
 *     if wback then R[n] = offset_addr;
 */
static inline Std_ReturnType arm_pseudo_ldrsb_imm(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_R(core, out->address, 1U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	out->Rt.regData = SignExtend(data, 8U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return STD_E_OK;
}

/*
 * LDRSH (immediate)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     data = MemU[address,2];
 *     if wback then R[n] = offset_addr;
 *     R[t] = SignExtend(data, 32);
 */
static inline Std_ReturnType arm_pseudo_ldrsh_imm(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_R(core, out->address, 2U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	out->Rt.regData = SignExtend(data, 16U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	return STD_E_OK;
}

/*
 * LDR (register)
 *     if ConditionPassed() then
 *     offset = Shift(R[m], shift_t, shift_n, APSR.C);
 *     offset_addr = if add then (R[n] + offset) else (R[n] - offset);
 *     address = if index then offset_addr else R[n];
 *     data = MemU[address,4];
 *     if wback then R[n] = offset_addr;
 *     if t == 15 then ... LoadWritePC(data) ... else R[t] = data;   (ArmLoadWordWrite)
 */
static inline Std_ReturnType arm_pseudo_ldr_reg(TargetCoreType *core, const arm_ls_reg_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, ArmRegisterOffset(core, in), in->index, in->add, out);
	err = ArmMemU_R(core, out->address, 4U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return ArmLoadWordWrite(core, in->Rt.regId, out->address, data, out);
}

/*
 * LDRB (register)
 *     if ConditionPassed() then
 *     offset = Shift(R[m], shift_t, shift_n, APSR.C);
 *     offset_addr = if add then (R[n] + offset) else (R[n] - offset);
 *     address = if index then offset_addr else R[n];
 *     R[t] = ZeroExtend(MemU[address,1], 32);
 *     if wback then R[n] = offset_addr;
 */
static inline Std_ReturnType arm_pseudo_ldrb_reg(TargetCoreType *core, const arm_ls_reg_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, ArmRegisterOffset(core, in), in->index, in->add, out);
	err = ArmMemU_R(core, out->address, 1U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	out->Rt.regData = ZeroExtend(data, 8U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return STD_E_OK;
}

/*
 * LDRH (register)
 *     if ConditionPassed() then
 *     offset = Shift(R[m], shift_t, shift_n, APSR.C);
 *     offset_addr = if add then (R[n] + offset) else (R[n] - offset);
 *     address = if index then offset_addr else R[n];
 *     data = MemU[address,2];
 *     if wback then R[n] = offset_addr;
 *     R[t] = ZeroExtend(data, 32);
 */
static inline Std_ReturnType arm_pseudo_ldrh_reg(TargetCoreType *core, const arm_ls_reg_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, ArmRegisterOffset(core, in), in->index, in->add, out);
	err = ArmMemU_R(core, out->address, 2U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	out->Rt.regData = ZeroExtend(data, 16U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	return STD_E_OK;
}

/*
 * LDRSB (register)
 *     if ConditionPassed() then
 *     offset = Shift(R[m], shift_t, shift_n, APSR.C);
 *     offset_addr = if add then (R[n] + offset) else (R[n] - offset);
 *     address = if index then offset_addr else R[n];
 *     R[t] = SignExtend(MemU[address,1], 32);
 *     if wback then R[n] = offset_addr;
 */
static inline Std_ReturnType arm_pseudo_ldrsb_reg(TargetCoreType *core, const arm_ls_reg_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, ArmRegisterOffset(core, in), in->index, in->add, out);
	err = ArmMemU_R(core, out->address, 1U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	out->Rt.regData = SignExtend(data, 8U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return STD_E_OK;
}

/*
 * LDRSH (register)
 *     if ConditionPassed() then
 *     offset = Shift(R[m], shift_t, shift_n, APSR.C);
 *     offset_addr = if add then (R[n] + offset) else (R[n] - offset);
 *     address = if index then offset_addr else R[n];
 *     data = MemU[address,2];
 *     if wback then R[n] = offset_addr;
 *     R[t] = SignExtend(data, 32);
 */
static inline Std_ReturnType arm_pseudo_ldrsh_reg(TargetCoreType *core, const arm_ls_reg_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, ArmRegisterOffset(core, in), in->index, in->add, out);
	err = ArmMemU_R(core, out->address, 2U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	out->Rt.regData = SignExtend(data, 16U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	return STD_E_OK;
}

/*
 * LDR (literal)
 *     if ConditionPassed() then
 *     base = Align(PC,4);
 *     address = if add then (base + imm32) else (base - imm32);
 *     data = MemU[address,4];
 *     if t == 15 then ... LoadWritePC(data) ... else R[t] = data;   (ArmLoadWordWrite)
 */
static inline Std_ReturnType arm_pseudo_ldr_lit(TargetCoreType *core, const arm_ls_lit_input_type *in, arm_ls_output_type *out)
{
	uint32 data;
	Std_ReturnType err;

	out->Rt = in->Rt;
	out->Rn = in->Rt;
	out->address = 0U;
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	if (!out->passed) {
		return STD_E_OK;
	}
	out->address = ArmLiteralAddress(core, in);
	err = ArmMemU_R(core, out->address, 4U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	return ArmLoadWordWrite(core, in->Rt.regId, out->address, data, out);
}

/*
 * LDRB (literal)
 *     if ConditionPassed() then
 *     base = Align(PC,4);
 *     address = if add then (base + imm32) else (base - imm32);
 *     R[t] = ZeroExtend(MemU[address,1], 32);
 */
static inline Std_ReturnType arm_pseudo_ldrb_lit(TargetCoreType *core, const arm_ls_lit_input_type *in, arm_ls_output_type *out)
{
	uint32 data;
	Std_ReturnType err;

	out->Rt = in->Rt;
	out->Rn = in->Rt;
	out->address = 0U;
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	if (!out->passed) {
		return STD_E_OK;
	}
	out->address = ArmLiteralAddress(core, in);
	err = ArmMemU_R(core, out->address, 1U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	out->Rt.regData = ZeroExtend(data, 8U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	return STD_E_OK;
}

/*
 * LDRH (literal)
 *     if ConditionPassed() then
 *     base = Align(PC,4);
 *     address = if add then (base + imm32) else (base - imm32);
 *     data = MemU[address,2];
 *     R[t] = ZeroExtend(data, 32);
 */
static inline Std_ReturnType arm_pseudo_ldrh_lit(TargetCoreType *core, const arm_ls_lit_input_type *in, arm_ls_output_type *out)
{
	uint32 data;
	Std_ReturnType err;

	out->Rt = in->Rt;
	out->Rn = in->Rt;
	out->address = 0U;
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	if (!out->passed) {
		return STD_E_OK;
	}
	out->address = ArmLiteralAddress(core, in);
	err = ArmMemU_R(core, out->address, 2U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	out->Rt.regData = ZeroExtend(data, 16U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	return STD_E_OK;
}

/*
 * LDRSB (literal)
 *     if ConditionPassed() then
 *     base = Align(PC,4);
 *     address = if add then (base + imm32) else (base - imm32);
 *     R[t] = SignExtend(MemU[address,1], 32);
 */
static inline Std_ReturnType arm_pseudo_ldrsb_lit(TargetCoreType *core, const arm_ls_lit_input_type *in, arm_ls_output_type *out)
{
	uint32 data;
	Std_ReturnType err;

	out->Rt = in->Rt;
	out->Rn = in->Rt;
	out->address = 0U;
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	if (!out->passed) {
		return STD_E_OK;
	}
	out->address = ArmLiteralAddress(core, in);
	err = ArmMemU_R(core, out->address, 1U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	out->Rt.regData = SignExtend(data, 8U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	return STD_E_OK;
}

/*
 * LDRSH (literal)
 *     if ConditionPassed() then
 *     base = Align(PC,4);
 *     address = if add then (base + imm32) else (base - imm32);
 *     data = MemU[address,2];
 *     R[t] = SignExtend(data, 32);
 */
static inline Std_ReturnType arm_pseudo_ldrsh_lit(TargetCoreType *core, const arm_ls_lit_input_type *in, arm_ls_output_type *out)
{
	uint32 data;
	Std_ReturnType err;

	out->Rt = in->Rt;
	out->Rn = in->Rt;
	out->address = 0U;
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	if (!out->passed) {
		return STD_E_OK;
	}
	out->address = ArmLiteralAddress(core, in);
	err = ArmMemU_R(core, out->address, 2U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	out->Rt.regData = SignExtend(data, 16U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	return STD_E_OK;
}

/*
 * LDRT (unprivileged; Armv7-M: offset form, A32: post-indexed)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     data = MemU_unpriv[address,4];
 *     if wback then R[n] = offset_addr;
 *     R[t] = data;
 */
static inline Std_ReturnType arm_pseudo_ldrt(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_unpriv_R(core, out->address, 4U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	out->Rt.regData = data;
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	return STD_E_OK;
}

/*
 * LDRBT (unprivileged; Armv7-M: offset form, A32: post-indexed)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     data = MemU_unpriv[address,1];
 *     if wback then R[n] = offset_addr;
 *     R[t] = ZeroExtend(data, 32);
 */
static inline Std_ReturnType arm_pseudo_ldrbt(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_unpriv_R(core, out->address, 1U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	out->Rt.regData = ZeroExtend(data, 8U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	return STD_E_OK;
}

/*
 * LDRHT (unprivileged; Armv7-M: offset form, A32: post-indexed)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     data = MemU_unpriv[address,2];
 *     if wback then R[n] = offset_addr;
 *     R[t] = ZeroExtend(data, 32);
 */
static inline Std_ReturnType arm_pseudo_ldrht(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_unpriv_R(core, out->address, 2U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	out->Rt.regData = ZeroExtend(data, 16U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	return STD_E_OK;
}

/*
 * LDRSBT (unprivileged; Armv7-M: offset form, A32: post-indexed)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     data = MemU_unpriv[address,1];
 *     if wback then R[n] = offset_addr;
 *     R[t] = SignExtend(data, 32);
 */
static inline Std_ReturnType arm_pseudo_ldrsbt(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_unpriv_R(core, out->address, 1U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	out->Rt.regData = SignExtend(data, 8U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	return STD_E_OK;
}

/*
 * LDRSHT (unprivileged; Armv7-M: offset form, A32: post-indexed)
 *     if ConditionPassed() then
 *     offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *     address = if index then offset_addr else R[n];
 *     data = MemU_unpriv[address,2];
 *     if wback then R[n] = offset_addr;
 *     R[t] = SignExtend(data, 32);
 */
static inline Std_ReturnType arm_pseudo_ldrsht(TargetCoreType *core, const arm_ls_imm_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemU_unpriv_R(core, out->address, 2U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	out->Rt.regData = SignExtend(data, 16U);
	ArmRWrite(core, in->Rt.regId, out->Rt.regData);
	return STD_E_OK;
}

#endif /* ARM_COMMON_ARM_PSEUDO_LOAD_H */
