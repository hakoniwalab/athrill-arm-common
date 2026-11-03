/*
 * Pseudo-code of the floating-point instructions that move data without
 * arithmetic: VMOV (immediate, register, and between the core and the
 * floating-point registers), VABS, VNEG, VSEL, VMRS and VMSR.
 *
 * A floating-point register operand is a register number: S[n] when the
 * instruction is single precision, D[n] when it is double precision
 * (dp_operation). ExecuteFPCheck() (the coprocessor access check) is the
 * target's, before the pseudo-code is called.
 */
#ifndef ARM_COMMON_ARM_PSEUDO_FP_MOVE_H
#define ARM_COMMON_ARM_PSEUDO_FP_MOVE_H

#include "arm_pseudo_dp.h"

/* An instruction on floating-point registers only. */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	bool dp_operation;
	uint32 d;
	uint32 n;
	uint32 m;
	uint64 imm64;		/* VMOV (immediate): VFPExpandImm(imm8, 32 or 64) */
	uint32 sel_cond;	/* VSEL: the condition of the selection */
} arm_fp_input_type;

typedef struct {
	bool passed;
	uint64 result;
} arm_fp_output_type;

/* if ConditionPassed() then EncodingSpecificOperations(); ExecuteFPCheck(); */
static inline bool ArmFPBegin(const TargetCoreType *core, uint32 cond, bool *passed)
{
	*passed = ConditionHolds(cond, ArmAPSR(core));
	return *passed;
}

/* S[n] or D[n] by the precision. */
static inline uint64 ArmFPRead(const TargetCoreType *core, bool dp_operation, uint32 n)
{
	if (dp_operation) {
		return ArmD(core, n);
	}
	return ArmS(core, n);
}

static inline void ArmFPWrite(TargetCoreType *core, bool dp_operation, uint32 n, uint64 value, arm_fp_output_type *out)
{
	out->result = value;
	if (dp_operation) {
		ArmDWrite(core, n, value);
	}
	else {
		ArmSWrite(core, n, (uint32)value);
	}
}

/*
 * bits(N) VFPExpandImm(bits(8) imm8, integer N)
 *     assert N IN {32,64};
 *     if N == 32 then E = 8; else E = 11;
 *     F = N - E - 1;
 *     sign = imm8<7>;
 *     exp  = NOT(imm8<6>):Replicate(imm8<6>,E-3):imm8<5:4>;
 *     frac = imm8<3:0>:Zeros(F-4);
 *     return sign : exp : frac;
 */
static inline uint64 VFPExpandImm(uint32 imm8, uint32 N)
{
	uint32 E = (N == 32U) ? 8U : 11U;
	uint32 F = N - E - 1U;
	uint64 b6 = ARM_BIT(imm8, 6U);
	uint64 exp = ((b6 ^ 1U) << (E - 1U)) | (((b6 != 0U) ? ((1U << (E - 3U)) - 1U) : 0U) << 2) | ((imm8 >> 4) & 0x3U);
	uint64 frac = (uint64)(imm8 & 0xFU) << (F - 4U);

	return ((uint64)ARM_BIT(imm8, 7U) << (N - 1U)) | (exp << F) | frac;
}

/*
 * VMOV (immediate)
 *     if dp_operation then D[d] = imm64; else S[d] = imm32;
 */
static inline Std_ReturnType arm_pseudo_vmov_imm(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	out->result = 0U;
	if (!ArmFPBegin(core, in->cond, &out->passed)) {
		return STD_E_OK;
	}
	ArmFPWrite(core, in->dp_operation, in->d, in->imm64, out);
	return STD_E_OK;
}

/*
 * VMOV (register)
 *     if dp_operation then D[d] = D[m]; else S[d] = S[m];
 */
static inline Std_ReturnType arm_pseudo_vmov_reg(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	out->result = 0U;
	if (!ArmFPBegin(core, in->cond, &out->passed)) {
		return STD_E_OK;
	}
	ArmFPWrite(core, in->dp_operation, in->d, ArmFPRead(core, in->dp_operation, in->m), out);
	return STD_E_OK;
}

/* The sign bit of an N-bit value. */
static inline uint64 ArmFPSignBit(bool dp_operation)
{
	return dp_operation ? ((uint64)1U << 63) : ((uint64)1U << 31);
}

/*
 * VABS
 *     if dp_operation then D[d] = FPAbs(D[m]); else S[d] = FPAbs(S[m]);
 * FPAbs(operand) = '0' : operand<N-2:0>;
 */
static inline Std_ReturnType arm_pseudo_vabs(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	out->result = 0U;
	if (!ArmFPBegin(core, in->cond, &out->passed)) {
		return STD_E_OK;
	}
	ArmFPWrite(core, in->dp_operation, in->d, ArmFPRead(core, in->dp_operation, in->m) & ~ArmFPSignBit(in->dp_operation), out);
	return STD_E_OK;
}

/*
 * VNEG
 *     if dp_operation then D[d] = FPNeg(D[m]); else S[d] = FPNeg(S[m]);
 * FPNeg(operand) = NOT(operand<N-1>) : operand<N-2:0>;
 */
static inline Std_ReturnType arm_pseudo_vneg(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	out->result = 0U;
	if (!ArmFPBegin(core, in->cond, &out->passed)) {
		return STD_E_OK;
	}
	ArmFPWrite(core, in->dp_operation, in->d, ArmFPRead(core, in->dp_operation, in->m) ^ ArmFPSignBit(in->dp_operation), out);
	return STD_E_OK;
}

/*
 * VSEL (not conditional: cond is AL; sel_cond is EQ, VS, GE or GT)
 *     EncodingSpecificOperations(); ExecuteFPCheck();
 *     if dp_operation then
 *         D[d] = if ConditionHolds(cond) then D[n] else D[m];
 *     else
 *         S[d] = if ConditionHolds(cond) then S[n] else S[m];
 */
static inline Std_ReturnType arm_pseudo_vsel(TargetCoreType *core, const arm_fp_input_type *in, arm_fp_output_type *out)
{
	uint32 source;

	out->result = 0U;
	if (!ArmFPBegin(core, in->cond, &out->passed)) {
		return STD_E_OK;
	}
	source = ConditionHolds(in->sel_cond, ArmAPSR(core)) ? in->n : in->m;
	ArmFPWrite(core, in->dp_operation, in->d, ArmFPRead(core, in->dp_operation, source), out);
	return STD_E_OK;
}

/*
 * VMOV between the core and the floating-point registers
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	bool to_arm_registers;
	PseudoCodeRegisterDataType Rt;
	PseudoCodeRegisterDataType Rt2;	/* the forms with two core registers */
	uint32 fp;			/* S[n], S[m] (and S[m+1]) or D[m], D[d] */
	uint32 index;			/* the scalar forms: the word of D[fp] */
} arm_fp_core_input_type;

/* if ConditionPassed() then EncodingSpecificOperations(); ExecuteFPCheck(); */
static inline bool ArmFPCoreBegin(const TargetCoreType *core, const arm_fp_core_input_type *in, arm_fp_output_type *out)
{
	out->result = 0U;
	return ArmFPBegin(core, in->cond, &out->passed);
}

/*
 * VMOV (between ARM core register and single-precision register)
 *     if to_arm_registers then R[t] = S[n]; else S[n] = R[t];
 */
static inline Std_ReturnType arm_pseudo_vmov_core_s(TargetCoreType *core, const arm_fp_core_input_type *in, arm_fp_output_type *out)
{
	if (!ArmFPCoreBegin(core, in, out)) {
		return STD_E_OK;
	}
	if (in->to_arm_registers) {
		ArmRWrite(core, in->Rt.regId, ArmS(core, in->fp));
	}
	else {
		ArmSWrite(core, in->fp, in->Rt.regData);
	}
	return STD_E_OK;
}

/*
 * VMOV (between two ARM core registers and two single-precision registers)
 *     if to_arm_registers then
 *         R[t] = S[m]; R[t2] = S[m+1];
 *     else
 *         S[m] = R[t]; S[m+1] = R[t2];
 */
static inline Std_ReturnType arm_pseudo_vmov_core_ss(TargetCoreType *core, const arm_fp_core_input_type *in, arm_fp_output_type *out)
{
	if (!ArmFPCoreBegin(core, in, out)) {
		return STD_E_OK;
	}
	if (in->to_arm_registers) {
		ArmRWrite(core, in->Rt.regId, ArmS(core, in->fp));
		ArmRWrite(core, in->Rt2.regId, ArmS(core, in->fp + 1U));
	}
	else {
		ArmSWrite(core, in->fp, in->Rt.regData);
		ArmSWrite(core, in->fp + 1U, in->Rt2.regData);
	}
	return STD_E_OK;
}

/*
 * VMOV (between two ARM core registers and a doubleword register)
 *     if to_arm_registers then
 *         R[t] = D[m]<31:0>; R[t2] = D[m]<63:32>;
 *     else
 *         D[m]<31:0> = R[t]; D[m]<63:32> = R[t2];
 */
static inline Std_ReturnType arm_pseudo_vmov_core_d(TargetCoreType *core, const arm_fp_core_input_type *in, arm_fp_output_type *out)
{
	uint64 value;

	if (!ArmFPCoreBegin(core, in, out)) {
		return STD_E_OK;
	}
	if (in->to_arm_registers) {
		value = ArmD(core, in->fp);
		ArmRWrite(core, in->Rt.regId, (uint32)value);
		ArmRWrite(core, in->Rt2.regId, (uint32)(value >> 32));
	}
	else {
		ArmDWrite(core, in->fp, ((uint64)in->Rt2.regData << 32) | in->Rt.regData);
	}
	return STD_E_OK;
}

/*
 * VMOV (ARM core register to scalar), VMOV (scalar to ARM core register),
 * 32-bit scalars:
 *     if to_arm_registers then R[t] = D[n]<index*32+31:index*32>;
 *     else D[d]<index*32+31:index*32> = R[t];
 */
static inline Std_ReturnType arm_pseudo_vmov_core_scalar(TargetCoreType *core, const arm_fp_core_input_type *in, arm_fp_output_type *out)
{
	uint32 shift = 32U * (in->index & 1U);
	uint64 value;

	if (!ArmFPCoreBegin(core, in, out)) {
		return STD_E_OK;
	}
	value = ArmD(core, in->fp);
	if (in->to_arm_registers) {
		ArmRWrite(core, in->Rt.regId, (uint32)(value >> shift));
	}
	else {
		value = (value & ~((uint64)0xFFFFFFFFU << shift)) | ((uint64)in->Rt.regData << shift);
		ArmDWrite(core, in->fp, value);
	}
	return STD_E_OK;
}

/*
 * VMRS (to_arm_registers), VMSR
 *     VMRS: if t != 15 then R[t] = FPSCR;
 *           else APSR.N = FPSCR.N; APSR.Z = FPSCR.Z; APSR.C = FPSCR.C; APSR.V = FPSCR.V;
 *     VMSR: FPSCR = R[t];
 */
static inline Std_ReturnType arm_pseudo_vmrs_vmsr(TargetCoreType *core, const arm_fp_core_input_type *in, arm_fp_output_type *out)
{
	uint32 nzcv = ARM_APSR_N | ARM_APSR_Z | ARM_APSR_C | ARM_APSR_V;

	if (!ArmFPCoreBegin(core, in, out)) {
		return STD_E_OK;
	}
	if (!in->to_arm_registers) {
		ArmFPSCRWrite(core, in->Rt.regData);
	}
	else if (in->Rt.regId != ARM_REG_PC) {
		ArmRWrite(core, in->Rt.regId, ArmFPSCR(core));
	}
	else {
		ArmAPSRWrite(core, (ArmAPSR(core) & ~nzcv) | (ArmFPSCR(core) & nzcv));
	}
	return STD_E_OK;
}

#endif /* ARM_COMMON_ARM_PSEUDO_FP_MOVE_H */
