/*
 * Pseudo-code of IT and of the M profile's MRS and MSR for the program
 * status registers (SYSm<7:3> == '00000': APSR, IAPSR, EAPSR, XPSR, IPSR,
 * EPSR, IEPSR). The other special registers of MRS and MSR (stack pointers,
 * PRIMASK, BASEPRI, FAULTMASK, CONTROL) belong to the exception model and
 * are not here.
 */
#ifndef ARM_COMMON_ARM_PSEUDO_SYSTEM_H
#define ARM_COMMON_ARM_PSEUDO_SYSTEM_H

#include "arm_pseudo_dp.h"

/*
 * IT
 */
typedef struct {
	const char *instrName;
	uint32 firstcond;
	uint32 mask;
} arm_it_input_type;

/*
 * EncodingSpecificOperations();
 * ITSTATE.IT<7:0> = firstcond:mask;
 *
 * (IT is not conditional. The encoding makes mask == '0000', firstcond ==
 * '1111', firstcond == '1110' with more than one instruction, and IT in an
 * IT block UNPREDICTABLE.)
 */
static inline Std_ReturnType arm_pseudo_it(TargetCoreType *core, const arm_it_input_type *in)
{
	ArmITStateWrite(core, ((in->firstcond & 0xFU) << 4) | (in->mask & 0xFU));
	return STD_E_OK;
}

/*
 * MRS, MSR (program status registers)
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rd;
	uint32 SYSm;	/* 0..7 */
} arm_mrs_input_type;

typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rn;
	uint32 SYSm;	/* 0..7 */
	uint32 mask;	/* mask<1:0> */
} arm_msr_input_type;

#define ARM_APSR_NZCVQ	(ARM_APSR_N | ARM_APSR_Z | ARM_APSR_C | ARM_APSR_V | ARM_APSR_Q)

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     R[d] = Zeros(32);
 *     case SYSm<7:3> of
 *         when '00000'
 *             if SYSm<0> == '1' then
 *                 R[d]<8:0> = IPSR<8:0>;
 *             if SYSm<1> == '1' then
 *                 R[d]<26:24> = '000';      // EPSR reads as zero
 *                 R[d]<15:10> = '000000';
 *             if SYSm<2> == '0' then
 *                 R[d]<31:27> = APSR<31:27>;
 *                 if HaveDSPExt() then
 *                     R[d]<19:16> = APSR<19:16>;
 */
static inline Std_ReturnType arm_pseudo_mrs_xpsr(TargetCoreType *core, const arm_mrs_input_type *in, arm_dp_output_type *out)
{
	uint32 result;

	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	result = 0U;
	if (ARM_BIT(in->SYSm, 0U)) {
		result |= ArmIPSR(core) & 0x1FFU;
	}
	if (ARM_BIT(in->SYSm, 2U) == 0U) {
		result |= ArmAPSR(core) & ARM_APSR_NZCVQ;
		if (ArmHaveDSPExt(core)) {
			result |= ArmAPSR(core) & ARM_APSR_GE;
		}
	}
	return ArmResultWrite(core, in->Rd.regId, result, out);
}

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     case SYSm<7:3> of
 *         when '00000'
 *             if SYSm<2> == '0' then        // Include APSR
 *                 if mask<0> == '1' then    // GE<3:0> bits
 *                     if !HaveDSPExt() then
 *                         UNPREDICTABLE;
 *                     else
 *                         APSR<19:16> = R[n]<19:16>;
 *                 if mask<1> == '1' then    // N, Z, C, V, Q bits
 *                     APSR<31:27> = R[n]<31:27>;
 * (IPSR and EPSR are not written.)
 */
static inline Std_ReturnType arm_pseudo_msr_xpsr(TargetCoreType *core, const arm_msr_input_type *in, arm_dp_output_type *out)
{
	uint32 apsr;

	ARM_NO_REG(out, Rd);
	out->status_flag = ApsrFlags(ArmAPSR(core));
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	if (!out->passed) {
		return STD_E_OK;
	}
	if (ARM_BIT(in->SYSm, 2U) != 0U) {
		return STD_E_OK;
	}
	apsr = ArmAPSR(core);
	if (ARM_BIT(in->mask, 0U) != 0U) {
		if (!ArmHaveDSPExt(core)) {
			return STD_E_INVALID;
		}
		apsr = (apsr & ~ARM_APSR_GE) | (in->Rn.regData & ARM_APSR_GE);
	}
	if (ARM_BIT(in->mask, 1U) != 0U) {
		apsr = (apsr & ~ARM_APSR_NZCVQ) | (in->Rn.regData & ARM_APSR_NZCVQ);
	}
	ArmAPSRWrite(core, apsr);
	return STD_E_OK;
}

#endif /* ARM_COMMON_ARM_PSEUDO_SYSTEM_H */
