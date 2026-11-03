/*
 * Pseudo-code of the floating-point loads and stores: VLDR, VSTR, VLDM,
 * VSTM (VPUSH and VPOP are VSTMDB and VLDM on SP).
 *
 * A doubleword is two word accesses, the low word at the lower address
 * (little-endian): D[d] = word2:word1.
 */
#ifndef ARM_COMMON_ARM_PSEUDO_FP_LS_H
#define ARM_COMMON_ARM_PSEUDO_FP_LS_H

#include "arm_pseudo_fp_move.h"

typedef struct {
	const char *instrName;
	PseudoCodeConditionDataType cond;
	bool single_regs;	/* S registers, or D registers */
	uint32 d;		/* the first register */
	PseudoCodeRegisterDataType Rn;
	PseudoCodeImmediateData32Type imm32;
	bool add;
	bool wback;		/* VLDM, VSTM */
	uint32 regs;		/* VLDM, VSTM: the number of registers */
} arm_fp_ls_input_type;

/* S[d] or D[d] from memory at address (MemA). */
static inline Std_ReturnType ArmFPLoad(TargetCoreType *core, bool single_regs, uint32 d, uint32 address)
{
	uint32 word1;
	uint32 word2;
	Std_ReturnType err;

	err = ArmMemA_R(core, address, 4U, &word1);
	if (err != STD_E_OK) {
		return err;
	}
	if (single_regs) {
		ArmSWrite(core, d, word1);
		return STD_E_OK;
	}
	err = ArmMemA_R(core, address + 4U, 4U, &word2);
	if (err != STD_E_OK) {
		return err;
	}
	ArmDWrite(core, d, ((uint64)word2 << 32) | word1);
	return STD_E_OK;
}

/* Memory at address (MemA) from S[d] or D[d]. */
static inline Std_ReturnType ArmFPStore(TargetCoreType *core, bool single_regs, uint32 d, uint32 address)
{
	uint64 value;
	Std_ReturnType err;

	if (single_regs) {
		return ArmMemA_W(core, address, 4U, ArmS(core, d));
	}
	value = ArmD(core, d);
	err = ArmMemA_W(core, address, 4U, (uint32)value);
	if (err != STD_E_OK) {
		return err;
	}
	return ArmMemA_W(core, address + 4U, 4U, (uint32)(value >> 32));
}

/*
 * VLDR, VSTR
 *     base = if n == 15 then Align(PC,4) else R[n];
 *     address = if add then (base + imm32) else (base - imm32);
 *     VLDR: if single_reg then S[d] = MemA[address,4];
 *           else word1 = MemA[address,4]; word2 = MemA[address+4,4];
 *                D[d] = if BigEndian() then word1:word2 else word2:word1;
 *     VSTR: if single_reg then MemA[address,4] = S[d];
 *           else MemA[address,4] = D[d]<31:0>; MemA[address+4,4] = D[d]<63:32>;
 */
static inline uint32 ArmFPAddress(const arm_fp_ls_input_type *in)
{
	uint32 base = (in->Rn.regId == ARM_REG_PC) ? Align(in->Rn.regData, 4U) : in->Rn.regData;

	if (in->add) {
		return base + in->imm32;
	}
	return base - in->imm32;
}

static inline Std_ReturnType arm_pseudo_vldr(TargetCoreType *core, const arm_fp_ls_input_type *in, arm_fp_output_type *out)
{
	out->result = 0U;
	if (!ArmFPBegin(core, in->cond, out)) {
		return out->check;
	}
	return ArmFPLoad(core, in->single_regs, in->d, ArmFPAddress(in));
}

static inline Std_ReturnType arm_pseudo_vstr(TargetCoreType *core, const arm_fp_ls_input_type *in, arm_fp_output_type *out)
{
	out->result = 0U;
	if (!ArmFPBegin(core, in->cond, out)) {
		return out->check;
	}
	return ArmFPStore(core, in->single_regs, in->d, ArmFPAddress(in));
}

/*
 * VLDM, VSTM (Armv7-M: the base register is written before the transfers)
 *     address = if add then R[n] else R[n]-imm32;
 *     if wback then R[n] = if add then R[n]+imm32 else R[n]-imm32;
 *     for r = 0 to regs-1
 *         if single_regs then
 *             S[d+r] = MemA[address,4]; address = address+4;          // VSTM: MemA[address,4] = S[d+r];
 *         else
 *             word1 = MemA[address,4]; word2 = MemA[address+4,4]; address = address+8;
 *             D[d+r] = if BigEndian() then word1:word2 else word2:word1;
 */
static inline Std_ReturnType ArmFPMultiple(TargetCoreType *core, const arm_fp_ls_input_type *in, bool load, arm_fp_output_type *out)
{
	uint32 address;
	uint32 step = in->single_regs ? 4U : 8U;
	uint32 r;
	Std_ReturnType err;

	out->result = 0U;
	if (!ArmFPBegin(core, in->cond, out)) {
		return out->check;
	}
	address = in->add ? in->Rn.regData : (in->Rn.regData - in->imm32);
	if (in->wback) {
		ArmRWrite(core, in->Rn.regId, in->add ? (in->Rn.regData + in->imm32) : (in->Rn.regData - in->imm32));
	}
	for (r = 0U; r < in->regs; r++) {
		if (load) {
			err = ArmFPLoad(core, in->single_regs, in->d + r, address);
		}
		else {
			err = ArmFPStore(core, in->single_regs, in->d + r, address);
		}
		if (err != STD_E_OK) {
			return err;
		}
		address += step;
	}
	return STD_E_OK;
}

static inline Std_ReturnType arm_pseudo_vldm(TargetCoreType *core, const arm_fp_ls_input_type *in, arm_fp_output_type *out)
{
	return ArmFPMultiple(core, in, TRUE, out);
}

static inline Std_ReturnType arm_pseudo_vstm(TargetCoreType *core, const arm_fp_ls_input_type *in, arm_fp_output_type *out)
{
	return ArmFPMultiple(core, in, FALSE, out);
}

#endif /* ARM_COMMON_ARM_PSEUDO_FP_LS_H */
