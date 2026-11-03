/*
 * Pseudo-code of the parallel addition and subtraction instructions (DSP
 * extension) and of SEL.
 *
 * Each of the six operations works on halfwords or bytes of R[n] and R[m]:
 *     ADD16   R[n]<15:0> + R[m]<15:0>,   R[n]<31:16> + R[m]<31:16>
 *     ASX     R[n]<15:0> - R[m]<31:16>,  R[n]<31:16> + R[m]<15:0>
 *     SAX     R[n]<15:0> + R[m]<31:16>,  R[n]<31:16> - R[m]<15:0>
 *     SUB16   R[n]<15:0> - R[m]<15:0>,   R[n]<31:16> - R[m]<31:16>
 *     ADD8    R[n]<8i+7:8i> + R[m]<8i+7:8i>, i = 0..3
 *     SUB8    R[n]<8i+7:8i> - R[m]<8i+7:8i>, i = 0..3
 * and the prefix says how the operands are read and the results written:
 *     S   SInt operands; result<w-1:0>; APSR.GE bits of a lane = (result >= 0)
 *     U   UInt operands; result<w-1:0>; APSR.GE bits of a lane = (result >= 2^w)
 *         for an addition, (result >= 0) for a subtraction
 *     Q   SInt operands; SignedSat(result, w)          (GE unchanged)
 *     UQ  UInt operands; UnsignedSat(result, w)        (GE unchanged)
 *     SH  SInt operands; result<w:1>                   (GE unchanged)
 *     UH  UInt operands; result<w:1>                   (GE unchanged)
 * (w = 16 or 8.) For example SADD16:
 *     if ConditionPassed() then
 *         EncodingSpecificOperations();
 *         sum1 = SInt(R[n]<15:0>) + SInt(R[m]<15:0>);
 *         sum2 = SInt(R[n]<31:16>) + SInt(R[m]<31:16>);
 *         R[d]<15:0>  = sum1<15:0>;
 *         R[d]<31:16> = sum2<15:0>;
 *         APSR.GE<1:0> = if sum1 >= 0 then '11' else '00';
 *         APSR.GE<3:2> = if sum2 >= 0 then '11' else '00';
 * and UQSUB8:
 *         diff1 = UInt(R[n]<7:0>) - UInt(R[m]<7:0>);   ... diff4 for bits 31:24
 *         R[d]<7:0> = UnsignedSat(diff1, 8);            ... R[d]<31:24>
 */
#ifndef ARM_COMMON_ARM_PSEUDO_PARALLEL_H
#define ARM_COMMON_ARM_PSEUDO_PARALLEL_H

#include "arm_pseudo_dp.h"

/* The DSP instructions on registers: Rd, Rn, Rm and, for an accumulate, Ra. */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rd;
	PseudoCodeRegisterDataType Rn;
	PseudoCodeRegisterDataType Rm;
	PseudoCodeRegisterDataType Ra;
	bool n_high;	/* SMLA<x><y>, SMUL<x><y>: the top halfword of R[n] */
	bool m_high;	/* ... of R[m] (and SMLAW<y>, SMULW<y>) */
	bool m_swap;	/* SMLAD, SMUAD, SMLSD, SMUSD with X: R[m] rotated by 16 */
	bool round;	/* SMMLA, SMMUL, SMMLS with R */
} arm_dsp_input_type;

typedef enum {
	ArmParallel_ADD16 = 0,
	ArmParallel_ASX,
	ArmParallel_SAX,
	ArmParallel_SUB16,
	ArmParallel_ADD8,
	ArmParallel_SUB8,
} ArmParallelOp;

typedef enum {
	ArmParallel_S = 0,
	ArmParallel_U,
	ArmParallel_Q,
	ArmParallel_UQ,
	ArmParallel_SH,
	ArmParallel_UH,
} ArmParallelKind;

/* Lane i of x, w bits wide, as SInt or UInt. */
static inline sint64 ArmLane(uint32 x, uint32 i, uint32 w, bool is_signed)
{
	uint32 lane = (x >> (i * w)) & ((1U << w) - 1U);

	if (is_signed) {
		return (sint64)(sint32)SignExtend(lane, w);
	}
	return (sint64)lane;
}

/*
 * The operation of the instruction's prefix (kind) and operation (op), as
 * the comment at the top of this file has it.
 */
static inline Std_ReturnType ArmParallel(TargetCoreType *core, const arm_dsp_input_type *in, ArmParallelKind kind,
	ArmParallelOp op, arm_dp_output_type *out)
{
	bool is_signed = (kind == ArmParallel_S) || (kind == ArmParallel_Q) || (kind == ArmParallel_SH);
	uint32 w = ((op == ArmParallel_ADD8) || (op == ArmParallel_SUB8)) ? 8U : 16U;
	uint32 lanes = 32U / w;
	uint32 mask = (1U << w) - 1U;
	uint32 result = 0U;
	uint32 ge = 0U;
	uint32 i;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	for (i = 0U; i < lanes; i++) {
		/* The lane of R[m] and whether this lane subtracts. */
		uint32 m_lane = i;
		bool subtract = (op == ArmParallel_SUB16) || (op == ArmParallel_SUB8);
		sint64 v;
		uint32 lane;
		bool lane_ge;

		if ((op == ArmParallel_ASX) || (op == ArmParallel_SAX)) {
			m_lane = 1U - i;
			subtract = (op == ArmParallel_ASX) ? (i == 0U) : (i == 1U);
		}
		if (subtract) {
			v = ArmLane(in->Rn.regData, i, w, is_signed) - ArmLane(in->Rm.regData, m_lane, w, is_signed);
		}
		else {
			v = ArmLane(in->Rn.regData, i, w, is_signed) + ArmLane(in->Rm.regData, m_lane, w, is_signed);
		}
		switch (kind) {
		case ArmParallel_Q:
			lane = (uint32)SignedSatQ(v, w, NULL) & mask;
			break;
		case ArmParallel_UQ:
			lane = UnsignedSatQ(v, w, NULL);
			break;
		case ArmParallel_SH:
		case ArmParallel_UH:
			/* result<w:1>: an arithmetic shift of the (w+1)-bit result */
			lane = (uint32)(v >> 1) & mask;
			break;
		default:
			lane = (uint32)v & mask;
			break;
		}
		result |= lane << (i * w);
		if (is_signed || subtract) {
			lane_ge = (v >= 0);
		}
		else {
			lane_ge = (v >= ((sint64)1 << w));
		}
		if (lane_ge) {
			ge |= ((w == 16U) ? 0x3U : 0x1U) << (i * (w / 8U));
		}
	}
	if ((kind == ArmParallel_S) || (kind == ArmParallel_U)) {
		ArmAPSRWrite(core, (ArmAPSR(core) & ~ARM_APSR_GE) | (ge << ARM_APSR_BITPOS_GE));
	}
	return ArmResultWrite(core, in->Rd.regId, result, out);
}

/*
 * SADD16, UADD16, QADD16, UQADD16, SHADD16, UHADD16
 */
static inline Std_ReturnType arm_pseudo_sadd16(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_S, ArmParallel_ADD16, out);
}

static inline Std_ReturnType arm_pseudo_uadd16(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_U, ArmParallel_ADD16, out);
}

static inline Std_ReturnType arm_pseudo_qadd16(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_Q, ArmParallel_ADD16, out);
}

static inline Std_ReturnType arm_pseudo_uqadd16(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_UQ, ArmParallel_ADD16, out);
}

static inline Std_ReturnType arm_pseudo_shadd16(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_SH, ArmParallel_ADD16, out);
}

static inline Std_ReturnType arm_pseudo_uhadd16(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_UH, ArmParallel_ADD16, out);
}


/*
 * SASX, UASX, QASX, UQASX, SHASX, UHASX
 */
static inline Std_ReturnType arm_pseudo_sasx(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_S, ArmParallel_ASX, out);
}

static inline Std_ReturnType arm_pseudo_uasx(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_U, ArmParallel_ASX, out);
}

static inline Std_ReturnType arm_pseudo_qasx(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_Q, ArmParallel_ASX, out);
}

static inline Std_ReturnType arm_pseudo_uqasx(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_UQ, ArmParallel_ASX, out);
}

static inline Std_ReturnType arm_pseudo_shasx(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_SH, ArmParallel_ASX, out);
}

static inline Std_ReturnType arm_pseudo_uhasx(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_UH, ArmParallel_ASX, out);
}


/*
 * SSAX, USAX, QSAX, UQSAX, SHSAX, UHSAX
 */
static inline Std_ReturnType arm_pseudo_ssax(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_S, ArmParallel_SAX, out);
}

static inline Std_ReturnType arm_pseudo_usax(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_U, ArmParallel_SAX, out);
}

static inline Std_ReturnType arm_pseudo_qsax(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_Q, ArmParallel_SAX, out);
}

static inline Std_ReturnType arm_pseudo_uqsax(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_UQ, ArmParallel_SAX, out);
}

static inline Std_ReturnType arm_pseudo_shsax(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_SH, ArmParallel_SAX, out);
}

static inline Std_ReturnType arm_pseudo_uhsax(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_UH, ArmParallel_SAX, out);
}


/*
 * SSUB16, USUB16, QSUB16, UQSUB16, SHSUB16, UHSUB16
 */
static inline Std_ReturnType arm_pseudo_ssub16(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_S, ArmParallel_SUB16, out);
}

static inline Std_ReturnType arm_pseudo_usub16(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_U, ArmParallel_SUB16, out);
}

static inline Std_ReturnType arm_pseudo_qsub16(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_Q, ArmParallel_SUB16, out);
}

static inline Std_ReturnType arm_pseudo_uqsub16(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_UQ, ArmParallel_SUB16, out);
}

static inline Std_ReturnType arm_pseudo_shsub16(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_SH, ArmParallel_SUB16, out);
}

static inline Std_ReturnType arm_pseudo_uhsub16(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_UH, ArmParallel_SUB16, out);
}


/*
 * SADD8, UADD8, QADD8, UQADD8, SHADD8, UHADD8
 */
static inline Std_ReturnType arm_pseudo_sadd8(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_S, ArmParallel_ADD8, out);
}

static inline Std_ReturnType arm_pseudo_uadd8(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_U, ArmParallel_ADD8, out);
}

static inline Std_ReturnType arm_pseudo_qadd8(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_Q, ArmParallel_ADD8, out);
}

static inline Std_ReturnType arm_pseudo_uqadd8(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_UQ, ArmParallel_ADD8, out);
}

static inline Std_ReturnType arm_pseudo_shadd8(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_SH, ArmParallel_ADD8, out);
}

static inline Std_ReturnType arm_pseudo_uhadd8(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_UH, ArmParallel_ADD8, out);
}


/*
 * SSUB8, USUB8, QSUB8, UQSUB8, SHSUB8, UHSUB8
 */
static inline Std_ReturnType arm_pseudo_ssub8(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_S, ArmParallel_SUB8, out);
}

static inline Std_ReturnType arm_pseudo_usub8(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_U, ArmParallel_SUB8, out);
}

static inline Std_ReturnType arm_pseudo_qsub8(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_Q, ArmParallel_SUB8, out);
}

static inline Std_ReturnType arm_pseudo_uqsub8(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_UQ, ArmParallel_SUB8, out);
}

static inline Std_ReturnType arm_pseudo_shsub8(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_SH, ArmParallel_SUB8, out);
}

static inline Std_ReturnType arm_pseudo_uhsub8(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	return ArmParallel(core, in, ArmParallel_UH, ArmParallel_SUB8, out);
}

/*
 * SEL
 *     if ConditionPassed() then
 *         EncodingSpecificOperations();
 *         R[d]<7:0>   = if APSR.GE<0> == '1' then R[n]<7:0>   else R[m]<7:0>;
 *         R[d]<15:8>  = if APSR.GE<1> == '1' then R[n]<15:8>  else R[m]<15:8>;
 *         R[d]<23:16> = if APSR.GE<2> == '1' then R[n]<23:16> else R[m]<23:16>;
 *         R[d]<31:24> = if APSR.GE<3> == '1' then R[n]<31:24> else R[m]<31:24>;
 */
static inline Std_ReturnType arm_pseudo_sel(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out)
{
	uint32 result = 0U;
	uint32 i;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	for (i = 0U; i < 4U; i++) {
		uint32 byte_mask = 0xFFU << (8U * i);

		if (ARM_BIT(ArmAPSR(core), ARM_APSR_BITPOS_GE + i) != 0U) {
			result |= in->Rn.regData & byte_mask;
		}
		else {
			result |= in->Rm.regData & byte_mask;
		}
	}
	return ArmResultWrite(core, in->Rd.regId, result, out);
}

#endif /* ARM_COMMON_ARM_PSEUDO_PARALLEL_H */
