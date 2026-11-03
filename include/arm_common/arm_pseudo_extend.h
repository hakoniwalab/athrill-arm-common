/*
 * Pseudo-code of the extend instructions: SXTB, SXTH, UXTB, UXTH, the
 * forms that add (SXTAB, SXTAH, UXTAB, UXTAH) and the dual 16-bit forms
 * (SXTB16, UXTB16, SXTAB16, UXTAB16).
 *
 * Every one rotates R[m] first:
 *     rotated = ROR(R[m], rotation);
 * The forms without an add leave Rn unset (ARM_NO_REG).
 */
#ifndef ARM_COMMON_ARM_PSEUDO_EXTEND_H
#define ARM_COMMON_ARM_PSEUDO_EXTEND_H

#include "arm_pseudo_dp.h"

typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rd;
	PseudoCodeRegisterDataType Rn;
	PseudoCodeRegisterDataType Rm;
	uint32 rotation;	/* 0, 8, 16 or 24 */
} arm_extend_input_type;

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     rotated = ROR(R[m], rotation);
 * Returns whether the condition passed, and rotated.
 */
static inline bool ArmExtendBegin(const TargetCoreType *core, const arm_extend_input_type *in, arm_dp_output_type *out,
	uint32 *rotated)
{
	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return FALSE;
	}
	*rotated = ROR(in->Rm.regData, in->rotation);
	return TRUE;
}

/* R[d]<31:16> = high; R[d]<15:0> = low; (the dual 16-bit forms) */
static inline uint32 ArmHalfwords(uint32 high, uint32 low)
{
	return ((high & 0xFFFFU) << 16) | (low & 0xFFFFU);
}

/*
 * SXTB
 *     R[d] = SignExtend(rotated<7:0>, 32);
 */
static inline Std_ReturnType arm_pseudo_sxtb(TargetCoreType *core, const arm_extend_input_type *in, arm_dp_output_type *out)
{
	uint32 rotated;

	if (!ArmExtendBegin(core, in, out, &rotated)) {
		return STD_E_OK;
	}
	return ArmResultWrite(core, in->Rd.regId, SignExtend(rotated, 8U), out);
}

/*
 * SXTH
 *     R[d] = SignExtend(rotated<15:0>, 32);
 */
static inline Std_ReturnType arm_pseudo_sxth(TargetCoreType *core, const arm_extend_input_type *in, arm_dp_output_type *out)
{
	uint32 rotated;

	if (!ArmExtendBegin(core, in, out, &rotated)) {
		return STD_E_OK;
	}
	return ArmResultWrite(core, in->Rd.regId, SignExtend(rotated, 16U), out);
}

/*
 * UXTB
 *     R[d] = ZeroExtend(rotated<7:0>, 32);
 */
static inline Std_ReturnType arm_pseudo_uxtb(TargetCoreType *core, const arm_extend_input_type *in, arm_dp_output_type *out)
{
	uint32 rotated;

	if (!ArmExtendBegin(core, in, out, &rotated)) {
		return STD_E_OK;
	}
	return ArmResultWrite(core, in->Rd.regId, ZeroExtend(rotated, 8U), out);
}

/*
 * UXTH
 *     R[d] = ZeroExtend(rotated<15:0>, 32);
 */
static inline Std_ReturnType arm_pseudo_uxth(TargetCoreType *core, const arm_extend_input_type *in, arm_dp_output_type *out)
{
	uint32 rotated;

	if (!ArmExtendBegin(core, in, out, &rotated)) {
		return STD_E_OK;
	}
	return ArmResultWrite(core, in->Rd.regId, ZeroExtend(rotated, 16U), out);
}

/*
 * SXTAB
 *     R[d] = R[n] + SignExtend(rotated<7:0>, 32);
 */
static inline Std_ReturnType arm_pseudo_sxtab(TargetCoreType *core, const arm_extend_input_type *in, arm_dp_output_type *out)
{
	uint32 rotated;

	if (!ArmExtendBegin(core, in, out, &rotated)) {
		return STD_E_OK;
	}
	return ArmResultWrite(core, in->Rd.regId, in->Rn.regData + SignExtend(rotated, 8U), out);
}

/*
 * SXTAH
 *     R[d] = R[n] + SignExtend(rotated<15:0>, 32);
 */
static inline Std_ReturnType arm_pseudo_sxtah(TargetCoreType *core, const arm_extend_input_type *in, arm_dp_output_type *out)
{
	uint32 rotated;

	if (!ArmExtendBegin(core, in, out, &rotated)) {
		return STD_E_OK;
	}
	return ArmResultWrite(core, in->Rd.regId, in->Rn.regData + SignExtend(rotated, 16U), out);
}

/*
 * UXTAB
 *     R[d] = R[n] + ZeroExtend(rotated<7:0>, 32);
 */
static inline Std_ReturnType arm_pseudo_uxtab(TargetCoreType *core, const arm_extend_input_type *in, arm_dp_output_type *out)
{
	uint32 rotated;

	if (!ArmExtendBegin(core, in, out, &rotated)) {
		return STD_E_OK;
	}
	return ArmResultWrite(core, in->Rd.regId, in->Rn.regData + ZeroExtend(rotated, 8U), out);
}

/*
 * UXTAH
 *     R[d] = R[n] + ZeroExtend(rotated<15:0>, 32);
 */
static inline Std_ReturnType arm_pseudo_uxtah(TargetCoreType *core, const arm_extend_input_type *in, arm_dp_output_type *out)
{
	uint32 rotated;

	if (!ArmExtendBegin(core, in, out, &rotated)) {
		return STD_E_OK;
	}
	return ArmResultWrite(core, in->Rd.regId, in->Rn.regData + ZeroExtend(rotated, 16U), out);
}

/*
 * SXTB16
 *     R[d]<15:0> = SignExtend(rotated<7:0>, 16);
 *     R[d]<31:16> = SignExtend(rotated<23:16>, 16);
 */
static inline Std_ReturnType arm_pseudo_sxtb16(TargetCoreType *core, const arm_extend_input_type *in, arm_dp_output_type *out)
{
	uint32 rotated;
	uint32 low;
	uint32 high;

	if (!ArmExtendBegin(core, in, out, &rotated)) {
		return STD_E_OK;
	}
	low = SignExtend(rotated, 8U);
	high = SignExtend(rotated >> 16, 8U);
	return ArmResultWrite(core, in->Rd.regId, ArmHalfwords(high, low), out);
}

/*
 * UXTB16
 *     R[d]<15:0> = ZeroExtend(rotated<7:0>, 16);
 *     R[d]<31:16> = ZeroExtend(rotated<23:16>, 16);
 */
static inline Std_ReturnType arm_pseudo_uxtb16(TargetCoreType *core, const arm_extend_input_type *in, arm_dp_output_type *out)
{
	uint32 rotated;
	uint32 low;
	uint32 high;

	if (!ArmExtendBegin(core, in, out, &rotated)) {
		return STD_E_OK;
	}
	low = ZeroExtend(rotated, 8U);
	high = ZeroExtend(rotated >> 16, 8U);
	return ArmResultWrite(core, in->Rd.regId, ArmHalfwords(high, low), out);
}

/*
 * SXTAB16
 *     R[d]<15:0> = R[n]<15:0> + SignExtend(rotated<7:0>, 16);
 *     R[d]<31:16> = R[n]<31:16> + SignExtend(rotated<23:16>, 16);
 */
static inline Std_ReturnType arm_pseudo_sxtab16(TargetCoreType *core, const arm_extend_input_type *in, arm_dp_output_type *out)
{
	uint32 rotated;
	uint32 low;
	uint32 high;

	if (!ArmExtendBegin(core, in, out, &rotated)) {
		return STD_E_OK;
	}
	low = in->Rn.regData + SignExtend(rotated, 8U);
	high = (in->Rn.regData >> 16) + SignExtend(rotated >> 16, 8U);
	return ArmResultWrite(core, in->Rd.regId, ArmHalfwords(high, low), out);
}

/*
 * UXTAB16
 *     R[d]<15:0> = R[n]<15:0> + ZeroExtend(rotated<7:0>, 16);
 *     R[d]<31:16> = R[n]<31:16> + ZeroExtend(rotated<23:16>, 16);
 */
static inline Std_ReturnType arm_pseudo_uxtab16(TargetCoreType *core, const arm_extend_input_type *in, arm_dp_output_type *out)
{
	uint32 rotated;
	uint32 low;
	uint32 high;

	if (!ArmExtendBegin(core, in, out, &rotated)) {
		return STD_E_OK;
	}
	low = in->Rn.regData + ZeroExtend(rotated, 8U);
	high = (in->Rn.regData >> 16) + ZeroExtend(rotated >> 16, 8U);
	return ArmResultWrite(core, in->Rd.regId, ArmHalfwords(high, low), out);
}

#endif /* ARM_COMMON_ARM_PSEUDO_EXTEND_H */
