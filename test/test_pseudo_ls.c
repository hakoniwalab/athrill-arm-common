/*
 * Tests of the load and store pseudo-code through the test profile.
 *
 * The test memory holds a known pattern (pattern(), from the address); the
 * expected values of loads are put together from it here, and stores are
 * checked byte by byte, so the expected values do not come from the
 * profile's or the pseudo-code's memory access.
 */
#include <stdio.h>
#include "arm_common/arm_pseudo_load.h"
#include "arm_common/arm_pseudo_store.h"
#include "arm_common/arm_pseudo_multiple.h"
#include "arm_common/arm_pseudo_exclusive.h"
#include "arm_common/arm_pseudo_branch.h"

static int failures;
static int checks;

#define CHECK(cond, ...) \
	do { \
		checks++; \
		if (!(cond)) { \
			failures++; \
			printf("FAIL %s:%d: ", __FILE__, __LINE__); \
			printf(__VA_ARGS__); \
			printf("\n"); \
		} \
	} while (0)

#define PC0	0x08000100U
#define M(off)	(TEST_MEM_BASE + (off))

static uint8 pattern(uint32 address)
{
	return (uint8)((address * 37U) ^ 0x5AU);
}

/* The little-endian value of size bytes of the pattern at address. */
static uint32 pattern_value(uint32 address, uint32 size)
{
	uint32 v = 0U;
	uint32 i;

	for (i = 0U; i < size; i++) {
		v |= (uint32)pattern(address + i) << (8U * i);
	}
	return v;
}

static uint32 mem_value(const TargetCoreType *core, uint32 address, uint32 size)
{
	uint32 v = 0U;
	uint32 i;

	for (i = 0U; i < size; i++) {
		v |= (uint32)core->mem[address - TEST_MEM_BASE + i] << (8U * i);
	}
	return v;
}

static void init(TargetCoreType *core, TestProfileType p)
{
	uint32 i;

	test_core_init(core, p, PC0);
	for (i = 0U; i < TEST_MEM_SIZE; i++) {
		core->mem[i] = pattern(TEST_MEM_BASE + i);
	}
}

/*
 * Single loads
 */

typedef Std_ReturnType (*LoadImm)(TargetCoreType *, const arm_ls_imm_input_type *, arm_ls_output_type *);
typedef Std_ReturnType (*LoadReg)(TargetCoreType *, const arm_ls_reg_input_type *, arm_ls_output_type *);
typedef Std_ReturnType (*LoadLit)(TargetCoreType *, const arm_ls_lit_input_type *, arm_ls_output_type *);

static Std_ReturnType ls_imm(LoadImm f, TargetCoreType *core, uint32 cond, uint32 t, uint32 n, uint32 imm32, bool index, bool add, bool wback)
{
	arm_ls_imm_input_type in;
	arm_ls_output_type out;

	in.instrName = "LS";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rt, t);
	ARM_SET_REG(core, &in, Rn, n);
	in.imm32 = imm32;
	in.index = index;
	in.add = add;
	in.wback = wback;
	return f(core, &in, &out);
}

static Std_ReturnType ls_reg(LoadReg f, TargetCoreType *core, uint32 cond, uint32 t, uint32 n, uint32 m, SRType st, uint32 sn, bool index, bool add, bool wback)
{
	arm_ls_reg_input_type in;
	arm_ls_output_type out;

	in.instrName = "LS";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rt, t);
	ARM_SET_REG(core, &in, Rn, n);
	ARM_SET_REG(core, &in, Rm, m);
	in.shift_t = st;
	in.shift_n = sn;
	in.index = index;
	in.add = add;
	in.wback = wback;
	return f(core, &in, &out);
}

static Std_ReturnType ls_lit(LoadLit f, TargetCoreType *core, uint32 cond, uint32 t, uint32 imm32, bool add)
{
	arm_ls_lit_input_type in;
	arm_ls_output_type out;

	in.instrName = "LS";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rt, t);
	in.imm32 = imm32;
	in.add = add;
	return f(core, &in, &out);
}

typedef struct {
	const char *name;
	LoadImm imm;
	LoadReg reg;
	LoadLit lit;
	LoadImm unpriv;
	uint32 size;
	bool sign;
} LoadKind;

static const LoadKind loads[] = {
	{ "LDR", arm_pseudo_ldr_imm, arm_pseudo_ldr_reg, arm_pseudo_ldr_lit, arm_pseudo_ldrt, 4U, FALSE },
	{ "LDRB", arm_pseudo_ldrb_imm, arm_pseudo_ldrb_reg, arm_pseudo_ldrb_lit, arm_pseudo_ldrbt, 1U, FALSE },
	{ "LDRH", arm_pseudo_ldrh_imm, arm_pseudo_ldrh_reg, arm_pseudo_ldrh_lit, arm_pseudo_ldrht, 2U, FALSE },
	{ "LDRSB", arm_pseudo_ldrsb_imm, arm_pseudo_ldrsb_reg, arm_pseudo_ldrsb_lit, arm_pseudo_ldrsbt, 1U, TRUE },
	{ "LDRSH", arm_pseudo_ldrsh_imm, arm_pseudo_ldrsh_reg, arm_pseudo_ldrsh_lit, arm_pseudo_ldrsht, 2U, TRUE },
};

static uint32 expected_load(const LoadKind *k, uint32 address)
{
	uint32 v = pattern_value(address, k->size);

	if (k->sign && (k->size == 1U) && ((v & 0x80U) != 0U)) {
		v |= 0xFFFFFF00U;
	}
	if (k->sign && (k->size == 2U) && ((v & 0x8000U) != 0U)) {
		v |= 0xFFFF0000U;
	}
	return v;
}

static void test_loads(void)
{
	TargetCoreType core;
	uint32 i;
	uint32 a;

	for (i = 0U; i < sizeof(loads) / sizeof(loads[0]); i++) {
		const LoadKind *k = &loads[i];
		TestProfileType p;

		for (p = TestProfile_A_ARM; p <= TestProfile_M; p++) {
			/* Offsets: addresses 0x10..0x13 (aligned and not), added and subtracted, no write-back. */
			for (a = 0x10U; a < 0x14U; a++) {
				init(&core, p);
				core.r[1] = M(a - 8U);
				CHECK(ls_imm(k->imm, &core, ARM_COND_AL, 0U, 1U, 8U, TRUE, TRUE, FALSE) == STD_E_OK, "%s imm", k->name);
				CHECK(core.r[0] == expected_load(k, M(a)) && core.r[1] == M(a - 8U), "%s [r1, #8] at %02x: %08x, expected %08x", k->name, a, core.r[0], expected_load(k, M(a)));
				core.r[1] = M(a + 4U);
				CHECK(ls_imm(k->imm, &core, ARM_COND_AL, 2U, 1U, 4U, TRUE, FALSE, FALSE) == STD_E_OK && core.r[2] == expected_load(k, M(a)), "%s [r1, #-4]", k->name);
			}
			/* Pre-indexed with write-back: [r1, #6]!. */
			init(&core, p);
			core.r[1] = M(0x20U);
			CHECK(ls_imm(k->imm, &core, ARM_COND_AL, 0U, 1U, 6U, TRUE, TRUE, TRUE) == STD_E_OK, "%s pre", k->name);
			CHECK(core.r[0] == expected_load(k, M(0x26U)) && core.r[1] == M(0x26U), "%s pre: %08x r1 %08x", k->name, core.r[0], core.r[1]);
			/* Post-indexed: [r1], #-2: loads at R[n], then R[n] - 2. */
			CHECK(ls_imm(k->imm, &core, ARM_COND_AL, 0U, 1U, 2U, FALSE, FALSE, TRUE) == STD_E_OK, "%s post", k->name);
			CHECK(core.r[0] == expected_load(k, M(0x26U)) && core.r[1] == M(0x24U), "%s post: %08x r1 %08x", k->name, core.r[0], core.r[1]);
			/* Register: [r1, r2, LSL #2] and subtracted. */
			init(&core, p);
			core.r[1] = M(0x30U);
			core.r[2] = 3U;
			CHECK(ls_reg(k->reg, &core, ARM_COND_AL, 0U, 1U, 2U, SRType_LSL, 2U, TRUE, TRUE, FALSE) == STD_E_OK && core.r[0] == expected_load(k, M(0x3CU)), "%s reg", k->name);
			CHECK(ls_reg(k->reg, &core, ARM_COND_AL, 0U, 1U, 2U, SRType_LSL, 0U, TRUE, FALSE, TRUE) == STD_E_OK && core.r[0] == expected_load(k, M(0x2DU)) && core.r[1] == M(0x2DU), "%s reg sub wback", k->name);
			/* Literal: Align(PC,4) +/- imm32 (PC is the instruction's address + 8 or + 4). */
			init(&core, p);
			core.pc = M(0x41U);
			a = ((core.pc + ((p == TestProfile_A_ARM) ? 8U : 4U)) & ~3U);
			CHECK(ls_lit(k->lit, &core, ARM_COND_AL, 3U, 0x11U, TRUE) == STD_E_OK && core.r[3] == expected_load(k, a + 0x11U), "%s lit +", k->name);
			CHECK(ls_lit(k->lit, &core, ARM_COND_AL, 3U, 0x21U, FALSE) == STD_E_OK && core.r[3] == expected_load(k, a - 0x21U), "%s lit -", k->name);
			/* Unprivileged: an offset form that goes through MemU_unpriv[]. */
			init(&core, p);
			core.r[1] = M(0x50U);
			CHECK(ls_imm(k->unpriv, &core, ARM_COND_AL, 0U, 1U, 3U, TRUE, TRUE, FALSE) == STD_E_OK, "%sT", k->name);
			CHECK(core.r[0] == expected_load(k, M(0x53U)) && core.unpriv_accesses == 1U, "%sT %08x", k->name, core.r[0]);
			/* Unprivileged, post-indexed (the ARM form). */
			CHECK(ls_imm(k->unpriv, &core, ARM_COND_AL, 0U, 1U, 3U, FALSE, TRUE, TRUE) == STD_E_OK && core.r[0] == expected_load(k, M(0x50U)) && core.r[1] == M(0x53U), "%sT post", k->name);
			/* A fault: nothing written. */
			core.r[0] = 0x12345678U;
			core.r[1] = 0x10000000U;
			CHECK(ls_imm(k->imm, &core, ARM_COND_AL, 0U, 1U, 4U, TRUE, TRUE, TRUE) == STD_E_SEGV && core.r[0] == 0x12345678U && core.r[1] == 0x10000000U, "%s fault", k->name);
			CHECK(ls_reg(k->reg, &core, ARM_COND_AL, 0U, 1U, 2U, SRType_LSL, 0U, TRUE, TRUE, TRUE) == STD_E_SEGV && core.r[1] == 0x10000000U, "%s reg fault", k->name);
			CHECK(ls_imm(k->unpriv, &core, ARM_COND_AL, 0U, 1U, 4U, TRUE, TRUE, TRUE) == STD_E_SEGV && core.r[0] == 0x12345678U, "%sT fault", k->name);
			core.pc = 0x10000000U;
			CHECK(ls_lit(k->lit, &core, ARM_COND_AL, 0U, 0U, TRUE) == STD_E_SEGV && core.r[0] == 0x12345678U, "%s lit fault", k->name);
		}
	}
	/* LDR (literal) and (register) to PC: LoadWritePC, aligned only. */
	init(&core, TestProfile_M);
	core.mem[0x60] = 0x01U;
	core.mem[0x61] = 0x02U;
	core.mem[0x62] = 0x00U;
	core.mem[0x63] = 0x08U;
	core.pc = M(0x5CU);
	CHECK(ls_lit(arm_pseudo_ldr_lit, &core, ARM_COND_AL, 15U, 0U, TRUE) == STD_E_OK && core.next_pc == 0x08000200U, "LDR pc, literal %08x", core.next_pc);
	core.r[1] = M(0x60U);
	core.r[2] = 0U;
	CHECK(ls_reg(arm_pseudo_ldr_reg, &core, ARM_COND_AL, 15U, 1U, 2U, SRType_LSL, 0U, TRUE, TRUE, FALSE) == STD_E_OK && core.next_pc == 0x08000200U, "LDR pc, reg");
	core.r[2] = 1U;
	CHECK(ls_reg(arm_pseudo_ldr_reg, &core, ARM_COND_AL, 15U, 1U, 2U, SRType_LSL, 0U, TRUE, TRUE, FALSE) == STD_E_INVALID, "LDR pc, reg unaligned");
}

/*
 * Single stores
 */

typedef struct {
	const char *name;
	LoadImm imm;
	LoadReg reg;
	LoadImm unpriv;
	uint32 size;
} StoreKind;

static const StoreKind stores[] = {
	{ "STR", arm_pseudo_str_imm, arm_pseudo_str_reg, arm_pseudo_strt, 4U },
	{ "STRB", arm_pseudo_strb_imm, arm_pseudo_strb_reg, arm_pseudo_strbt, 1U },
	{ "STRH", arm_pseudo_strh_imm, arm_pseudo_strh_reg, arm_pseudo_strht, 2U },
};

/* The bytes at address after a store of value, size bytes; the others keep the pattern. */
static bool stored(const TargetCoreType *core, uint32 address, uint32 size, uint32 value)
{
	uint32 mask = (size == 4U) ? 0xFFFFFFFFU : ((1U << (8U * size)) - 1U);

	return (mem_value(core, address, size) == (value & mask))
		&& (core->mem[address - TEST_MEM_BASE - 1U] == pattern(address - 1U))
		&& (core->mem[address - TEST_MEM_BASE + size] == pattern(address + size));
}

static void test_stores(void)
{
	TargetCoreType core;
	uint32 i;

	for (i = 0U; i < sizeof(stores) / sizeof(stores[0]); i++) {
		const StoreKind *k = &stores[i];

		/* Offset, unaligned address. */
		init(&core, TestProfile_M);
		core.r[0] = 0xA1B2C3D4U;
		core.r[1] = M(0x10U);
		CHECK(ls_imm(k->imm, &core, ARM_COND_AL, 0U, 1U, 5U, TRUE, TRUE, FALSE) == STD_E_OK && stored(&core, M(0x15U), k->size, 0xA1B2C3D4U) && core.r[1] == M(0x10U), "%s imm", k->name);
		/* Pre-indexed, subtracted, write-back. */
		init(&core, TestProfile_M);
		core.r[0] = 0x0BADF00DU;
		core.r[1] = M(0x30U);
		CHECK(ls_imm(k->imm, &core, ARM_COND_AL, 0U, 1U, 8U, TRUE, FALSE, TRUE) == STD_E_OK && stored(&core, M(0x28U), k->size, 0x0BADF00DU) && core.r[1] == M(0x28U), "%s pre", k->name);
		/* Post-indexed. */
		CHECK(ls_imm(k->imm, &core, ARM_COND_AL, 0U, 1U, 0x10U, FALSE, TRUE, TRUE) == STD_E_OK && core.r[1] == M(0x38U), "%s post", k->name);
		CHECK(mem_value(&core, M(0x28U), k->size) == (0x0BADF00DU & ((k->size == 4U) ? 0xFFFFFFFFU : ((1U << (8U * k->size)) - 1U))), "%s post value", k->name);
		/* Register, [r1, r2, LSL #1]. */
		init(&core, TestProfile_M);
		core.r[0] = 0x55AA33CCU;
		core.r[1] = M(0x40U);
		core.r[2] = 6U;
		CHECK(ls_reg(k->reg, &core, ARM_COND_AL, 0U, 1U, 2U, SRType_LSL, 1U, TRUE, TRUE, FALSE) == STD_E_OK && stored(&core, M(0x4CU), k->size, 0x55AA33CCU), "%s reg", k->name);
		CHECK(ls_reg(k->reg, &core, ARM_COND_AL, 0U, 1U, 2U, SRType_LSL, 0U, FALSE, FALSE, TRUE) == STD_E_OK && core.r[1] == M(0x3AU), "%s reg post", k->name);
		/* Unprivileged. */
		init(&core, TestProfile_M);
		core.r[0] = 0x13579BDFU;
		core.r[1] = M(0x60U);
		CHECK(ls_imm(k->unpriv, &core, ARM_COND_AL, 0U, 1U, 2U, TRUE, TRUE, FALSE) == STD_E_OK && stored(&core, M(0x62U), k->size, 0x13579BDFU) && core.unpriv_accesses == 1U, "%sT", k->name);
		CHECK(ls_imm(k->unpriv, &core, ARM_COND_AL, 0U, 1U, 2U, FALSE, FALSE, TRUE) == STD_E_OK && core.r[1] == M(0x5EU), "%sT post", k->name);
		/* A fault: no write-back. */
		core.r[1] = 0x10000000U;
		CHECK(ls_imm(k->imm, &core, ARM_COND_AL, 0U, 1U, 4U, TRUE, TRUE, TRUE) == STD_E_SEGV && core.r[1] == 0x10000000U, "%s fault", k->name);
		CHECK(ls_reg(k->reg, &core, ARM_COND_AL, 0U, 1U, 2U, SRType_LSL, 0U, TRUE, TRUE, TRUE) == STD_E_SEGV && core.r[1] == 0x10000000U, "%s reg fault", k->name);
		CHECK(ls_imm(k->unpriv, &core, ARM_COND_AL, 0U, 1U, 4U, TRUE, TRUE, TRUE) == STD_E_SEGV && core.r[1] == 0x10000000U, "%sT fault", k->name);
	}
	/* STR of PC stores the value read from PC (address + 8 in ARM state). */
	init(&core, TestProfile_A_ARM);
	core.r[1] = M(0x70U);
	CHECK(ls_imm(arm_pseudo_str_imm, &core, ARM_COND_AL, 15U, 1U, 0U, TRUE, TRUE, FALSE) == STD_E_OK && mem_value(&core, M(0x70U), 4U) == PC0 + 8U, "STR pc");
}

/*
 * Dual and multiple
 */

static Std_ReturnType dual(Std_ReturnType (*f)(TargetCoreType *, const arm_ls_dual_input_type *, arm_ls_output_type *),
	TargetCoreType *core, uint32 cond, uint32 t, uint32 t2, uint32 n, uint32 imm32, bool index, bool add, bool wback)
{
	arm_ls_dual_input_type in;
	arm_ls_output_type out;

	in.instrName = "LSD";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rt, t);
	ARM_SET_REG(core, &in, Rt2, t2);
	ARM_SET_REG(core, &in, Rn, n);
	in.imm32 = imm32;
	in.index = index;
	in.add = add;
	in.wback = wback;
	return f(core, &in, &out);
}

static void test_dual(void)
{
	TargetCoreType core;

	init(&core, TestProfile_M);
	core.r[4] = M(0x20U);
	CHECK(dual(arm_pseudo_ldrd_imm, &core, ARM_COND_AL, 0U, 1U, 4U, 8U, TRUE, TRUE, TRUE) == STD_E_OK, "LDRD pre");
	CHECK(core.r[0] == pattern_value(M(0x28U), 4U) && core.r[1] == pattern_value(M(0x2CU), 4U) && core.r[4] == M(0x28U), "LDRD pre values");
	CHECK(dual(arm_pseudo_ldrd_imm, &core, ARM_COND_AL, 2U, 3U, 4U, 8U, FALSE, FALSE, TRUE) == STD_E_OK && core.r[2] == pattern_value(M(0x28U), 4U) && core.r[4] == M(0x20U), "LDRD post");
	/* MemA: an unaligned address is an alignment fault, nothing written. */
	core.r[4] = M(0x22U);
	core.r[0] = 7U;
	CHECK(dual(arm_pseudo_ldrd_imm, &core, ARM_COND_AL, 0U, 1U, 4U, 0U, TRUE, TRUE, TRUE) == STD_E_SEGV && core.r[0] == 7U, "LDRD unaligned");
	core.r[4] = M(0xFCU);
	CHECK(dual(arm_pseudo_ldrd_imm, &core, ARM_COND_AL, 0U, 1U, 4U, 0U, TRUE, TRUE, TRUE) == STD_E_SEGV && core.r[0] == 7U, "LDRD second word fault");
	/* LDRD (literal): Align(PC,4) - imm32 (Thumb: PC = address + 4). */
	core.pc = M(0x62U);
	CHECK(dual(arm_pseudo_ldrd_lit, &core, ARM_COND_AL, 5U, 6U, 15U, 0x18U, TRUE, FALSE, FALSE) == STD_E_OK, "LDRD lit");
	CHECK(core.r[5] == pattern_value(M(0x4CU), 4U) && core.r[6] == pattern_value(M(0x50U), 4U), "LDRD lit values");
	core.pc = M(0x63U);
	CHECK(dual(arm_pseudo_ldrd_lit, &core, ARM_COND_AL, 5U, 6U, 15U, 0x200U, TRUE, TRUE, FALSE) == STD_E_SEGV, "LDRD lit fault");
	core.pc = M(0xF4U);
	CHECK(dual(arm_pseudo_ldrd_lit, &core, ARM_COND_AL, 5U, 6U, 15U, 4U, TRUE, TRUE, FALSE) == STD_E_SEGV, "LDRD lit second word fault");
	/* STRD, offset form with subtraction. */
	init(&core, TestProfile_M);
	core.r[0] = 0x11223344U;
	core.r[1] = 0x55667788U;
	core.r[4] = M(0x40U);
	CHECK(dual(arm_pseudo_strd_imm, &core, ARM_COND_AL, 0U, 1U, 4U, 0x10U, TRUE, FALSE, FALSE) == STD_E_OK, "STRD");
	CHECK(mem_value(&core, M(0x30U), 4U) == 0x11223344U && mem_value(&core, M(0x34U), 4U) == 0x55667788U && core.r[4] == M(0x40U), "STRD values");
	CHECK(dual(arm_pseudo_strd_imm, &core, ARM_COND_AL, 0U, 1U, 4U, 0x8U, FALSE, TRUE, TRUE) == STD_E_OK && core.r[4] == M(0x48U), "STRD post");
	core.r[4] = M(0x31U);
	CHECK(dual(arm_pseudo_strd_imm, &core, ARM_COND_AL, 0U, 1U, 4U, 0U, TRUE, TRUE, TRUE) == STD_E_SEGV && core.r[4] == M(0x31U), "STRD unaligned");
	core.r[4] = M(0xFCU);
	CHECK(dual(arm_pseudo_strd_imm, &core, ARM_COND_AL, 0U, 1U, 4U, 0U, TRUE, TRUE, TRUE) == STD_E_SEGV && core.r[4] == M(0xFCU), "STRD second word fault");
}

static Std_ReturnType multiple(Std_ReturnType (*f)(TargetCoreType *, const arm_ls_multiple_input_type *, arm_ls_output_type *),
	TargetCoreType *core, uint32 cond, uint32 n, uint32 registers, bool wback)
{
	arm_ls_multiple_input_type in;
	arm_ls_output_type out;

	in.instrName = "LSM";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rn, n);
	in.registers = registers;
	in.wback = wback;
	return f(core, &in, &out);
}

static void test_multiple(void)
{
	TargetCoreType core;
	uint32 i;

	/* LDM r8!, {r0, r2, r5}: three words from R[n], R[n] + 12. */
	init(&core, TestProfile_M);
	core.r[8] = M(0x10U);
	CHECK(multiple(arm_pseudo_ldm, &core, ARM_COND_AL, 8U, 0x0025U, TRUE) == STD_E_OK, "LDM");
	CHECK(core.r[0] == pattern_value(M(0x10U), 4U) && core.r[2] == pattern_value(M(0x14U), 4U) && core.r[5] == pattern_value(M(0x18U), 4U) && core.r[8] == M(0x1CU), "LDM values");
	/* LDMDB r8!, {r1, r3}: from R[n] - 8. */
	CHECK(multiple(arm_pseudo_ldmdb, &core, ARM_COND_AL, 8U, 0x000AU, TRUE) == STD_E_OK, "LDMDB");
	CHECK(core.r[1] == pattern_value(M(0x14U), 4U) && core.r[3] == pattern_value(M(0x18U), 4U) && core.r[8] == M(0x14U), "LDMDB values");
	/* LDM r8, {r6} without write-back: R[n] unchanged. */
	CHECK(multiple(arm_pseudo_ldm, &core, ARM_COND_AL, 8U, 0x0040U, FALSE) == STD_E_OK && core.r[6] == pattern_value(M(0x14U), 4U) && core.r[8] == M(0x14U), "LDM no wback");
	/* LDM with R[n] in the list: R[n] is the loaded value, no write-back. */
	core.r[8] = M(0x20U);
	CHECK(multiple(arm_pseudo_ldm, &core, ARM_COND_AL, 8U, 0x0101U, TRUE) == STD_E_OK && core.r[8] == pattern_value(M(0x24U), 4U), "LDM Rn in list");
	/* POP {r4, pc} is LDM SP!, {r4, pc}: LoadWritePC after the write-back. */
	init(&core, TestProfile_M);
	core.mem[0x34] = 0x81U;
	core.mem[0x35] = 0x03U;
	core.mem[0x36] = 0x00U;
	core.mem[0x37] = 0x08U;
	core.r[13] = M(0x30U);
	CHECK(multiple(arm_pseudo_ldm, &core, ARM_COND_AL, 13U, 0x8010U, TRUE) == STD_E_OK, "POP pc");
	CHECK(core.r[4] == pattern_value(M(0x30U), 4U) && core.r[13] == M(0x38U) && core.next_pc == 0x08000380U, "POP pc values %08x", core.next_pc);
	/* A fault in the middle of the list: nothing written. */
	core.r[13] = M(0xF8U);
	core.r[4] = 0x44U;
	CHECK(multiple(arm_pseudo_ldm, &core, ARM_COND_AL, 13U, 0x0070U, TRUE) == STD_E_SEGV && core.r[4] == 0x44U && core.r[13] == M(0xF8U), "LDM fault");
	/* PUSH {r0, r1, lr} is STMDB SP!, {r0, r1, lr}. */
	init(&core, TestProfile_M);
	core.r[0] = 0x10101010U;
	core.r[1] = 0x20202020U;
	core.r[14] = 0x0800FFFFU;
	core.r[13] = M(0x80U);
	CHECK(multiple(arm_pseudo_stmdb, &core, ARM_COND_AL, 13U, 0x4003U, TRUE) == STD_E_OK, "PUSH");
	CHECK(mem_value(&core, M(0x74U), 4U) == 0x10101010U && mem_value(&core, M(0x78U), 4U) == 0x20202020U && mem_value(&core, M(0x7CU), 4U) == 0x0800FFFFU && core.r[13] == M(0x74U), "PUSH values");
	/* STM r2, {r0, r1} without write-back. */
	core.r[2] = M(0x90U);
	CHECK(multiple(arm_pseudo_stm, &core, ARM_COND_AL, 2U, 0x0003U, FALSE) == STD_E_OK && mem_value(&core, M(0x94U), 4U) == 0x20202020U && core.r[2] == M(0x90U), "STM");
	CHECK(multiple(arm_pseudo_stm, &core, ARM_COND_AL, 2U, 0x0003U, TRUE) == STD_E_OK && core.r[2] == M(0x98U), "STM wback");
	/* STM in ARM state with PC in the list stores PC + 8. */
	init(&core, TestProfile_A_ARM);
	core.r[2] = M(0xA0U);
	CHECK(multiple(arm_pseudo_stm, &core, ARM_COND_AL, 2U, 0x8000U, FALSE) == STD_E_OK && mem_value(&core, M(0xA0U), 4U) == PC0 + 8U, "STM pc");
	/* Store faults: no write-back. */
	core.r[2] = M(0xFCU);
	CHECK(multiple(arm_pseudo_stm, &core, ARM_COND_AL, 2U, 0x0003U, TRUE) == STD_E_SEGV && core.r[2] == M(0xFCU), "STM fault");
	core.r[2] = M(0x02U);
	CHECK(multiple(arm_pseudo_stmdb, &core, ARM_COND_AL, 2U, 0x0003U, TRUE) == STD_E_SEGV && core.r[2] == M(0x02U), "STMDB fault");
	/* Failing condition. */
	init(&core, TestProfile_M);
	core.apsr = ARM_APSR_Z;
	core.r[2] = M(0x10U);
	for (i = 0U; i < 4U; i++) {
		static Std_ReturnType (*const ops[4])(TargetCoreType *, const arm_ls_multiple_input_type *, arm_ls_output_type *) = {
			arm_pseudo_ldm, arm_pseudo_ldmdb, arm_pseudo_stm, arm_pseudo_stmdb,
		};
		CHECK(multiple(ops[i], &core, 0x1U, 2U, 0x0003U, TRUE) == STD_E_OK && core.r[2] == M(0x10U) && core.r[0] == 0U, "multiple %u skipped", i);
	}
}

/*
 * Exclusive and table branch
 */

static Std_ReturnType exclusive(Std_ReturnType (*f)(TargetCoreType *, const arm_ls_exclusive_input_type *, arm_ls_output_type *),
	TargetCoreType *core, uint32 cond, uint32 d, uint32 t, uint32 n, uint32 imm32)
{
	arm_ls_exclusive_input_type in;
	arm_ls_output_type out;

	in.instrName = "EX";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rd, d);
	ARM_SET_REG(core, &in, Rt, t);
	ARM_SET_REG(core, &in, Rn, n);
	in.imm32 = imm32;
	return f(core, &in, &out);
}

static void test_exclusive(void)
{
	TargetCoreType core;

	init(&core, TestProfile_M);
	core.r[1] = M(0x40U);
	/* LDREX, then STREX to the same address passes once (status 0), then fails (1). */
	CHECK(exclusive(arm_pseudo_ldrex, &core, ARM_COND_AL, 0U, 2U, 1U, 8U) == STD_E_OK && core.r[2] == pattern_value(M(0x48U), 4U), "LDREX");
	core.r[3] = 0xCAFEBABEU;
	CHECK(exclusive(arm_pseudo_strex, &core, ARM_COND_AL, 4U, 3U, 1U, 8U) == STD_E_OK && core.r[4] == 0U && mem_value(&core, M(0x48U), 4U) == 0xCAFEBABEU, "STREX pass");
	core.r[3] = 0x11111111U;
	CHECK(exclusive(arm_pseudo_strex, &core, ARM_COND_AL, 4U, 3U, 1U, 8U) == STD_E_OK && core.r[4] == 1U && mem_value(&core, M(0x48U), 4U) == 0xCAFEBABEU, "STREX fail");
	/* CLREX clears the monitor. */
	CHECK(exclusive(arm_pseudo_ldrexb, &core, ARM_COND_AL, 0U, 2U, 1U, 1U) == STD_E_OK && core.r[2] == pattern(M(0x41U)), "LDREXB");
	core.apsr = ARM_APSR_Z;
	CHECK(arm_pseudo_clrex(&core, 0x1U) == STD_E_OK && core.exclusive_valid, "CLREXNE with Z set: skipped");
	CHECK(arm_pseudo_clrex(&core, ARM_COND_AL) == STD_E_OK && !core.exclusive_valid, "CLREX");
	core.apsr = 0U;
	CHECK(exclusive(arm_pseudo_strexb, &core, ARM_COND_AL, 4U, 3U, 1U, 1U) == STD_E_OK && core.r[4] == 1U, "STREXB after CLREX fails");
	/* Byte and halfword pairs. */
	CHECK(exclusive(arm_pseudo_ldrexh, &core, ARM_COND_AL, 0U, 2U, 1U, 2U) == STD_E_OK && core.r[2] == pattern_value(M(0x42U), 2U), "LDREXH");
	core.r[3] = 0xFFFF1234U;
	CHECK(exclusive(arm_pseudo_strexh, &core, ARM_COND_AL, 4U, 3U, 1U, 2U) == STD_E_OK && core.r[4] == 0U && mem_value(&core, M(0x42U), 2U) == 0x1234U && core.mem[0x44] == pattern(M(0x44U)), "STREXH");
	CHECK(exclusive(arm_pseudo_ldrexb, &core, ARM_COND_AL, 0U, 2U, 1U, 5U) == STD_E_OK, "LDREXB 2");
	core.r[3] = 0x99U;
	CHECK(exclusive(arm_pseudo_strexb, &core, ARM_COND_AL, 4U, 3U, 1U, 5U) == STD_E_OK && core.r[4] == 0U && core.mem[0x45] == 0x99U, "STREXB");
	/* STREX to another address than the monitor's fails. */
	CHECK(exclusive(arm_pseudo_ldrex, &core, ARM_COND_AL, 0U, 2U, 1U, 0U) == STD_E_OK, "LDREX 2");
	CHECK(exclusive(arm_pseudo_strex, &core, ARM_COND_AL, 4U, 3U, 1U, 4U) == STD_E_OK && core.r[4] == 1U, "STREX other address");
	/* Alignment faults (MemA, ExclusiveMonitorsPass). */
	CHECK(exclusive(arm_pseudo_ldrex, &core, ARM_COND_AL, 0U, 2U, 1U, 2U) == STD_E_SEGV, "LDREX unaligned");
	CHECK(exclusive(arm_pseudo_strex, &core, ARM_COND_AL, 4U, 3U, 1U, 2U) == STD_E_SEGV, "STREX unaligned");
	/* STREX that passes but faults on the store. */
	core.r[1] = M(0xFEU);
	CHECK(exclusive(arm_pseudo_ldrexb, &core, ARM_COND_AL, 0U, 2U, 1U, 0U) == STD_E_OK, "LDREXB at the end");
	core.exclusive_address = M(0x100U);
	core.r[1] = M(0x100U);
	CHECK(exclusive(arm_pseudo_strex, &core, ARM_COND_AL, 4U, 3U, 1U, 0U) == STD_E_SEGV, "STREX store fault");
	/* Failing condition. */
	core.apsr = ARM_APSR_Z;
	core.r[2] = 0x77U;
	CHECK(exclusive(arm_pseudo_ldrex, &core, 0x1U, 0U, 2U, 1U, 0U) == STD_E_OK && core.r[2] == 0x77U, "LDREXNE skipped");
	CHECK(exclusive(arm_pseudo_strex, &core, 0x1U, 4U, 3U, 1U, 0U) == STD_E_OK && core.r[4] == 1U, "STREXNE skipped");
}

static Std_ReturnType tb(Std_ReturnType (*f)(TargetCoreType *, const arm_tb_input_type *, arm_b_output_type *),
	TargetCoreType *core, uint32 cond, uint32 n, uint32 m)
{
	arm_tb_input_type in;
	arm_b_output_type out;

	in.instrName = "TB";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rn, n);
	ARM_SET_REG(core, &in, Rm, m);
	return f(core, &in, &out);
}

static void test_table_branch(void)
{
	TargetCoreType core;

	init(&core, TestProfile_M);
	core.r[1] = M(0x10U);
	core.r[2] = 3U;
	/* TBB [r1, r2]: target = PC + 2 * byte at R[n] + R[m]. */
	CHECK(tb(arm_pseudo_tbb, &core, ARM_COND_AL, 1U, 2U) == STD_E_OK && core.next_pc == PC0 + 4U + 2U * pattern(M(0x13U)), "TBB %08x", core.next_pc);
	/* TBH [r1, r2, LSL #1]: halfword at R[n] + 2 * R[m]. */
	CHECK(tb(arm_pseudo_tbh, &core, ARM_COND_AL, 1U, 2U) == STD_E_OK && core.next_pc == PC0 + 4U + 2U * pattern_value(M(0x16U), 2U), "TBH %08x", core.next_pc);
	core.r[2] = 0x1000U;
	CHECK(tb(arm_pseudo_tbb, &core, ARM_COND_AL, 1U, 2U) == STD_E_SEGV, "TBB fault");
	core.apsr = ARM_APSR_Z;
	core.next_pc = 0U;
	CHECK(tb(arm_pseudo_tbh, &core, 0x1U, 1U, 2U) == STD_E_OK && core.next_pc == 0U, "TBH skipped");
}

static Std_ReturnType bl(TargetCoreType *core, uint32 cond, sint32 offset)
{
	arm_b_input_type in;
	arm_b_output_type out;

	in.instrName = "BL";
	in.cond = cond;
	in.imm32 = (uint32)offset;
	return arm_pseudo_bl(core, &in, &out);
}

static Std_ReturnType blx(TargetCoreType *core, uint32 cond, uint32 m)
{
	arm_blx_reg_input_type in;
	arm_b_output_type out;

	in.instrName = "BLX";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rm, m);
	return arm_pseudo_blx_reg(core, &in, &out);
}

static Std_ReturnType bx(TargetCoreType *core, uint32 cond, uint32 m)
{
	arm_blx_reg_input_type in;
	arm_b_output_type out;

	in.instrName = "BX";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rm, m);
	return arm_pseudo_bx(core, &in, &out);
}

static Std_ReturnType cb(TargetCoreType *core, bool nonzero, uint32 n, uint32 imm32)
{
	arm_cb_input_type in;
	arm_b_output_type out;

	in.instrName = nonzero ? "CBNZ" : "CBZ";
	ARM_SET_REG(core, &in, Rn, n);
	in.imm32 = imm32;
	in.nonzero = nonzero;
	return arm_pseudo_cb(core, &in, &out);
}

static void test_branch_link(void)
{
	TargetCoreType core;

	/* BL, Thumb (a 32-bit encoding): LR = address + 4 with bit 0 set, target = address + 4 + imm32. */
	init(&core, TestProfile_M);
	CHECK(bl(&core, ARM_COND_AL, -0x104) == STD_E_OK && core.r[14] == PC0 + 5U && core.next_pc == PC0 - 0x100U,
	      "BL Thumb LR %08x PC %08x", core.r[14], core.next_pc);
	/* BL, ARM: LR = address + 4, target = address + 8 + imm32. */
	init(&core, TestProfile_A_ARM);
	CHECK(bl(&core, ARM_COND_AL, 0x400) == STD_E_OK && core.r[14] == PC0 + 4U && core.next_pc == PC0 + 0x408U,
	      "BL ARM LR %08x PC %08x", core.r[14], core.next_pc);
	/* BLNE with Z set: neither LR nor PC changes. */
	init(&core, TestProfile_A_ARM);
	core.apsr = ARM_APSR_Z;
	core.r[14] = 0x5555U;
	CHECK(bl(&core, 0x1U, 0x400) == STD_E_OK && core.r[14] == 0x5555U && core.next_pc == PC0 + 4U, "BLNE skipped");

	/* BLX r3, Thumb (16-bit): LR = address + 2 with bit 0 set; LR as Rm is read before it is written. */
	init(&core, TestProfile_M);
	core.r[3] = 0x08001235U;
	CHECK(blx(&core, ARM_COND_AL, 3U) == STD_E_OK && core.r[14] == PC0 + 3U && core.next_pc == 0x08001234U,
	      "BLX Thumb LR %08x PC %08x", core.r[14], core.next_pc);
	init(&core, TestProfile_M);
	core.r[14] = 0x08002001U;
	CHECK(blx(&core, ARM_COND_AL, 14U) == STD_E_OK && core.r[14] == PC0 + 3U && core.next_pc == 0x08002000U, "BLX lr");
	/* M profile: a target with bit 0 clear is INVSTATE, after LR is written. */
	init(&core, TestProfile_M);
	core.r[3] = 0x08001234U;
	CHECK(blx(&core, ARM_COND_AL, 3U) == STD_E_EXEC && core.r[14] == PC0 + 3U, "BLX Thumb INVSTATE");
	/* BLX r3, ARM: LR = address + 4; bit 0 of the target selects Thumb. */
	init(&core, TestProfile_A_ARM);
	core.r[3] = 0x08001235U;
	CHECK(blx(&core, ARM_COND_AL, 3U) == STD_E_OK && core.r[14] == PC0 + 4U && core.next_pc == 0x08001234U && core.thumb,
	      "BLX ARM LR %08x PC %08x", core.r[14], core.next_pc);
	init(&core, TestProfile_A_ARM);
	core.apsr = ARM_APSR_Z;
	core.r[14] = 0x5555U;
	CHECK(blx(&core, 0x1U, 3U) == STD_E_OK && core.r[14] == 0x5555U && core.next_pc == PC0 + 4U, "BLXNE skipped");

	/* BX r3: BXWritePC(R[m]); LR unchanged. */
	init(&core, TestProfile_M);
	core.r[3] = 0x08001235U;
	core.r[14] = 0x5555U;
	CHECK(bx(&core, ARM_COND_AL, 3U) == STD_E_OK && core.next_pc == 0x08001234U && core.r[14] == 0x5555U, "BX");
	init(&core, TestProfile_M);
	core.r[3] = 0x08001234U;
	CHECK(bx(&core, ARM_COND_AL, 3U) == STD_E_EXEC, "BX INVSTATE");
	init(&core, TestProfile_M);
	core.apsr = ARM_APSR_Z;
	CHECK(bx(&core, 0x1U, 3U) == STD_E_OK && core.next_pc == PC0 + 2U, "BXNE skipped");

	/* CBZ / CBNZ: target = address + 4 + imm32 when R[n] is (not) zero. */
	init(&core, TestProfile_M);
	core.r[2] = 0U;
	CHECK(cb(&core, FALSE, 2U, 0x7EU) == STD_E_OK && core.next_pc == PC0 + 0x82U, "CBZ taken %08x", core.next_pc);
	init(&core, TestProfile_M);
	CHECK(cb(&core, TRUE, 2U, 0x7EU) == STD_E_OK && core.next_pc == PC0 + 2U, "CBNZ not taken");
	init(&core, TestProfile_M);
	core.r[2] = 0x80000000U;
	CHECK(cb(&core, FALSE, 2U, 0x10U) == STD_E_OK && core.next_pc == PC0 + 2U, "CBZ not taken");
	init(&core, TestProfile_M);
	core.r[2] = 0x80000000U;
	CHECK(cb(&core, TRUE, 2U, 0x10U) == STD_E_OK && core.next_pc == PC0 + 0x14U, "CBNZ taken");
}

/* Every load and store with a failing condition (NE with Z set) changes nothing. */
static void test_condition_failed(void)
{
	TargetCoreType core;
	TargetCoreType before;
	uint32 i;

	init(&core, TestProfile_M);
	core.r[1] = M(0x10U);
	core.apsr = ARM_APSR_Z;
	before = core;
	for (i = 0U; i < sizeof(loads) / sizeof(loads[0]); i++) {
		CHECK(ls_imm(loads[i].imm, &core, 0x1U, 0U, 1U, 4U, TRUE, TRUE, TRUE) == STD_E_OK, "skip");
		CHECK(ls_reg(loads[i].reg, &core, 0x1U, 0U, 1U, 2U, SRType_LSL, 0U, TRUE, TRUE, TRUE) == STD_E_OK, "skip");
		CHECK(ls_lit(loads[i].lit, &core, 0x1U, 0U, 0U, TRUE) == STD_E_OK, "skip");
		CHECK(ls_imm(loads[i].unpriv, &core, 0x1U, 0U, 1U, 4U, TRUE, TRUE, TRUE) == STD_E_OK, "skip");
	}
	for (i = 0U; i < sizeof(stores) / sizeof(stores[0]); i++) {
		CHECK(ls_imm(stores[i].imm, &core, 0x1U, 0U, 1U, 4U, TRUE, TRUE, TRUE) == STD_E_OK, "skip");
		CHECK(ls_reg(stores[i].reg, &core, 0x1U, 0U, 1U, 2U, SRType_LSL, 0U, TRUE, TRUE, TRUE) == STD_E_OK, "skip");
		CHECK(ls_imm(stores[i].unpriv, &core, 0x1U, 0U, 1U, 4U, TRUE, TRUE, TRUE) == STD_E_OK, "skip");
	}
	CHECK(dual(arm_pseudo_ldrd_imm, &core, 0x1U, 2U, 3U, 1U, 0U, TRUE, TRUE, TRUE) == STD_E_OK, "skip");
	CHECK(dual(arm_pseudo_ldrd_lit, &core, 0x1U, 2U, 3U, 15U, 0U, TRUE, TRUE, FALSE) == STD_E_OK, "skip");
	CHECK(dual(arm_pseudo_strd_imm, &core, 0x1U, 2U, 3U, 1U, 0U, TRUE, TRUE, TRUE) == STD_E_OK, "skip");
	CHECK(memcmp(&core, &before, sizeof(core)) == 0, "a load or store with a failing condition changed the core");
}

int main(void)
{
	test_loads();
	test_stores();
	test_dual();
	test_multiple();
	test_exclusive();
	test_table_branch();
	test_branch_link();
	test_condition_failed();
	printf("%d checks, %d failures\n", checks, failures);
	return (failures == 0) ? 0 : 1;
}
