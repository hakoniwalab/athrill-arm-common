/*
 * Pseudo-code of the bitfield instructions (SBFX, UBFX, BFI, BFC) and of
 * the bit and byte operations on one register (REV, REV16, REVSH, RBIT,
 * CLZ).
 */
#ifndef ARM_COMMON_ARM_PSEUDO_BITFIELD_H
#define ARM_COMMON_ARM_PSEUDO_BITFIELD_H

#include "arm_pseudo_dp.h"

/* R[n]<msbit:lsbit>, shifted down to bit 0 (msbit >= lsbit, both 0..31). */
static inline uint32 ArmBitfield(uint32 x, uint32 msbit, uint32 lsbit)
{
	return ZeroExtend(x >> lsbit, msbit - lsbit + 1U);
}

/*
 * SBFX, UBFX
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rd;
	PseudoCodeRegisterDataType Rn;
	uint32 lsbit;
	uint32 widthminus1;
} arm_bfx_input_type;

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     msbit = lsbit + widthminus1;
 *     if msbit <= 31 then
 *         R[d] = SignExtend(R[n]<msbit:lsbit>, 32);
 *     else
 *         UNPREDICTABLE;
 */
static inline Std_ReturnType arm_pseudo_sbfx(TargetCoreType *core, const arm_bfx_input_type *in, arm_dp_output_type *out)
{
	uint32 msbit;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	msbit = in->lsbit + in->widthminus1;
	if (msbit > 31U) {
		return STD_E_INVALID;
	}
	return ArmResultWrite(core, in->Rd.regId, SignExtend(ArmBitfield(in->Rn.regData, msbit, in->lsbit), in->widthminus1 + 1U), out);
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     msbit = lsbit + widthminus1;
 *     if msbit <= 31 then
 *         R[d] = ZeroExtend(R[n]<msbit:lsbit>, 32);
 *     else
 *         UNPREDICTABLE;
 */
static inline Std_ReturnType arm_pseudo_ubfx(TargetCoreType *core, const arm_bfx_input_type *in, arm_dp_output_type *out)
{
	uint32 msbit;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	msbit = in->lsbit + in->widthminus1;
	if (msbit > 31U) {
		return STD_E_INVALID;
	}
	return ArmResultWrite(core, in->Rd.regId, ArmBitfield(in->Rn.regData, msbit, in->lsbit), out);
}

/*
 * BFI, BFC (BFC leaves Rn unset: its source is zeros)
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rd;
	PseudoCodeRegisterDataType Rn;
	uint32 lsbit;
	uint32 msbit;
} arm_bfi_input_type;

/* R[d]<msbit:lsbit> = value<(msbit-lsbit):0>; the other bits of R[d] unchanged. */
static inline uint32 ArmBitfieldInsert(uint32 d, uint32 value, uint32 msbit, uint32 lsbit)
{
	uint32 mask = ZeroExtend(0xFFFFFFFFU, msbit - lsbit + 1U) << lsbit;

	return (d & ~mask) | ((value << lsbit) & mask);
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     if msbit >= lsbit then
 *         R[d]<msbit:lsbit> = R[n]<(msbit-lsbit):0>;
 *         // Other bits of R[d] are unchanged
 *     else
 *         UNPREDICTABLE;
 */
static inline Std_ReturnType arm_pseudo_bfi(TargetCoreType *core, const arm_bfi_input_type *in, arm_dp_output_type *out)
{
	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	if (in->msbit < in->lsbit) {
		return STD_E_INVALID;
	}
	return ArmResultWrite(core, in->Rd.regId, ArmBitfieldInsert(in->Rd.regData, in->Rn.regData, in->msbit, in->lsbit), out);
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     if msbit >= lsbit then
 *         R[d]<msbit:lsbit> = Replicate('0', msbit-lsbit+1);
 *         // Other bits of R[d] are unchanged
 *     else
 *         UNPREDICTABLE;
 */
static inline Std_ReturnType arm_pseudo_bfc(TargetCoreType *core, const arm_bfi_input_type *in, arm_dp_output_type *out)
{
	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	if (in->msbit < in->lsbit) {
		return STD_E_INVALID;
	}
	return ArmResultWrite(core, in->Rd.regId, ArmBitfieldInsert(in->Rd.regData, 0U, in->msbit, in->lsbit), out);
}

/*
 * REV, REV16, REVSH, RBIT, CLZ
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rd;
	PseudoCodeRegisterDataType Rm;
} arm_unary_input_type;

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     bits(32) result;
 *     result<31:24> = R[m]<7:0>;
 *     result<23:16> = R[m]<15:8>;
 *     result<15:8>  = R[m]<23:16>;
 *     result<7:0>   = R[m]<31:24>;
 *     R[d] = result;
 */
static inline Std_ReturnType arm_pseudo_rev(TargetCoreType *core, const arm_unary_input_type *in, arm_dp_output_type *out)
{
	uint32 m;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	m = in->Rm.regData;
	result = ((m & 0xFFU) << 24)
		| (((m >> 8) & 0xFFU) << 16)
		| (((m >> 16) & 0xFFU) << 8)
		| ((m >> 24) & 0xFFU);
	return ArmResultWrite(core, in->Rd.regId, result, out);
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     bits(32) result;
 *     result<31:24> = R[m]<23:16>;
 *     result<23:16> = R[m]<31:24>;
 *     result<15:8>  = R[m]<7:0>;
 *     result<7:0>   = R[m]<15:8>;
 *     R[d] = result;
 */
static inline Std_ReturnType arm_pseudo_rev16(TargetCoreType *core, const arm_unary_input_type *in, arm_dp_output_type *out)
{
	uint32 m;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	m = in->Rm.regData;
	result = (((m >> 16) & 0xFFU) << 24)
		| (((m >> 24) & 0xFFU) << 16)
		| ((m & 0xFFU) << 8)
		| ((m >> 8) & 0xFFU);
	return ArmResultWrite(core, in->Rd.regId, result, out);
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     bits(32) result;
 *     result<31:8> = SignExtend(R[m]<7:0>, 24);
 *     result<7:0>  = R[m]<15:8>;
 *     R[d] = result;
 */
static inline Std_ReturnType arm_pseudo_revsh(TargetCoreType *core, const arm_unary_input_type *in, arm_dp_output_type *out)
{
	uint32 m;
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	m = in->Rm.regData;
	result = (SignExtend(m, 8U) << 8) | ((m >> 8) & 0xFFU);
	return ArmResultWrite(core, in->Rd.regId, result, out);
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     bits(32) result;
 *     for i = 0 to 31
 *         result<31-i> = R[m]<i>;
 *     R[d] = result;
 */
static inline Std_ReturnType arm_pseudo_rbit(TargetCoreType *core, const arm_unary_input_type *in, arm_dp_output_type *out)
{
	uint32 result;
	uint32 i;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = 0U;
	for (i = 0U; i < 32U; i++) {
		if (ARM_BIT(in->Rm.regData, i)) {
			result |= 1U << (31U - i);
		}
	}
	return ArmResultWrite(core, in->Rd.regId, result, out);
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     result = CountLeadingZeroBits(R[m]);
 *     R[d] = result<31:0>;
 */
static inline Std_ReturnType arm_pseudo_clz(TargetCoreType *core, const arm_unary_input_type *in, arm_dp_output_type *out)
{
	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	return ArmResultWrite(core, in->Rd.regId, CountLeadingZeroBits(in->Rm.regData), out);
}

#endif /* ARM_COMMON_ARM_PSEUDO_BITFIELD_H */
