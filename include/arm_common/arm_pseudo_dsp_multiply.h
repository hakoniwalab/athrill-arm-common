/*
 * Pseudo-code of the multiply instructions of the DSP extension: on
 * halfwords (SMUL<x><y>, SMLA<x><y>, SMULW<y>, SMLAW<y>, SMLAL<x><y>), dual
 * (SMUAD, SMLAD, SMUSD, SMLSD, SMLALD, SMLSLD, with X), most significant
 * word (SMMUL, SMMLA, SMMLS, with R), USAD8, USADA8 and UMAAL.
 *
 * An accumulation that overflows 32 bits sets APSR.Q (SMLA<x><y>,
 * SMLAW<y>, SMLAD, SMUAD, SMLSD); the 64-bit ones wrap.
 */
#ifndef ARM_COMMON_ARM_PSEUDO_DSP_MULTIPLY_H
#define ARM_COMMON_ARM_PSEUDO_DSP_MULTIPLY_H

#include "arm_pseudo_saturate.h"
#include "arm_pseudo_multiply.h"

/* if n_high then x<31:16> else x<15:0>, as SInt. */
static inline sint64 ArmHalfword(uint32 x, bool high)
{
	return ArmLane(x, high ? 1U : 0U, 16U, TRUE);
}

/* R[d] = result<31:0>; if result != SInt(result<31:0>) then APSR.Q = '1'; */
static inline Std_ReturnType ArmResultWriteQ(TargetCoreType *core, uint32 d, sint64 result, arm_dp_output_type *out)
{
	if (result != (sint64)(sint32)(uint32)result) {
		ArmSetQ(core);
	}
	return ArmResultWrite(core, d, (uint32)result, out);
}

/*
 * SMUL<x><y>
 *     operand1 = if n_high then R[n]<31:16> else R[n]<15:0>;
 *     operand2 = if m_high then R[m]<31:16> else R[m]<15:0>;
 *     result = SInt(operand1) * SInt(operand2);
 *     R[d] = result<31:0>;
 *     // Signed overflow cannot occur
 */
static inline Std_ReturnType arm_pseudo_smulxy(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	sint64 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = ArmHalfword(in->Rn.regData, in->n_high) * ArmHalfword(in->Rm.regData, in->m_high);
	return ArmResultWrite(core, in->Rd.regId, (uint32)result, out);
}

/*
 * SMLA<x><y>
 *     operand1 = if n_high then R[n]<31:16> else R[n]<15:0>;
 *     operand2 = if m_high then R[m]<31:16> else R[m]<15:0>;
 *     result = SInt(operand1) * SInt(operand2) + SInt(R[a]);
 *     R[d] = result<31:0>;
 *     if result != SInt(result<31:0>) then  // Signed overflow
 *         APSR.Q = '1';
 */
static inline Std_ReturnType arm_pseudo_smlaxy(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	sint64 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = ArmHalfword(in->Rn.regData, in->n_high) * ArmHalfword(in->Rm.regData, in->m_high)
		+ (sint64)(sint32)in->Ra.regData;
	return ArmResultWriteQ(core, in->Rd.regId, result, out);
}

/*
 * SMULW<y>
 *     operand2 = if m_high then R[m]<31:16> else R[m]<15:0>;
 *     product = SInt(R[n]) * SInt(operand2);
 *     R[d] = product<47:16>;
 *     // Signed overflow cannot occur
 */
static inline Std_ReturnType arm_pseudo_smulwy(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	sint64 product;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	product = (sint64)(sint32)in->Rn.regData * ArmHalfword(in->Rm.regData, in->m_high);
	return ArmResultWrite(core, in->Rd.regId, (uint32)(product >> 16), out);
}

/*
 * SMLAW<y>
 *     operand2 = if m_high then R[m]<31:16> else R[m]<15:0>;
 *     result = SInt(R[n]) * SInt(operand2) + (SInt(R[a]) << 16);
 *     R[d] = result<47:16>;
 *     if (result >> 16) != SInt(R[d]) then  // Signed overflow
 *         APSR.Q = '1';
 */
static inline Std_ReturnType arm_pseudo_smlawy(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	sint64 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = (sint64)(sint32)in->Rn.regData * ArmHalfword(in->Rm.regData, in->m_high)
		+ (sint64)(sint32)in->Ra.regData * 65536;
	return ArmResultWriteQ(core, in->Rd.regId, result >> 16, out);
}

/*
 * The products of the dual multiplies:
 *     operand2 = if m_swap then ROR(R[m],16) else R[m];
 *     product1 = SInt(R[n]<15:0>) * SInt(operand2<15:0>);
 *     product2 = SInt(R[n]<31:16>) * SInt(operand2<31:16>);
 */
static inline void ArmDualProducts(const arm_dsp_input_type *in, sint64 *product1, sint64 *product2)
{
	uint32 operand2 = in->m_swap ? ROR(in->Rm.regData, 16U) : in->Rm.regData;

	*product1 = ArmHalfword(in->Rn.regData, FALSE) * ArmHalfword(operand2, FALSE);
	*product2 = ArmHalfword(in->Rn.regData, TRUE) * ArmHalfword(operand2, TRUE);
}

/*
 * SMUAD
 *     result = product1 + product2;
 *     R[d] = result<31:0>;
 *     if result != SInt(result<31:0>) then  // Signed overflow
 *         APSR.Q = '1';
 */
static inline Std_ReturnType arm_pseudo_smuad(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	sint64 product1;
	sint64 product2;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	ArmDualProducts(in, &product1, &product2);
	return ArmResultWriteQ(core, in->Rd.regId, product1 + product2, out);
}

/*
 * SMLAD
 *     result = product1 + product2 + SInt(R[a]);
 *     R[d] = result<31:0>;
 *     if result != SInt(result<31:0>) then  // Signed overflow
 *         APSR.Q = '1';
 */
static inline Std_ReturnType arm_pseudo_smlad(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	sint64 product1;
	sint64 product2;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	ArmDualProducts(in, &product1, &product2);
	return ArmResultWriteQ(core, in->Rd.regId, product1 + product2 + (sint64)(sint32)in->Ra.regData, out);
}

/*
 * SMUSD
 *     result = product1 - product2;
 *     R[d] = result<31:0>;
 *     // Signed overflow cannot occur
 */
static inline Std_ReturnType arm_pseudo_smusd(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	sint64 product1;
	sint64 product2;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	ArmDualProducts(in, &product1, &product2);
	return ArmResultWrite(core, in->Rd.regId, (uint32)(product1 - product2), out);
}

/*
 * SMLSD
 *     result = product1 - product2 + SInt(R[a]);
 *     R[d] = result<31:0>;
 *     if result != SInt(result<31:0>) then  // Signed overflow
 *         APSR.Q = '1';
 */
static inline Std_ReturnType arm_pseudo_smlsd(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	sint64 product1;
	sint64 product2;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	ArmDualProducts(in, &product1, &product2);
	return ArmResultWriteQ(core, in->Rd.regId, product1 - product2 + (sint64)(sint32)in->Ra.regData, out);
}

/*
 * The most significant word multiplies, modulo 2^64 (only result<63:32> is
 * kept):
 *     result = SInt(R[n]) * SInt(R[m]);                     // SMMUL
 *     result = (SInt(R[a]) << 32) + SInt(R[n]) * SInt(R[m]);  // SMMLA
 *     result = (SInt(R[a]) << 32) - SInt(R[n]) * SInt(R[m]);  // SMMLS
 *     if round then result = result + 0x80000000;
 *     R[d] = result<63:32>;
 */
static inline Std_ReturnType ArmMultiplyMostSignificant(TargetCoreType *core, const arm_dsp_input_type *in, uint64 accumulate,
	bool subtract, arm_dp_output_type *out)
{
	uint64 product;
	uint64 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	product = (uint64)((sint64)(sint32)in->Rn.regData * (sint64)(sint32)in->Rm.regData);
	result = subtract ? (accumulate - product) : (accumulate + product);
	if (in->round) {
		result += 0x80000000U;
	}
	return ArmResultWrite(core, in->Rd.regId, (uint32)(result >> 32), out);
}

static inline Std_ReturnType arm_pseudo_smmul(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmMultiplyMostSignificant(core, in, 0U, FALSE, out);
}

static inline Std_ReturnType arm_pseudo_smmla(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmMultiplyMostSignificant(core, in, (uint64)in->Ra.regData << 32, FALSE, out);
}

static inline Std_ReturnType arm_pseudo_smmls(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmMultiplyMostSignificant(core, in, (uint64)in->Ra.regData << 32, TRUE, out);
}

/*
 * USAD8, USADA8 (USAD8 leaves Ra unset: its value is 0)
 *     absdiff1 = Abs(UInt(R[n]<7:0>)   - UInt(R[m]<7:0>));
 *     absdiff2 = Abs(UInt(R[n]<15:8>)  - UInt(R[m]<15:8>));
 *     absdiff3 = Abs(UInt(R[n]<23:16>) - UInt(R[m]<23:16>));
 *     absdiff4 = Abs(UInt(R[n]<31:24>) - UInt(R[m]<31:24>));
 *     result = UInt(R[a]) + absdiff1 + absdiff2 + absdiff3 + absdiff4;  // USAD8: no UInt(R[a])
 *     R[d] = result<31:0>;
 */
static inline Std_ReturnType arm_pseudo_usada8(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	uint32 result;
	uint32 i;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = in->Ra.regData;
	for (i = 0U; i < 4U; i++) {
		sint64 diff = ArmLane(in->Rn.regData, i, 8U, FALSE) - ArmLane(in->Rm.regData, i, 8U, FALSE);

		result += (uint32)((diff < 0) ? -diff : diff);
	}
	return ArmResultWrite(core, in->Rd.regId, result, out);
}

static inline Std_ReturnType arm_pseudo_usad8(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return arm_pseudo_usada8(core, in, out);
}

/*
 * SMLAL<x><y>, SMLALD, SMLSLD, UMAAL
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType RdLo;
	PseudoCodeRegisterDataType RdHi;
	PseudoCodeRegisterDataType Rn;
	PseudoCodeRegisterDataType Rm;
	bool n_high;
	bool m_high;
	bool m_swap;
} arm_dsp_long_input_type;

/* R[dHi] = result<63:32>; R[dLo] = result<31:0>; (dHi == dLo or PC: UNPREDICTABLE) */
static inline Std_ReturnType ArmDspLongWrite(TargetCoreType *core, const arm_dsp_long_input_type *in, uint64 result,
	arm_mul_long_output_type *out)
{
	out->result = result;
	if ((in->RdHi.regId == ARM_REG_PC) || (in->RdLo.regId == ARM_REG_PC) || (in->RdHi.regId == in->RdLo.regId)) {
		return STD_E_INVALID;
	}
	ArmRWrite(core, in->RdHi.regId, (uint32)(result >> 32));
	ArmRWrite(core, in->RdLo.regId, (uint32)result);
	return STD_E_OK;
}

/* if ConditionPassed() then EncodingSpecificOperations(); and the accumulator R[dHi]:R[dLo]. */
static inline bool ArmDspLongBegin(const TargetCoreType *core, const arm_dsp_long_input_type *in, arm_mul_long_output_type *out,
	uint64 *accumulator)
{
	out->result = 0U;
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	*accumulator = ((uint64)in->RdHi.regData << 32) | (uint64)in->RdLo.regData;
	return out->passed;
}

/*
 * SMLAL<x><y>
 *     operand1 = if n_high then R[n]<31:16> else R[n]<15:0>;
 *     operand2 = if m_high then R[m]<31:16> else R[m]<15:0>;
 *     result = SInt(operand1) * SInt(operand2) + SInt(R[dHi]:R[dLo]);
 *     R[dHi] = result<63:32>;
 *     R[dLo] = result<31:0>;
 */
static inline Std_ReturnType arm_pseudo_smlalxy(TargetCoreType *core, const arm_dsp_long_input_type *in, arm_mul_long_output_type *out)
{
	uint64 accumulator;
	sint64 product;

	if (!ArmDspLongBegin(core, in, out, &accumulator)) {
		return STD_E_OK;
	}
	product = ArmHalfword(in->Rn.regData, in->n_high) * ArmHalfword(in->Rm.regData, in->m_high);
	return ArmDspLongWrite(core, in, accumulator + (uint64)product, out);
}

/* The dual products of SMLALD and SMLSLD (m_swap: X). */
static inline void ArmDualProductsLong(const arm_dsp_long_input_type *in, sint64 *product1, sint64 *product2)
{
	uint32 operand2 = in->m_swap ? ROR(in->Rm.regData, 16U) : in->Rm.regData;

	*product1 = ArmHalfword(in->Rn.regData, FALSE) * ArmHalfword(operand2, FALSE);
	*product2 = ArmHalfword(in->Rn.regData, TRUE) * ArmHalfword(operand2, TRUE);
}

/*
 * SMLALD
 *     operand2 = if m_swap then ROR(R[m],16) else R[m];
 *     product1 = SInt(R[n]<15:0>) * SInt(operand2<15:0>);
 *     product2 = SInt(R[n]<31:16>) * SInt(operand2<31:16>);
 *     result = product1 + product2 + SInt(R[dHi]:R[dLo]);
 *     R[dHi] = result<63:32>;
 *     R[dLo] = result<31:0>;
 */
static inline Std_ReturnType arm_pseudo_smlald(TargetCoreType *core, const arm_dsp_long_input_type *in, arm_mul_long_output_type *out)
{
	uint64 accumulator;
	sint64 product1;
	sint64 product2;

	if (!ArmDspLongBegin(core, in, out, &accumulator)) {
		return STD_E_OK;
	}
	ArmDualProductsLong(in, &product1, &product2);
	return ArmDspLongWrite(core, in, accumulator + (uint64)(product1 + product2), out);
}

/*
 * SMLSLD
 *     (operand2, product1, product2 as SMLALD)
 *     result = product1 - product2 + SInt(R[dHi]:R[dLo]);
 *     R[dHi] = result<63:32>;
 *     R[dLo] = result<31:0>;
 */
static inline Std_ReturnType arm_pseudo_smlsld(TargetCoreType *core, const arm_dsp_long_input_type *in, arm_mul_long_output_type *out)
{
	uint64 accumulator;
	sint64 product1;
	sint64 product2;

	if (!ArmDspLongBegin(core, in, out, &accumulator)) {
		return STD_E_OK;
	}
	ArmDualProductsLong(in, &product1, &product2);
	return ArmDspLongWrite(core, in, accumulator + (uint64)(product1 - product2), out);
}

/*
 * UMAAL
 *     result = UInt(R[n]) * UInt(R[m]) + UInt(R[dHi]) + UInt(R[dLo]);
 *     R[dHi] = result<63:32>;
 *     R[dLo] = result<31:0>;
 */
static inline Std_ReturnType arm_pseudo_umaal(TargetCoreType *core, const arm_dsp_long_input_type *in, arm_mul_long_output_type *out)
{
	uint64 accumulator;
	uint64 result;

	if (!ArmDspLongBegin(core, in, out, &accumulator)) {
		return STD_E_OK;
	}
	result = (uint64)in->Rn.regData * (uint64)in->Rm.regData + (uint64)in->RdHi.regData + (uint64)in->RdLo.regData;
	return ArmDspLongWrite(core, in, result, out);
}

#endif /* ARM_COMMON_ARM_PSEUDO_DSP_MULTIPLY_H */
