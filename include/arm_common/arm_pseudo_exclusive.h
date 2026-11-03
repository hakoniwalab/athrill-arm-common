/*
 * Pseudo-code of the exclusive load and store instructions (LDREX, LDREXB,
 * LDREXH, STREX, STREXB, STREXH, CLREX). The exclusive monitors are the
 * profile's (ArmSetExclusiveMonitors, ArmExclusiveMonitorsPass,
 * ArmClearExclusiveLocal).
 */
#ifndef ARM_COMMON_ARM_PSEUDO_EXCLUSIVE_H
#define ARM_COMMON_ARM_PSEUDO_EXCLUSIVE_H

#include "arm_pseudo_ls.h"

typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rd;		/* STREX status; unset for LDREX */
	PseudoCodeRegisterDataType Rt;
	PseudoCodeRegisterDataType Rn;
	PseudoCodeImmediateData32Type imm32;
} arm_ls_exclusive_input_type;

/*
 * LDREX (size 4), LDREXB (1), LDREXH (2)
 *     if ConditionPassed() then
 *         address = R[n] + imm32;
 *         SetExclusiveMonitors(address,size);
 *         R[t] = ZeroExtend(MemA[address,size], 32);
 */
static inline Std_ReturnType ArmLoadExclusive(TargetCoreType *core, const arm_ls_exclusive_input_type *in, uint32 size, arm_ls_output_type *out)
{
	uint32 data;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	out->address = in->Rn.regData + in->imm32;
	ArmSetExclusiveMonitors(core, out->address, size);
	err = ArmMemA_R(core, out->address, size, &data);
	if (err != STD_E_OK) {
		return err;
	}
	out->Rt.regData = data;
	ArmRWrite(core, in->Rt.regId, data);
	return STD_E_OK;
}

static inline Std_ReturnType arm_pseudo_ldrex(TargetCoreType *core, const arm_ls_exclusive_input_type *in, arm_ls_output_type *out)
{
	return ArmLoadExclusive(core, in, 4U, out);
}

static inline Std_ReturnType arm_pseudo_ldrexb(TargetCoreType *core, const arm_ls_exclusive_input_type *in, arm_ls_output_type *out)
{
	return ArmLoadExclusive(core, in, 1U, out);
}

static inline Std_ReturnType arm_pseudo_ldrexh(TargetCoreType *core, const arm_ls_exclusive_input_type *in, arm_ls_output_type *out)
{
	return ArmLoadExclusive(core, in, 2U, out);
}

/*
 * STREX (size 4), STREXB (1), STREXH (2)
 *     if ConditionPassed() then
 *         address = R[n] + imm32;
 *         if ExclusiveMonitorsPass(address,size) then
 *             MemA[address,size] = R[t]<8*size-1:0>;
 *             R[d] = ZeroExtend('0');
 *         else
 *             R[d] = ZeroExtend('1');
 * (out->Rt holds the status written to R[d].)
 */
static inline Std_ReturnType ArmStoreExclusive(TargetCoreType *core, const arm_ls_exclusive_input_type *in, uint32 size, arm_ls_output_type *out)
{
	bool pass;
	Std_ReturnType err;

	if (!ArmLoadStoreBegin(core, in->cond, &in->Rt, &in->Rn, out)) {
		return STD_E_OK;
	}
	out->address = in->Rn.regData + in->imm32;
	err = ArmExclusiveMonitorsPass(core, out->address, size, &pass);
	if (err != STD_E_OK) {
		return err;
	}
	if (pass) {
		err = ArmMemA_W(core, out->address, size, in->Rt.regData);
		if (err != STD_E_OK) {
			return err;
		}
		out->Rt.regData = 0U;
	}
	else {
		out->Rt.regData = 1U;
	}
	ArmRWrite(core, in->Rd.regId, out->Rt.regData);
	return STD_E_OK;
}

static inline Std_ReturnType arm_pseudo_strex(TargetCoreType *core, const arm_ls_exclusive_input_type *in, arm_ls_output_type *out)
{
	return ArmStoreExclusive(core, in, 4U, out);
}

static inline Std_ReturnType arm_pseudo_strexb(TargetCoreType *core, const arm_ls_exclusive_input_type *in, arm_ls_output_type *out)
{
	return ArmStoreExclusive(core, in, 1U, out);
}

static inline Std_ReturnType arm_pseudo_strexh(TargetCoreType *core, const arm_ls_exclusive_input_type *in, arm_ls_output_type *out)
{
	return ArmStoreExclusive(core, in, 2U, out);
}

/*
 * CLREX
 *     if ConditionPassed() then
 *         ClearExclusiveLocal(ProcessorID());
 */
static inline Std_ReturnType arm_pseudo_clrex(TargetCoreType *core, uint32 cond)
{
	if (ConditionHolds(cond, ArmAPSR(core))) {
		ArmClearExclusiveLocal(core);
	}
	return STD_E_OK;
}

#endif /* ARM_COMMON_ARM_PSEUDO_EXCLUSIVE_H */
