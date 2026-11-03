/*
 * Pseudo-code of the floating-point arithmetic instructions: VADD, VSUB,
 * VMUL, VNMUL, VMLA, VMLS, VNMLA, VNMLS, VDIV, VSQRT, VFMA, VFMS, VFNMA,
 * VFNMS, VCMP, VCMPE.
 *
 * Each reads FPSCR, computes with arm_fp.h (which sets the cumulative
 * exception bits) and writes FPSCR back.
 */
#ifndef ARM_COMMON_ARM_PSEUDO_FP_ARITH_H
#define ARM_COMMON_ARM_PSEUDO_FP_ARITH_H

#include "arm_pseudo_fp_move.h"
#include "arm_fp.h"

typedef enum {
	ArmFPOp_VADD = 0,
	ArmFPOp_VSUB,
	ArmFPOp_VMUL,
	ArmFPOp_VNMUL,
	ArmFPOp_VMLA,
	ArmFPOp_VMLS,
	ArmFPOp_VNMLA,
	ArmFPOp_VNMLS,
	ArmFPOp_VDIV,
	ArmFPOp_VFMA,
	ArmFPOp_VFMS,
	ArmFPOp_VFNMA,
	ArmFPOp_VFNMS,
} ArmFPOp;

/*
 * The operations, on N-bit values d (the destination's old value), n, m:
 *     VADD   FPAdd(n, m)
 *     VSUB   FPSub(n, m)
 *     VMUL   FPMul(n, m)
 *     VNMUL  FPNeg(FPMul(n, m))
 *     VMLA   FPAdd(d, FPMul(n, m))
 *     VMLS   FPAdd(d, FPNeg(FPMul(n, m)))
 *     VNMLA  FPAdd(FPNeg(d), FPNeg(FPMul(n, m)))
 *     VNMLS  FPAdd(FPNeg(d), FPMul(n, m))
 *     VDIV   FPDiv(n, m)
 *     VFMA   FPMulAdd(d, n, m)
 *     VFMS   FPMulAdd(d, FPNeg(n), m)
 *     VFNMA  FPMulAdd(FPNeg(d), FPNeg(n), m)
 *     VFNMS  FPMulAdd(FPNeg(d), n, m)
 */
static inline uint64 ArmFPOperation(ArmFPOp op, uint64 d, uint64 n, uint64 m, uint32 N, uint32 *fpscr)
{
	switch (op) {
	case ArmFPOp_VADD:
		return FPAdd(n, m, N, fpscr);
	case ArmFPOp_VSUB:
		return FPSub(n, m, N, fpscr);
	case ArmFPOp_VMUL:
		return FPMul(n, m, N, fpscr);
	case ArmFPOp_VNMUL:
		return FPNeg(FPMul(n, m, N, fpscr), N);
	case ArmFPOp_VMLA:
		return FPAdd(d, FPMul(n, m, N, fpscr), N, fpscr);
	case ArmFPOp_VMLS:
		return FPAdd(d, FPNeg(FPMul(n, m, N, fpscr), N), N, fpscr);
	case ArmFPOp_VNMLA:
		return FPAdd(FPNeg(d, N), FPNeg(FPMul(n, m, N, fpscr), N), N, fpscr);
	case ArmFPOp_VNMLS:
		return FPAdd(FPNeg(d, N), FPMul(n, m, N, fpscr), N, fpscr);
	case ArmFPOp_VDIV:
		return FPDiv(n, m, N, fpscr);
	case ArmFPOp_VFMA:
		return FPMulAdd(d, n, m, N, fpscr);
	case ArmFPOp_VFMS:
		return FPMulAdd(d, FPNeg(n, N), m, N, fpscr);
	case ArmFPOp_VFNMA:
		return FPMulAdd(FPNeg(d, N), FPNeg(n, N), m, N, fpscr);
	default:
		return FPMulAdd(FPNeg(d, N), n, m, N, fpscr);
	}
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations(); ExecuteFPCheck();
 *     if dp_operation then D[d] = <operation>(D[d], D[n], D[m]);
 *     else S[d] = <operation>(S[d], S[n], S[m]);
 */
static inline Std_ReturnType ArmFPArith(TargetCoreType *core, const arm_fp_input_type *in, ArmFPOp op, arm_fp_output_type *out)
{
	uint32 N = in->dp_operation ? 64U : 32U;
	uint32 fpscr;
	uint64 result;

	out->result = 0U;
	if (!ArmFPBegin(core, in->cond, out)) {
		return out->check;
	}
	fpscr = ArmFPSCR(core);
	result = ArmFPOperation(op, ArmFPRead(core, in->dp_operation, in->d), ArmFPRead(core, in->dp_operation, in->n),
		ArmFPRead(core, in->dp_operation, in->m), N, &fpscr);
	ArmFPSCRWrite(core, fpscr);
	ArmFPWrite(core, in->dp_operation, in->d, result, out);
	return STD_E_OK;
}

static inline Std_ReturnType arm_pseudo_vadd(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPArith(core, in, ArmFPOp_VADD, out);
}

static inline Std_ReturnType arm_pseudo_vsub(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPArith(core, in, ArmFPOp_VSUB, out);
}

static inline Std_ReturnType arm_pseudo_vmul(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPArith(core, in, ArmFPOp_VMUL, out);
}

static inline Std_ReturnType arm_pseudo_vnmul(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPArith(core, in, ArmFPOp_VNMUL, out);
}

static inline Std_ReturnType arm_pseudo_vmla(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPArith(core, in, ArmFPOp_VMLA, out);
}

static inline Std_ReturnType arm_pseudo_vmls(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPArith(core, in, ArmFPOp_VMLS, out);
}

static inline Std_ReturnType arm_pseudo_vnmla(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPArith(core, in, ArmFPOp_VNMLA, out);
}

static inline Std_ReturnType arm_pseudo_vnmls(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPArith(core, in, ArmFPOp_VNMLS, out);
}

static inline Std_ReturnType arm_pseudo_vdiv(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPArith(core, in, ArmFPOp_VDIV, out);
}

static inline Std_ReturnType arm_pseudo_vfma(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPArith(core, in, ArmFPOp_VFMA, out);
}

static inline Std_ReturnType arm_pseudo_vfms(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPArith(core, in, ArmFPOp_VFMS, out);
}

static inline Std_ReturnType arm_pseudo_vfnma(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPArith(core, in, ArmFPOp_VFNMA, out);
}

static inline Std_ReturnType arm_pseudo_vfnms(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPArith(core, in, ArmFPOp_VFNMS, out);
}

/*
 * VSQRT
 *     if dp_operation then D[d] = FPSqrt(D[m]); else S[d] = FPSqrt(S[m]);
 */
static inline Std_ReturnType arm_pseudo_vsqrt(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	uint32 N = in->dp_operation ? 64U : 32U;
	uint32 fpscr;
	uint64 result;

	out->result = 0U;
	if (!ArmFPBegin(core, in->cond, out)) {
		return out->check;
	}
	fpscr = ArmFPSCR(core);
	result = FPSqrt(ArmFPRead(core, in->dp_operation, in->m), N, &fpscr);
	ArmFPSCRWrite(core, fpscr);
	ArmFPWrite(core, in->dp_operation, in->d, result, out);
	return STD_E_OK;
}

/*
 * VCMP, VCMPE (quiet_nan_exc), with a register or with zero (with_zero)
 *     if dp_operation then
 *         op64 = if with_zero then FPZero('0',64) else D[m];
 *         FPSCR<31:28> = FPCompare(D[d], op64, quiet_nan_exc, TRUE);
 *     else
 *         op32 = if with_zero then FPZero('0',32) else S[m];
 *         FPSCR<31:28> = FPCompare(S[d], op32, quiet_nan_exc, TRUE);
 */
static inline Std_ReturnType ArmFPCompareInstr(TargetCoreType *core, const arm_fp_input_type *in, bool quiet_nan_exc, bool with_zero,
	arm_fp_output_type *out)
{
	uint32 N = in->dp_operation ? 64U : 32U;
	uint32 fpscr;
	uint32 nzcv;
	uint64 op2;

	out->result = 0U;
	if (!ArmFPBegin(core, in->cond, out)) {
		return out->check;
	}
	fpscr = ArmFPSCR(core);
	op2 = with_zero ? FPZero(FALSE, N) : ArmFPRead(core, in->dp_operation, in->m);
	nzcv = FPCompare(ArmFPRead(core, in->dp_operation, in->d), op2, quiet_nan_exc, N, &fpscr);
	out->result = nzcv;
	ArmFPSCRWrite(core, (fpscr & 0x0FFFFFFFU) | (nzcv << 28));
	return STD_E_OK;
}

static inline Std_ReturnType arm_pseudo_vcmp(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPCompareInstr(core, in, FALSE, FALSE, out);
}

static inline Std_ReturnType arm_pseudo_vcmpe(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPCompareInstr(core, in, TRUE, FALSE, out);
}

static inline Std_ReturnType arm_pseudo_vcmp_zero(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPCompareInstr(core, in, FALSE, TRUE, out);
}

static inline Std_ReturnType arm_pseudo_vcmpe_zero(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	return ArmFPCompareInstr(core, in, TRUE, TRUE, out);
}

#endif /* ARM_COMMON_ARM_PSEUDO_FP_ARITH_H */
