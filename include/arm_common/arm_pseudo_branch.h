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

#endif /* ARM_COMMON_ARM_PSEUDO_BRANCH_H */
