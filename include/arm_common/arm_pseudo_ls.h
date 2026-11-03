/*
 * What the single load and store instructions share: their input and output
 * structures and the address calculation.
 *
 * Three addressing forms, as the pseudo-code has them:
 *   immediate   offset = imm32, from R[n]
 *   register    offset = Shift(R[m], shift_t, shift_n, APSR.C), from R[n]
 *   literal     base = Align(PC,4), offset = imm32 (no write-back)
 * with index (offset or pre-indexed), add (offset added or subtracted) and
 * wback (R[n] written with the offset address).
 */
#ifndef ARM_COMMON_ARM_PSEUDO_LS_H
#define ARM_COMMON_ARM_PSEUDO_LS_H

#include "arm_profile.h"

typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rt;
	PseudoCodeRegisterDataType Rn;
	PseudoCodeImmediateData32Type imm32;
	bool index;
	bool add;
	bool wback;
} arm_ls_imm_input_type;

typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rt;
	PseudoCodeRegisterDataType Rn;
	PseudoCodeRegisterDataType Rm;
	PseudoCodeSRType shift_t;
	PseudoCodeShiftNType shift_n;
	bool index;
	bool add;
	bool wback;
} arm_ls_reg_input_type;

typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	PseudoCodeRegisterDataType Rt;
	PseudoCodeImmediateData32Type imm32;
	bool add;
} arm_ls_lit_input_type;

typedef struct {
	bool passed;
	uint32 address;
	PseudoCodeRegisterDataType Rt;		/* the value loaded or stored */
	PseudoCodeRegisterDataType Rn;		/* after write-back */
} arm_ls_output_type;

/*
 * The start of every load and store:
 *     if ConditionPassed() then
 *         EncodingSpecificOperations();
 */
static inline bool ArmLoadStoreBegin(const TargetCoreType *core, uint32 cond, const PseudoCodeRegisterDataType *Rt,
	const PseudoCodeRegisterDataType *Rn, arm_ls_output_type *out)
{
	out->Rt = *Rt;
	out->Rn = *Rn;
	out->address = 0U;
	out->passed = ConditionHolds(cond, ArmAPSR(core));
	return out->passed;
}

/*
 *     offset_addr = if add then (R[n] + offset) else (R[n] - offset);
 *     address = if index then offset_addr else R[n];
 * Returns offset_addr; out->address gets address.
 */
static inline uint32 ArmLoadStoreAddress(uint32 rn, uint32 offset, bool index, bool add, arm_ls_output_type *out)
{
	uint32 offset_addr = add ? (rn + offset) : (rn - offset);

	out->address = index ? offset_addr : rn;
	return offset_addr;
}

/*
 *     if wback then R[n] = offset_addr;
 */
static inline void ArmLoadStoreWriteBack(TargetCoreType *core, bool wback, uint32 n, uint32 offset_addr, arm_ls_output_type *out)
{
	if (wback) {
		ArmRWrite(core, n, offset_addr);
		out->Rn.regData = offset_addr;
	}
}

/*
 * The literal address:
 *     base = Align(PC,4);
 *     address = if add then (base + imm32) else (base - imm32);
 */
static inline uint32 ArmLiteralAddress(const TargetCoreType *core, const arm_ls_lit_input_type *in)
{
	uint32 base = Align(ArmPC(core), 4U);

	return in->add ? (base + in->imm32) : (base - in->imm32);
}

/* The offset of a register form: Shift(R[m], shift_t, shift_n, APSR.C). */
static inline uint32 ArmRegisterOffset(const TargetCoreType *core, const arm_ls_reg_input_type *in)
{
	return Shift(in->Rm.regData, in->shift_t, in->shift_n, ApsrFlags(ArmAPSR(core)).carry);
}

#endif /* ARM_COMMON_ARM_PSEUDO_LS_H */
