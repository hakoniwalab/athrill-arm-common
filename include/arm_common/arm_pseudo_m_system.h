/*
 * Pseudo-code of the M profile's system instructions on the exception
 * model's state (arm_m_exception.h): MRS and MSR for all the special
 * registers (the program status registers through arm_pseudo_system.h),
 * CPS (CPSIE, CPSID) and SVC.
 */
#ifndef ARM_COMMON_ARM_PSEUDO_M_SYSTEM_H
#define ARM_COMMON_ARM_PSEUDO_M_SYSTEM_H

#include "arm_pseudo_system.h"
#include "arm_m_exception.h"

/* The implemented bits of a priority value: the top priority_bits of 8. */
static inline uint32 ArmMPriorityMask(const ArmMStateType *m)
{
	return (0xFF00U >> m->priority_bits) & 0xFFU;
}

/*
 * MRS
 *     if ConditionPassed() then
 *         EncodingSpecificOperations();
 *         R[d] = Zeros(32);
 *         case SYSm<7:3> of
 *             when '00000'  (the program status registers: arm_pseudo_mrs_xpsr)
 *             when '00001'
 *                 if CurrentModeIsPrivileged() then
 *                     case SYSm<2:0> of
 *                         when '000' R[d] = SP_main;
 *                         when '001' R[d] = SP_process;
 *             when '00010'
 *                 case SYSm<2:0> of
 *                     when '000' R[d]<0> = PRIMASK<0>;
 *                     when '001' R[d]<7:0> = BASEPRI<7:0>;
 *                     when '010' R[d]<7:0> = BASEPRI<7:0>;
 *                     when '011' R[d]<0> = FAULTMASK<0>;
 *                     when '100'
 *                         if HaveFPExt() then R[d]<2:0> = CONTROL<2:0>;
 *                         else R[d]<1:0> = CONTROL<1:0>;
 */
static inline Std_ReturnType arm_pseudo_mrs(TargetCoreType *core, const arm_mrs_input_type *in, arm_dp_output_type *out)
{
	const ArmMStateType *m = ArmMState(core);
	uint32 result = 0U;

	if (in->SYSm < 8U) {
		return arm_pseudo_mrs_xpsr(core, in, out);
	}
	if (!ArmDataProcessingBegin(core, in->cond, &in->Rd, out)) {
		return STD_E_OK;
	}
	switch (in->SYSm) {
	case 8U:
		result = ArmMPrivileged(m) ? ArmMSPMain(core) : 0U;
		break;
	case 9U:
		result = ArmMPrivileged(m) ? ArmMSPProcess(core) : 0U;
		break;
	case 16U:
		result = m->primask & 1U;
		break;
	case 17U:
	case 18U:
		result = m->basepri & 0xFFU;
		break;
	case 19U:
		result = m->faultmask & 1U;
		break;
	default:	/* 20: CONTROL */
		result = m->control & (m->have_fp ? 0x7U : 0x3U);
		break;
	}
	return ArmResultWrite(core, in->Rd.regId, result, out);
}

/*
 * MSR
 *     if ConditionPassed() then
 *         EncodingSpecificOperations();
 *         case SYSm<7:3> of
 *             when '00000'  (the program status registers: arm_pseudo_msr_xpsr)
 *             when '00001'
 *                 if CurrentModeIsPrivileged() then
 *                     case SYSm<2:0> of
 *                         when '000' SP_main = R[n]<31:2>:'00';
 *                         when '001' SP_process = R[n]<31:2>:'00';
 *             when '00010'
 *                 case SYSm<2:0> of
 *                     when '000'
 *                         if CurrentModeIsPrivileged() then PRIMASK<0> = R[n]<0>;
 *                     when '001'
 *                         if CurrentModeIsPrivileged() then BASEPRI<7:0> = R[n]<7:0>;
 *                     when '010'
 *                         if CurrentModeIsPrivileged() &&
 *                            (R[n]<7:0> != '00000000') &&
 *                            (UInt(R[n]<7:0>) < UInt(BASEPRI<7:0>) || BASEPRI<7:0> == '00000000') then
 *                             BASEPRI<7:0> = R[n]<7:0>;
 *                     when '011'
 *                         if CurrentModeIsPrivileged() && (ExecutionPriority() > -1) then
 *                             FAULTMASK<0> = R[n]<0>;
 *                     when '100'
 *                         if CurrentModeIsPrivileged() then
 *                             CONTROL.nPRIV = R[n]<0>;
 *                             if CurrentMode == Mode_Thread then
 *                                 CONTROL.SPSEL = R[n]<1>;
 *                             if HaveFPExt() then CONTROL.FPCA = R[n]<2>;
 * (BASEPRI keeps the implemented priority bits only.)
 */
static inline Std_ReturnType arm_pseudo_msr(TargetCoreType *core, const arm_msr_input_type *in, arm_dp_output_type *out)
{
	ArmMStateType *m = ArmMState(core);
	uint32 value = in->Rn.regData;
	uint32 basepri;

	if (in->SYSm < 8U) {
		return arm_pseudo_msr_xpsr(core, in, out);
	}
	ARM_NO_REG(out, Rd);
	out->status_flag = ApsrFlags(ArmAPSR(core));
	out->passed = ConditionHolds(in->cond, ArmAPSR(core));
	if (!out->passed || !ArmMPrivileged(m)) {
		return STD_E_OK;
	}
	basepri = value & ArmMPriorityMask(m);
	m->priority_check = TRUE;	/* PRIMASK, BASEPRI, FAULTMASK: the execution priority may change */
	switch (in->SYSm) {
	case 8U:
		ArmMSPMainWrite(core, value & ~3U);
		break;
	case 9U:
		ArmMSPProcessWrite(core, value & ~3U);
		break;
	case 16U:
		m->primask = value & 1U;
		break;
	case 17U:
		m->basepri = basepri;
		break;
	case 18U:
		if ((basepri != 0U) && ((basepri < m->basepri) || (m->basepri == 0U))) {
			m->basepri = basepri;
		}
		break;
	case 19U:
		if (ArmMExecutionPriority(m) > -1) {
			m->faultmask = value & 1U;
		}
		break;
	default:	/* 20: CONTROL */
		m->control = (m->control & ~ARM_M_CONTROL_NPRIV) | (value & ARM_M_CONTROL_NPRIV);
		if (!m->handler_mode) {
			ArmMSetModeAndSPSEL(core, FALSE, (value & ARM_M_CONTROL_SPSEL) != 0U);
		}
		if (m->have_fp) {
			m->control = (m->control & ~ARM_M_CONTROL_FPCA) | (value & ARM_M_CONTROL_FPCA);
		}
		break;
	}
	return STD_E_OK;
}

/*
 * CPS (CPSIE, CPSID; not conditional)
 */
typedef struct {
	const char *instrName;
	bool enable;		/* CPSIE (im == '0'), else CPSID */
	bool affectPRI;		/* I */
	bool affectFAULT;	/* F */
} arm_cps_input_type;

/*
 * EncodingSpecificOperations();
 * if CurrentModeIsPrivileged() then
 *     if enable then
 *         if affectPRI then PRIMASK<0> = '0';
 *         if affectFAULT then FAULTMASK<0> = '0';
 *     if disable then
 *         if affectPRI then PRIMASK<0> = '1';
 *         if affectFAULT && ExecutionPriority() > -1 then FAULTMASK<0> = '1';
 */
static inline Std_ReturnType arm_pseudo_cps(TargetCoreType *core, const arm_cps_input_type *in)
{
	ArmMStateType *m = ArmMState(core);

	if (!ArmMPrivileged(m)) {
		return STD_E_OK;
	}
	m->priority_check = TRUE;	/* PRIMASK, FAULTMASK: the execution priority may change */
	if (in->enable) {
		if (in->affectPRI) {
			m->primask = 0U;
		}
		if (in->affectFAULT) {
			m->faultmask = 0U;
		}
	}
	else {
		if (in->affectPRI) {
			m->primask = 1U;
		}
		if (in->affectFAULT && (ArmMExecutionPriority(m) > -1)) {
			m->faultmask = 1U;
		}
	}
	return STD_E_OK;
}

/*
 * SVC
 */
typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	uint32 imm32;
} arm_svc_input_type;

/*
 * if ConditionPassed() then
 *     EncodingSpecificOperations();
 *     CallSupervisor();
 */
static inline Std_ReturnType arm_pseudo_svc(TargetCoreType *core, const arm_svc_input_type *in)
{
	if (ConditionHolds(in->cond, ArmAPSR(core))) {
		ArmCallSupervisor(core);
	}
	return STD_E_OK;
}

#endif /* ARM_COMMON_ARM_PSEUDO_M_SYSTEM_H */
