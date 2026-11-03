/*
 * A test profile: a core with R0-R14, an APSR, a PC and 256 bytes of
 * memory at TEST_MEM_BASE. It behaves as the A profile in ARM state or as the
 * M profile (Thumb), so the pseudo-code is tested through both.
 */
#ifndef ARM_TEST_PROFILE_H
#define ARM_TEST_PROFILE_H

#include <string.h>
#include "std_errno.h"
#include "arm_common/arm_types.h"

#define TEST_MEM_BASE	0x20000000U
#define TEST_MEM_SIZE	256U

typedef enum {
	TestProfile_A_ARM = 0,	/* A profile, ARM state */
	TestProfile_M,		/* M profile (Thumb only) */
} TestProfileType;

typedef struct {
	TestProfileType profile;
	uint32 r[15];
	uint32 apsr;
	uint32 pc;		/* address of the current instruction */
	uint32 next_pc;		/* address of the next instruction */
	bool thumb;		/* instruction set state after an interworking write (A profile) */
	bool exclusive_valid;	/* local exclusive monitor */
	uint32 exclusive_address;
	bool div_0_trp;		/* CCR.DIV_0_TRP (M profile) */
	uint32 itstate;		/* ITSTATE.IT<7:0> */
	uint32 ipsr;		/* IPSR (M profile) */
	bool no_dsp;		/* without the DSP extension */
	uint32 unpriv_accesses;	/* MemU_unpriv[] accesses, to check they are used */
	uint8 mem[TEST_MEM_SIZE];
} TargetCoreType;

static inline uint32 ArmR(const TargetCoreType *core, uint32 n)
{
	return core->r[n];
}

static inline void ArmRWrite(TargetCoreType *core, uint32 n, uint32 value)
{
	core->r[n] = value;
}

static inline uint32 ArmPC(const TargetCoreType *core)
{
	return core->pc + ((core->profile == TestProfile_A_ARM) ? 8U : 4U);
}

static inline ArmInstrSet ArmCurrentInstrSet(const TargetCoreType *core)
{
	return (core->profile == TestProfile_A_ARM) ? InstrSet_ARM : InstrSet_Thumb;
}

static inline uint32 ArmAPSR(const TargetCoreType *core)
{
	return core->apsr;
}

static inline void ArmAPSRWrite(TargetCoreType *core, uint32 apsr)
{
	core->apsr = apsr & (ARM_APSR_N | ARM_APSR_Z | ARM_APSR_C | ARM_APSR_V | ARM_APSR_Q | ARM_APSR_GE);
}

static inline Std_ReturnType ArmBranchWritePC(TargetCoreType *core, uint32 address)
{
	if (core->profile == TestProfile_A_ARM) {
		core->next_pc = address & ~3U;
	}
	else {
		core->next_pc = address & ~1U;
	}
	return STD_E_OK;
}

static inline Std_ReturnType ArmBXWritePC(TargetCoreType *core, uint32 address)
{
	if (core->profile == TestProfile_M) {
		if ((address & 1U) == 0U) {
			return STD_E_EXEC;	/* UsageFault (INVSTATE) */
		}
		core->next_pc = address & ~1U;
		return STD_E_OK;
	}
	if ((address & 1U) != 0U) {
		core->thumb = TRUE;
		core->next_pc = address & ~1U;
	}
	else if ((address & 2U) == 0U) {
		core->thumb = FALSE;
		core->next_pc = address;
	}
	else {
		return STD_E_INVALID;	/* UNPREDICTABLE */
	}
	return STD_E_OK;
}

/* BLXWritePC: the A profile's BXWritePC; in the M profile, the same
 * without an exception return. */
static inline Std_ReturnType ArmBLXWritePC(TargetCoreType *core, uint32 address)
{
	return ArmBXWritePC(core, address);
}

static inline Std_ReturnType ArmLoadWritePC(TargetCoreType *core, uint32 address)
{
	return ArmBXWritePC(core, address);
}

static inline Std_ReturnType ArmALUWritePC(TargetCoreType *core, uint32 address)
{
	if (core->profile == TestProfile_A_ARM) {
		return ArmBXWritePC(core, address);
	}
	return ArmBranchWritePC(core, address);
}

static inline Std_ReturnType ArmMemU_R(TargetCoreType *core, uint32 address, uint32 size, uint32 *value)
{
	uint32 v = 0U;
	uint32 i;

	if ((address < TEST_MEM_BASE) || ((address - TEST_MEM_BASE) + size > TEST_MEM_SIZE)) {
		return STD_E_SEGV;
	}
	for (i = 0U; i < size; i++) {
		v |= (uint32)core->mem[address - TEST_MEM_BASE + i] << (8U * i);
	}
	*value = v;
	return STD_E_OK;
}

static inline Std_ReturnType ArmMemU_W(TargetCoreType *core, uint32 address, uint32 size, uint32 value)
{
	uint32 i;

	if ((address < TEST_MEM_BASE) || ((address - TEST_MEM_BASE) + size > TEST_MEM_SIZE)) {
		return STD_E_SEGV;
	}
	for (i = 0U; i < size; i++) {
		core->mem[address - TEST_MEM_BASE + i] = (uint8)(value >> (8U * i));
	}
	return STD_E_OK;
}

static inline Std_ReturnType ArmMemA_R(TargetCoreType *core, uint32 address, uint32 size, uint32 *value)
{
	if ((address & (size - 1U)) != 0U) {
		return STD_E_SEGV;	/* alignment fault */
	}
	return ArmMemU_R(core, address, size, value);
}

static inline Std_ReturnType ArmMemA_W(TargetCoreType *core, uint32 address, uint32 size, uint32 value)
{
	if ((address & (size - 1U)) != 0U) {
		return STD_E_SEGV;	/* alignment fault */
	}
	return ArmMemU_W(core, address, size, value);
}

static inline Std_ReturnType ArmMemU_unpriv_R(TargetCoreType *core, uint32 address, uint32 size, uint32 *value)
{
	core->unpriv_accesses++;
	return ArmMemU_R(core, address, size, value);
}

static inline Std_ReturnType ArmMemU_unpriv_W(TargetCoreType *core, uint32 address, uint32 size, uint32 value)
{
	core->unpriv_accesses++;
	return ArmMemU_W(core, address, size, value);
}

static inline void ArmSetExclusiveMonitors(TargetCoreType *core, uint32 address, uint32 size)
{
	(void)size;
	core->exclusive_valid = TRUE;
	core->exclusive_address = address;
}

static inline Std_ReturnType ArmExclusiveMonitorsPass(TargetCoreType *core, uint32 address, uint32 size, bool *pass)
{
	if ((address & (size - 1U)) != 0U) {
		return STD_E_SEGV;	/* alignment fault */
	}
	*pass = core->exclusive_valid && (core->exclusive_address == address);
	if (*pass) {
		core->exclusive_valid = FALSE;
	}
	return STD_E_OK;
}

static inline void ArmClearExclusiveLocal(TargetCoreType *core)
{
	core->exclusive_valid = FALSE;
}

static inline bool ArmIntegerZeroDivideTrappingEnabled(const TargetCoreType *core)
{
	return (core->profile == TestProfile_M) && core->div_0_trp;
}

static inline Std_ReturnType ArmGenerateIntegerZeroDivide(TargetCoreType *core)
{
	(void)core;
	return STD_E_EXEC;	/* UsageFault (DIVBYZERO) */
}

static inline void ArmITStateWrite(TargetCoreType *core, uint32 itstate)
{
	core->itstate = itstate;
}

static inline uint32 ArmIPSR(const TargetCoreType *core)
{
	return core->ipsr;
}

static inline bool ArmHaveDSPExt(const TargetCoreType *core)
{
	return !core->no_dsp;
}

static inline void test_core_init(TargetCoreType *core, TestProfileType profile, uint32 pc)
{
	memset(core, 0, sizeof(*core));
	core->profile = profile;
	core->pc = pc;
	core->next_pc = pc + ((profile == TestProfile_A_ARM) ? 4U : 2U);
}

#endif /* ARM_TEST_PROFILE_H */
