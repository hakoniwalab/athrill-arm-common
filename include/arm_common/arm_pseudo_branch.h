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

#endif /* ARM_COMMON_ARM_PSEUDO_BRANCH_H */
