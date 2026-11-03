/*
 * Pseudo-code of the branch instructions.
 */
#ifndef ARM_COMMON_ARM_PSEUDO_BRANCH_H
#define ARM_COMMON_ARM_PSEUDO_BRANCH_H

#include "arm_profile.h"

/*
 * B
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeImmediateData32Type imm32;	/* signed offset */
} arm_b_input_type;

typedef struct {
	bool passed;
	uint32 target;
} arm_b_output_type;

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     BranchWritePC(PC + imm32);
 */
static inline Std_ReturnType arm_pseudo_b(TargetCoreType *core, const arm_b_input_type *in, arm_b_output_type *out)
{
	out->target = 0U;
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	if (!out->passed) {
		return STD_E_OK;
	}
	out->target = ArmPC(core) + in->imm32;
	return ArmBranchWritePC(core, out->target);
}

/*
 * TBB, TBH
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rn;
	PseudoCodeRegisterDataType Rm;
} arm_tb_input_type;

/*
 * if ConditionPassed() then
 *     if is_tbh then
 *         halfwords = UInt(MemU[R[n]+LSL(R[m],1), 2]);
 *     else
 *         halfwords = UInt(MemU[R[n]+R[m], 1]);
 *     BranchWritePC(PC + 2*halfwords);
 */
static inline Std_ReturnType ArmTableBranch(TargetCoreType *core, const arm_tb_input_type *in, bool is_tbh, arm_b_output_type *out)
{
	uint32 halfwords;
	Std_ReturnType err;

	out->target = 0U;
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	if (!out->passed) {
		return STD_E_OK;
	}
	if (is_tbh) {
		err = ArmMemU_R(core, in->Rn.regData + LSL(in->Rm.regData, 1U), 2U, &halfwords);
	}
	else {
		err = ArmMemU_R(core, in->Rn.regData + in->Rm.regData, 1U, &halfwords);
	}
	if (err != STD_E_OK) {
		return err;
	}
	out->target = ArmPC(core) + (2U * halfwords);
	return ArmBranchWritePC(core, out->target);
}

static inline Std_ReturnType arm_pseudo_tbb(TargetCoreType *core, const arm_tb_input_type *in, arm_b_output_type *out)
{
	return ArmTableBranch(core, in, FALSE, out);
}

static inline Std_ReturnType arm_pseudo_tbh(TargetCoreType *core, const arm_tb_input_type *in, arm_b_output_type *out)
{
	return ArmTableBranch(core, in, TRUE, out);
}

#define ARM_REG_LR	14U

/*
 * BL (immediate)
 */

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     if CurrentInstrSet() == InstrSet_ARM then
 *         LR = PC - 4;
 *     else
 *         LR = PC<31:1> : '1';
 *     BranchWritePC(PC + imm32);
 *
 * (Armv7-A's BL, whose target instruction set is the current one; BLX
 * (immediate), which changes it, is not here. Armv7-M's BL is the Thumb case.)
 */
static inline Std_ReturnType arm_pseudo_bl(TargetCoreType *core, const arm_b_input_type *in, arm_b_output_type *out)
{
	out->target = 0U;
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	if (!out->passed) {
		return STD_E_OK;
	}
	if (ArmCurrentInstrSet(core) == InstrSet_ARM) {
		ArmRWrite(core, ARM_REG_LR, ArmPC(core) - 4U);
	}
	else {
		ArmRWrite(core, ARM_REG_LR, ArmPC(core) | 1U);
	}
	out->target = ArmPC(core) + in->imm32;
	return ArmBranchWritePC(core, out->target);
}

/*
 * BX, BLX (register)
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rm;
} arm_blx_reg_input_type;

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     target = R[m];
 *     if CurrentInstrSet() == InstrSet_ARM then
 *         next_instr_addr = PC - 4;
 *         LR = next_instr_addr;
 *     else
 *         next_instr_addr = PC - 2;
 *         LR = next_instr_addr<31:1> : '1';
 *     BLXWritePC(target);
 *
 * (Armv7-A writes PC with BXWritePC, Armv7-M with BLXWritePC; the profile's
 * ArmBLXWritePC is the one of its profile.)
 */
static inline Std_ReturnType arm_pseudo_blx_reg(TargetCoreType *core, const arm_blx_reg_input_type *in, arm_b_output_type *out)
{
	uint32 next_instr_addr;

	out->target = 0U;
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	if (!out->passed) {
		return STD_E_OK;
	}
	out->target = in->Rm.regData;
	if (ArmCurrentInstrSet(core) == InstrSet_ARM) {
		next_instr_addr = ArmPC(core) - 4U;
		ArmRWrite(core, ARM_REG_LR, next_instr_addr);
	}
	else {
		next_instr_addr = ArmPC(core) - 2U;
		ArmRWrite(core, ARM_REG_LR, next_instr_addr | 1U);
	}
	return ArmBLXWritePC(core, out->target);
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     BXWritePC(R[m]);
 */
static inline Std_ReturnType arm_pseudo_bx(TargetCoreType *core, const arm_blx_reg_input_type *in, arm_b_output_type *out)
{
	out->target = 0U;
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	if (!out->passed) {
		return STD_E_OK;
	}
	out->target = in->Rm.regData;
	return ArmBXWritePC(core, out->target);
}

/*
 * CBZ, CBNZ
 */
typedef struct {
	const char *instrName;
	PseudoCodeRegisterDataType Rn;
	PseudoCodeImmediateData32Type imm32;
	bool nonzero;
} arm_cb_input_type;

/*
 * EncodingSpecificOperations();
 * if nonzero != IsZero(R[n]) then
 *     BranchWritePC(PC + imm32);
 *
 * (No ConditionPassed(): CBZ and CBNZ are not conditional.)
 */
static inline Std_ReturnType arm_pseudo_cb(TargetCoreType *core, const arm_cb_input_type *in, arm_b_output_type *out)
{
	out->target = 0U;
	out->passed = (in->nonzero != (in->Rn.regData == 0U));
	if (!out->passed) {
		return STD_E_OK;
	}
	out->target = ArmPC(core) + in->imm32;
	return ArmBranchWritePC(core, out->target);
}

#endif /* ARM_COMMON_ARM_PSEUDO_BRANCH_H */
