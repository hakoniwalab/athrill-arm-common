/*
 * Pseudo-code of the dual and multiple load and store instructions (LDRD,
 * STRD; LDM, LDMDB, STM, STMDB, and PUSH and POP, which are STMDB and LDM
 * on SP with write-back).
 */
#ifndef ARM_COMMON_ARM_PSEUDO_MULTIPLE_H
#define ARM_COMMON_ARM_PSEUDO_MULTIPLE_H

#include "arm_pseudo_ls.h"

/*
 * LDRD, STRD
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rt;
	PseudoCodeRegisterDataType Rt2;
	PseudoCodeRegisterDataType Rn;		/* unset for LDRD (literal) */
	PseudoCodeImmediateData32Type imm32;
	bool index;
	bool add;
	bool wback;
} arm_ls_dual_input_type;

/*
 * LDRD (immediate)
 *     if ConditionPassed() then
 *         offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *         address = if index then offset_addr else R[n];
 *         R[t] = MemA[address,4];
 *         R[t2] = MemA[address+4,4];
 *         if wback then R[n] = offset_addr;
 * (out->Rt holds R[t]; R[t2] is in the core.)
 */
static inline Std_ReturnType arm_pseudo_ldrd_imm(TargetCoreType *core, const arm_ls_dual_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	uint32 data;
	uint32 data2;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemA_R(core, out->address, 4U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	err = ArmMemA_R(core, out->address + 4U, 4U, &data2);
	if (err != STD_E_OK) {
		return err;
	}
	ArmRWrite(core, in->Rt.regId, data);
	ArmRWrite(core, in->Rt2.regId, data2);
	out->Rt.regData = data;
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return STD_E_OK;
}

/*
 * LDRD (literal)
 *     if ConditionPassed() then
 *         address = if add then (Align(PC,4) + imm32) else (Align(PC,4) - imm32);
 *         R[t] = MemA[address,4];
 *         R[t2] = MemA[address+4,4];
 */
static inline Std_ReturnType arm_pseudo_ldrd_lit(TargetCoreType *core, const arm_ls_dual_input_type *in, arm_ls_output_type *out)
{
	uint32 base = Align(ArmPC(core), 4U);
	uint32 data;
	uint32 data2;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	out->address = in->add ? (base + in->imm32) : (base - in->imm32);
	err = ArmMemA_R(core, out->address, 4U, &data);
	if (err != STD_E_OK) {
		return err;
	}
	err = ArmMemA_R(core, out->address + 4U, 4U, &data2);
	if (err != STD_E_OK) {
		return err;
	}
	ArmRWrite(core, in->Rt.regId, data);
	ArmRWrite(core, in->Rt2.regId, data2);
	out->Rt.regData = data;
	return STD_E_OK;
}

/*
 * STRD (immediate)
 *     if ConditionPassed() then
 *         offset_addr = if add then (R[n] + imm32) else (R[n] - imm32);
 *         address = if index then offset_addr else R[n];
 *         MemA[address,4] = R[t];
 *         MemA[address+4,4] = R[t2];
 *         if wback then R[n] = offset_addr;
 */
static inline Std_ReturnType arm_pseudo_strd_imm(TargetCoreType *core, const arm_ls_dual_input_type *in, arm_ls_output_type *out)
{
	uint32 offset_addr;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	offset_addr = ArmLoadStoreAddress(in->Rn.regData, in->imm32, in->index, in->add, out);
	err = ArmMemA_W(core, out->address, 4U, in->Rt.regData);
	if (err != STD_E_OK) {
		return err;
	}
	err = ArmMemA_W(core, out->address + 4U, 4U, in->Rt2.regData);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, offset_addr, out);
	return STD_E_OK;
}

/*
 * LDM, LDMDB, STM, STMDB
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rn;
	uint32 registers;	/* bit i set: R[i] in the list */
	bool wback;
} arm_ls_multiple_input_type;

/*
 * Loads the list from address upwards:
 *     for i = 0 to 14
 *         if registers<i> == '1' then
 *             R[i] = MemA[address,4]; address = address + 4;
 *     if registers<15> == '1' then
 *         LoadWritePC(MemA[address,4]);
 * Write-back is done by the caller before LoadWritePC is taken, as in the
 * pseudo-code: the loads, then R[n], then the branch.
 */
static inline Std_ReturnType ArmLoadMultiple(TargetCoreType *core, const arm_ls_multiple_input_type *in, uint32 address,
	uint32 wback_value, arm_ls_output_type *out)
{
	uint32 values[16];
	uint32 i;
	Std_ReturnType err;

	for (i = 0U; i < 16U; i++) {
		if (ARM_BIT(in->registers, i) != 0U) {
			err = ArmMemA_R(core, address, 4U, &values[i]);
			if (err != STD_E_OK) {
				return err;
			}
			address += 4U;
		}
	}
	for (i = 0U; i < 15U; i++) {
		if (ARM_BIT(in->registers, i) != 0U) {
			ArmRWrite(core, i, values[i]);
		}
	}
	if (in->wback && (ARM_BIT(in->registers, in->Rn.regId) == 0U)) {
		ArmRWrite(core, in->Rn.regId, wback_value);
		out->Rn.regData = wback_value;
	}
	if (ARM_BIT(in->registers, 15U) != 0U) {
		return ArmLoadWritePC(core, values[15]);
	}
	return STD_E_OK;
}

/*
 * Stores the list from address upwards:
 *     for i = 0 to 14 (15 in ARM state)
 *         if registers<i> == '1' then
 *             MemA[address,4] = R[i]; address = address + 4;
 * (R[n] in the list with write-back and not the lowest register stores an
 * UNKNOWN value; the value before write-back is stored.)
 */
static inline Std_ReturnType ArmStoreMultiple(TargetCoreType *core, const arm_ls_multiple_input_type *in, uint32 address)
{
	uint32 i;
	Std_ReturnType err;

	for (i = 0U; i < 16U; i++) {
		if (ARM_BIT(in->registers, i) != 0U) {
			err = ArmMemA_W(core, address, 4U, ArmReadReg(core, i));
			if (err != STD_E_OK) {
				return err;
			}
			address += 4U;
		}
	}
	return STD_E_OK;
}

static inline bool ArmMultipleBegin(const TargetCoreType *core, const arm_ls_multiple_input_type *in, arm_ls_output_type *out)
{
	out->Rt = in->Rn;
	out->Rn = in->Rn;
	out->address = 0U;
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	return out->passed;
}

/*
 * LDM / LDMIA / LDMFD (and POP)
 *     if ConditionPassed() then
 *         address = R[n];
 *         (loads, ArmLoadMultiple)
 *         if wback && registers<n> == '0' then R[n] = R[n] + 4*BitCount(registers);
 */
static inline Std_ReturnType arm_pseudo_ldm(TargetCoreType *core, const arm_ls_multiple_input_type *in, arm_ls_output_type *out)
{
	uint32 size;

	if (!ArmMultipleBegin(core, in, out)) {
		return STD_E_OK;
	}
	size = 4U * BitCount(in->registers);
	out->address = in->Rn.regData;
	return ArmLoadMultiple(core, in, out->address, in->Rn.regData + size, out);
}

/*
 * LDMDB / LDMEA
 *     if ConditionPassed() then
 *         address = R[n] - 4*BitCount(registers);
 *         (loads, ArmLoadMultiple)
 *         if wback && registers<n> == '0' then R[n] = R[n] - 4*BitCount(registers);
 */
static inline Std_ReturnType arm_pseudo_ldmdb(TargetCoreType *core, const arm_ls_multiple_input_type *in, arm_ls_output_type *out)
{
	uint32 size;

	if (!ArmMultipleBegin(core, in, out)) {
		return STD_E_OK;
	}
	size = 4U * BitCount(in->registers);
	out->address = in->Rn.regData - size;
	return ArmLoadMultiple(core, in, out->address, in->Rn.regData - size, out);
}

/*
 * STM / STMIA / STMEA
 *     if ConditionPassed() then
 *         address = R[n];
 *         (stores, ArmStoreMultiple)
 *         if wback then R[n] = R[n] + 4*BitCount(registers);
 */
static inline Std_ReturnType arm_pseudo_stm(TargetCoreType *core, const arm_ls_multiple_input_type *in, arm_ls_output_type *out)
{
	uint32 size;
	Std_ReturnType err;

	if (!ArmMultipleBegin(core, in, out)) {
		return STD_E_OK;
	}
	size = 4U * BitCount(in->registers);
	out->address = in->Rn.regData;
	err = ArmStoreMultiple(core, in, out->address);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, in->Rn.regData + size, out);
	return STD_E_OK;
}

/*
 * STMDB / STMFD (and PUSH)
 *     if ConditionPassed() then
 *         address = R[n] - 4*BitCount(registers);
 *         (stores, ArmStoreMultiple)
 *         if wback then R[n] = R[n] - 4*BitCount(registers);
 */
static inline Std_ReturnType arm_pseudo_stmdb(TargetCoreType *core, const arm_ls_multiple_input_type *in, arm_ls_output_type *out)
{
	uint32 size;
	Std_ReturnType err;

	if (!ArmMultipleBegin(core, in, out)) {
		return STD_E_OK;
	}
	size = 4U * BitCount(in->registers);
	out->address = in->Rn.regData - size;
	err = ArmStoreMultiple(core, in, out->address);
	if (err != STD_E_OK) {
		return err;
	}
	ArmLoadStoreWriteBack(core, in->wback, in->Rn.regId, in->Rn.regData - size, out);
	return STD_E_OK;
}

#endif /* ARM_COMMON_ARM_PSEUDO_MULTIPLE_H */
