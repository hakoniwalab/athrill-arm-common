/*
 * Pseudo-code of the floating-point conversions (VCVT between the
 * formats, VCVTB/VCVTT, VCVT and VCVTR to and from integers, VCVT to and
 * from fixed point, VCVTA/VCVTN/VCVTP/VCVTM), of VRINTA ... VRINTX and of
 * VMAXNM, VMINNM.
 */
#ifndef ARM_COMMON_ARM_PSEUDO_FP_CONVERT_H
#define ARM_COMMON_ARM_PSEUDO_FP_CONVERT_H

#include "arm_pseudo_fp_arith.h"

typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	bool dp_operation;	/* the floating-point side is double precision */
	uint32 d;
	uint32 m;
	bool unsigned_int;	/* integer and fixed point: unsigned */
	uint32 size;		/* fixed point: 16 or 32 */
	uint32 fraction_bits;	/* fixed point */
	bool fpscr_rounding;	/* round by FPSCR.RMode, not by rounding */
	FPRounding rounding;
	bool exact;		/* VRINTX: signal Inexact */
	bool top;		/* VCVTT: the top halfword (lowbit = 16) */
} arm_fp_cvt_input_type;

/* if ConditionPassed() then EncodingSpecificOperations(); ExecuteFPCheck(); and FPSCR. */
static inline bool ArmFPCvtBegin(TargetCoreType *core, const arm_fp_cvt_input_type *in, arm_fp_output_type *out, uint32 *fpscr)
{
	out->result = 0U;
	if (!ArmFPBegin(core, in->cond, out)) {
		return FALSE;
	}
	*fpscr = ArmFPSCR(core);
	return TRUE;
}

static inline FPRounding ArmFPCvtRounding(const arm_fp_cvt_input_type *in, uint32 fpscr)
{
	return in->fpscr_rounding ? ArmFPSCRRounding(fpscr) : in->rounding;
}

/*
 * VCVT (between double-precision and single-precision)
 *     if double_to_single then S[d] = FPDoubleToSingle(D[m], TRUE);
 *     else D[d] = FPSingleToDouble(S[m], TRUE);
 * (dp_operation: the source is double precision.)
 */
static inline Std_ReturnType arm_pseudo_vcvt_fp(TargetCoreType *core, const arm_fp_cvt_input_type *in, arm_fp_output_type *out)
{
	uint32 fpscr;
	uint64 result;

	if (!ArmFPCvtBegin(core, in, out, &fpscr)) {
		return out->check;
	}
	if (in->dp_operation) {
		result = FPConvert(ArmD(core, in->m), 64U, 32U, &fpscr);
		ArmSWrite(core, in->d, (uint32)result);
	}
	else {
		result = FPConvert(ArmS(core, in->m), 32U, 64U, &fpscr);
		ArmDWrite(core, in->d, result);
	}
	out->result = result;
	ArmFPSCRWrite(core, fpscr);
	return STD_E_OK;
}

/*
 * VCVTB, VCVTT (half-precision and single- or double-precision)
 *     lowbit = if T == '1' then 16 else 0;
 *     if half_to_single then
 *         if dp_operation then D[d] = FPHalfToDouble(S[m]<lowbit+15:lowbit>, TRUE);
 *         else S[d] = FPHalfToSingle(S[m]<lowbit+15:lowbit>, TRUE);
 *     else
 *         if dp_operation then S[d]<lowbit+15:lowbit> = FPDoubleToHalf(D[m], TRUE);
 *         else S[d]<lowbit+15:lowbit> = FPSingleToHalf(S[m], TRUE);
 *         // S[d]'s other halfword unchanged
 */
static inline Std_ReturnType arm_pseudo_vcvt_from_half(TargetCoreType *core, const arm_fp_cvt_input_type *in, arm_fp_output_type *out)
{
	uint32 fpscr;
	uint64 half;

	if (!ArmFPCvtBegin(core, in, out, &fpscr)) {
		return out->check;
	}
	half = (ArmS(core, in->m) >> (in->top ? 16U : 0U)) & 0xFFFFU;
	if (in->dp_operation) {
		out->result = FPConvert(half, 16U, 64U, &fpscr);
		ArmDWrite(core, in->d, out->result);
	}
	else {
		out->result = FPConvert(half, 16U, 32U, &fpscr);
		ArmSWrite(core, in->d, (uint32)out->result);
	}
	ArmFPSCRWrite(core, fpscr);
	return STD_E_OK;
}

static inline Std_ReturnType arm_pseudo_vcvt_to_half(TargetCoreType *core, const arm_fp_cvt_input_type *in, arm_fp_output_type *out)
{
	uint32 fpscr;
	uint32 lowbit = in->top ? 16U : 0U;
	uint64 half;

	if (!ArmFPCvtBegin(core, in, out, &fpscr)) {
		return out->check;
	}
	if (in->dp_operation) {
		half = FPConvert(ArmD(core, in->m), 64U, 16U, &fpscr);
	}
	else {
		half = FPConvert(ArmS(core, in->m), 32U, 16U, &fpscr);
	}
	out->result = half;
	ArmSWrite(core, in->d, (ArmS(core, in->d) & ~(0xFFFFU << lowbit)) | ((uint32)half << lowbit));
	ArmFPSCRWrite(core, fpscr);
	return STD_E_OK;
}

/*
 * VCVT, VCVTR (floating-point to integer), VCVTA, VCVTN, VCVTP, VCVTM
 *     if dp_operation then S[d] = FPToFixed(D[m], 32, 0, unsigned, rounding);
 *     else S[d] = FPToFixed(S[m], 32, 0, unsigned, rounding);
 * (VCVT: FPRounding_ZERO; VCVTR: FPSCR's; the others their own.)
 */
static inline Std_ReturnType arm_pseudo_vcvt_to_int(TargetCoreType *core, const arm_fp_cvt_input_type *in, arm_fp_output_type *out)
{
	uint32 fpscr;
	uint32 N = in->dp_operation ? 64U : 32U;

	if (!ArmFPCvtBegin(core, in, out, &fpscr)) {
		return out->check;
	}
	out->result = FPToFixed(ArmFPRead(core, in->dp_operation, in->m), N, 32U, 0U, in->unsigned_int,
		ArmFPCvtRounding(in, fpscr), &fpscr);
	ArmSWrite(core, in->d, (uint32)out->result);
	ArmFPSCRWrite(core, fpscr);
	return STD_E_OK;
}

/*
 * VCVT (integer to floating-point)
 *     if dp_operation then D[d] = FixedToFP(S[m], 64, 0, unsigned, FALSE, TRUE);  // FPSCR's rounding
 *     else S[d] = FixedToFP(S[m], 32, 0, unsigned, FALSE, TRUE);
 */
static inline Std_ReturnType arm_pseudo_vcvt_from_int(TargetCoreType *core, const arm_fp_cvt_input_type *in, arm_fp_output_type *out)
{
	uint32 fpscr;

	if (!ArmFPCvtBegin(core, in, out, &fpscr)) {
		return out->check;
	}
	out->result = FixedToFP(ArmS(core, in->m), 32U, in->dp_operation ? 64U : 32U, 0U, in->unsigned_int, FALSE, &fpscr);
	ArmFPWrite(core, in->dp_operation, in->d, out->result, out);
	ArmFPSCRWrite(core, fpscr);
	return STD_E_OK;
}

/*
 * VCVT (between floating-point and fixed-point), in place:
 *     if to_fixed then
 *         if dp_operation then
 *             result = FPToFixed(D[d], size, frac_bits, unsigned, FPRounding_ZERO);
 *             D[d] = if unsigned then ZeroExtend(result, 64) else SignExtend(result, 64);
 *         else
 *             result = FPToFixed(S[d], size, frac_bits, unsigned, FPRounding_ZERO);
 *             S[d] = if unsigned then ZeroExtend(result, 32) else SignExtend(result, 32);
 *     else
 *         if dp_operation then D[d] = FixedToFP(D[d]<size-1:0>, 64, frac_bits, unsigned, TRUE, TRUE);
 *         else S[d] = FixedToFP(S[d]<size-1:0>, 32, frac_bits, unsigned, TRUE, TRUE);
 * (From fixed point: round to nearest, whatever FPSCR.RMode is.)
 */
static inline Std_ReturnType arm_pseudo_vcvt_to_fixed(TargetCoreType *core, const arm_fp_cvt_input_type *in, arm_fp_output_type *out)
{
	uint32 fpscr;
	uint32 N = in->dp_operation ? 64U : 32U;
	uint32 result;
	uint64 extended;

	if (!ArmFPCvtBegin(core, in, out, &fpscr)) {
		return out->check;
	}
	result = FPToFixed(ArmFPRead(core, in->dp_operation, in->d), N, in->size, in->fraction_bits, in->unsigned_int,
		FPRounding_ZERO, &fpscr);
	if (in->unsigned_int) {
		extended = result;
	}
	else {
		extended = (uint64)(sint64)(sint32)SignExtend(result, in->size);
	}
	ArmFPWrite(core, in->dp_operation, in->d, (N == 64U) ? extended : (extended & 0xFFFFFFFFU), out);
	ArmFPSCRWrite(core, fpscr);
	return STD_E_OK;
}

static inline Std_ReturnType arm_pseudo_vcvt_from_fixed(TargetCoreType *core, const arm_fp_cvt_input_type *in, arm_fp_output_type *out)
{
	uint32 fpscr;
	uint32 N = in->dp_operation ? 64U : 32U;

	if (!ArmFPCvtBegin(core, in, out, &fpscr)) {
		return out->check;
	}
	out->result = FixedToFP((uint32)ArmFPRead(core, in->dp_operation, in->d), in->size, N, in->fraction_bits, in->unsigned_int,
		TRUE, &fpscr);
	ArmFPWrite(core, in->dp_operation, in->d, out->result, out);
	ArmFPSCRWrite(core, fpscr);
	return STD_E_OK;
}

/*
 * VRINTA, VRINTN, VRINTP, VRINTM, VRINTZ, VRINTR, VRINTX
 *     if dp_operation then D[d] = FPRoundInt(D[m], rounding, exact);
 *     else S[d] = FPRoundInt(S[m], rounding, exact);
 * (VRINTR, VRINTX: FPSCR's rounding; VRINTX: exact.)
 */
static inline Std_ReturnType arm_pseudo_vrint(TargetCoreType *core, const arm_fp_cvt_input_type *in, arm_fp_output_type *out)
{
	uint32 fpscr;
	uint32 N = in->dp_operation ? 64U : 32U;

	if (!ArmFPCvtBegin(core, in, out, &fpscr)) {
		return out->check;
	}
	out->result = FPRoundInt(ArmFPRead(core, in->dp_operation, in->m), N, ArmFPCvtRounding(in, fpscr), in->exact, &fpscr);
	ArmFPWrite(core, in->dp_operation, in->d, out->result, out);
	ArmFPSCRWrite(core, fpscr);
	return STD_E_OK;
}

/*
 * VMAXNM, VMINNM (not conditional)
 *     if dp_operation then D[d] = FPMaxNum(D[n], D[m], TRUE);  // VMINNM: FPMinNum
 *     else S[d] = FPMaxNum(S[n], S[m], TRUE);
 */
static inline Std_ReturnType ArmFPMaxMinInstr(TargetCoreType *core, const arm_fp_input_type *in, bool max, arm_fp_output_type *out)
{
	uint32 N = in->dp_operation ? 64U : 32U;
	uint32 fpscr;
	uint64 op1;
	uint64 op2;

	out->result = 0U;
	if (!ArmFPBegin(core, in->cond, out)) {
		return out->check;
	}
	fpscr = ArmFPSCR(core);
	op1 = ArmFPRead(core, in->dp_operation, in->n);
	op2 = ArmFPRead(core, in->dp_operation, in->m);
	ArmFPWrite(core, in->dp_operation, in->d, max ? FPMaxNum(op1, op2, N, &fpscr) : FPMinNum(op1, op2, N, &fpscr), out);
	ArmFPSCRWrite(core, fpscr);
	return STD_E_OK;
}

static inline Std_ReturnType arm_pseudo_vmaxnm(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPMaxMinInstr(core, in, TRUE, out);
}

static inline Std_ReturnType arm_pseudo_vminnm(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPMaxMinInstr(core, in, FALSE, out);
}

#endif /* ARM_COMMON_ARM_PSEUDO_FP_CONVERT_H */
