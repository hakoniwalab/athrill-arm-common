/*
 * Tests of the pseudo-code functions through the test profile, in the A
 * profile's ARM state and in the M profile. Expected values are worked out
 * by hand from the pseudo-code.
 */
#include <stdio.h>
#include "arm_common/arm_pseudo_arith.h"
#include "arm_common/arm_pseudo_logic.h"
#include "arm_common/arm_pseudo_move.h"
#include "arm_common/arm_pseudo_load.h"
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

static Std_ReturnType add_imm(TargetCoreType *core, uint32 cond, bool setflags, uint32 d, uint32 n, uint32 imm32)
{
	arm_dp_imm_input_type in;
	arm_dp_output_type out;

	in.instrName = "ADD";
	in.cond = cond;
	in.setflags = setflags;
	ARM_SET_REG(core, &in, Rd, d);
	ARM_SET_REG(core, &in, Rn, n);
	in.imm32 = imm32;
	in.carry = FALSE;
	return arm_pseudo_add_imm(core, &in, &out);
}

static void test_add_imm(void)
{
	TargetCoreType core;
	TestProfileType p;

	for (p = TestProfile_A_ARM; p <= TestProfile_M; p++) {
		/* ADDS r0, r1, #1 with r1 = 0xFFFFFFFF: 0, Z and C set. */
		test_core_init(&core, p, PC0);
		core.r[1] = 0xFFFFFFFFU;
		core.apsr = ARM_APSR_N | ARM_APSR_V | ARM_APSR_Q;
		CHECK(add_imm(&core, ARM_COND_AL, TRUE, 0U, 1U, 1U) == STD_E_OK, "ADDS");
		CHECK(core.r[0] == 0U, "ADDS result %08x", core.r[0]);
		CHECK(core.apsr == (ARM_APSR_Z | ARM_APSR_C | ARM_APSR_Q), "ADDS flags %08x", core.apsr);
		CHECK(core.next_pc == PC0 + ((p == TestProfile_A_ARM) ? 4U : 2U), "ADDS does not branch");

		/* ADDS r2, r2, #1 with r2 = 0x7FFFFFFF: N and V set. */
		core.r[2] = 0x7FFFFFFFU;
		CHECK(add_imm(&core, ARM_COND_AL, TRUE, 2U, 2U, 1U) == STD_E_OK, "ADDS overflow");
		CHECK(core.r[2] == 0x80000000U && core.apsr == (ARM_APSR_N | ARM_APSR_V | ARM_APSR_Q), "ADDS overflow flags %08x", core.apsr);

		/* ADD (no S) leaves the flags. */
		core.r[3] = 5U;
		CHECK(add_imm(&core, ARM_COND_AL, FALSE, 4U, 3U, 0xFFFFFFFFU) == STD_E_OK, "ADD");
		CHECK(core.r[4] == 4U && core.apsr == (ARM_APSR_N | ARM_APSR_V | ARM_APSR_Q), "ADD keeps flags");

		/* ADDEQ with Z clear: nothing happens. */
		core.r[5] = 0x1234U;
		CHECK(add_imm(&core, 0x0U, TRUE, 5U, 3U, 1U) == STD_E_OK && core.r[5] == 0x1234U, "ADDEQ not taken");
		/* ADDMI with N set: taken. */
		CHECK(add_imm(&core, 0x4U, FALSE, 5U, 3U, 1U) == STD_E_OK && core.r[5] == 6U, "ADDMI taken");
	}
	/* ADD pc, r1, #imm (ARM encoding only): ALUWritePC, interworking in ARM state. */
	test_core_init(&core, TestProfile_A_ARM, PC0);
	core.r[1] = 0x08000200U;
	CHECK(add_imm(&core, ARM_COND_AL, FALSE, 15U, 1U, 1U) == STD_E_OK, "ADD pc");
	CHECK(core.next_pc == 0x08000200U && core.thumb == TRUE, "ADD pc to Thumb %08x", core.next_pc);
	/* ADD r0, pc, #4 reads PC: the instruction's address + 8 (ARM), + 4 (Thumb). */
	test_core_init(&core, TestProfile_A_ARM, PC0);
	CHECK(add_imm(&core, ARM_COND_AL, FALSE, 0U, 15U, 4U) == STD_E_OK && core.r[0] == PC0 + 12U, "ADD r0, pc ARM");
	test_core_init(&core, TestProfile_M, PC0);
	CHECK(add_imm(&core, ARM_COND_AL, FALSE, 0U, 15U, 4U) == STD_E_OK && core.r[0] == PC0 + 8U, "ADD r0, pc Thumb");
}

static Std_ReturnType ldr_imm(TargetCoreType *core, uint32 t, uint32 n, uint32 imm32, bool index, bool add, bool wback)
{
	arm_ls_imm_input_type in;
	arm_ls_output_type out;

	in.instrName = "LDR";
	in.cond = ARM_COND_AL;
	ARM_SET_REG(core, &in, Rt, t);
	ARM_SET_REG(core, &in, Rn, n);
	in.imm32 = imm32;
	in.index = index;
	in.add = add;
	in.wback = wback;
	return arm_pseudo_ldr_imm(core, &in, &out);
}

static void put32(TargetCoreType *core, uint32 address, uint32 value)
{
	(void)ArmMemU_W(core, address, 4U, value);
}

static void test_ldr_imm(void)
{
	TargetCoreType core;
	TestProfileType p;

	for (p = TestProfile_A_ARM; p <= TestProfile_M; p++) {
		test_core_init(&core, p, PC0);
		put32(&core, TEST_MEM_BASE + 0x10U, 0x11111111U);
		put32(&core, TEST_MEM_BASE + 0x14U, 0x22222222U);
		put32(&core, TEST_MEM_BASE + 0x20U, 0x08000301U);
		put32(&core, TEST_MEM_BASE + 0x24U, 0x08000300U);

		/* LDR r0, [r1, #4]: offset, no writeback. */
		core.r[1] = TEST_MEM_BASE + 0x10U;
		CHECK(ldr_imm(&core, 0U, 1U, 4U, TRUE, TRUE, FALSE) == STD_E_OK, "LDR offset");
		CHECK(core.r[0] == 0x22222222U && core.r[1] == TEST_MEM_BASE + 0x10U, "LDR offset %08x", core.r[0]);

		/* LDR r0, [r1, #-4]!: pre-indexed, subtract, writeback. */
		core.r[1] = TEST_MEM_BASE + 0x14U;
		CHECK(ldr_imm(&core, 0U, 1U, 4U, TRUE, FALSE, TRUE) == STD_E_OK, "LDR pre");
		CHECK(core.r[0] == 0x11111111U && core.r[1] == TEST_MEM_BASE + 0x10U, "LDR pre %08x %08x", core.r[0], core.r[1]);

		/* LDR r0, [r1], #4: post-indexed: loads from R[n], then adds. */
		CHECK(ldr_imm(&core, 0U, 1U, 4U, FALSE, TRUE, TRUE) == STD_E_OK, "LDR post");
		CHECK(core.r[0] == 0x11111111U && core.r[1] == TEST_MEM_BASE + 0x14U, "LDR post %08x %08x", core.r[0], core.r[1]);

		/* Unaligned LDR (UnalignedSupport()): bytes 0x12..0x15, little-endian. */
		core.r[1] = TEST_MEM_BASE + 0x12U;
		CHECK(ldr_imm(&core, 0U, 1U, 0U, TRUE, TRUE, FALSE) == STD_E_OK && core.r[0] == 0x22221111U, "LDR unaligned %08x", core.r[0]);

		/* A memory fault stops the instruction: no writeback. */
		core.r[1] = 0x10000000U;
		core.r[0] = 0xAAAAAAAAU;
		CHECK(ldr_imm(&core, 0U, 1U, 4U, TRUE, TRUE, TRUE) == STD_E_SEGV, "LDR fault");
		CHECK(core.r[0] == 0xAAAAAAAAU && core.r[1] == 0x10000000U, "LDR fault changes nothing");

		/* LDR pc, [r1]: LoadWritePC; to a Thumb address (bit 0 set). */
		core.r[1] = TEST_MEM_BASE + 0x20U;
		CHECK(ldr_imm(&core, 15U, 1U, 0U, TRUE, TRUE, FALSE) == STD_E_OK && core.next_pc == 0x08000300U, "LDR pc %08x", core.next_pc);
		/* LDR pc with bit 0 clear: ARM state in the A profile, INVSTATE in the M profile. */
		core.r[1] = TEST_MEM_BASE + 0x24U;
		if (p == TestProfile_A_ARM) {
			CHECK(ldr_imm(&core, 15U, 1U, 0U, TRUE, TRUE, FALSE) == STD_E_OK && core.thumb == FALSE, "LDR pc ARM");
		}
		else {
			CHECK(ldr_imm(&core, 15U, 1U, 0U, TRUE, TRUE, FALSE) == STD_E_EXEC, "LDR pc INVSTATE");
		}
		/* LDR pc from an unaligned address: UNPREDICTABLE. */
		core.r[1] = TEST_MEM_BASE + 0x21U;
		CHECK(ldr_imm(&core, 15U, 1U, 0U, TRUE, TRUE, FALSE) == STD_E_INVALID, "LDR pc unaligned");
	}
}

static Std_ReturnType b(TargetCoreType *core, uint32 cond, sint32 offset)
{
	arm_b_input_type in;
	arm_b_output_type out;

	in.instrName = "B";
	in.cond = cond;
	in.imm32 = (uint32)offset;
	return arm_pseudo_b(core, &in, &out);
}

static void test_b(void)
{
	TargetCoreType core;

	/* ARM: target = address + 8 + imm32. */
	test_core_init(&core, TestProfile_A_ARM, PC0);
	CHECK(b(&core, ARM_COND_AL, 0x100) == STD_E_OK && core.next_pc == PC0 + 8U + 0x100U, "B ARM %08x", core.next_pc);
	test_core_init(&core, TestProfile_A_ARM, PC0);
	CHECK(b(&core, ARM_COND_AL, -8) == STD_E_OK && core.next_pc == PC0, "B ARM back to itself");

	/* Thumb: target = address + 4 + imm32. */
	test_core_init(&core, TestProfile_M, PC0);
	CHECK(b(&core, ARM_COND_AL, -4) == STD_E_OK && core.next_pc == PC0, "B Thumb to itself %08x", core.next_pc);
	test_core_init(&core, TestProfile_M, PC0);
	CHECK(b(&core, ARM_COND_AL, 0x7FE) == STD_E_OK && core.next_pc == PC0 + 4U + 0x7FEU, "B Thumb forward");

	/* BNE with Z set: falls through. BNE with Z clear: taken. */
	test_core_init(&core, TestProfile_M, PC0);
	core.apsr = ARM_APSR_Z;
	CHECK(b(&core, 0x1U, 0x10) == STD_E_OK && core.next_pc == PC0 + 2U, "BNE not taken");
	core.apsr = 0U;
	CHECK(b(&core, 0x1U, 0x10) == STD_E_OK && core.next_pc == PC0 + 0x14U, "BNE taken");
}


/*
 * Data processing.
 */

typedef Std_ReturnType (*DpImm)(TargetCoreType *, const arm_dp_imm_input_type *, arm_dp_output_type *);
typedef Std_ReturnType (*DpReg)(TargetCoreType *, const arm_dp_reg_input_type *, arm_dp_output_type *);

static Std_ReturnType dp_imm(DpImm f, TargetCoreType *core, uint32 cond, bool setflags, uint32 d, uint32 n, uint32 imm32, bool carry)
{
	arm_dp_imm_input_type in;
	arm_dp_output_type out;

	in.instrName = "DP";
	in.cond = cond;
	in.setflags = setflags;
	ARM_SET_REG(core, &in, Rd, d);
	ARM_SET_REG(core, &in, Rn, n);
	in.imm32 = imm32;
	in.carry = carry;
	return f(core, &in, &out);
}

static Std_ReturnType dp_reg_cond(DpReg f, TargetCoreType *core, uint32 cond, bool setflags, uint32 d, uint32 n, uint32 m, SRType t, uint32 shift_n)
{
	arm_dp_reg_input_type in;
	arm_dp_output_type out;

	in.instrName = "DP";
	in.cond = cond;
	in.setflags = setflags;
	ARM_SET_REG(core, &in, Rd, d);
	ARM_SET_REG(core, &in, Rn, n);
	ARM_SET_REG(core, &in, Rm, m);
	in.shift_t = t;
	in.shift_n = shift_n;
	return f(core, &in, &out);
}

static Std_ReturnType dp_reg(DpReg f, TargetCoreType *core, bool setflags, uint32 d, uint32 n, uint32 m, SRType t, uint32 shift_n)
{
	return dp_reg_cond(f, core, ARM_COND_AL, setflags, d, n, m, t, shift_n);
}

static Std_ReturnType mov_rsr_cond(TargetCoreType *core, uint32 cond, bool setflags, uint32 d, uint32 m, uint32 s, SRType t)
{
	arm_dp_rsr_input_type in;
	arm_dp_output_type out;

	in.instrName = "MOV";
	in.cond = cond;
	in.setflags = setflags;
	ARM_SET_REG(core, &in, Rd, d);
	ARM_NO_REG(&in, Rn);
	ARM_SET_REG(core, &in, Rm, m);
	ARM_SET_REG(core, &in, Rs, s);
	in.shift_t = t;
	return arm_pseudo_mov_rsr(core, &in, &out);
}

static Std_ReturnType mov_rsr(TargetCoreType *core, bool setflags, uint32 d, uint32 m, uint32 s, SRType t)
{
	return mov_rsr_cond(core, ARM_COND_AL, setflags, d, m, s, t);
}

#define NZCV(n, z, c, v) (((n) ? ARM_APSR_N : 0U) | ((z) ? ARM_APSR_Z : 0U) | ((c) ? ARM_APSR_C : 0U) | ((v) ? ARM_APSR_V : 0U))

static void test_sub_cmp(void)
{
	TargetCoreType core;

	test_core_init(&core, TestProfile_M, PC0);
	/* SUBS r0, r1, #3 with r1 = 5: 2, C (no borrow). */
	core.r[1] = 5U;
	CHECK(dp_imm(arm_pseudo_sub_imm, &core, ARM_COND_AL, TRUE, 0U, 1U, 3U, FALSE) == STD_E_OK, "SUBS");
	CHECK(core.r[0] == 2U && core.apsr == NZCV(0, 0, 1, 0), "SUBS 5-3 %08x %08x", core.r[0], core.apsr);
	/* SUBS r0, r1, #6: -1, N, borrow (C clear). */
	CHECK(dp_imm(arm_pseudo_sub_imm, &core, ARM_COND_AL, TRUE, 0U, 1U, 6U, FALSE) == STD_E_OK, "SUBS");
	CHECK(core.r[0] == 0xFFFFFFFFU && core.apsr == NZCV(1, 0, 0, 0), "SUBS 5-6 %08x", core.apsr);
	/* SUB (no S) with 0x80000000 - 1: 0x7FFFFFFF, flags unchanged. */
	core.r[2] = 0x80000000U;
	CHECK(dp_imm(arm_pseudo_sub_imm, &core, ARM_COND_AL, FALSE, 3U, 2U, 1U, FALSE) == STD_E_OK, "SUB");
	CHECK(core.r[3] == 0x7FFFFFFFU && core.apsr == NZCV(1, 0, 0, 0), "SUB keeps flags");
	/* CMP r2, #1: 0x80000000 - 1 overflows: V and C, r2 unchanged. */
	CHECK(dp_imm(arm_pseudo_cmp_imm, &core, ARM_COND_AL, TRUE, 0U, 2U, 1U, FALSE) == STD_E_OK, "CMP");
	CHECK(core.r[2] == 0x80000000U && core.apsr == NZCV(0, 0, 1, 1), "CMP overflow %08x", core.apsr);
	/* CMP equal: Z and C. */
	core.r[4] = 77U;
	CHECK(dp_imm(arm_pseudo_cmp_imm, &core, ARM_COND_AL, TRUE, 0U, 4U, 77U, FALSE) == STD_E_OK && core.apsr == NZCV(0, 1, 1, 0), "CMP equal");
	/* CMPNE after an equal compare: not executed. */
	CHECK(dp_imm(arm_pseudo_cmp_imm, &core, 0x1U, TRUE, 0U, 4U, 78U, FALSE) == STD_E_OK && core.apsr == NZCV(0, 1, 1, 0), "CMPNE skipped");
	/* CMP (register) r4, r5, LSL #1 with r5 = 40: 77 - 80 < 0: N, borrow. */
	core.r[5] = 40U;
	CHECK(dp_reg(arm_pseudo_cmp_reg, &core, TRUE, 0U, 4U, 5U, SRType_LSL, 1U) == STD_E_OK && core.apsr == NZCV(1, 0, 0, 0), "CMP reg %08x", core.apsr);
	/* CMP (register) with RRX: shifted = C:r5<31:1>; with C clear, 20; 77 - 20 = 57, C. */
	CHECK(dp_reg(arm_pseudo_cmp_reg, &core, TRUE, 0U, 4U, 5U, SRType_RRX, 1U) == STD_E_OK && core.apsr == NZCV(0, 0, 1, 0), "CMP RRX %08x", core.apsr);
	/* SUB pc, r1, #1 in ARM state: ALUWritePC (interworking). */
	test_core_init(&core, TestProfile_A_ARM, PC0);
	core.r[1] = 0x08000301U;
	CHECK(dp_imm(arm_pseudo_sub_imm, &core, ARM_COND_AL, FALSE, 15U, 1U, 0U, FALSE) == STD_E_OK && core.next_pc == 0x08000300U && core.thumb, "SUB pc");
}

static void test_add_reg(void)
{
	TargetCoreType core;

	test_core_init(&core, TestProfile_M, PC0);
	/* ADDS r0, r1, r2, LSL #4: 1 + 0x10 * 0x0FFFFFFF... */
	core.r[1] = 0x00000010U;
	core.r[2] = 0x0FFFFFFFU;
	CHECK(dp_reg(arm_pseudo_add_reg, &core, TRUE, 0U, 1U, 2U, SRType_LSL, 4U) == STD_E_OK, "ADDS reg");
	CHECK(core.r[0] == 0x00000000U && core.apsr == NZCV(0, 1, 1, 0), "ADDS reg 0x10 + 0xFFFFFFF0 %08x %08x", core.r[0], core.apsr);
	/* ADD r3, r1, r2, ASR #32: shifted = 0 (r2 positive): 0x10, flags unchanged. */
	CHECK(dp_reg(arm_pseudo_add_reg, &core, FALSE, 3U, 1U, 2U, SRType_ASR, 32U) == STD_E_OK && core.r[3] == 0x10U && core.apsr == NZCV(0, 1, 1, 0), "ADD ASR #32");
	/* ADD with RRX takes APSR.C as the top bit: C set, r2 = 2 -> shifted 0x80000001. */
	core.r[2] = 2U;
	CHECK(dp_reg(arm_pseudo_add_reg, &core, FALSE, 3U, 1U, 2U, SRType_RRX, 1U) == STD_E_OK && core.r[3] == 0x80000011U, "ADD RRX %08x", core.r[3]);
	/* ADD pc, r1 (Thumb T2): ALUWritePC = BranchWritePC. */
	core.r[1] = 0x08000400U;
	core.r[2] = 1U;
	CHECK(dp_reg(arm_pseudo_add_reg, &core, FALSE, 15U, 1U, 2U, SRType_LSL, 0U) == STD_E_OK && core.next_pc == 0x08000400U, "ADD pc %08x", core.next_pc);
	/* ADD r0, pc, r2 reads PC + 4. */
	CHECK(dp_reg(arm_pseudo_add_reg, &core, FALSE, 0U, 15U, 2U, SRType_LSL, 0U) == STD_E_OK && core.r[0] == PC0 + 5U, "ADD r0, pc");
}

static void test_and(void)
{
	TargetCoreType core;

	test_core_init(&core, TestProfile_M, PC0);
	/* ANDS r0, r1, #0xFF00 with the expansion's carry set: C from it, V unchanged. */
	core.r[1] = 0x12345678U;
	core.apsr = NZCV(0, 0, 0, 1);
	CHECK(dp_imm(arm_pseudo_and_imm, &core, ARM_COND_AL, TRUE, 0U, 1U, 0x0000FF00U, TRUE) == STD_E_OK, "ANDS imm");
	CHECK(core.r[0] == 0x00005600U && core.apsr == NZCV(0, 0, 1, 1), "ANDS imm %08x %08x", core.r[0], core.apsr);
	/* ANDS with result 0 and expansion carry clear: Z, C clear, V still set. */
	CHECK(dp_imm(arm_pseudo_and_imm, &core, ARM_COND_AL, TRUE, 0U, 1U, 0x80000000U, FALSE) == STD_E_OK && core.r[0] == 0U && core.apsr == NZCV(0, 1, 0, 1), "ANDS zero");
	/* AND (no S): flags unchanged. */
	CHECK(dp_imm(arm_pseudo_and_imm, &core, ARM_COND_AL, FALSE, 2U, 1U, 0xF0000000U, TRUE) == STD_E_OK && core.r[2] == 0x10000000U && core.apsr == NZCV(0, 1, 0, 1), "AND");
	/* ANDS (register) r0, r1, r3, LSR #4: r3 LSR 4 = 0x0FFFFFFF, C is r3<3>. */
	core.r[3] = 0xFFFFFFF8U;
	CHECK(dp_reg(arm_pseudo_and_reg, &core, TRUE, 0U, 1U, 3U, SRType_LSR, 4U) == STD_E_OK, "ANDS reg");
	CHECK(core.r[0] == 0x02345678U && core.apsr == NZCV(0, 0, 1, 1), "ANDS reg %08x %08x", core.r[0], core.apsr);
	/* ANDS (register) without shift: C unchanged (Shift_C with 0). */
	core.r[4] = 0x80000000U;
	core.r[5] = 0xFFFFFFFFU;
	CHECK(dp_reg(arm_pseudo_and_reg, &core, TRUE, 0U, 4U, 5U, SRType_LSL, 0U) == STD_E_OK && core.apsr == NZCV(1, 0, 1, 1), "ANDS reg no shift %08x", core.apsr);
	/* ANDEQ with Z clear: skipped. */
	core.r[0] = 0x55U;
	CHECK(dp_imm(arm_pseudo_and_imm, &core, 0x0U, TRUE, 0U, 1U, 0U, FALSE) == STD_E_OK && core.r[0] == 0x55U, "ANDEQ skipped");
}

static void test_mov(void)
{
	TargetCoreType core;

	test_core_init(&core, TestProfile_M, PC0);
	core.apsr = NZCV(0, 0, 1, 1);
	/* MOVS r0, #0: Z, C from the expansion (clear), V unchanged. */
	CHECK(dp_imm(arm_pseudo_mov_imm, &core, ARM_COND_AL, TRUE, 0U, 0U, 0U, FALSE) == STD_E_OK && core.r[0] == 0U && core.apsr == NZCV(0, 1, 0, 1), "MOVS #0 %08x", core.apsr);
	/* MOV r1, #0xE000E000 (no S). */
	CHECK(dp_imm(arm_pseudo_mov_imm, &core, ARM_COND_AL, FALSE, 1U, 0U, 0xE000E000U, TRUE) == STD_E_OK && core.r[1] == 0xE000E000U && core.apsr == NZCV(0, 1, 0, 1), "MOV imm");
	/* MOVS r2, r1 (no shift): N, C unchanged. */
	CHECK(dp_reg(arm_pseudo_mov_reg, &core, TRUE, 2U, 0U, 1U, SRType_LSL, 0U) == STD_E_OK && core.r[2] == 0xE000E000U && core.apsr == NZCV(1, 0, 0, 1), "MOVS reg %08x", core.apsr);
	/* LSLS r3, r1, #3 (MOV with shift): 0x00070000, C = r1<29> = 1. */
	CHECK(dp_reg(arm_pseudo_mov_reg, &core, TRUE, 3U, 0U, 1U, SRType_LSL, 3U) == STD_E_OK && core.r[3] == 0x00070000U && core.apsr == NZCV(0, 0, 1, 1), "LSLS imm %08x %08x", core.r[3], core.apsr);
	/* ASRS r3, r1, #32: all ones, C = r1<31>. */
	CHECK(dp_reg(arm_pseudo_mov_reg, &core, TRUE, 3U, 0U, 1U, SRType_ASR, 32U) == STD_E_OK && core.r[3] == 0xFFFFFFFFU && core.apsr == NZCV(1, 0, 1, 1), "ASRS #32");
	/* MOV pc, r1 (Thumb): BranchWritePC. */
	core.r[4] = 0x08000201U;
	CHECK(dp_reg(arm_pseudo_mov_reg, &core, FALSE, 15U, 0U, 4U, SRType_LSL, 0U) == STD_E_OK && core.next_pc == 0x08000200U, "MOV pc");
	/* LSLS r5, r1, r6 (register): only R[s]<7:0> counts: 0x103 -> 3. */
	core.r[6] = 0x103U;
	CHECK(mov_rsr(&core, TRUE, 5U, 1U, 6U, SRType_LSL) == STD_E_OK && core.r[5] == 0x00070000U && core.apsr == NZCV(0, 0, 1, 1), "LSLS reg %08x", core.r[5]);
	/* LSRS r5, r1, r6 with R[s] = 32: 0, C = r1<31>. */
	core.r[6] = 32U;
	CHECK(mov_rsr(&core, TRUE, 5U, 1U, 6U, SRType_LSR) == STD_E_OK && core.r[5] == 0U && core.apsr == NZCV(0, 1, 1, 1), "LSRS by 32 %08x", core.apsr);
	/* RORS by 0 (R[s] = 0x100): value and C unchanged. */
	core.r[6] = 0x100U;
	core.apsr = NZCV(0, 0, 0, 0);
	CHECK(mov_rsr(&core, TRUE, 5U, 1U, 6U, SRType_ROR) == STD_E_OK && core.r[5] == 0xE000E000U && core.apsr == NZCV(1, 0, 0, 0), "RORS by 0");
	/* LSL (no S) by 40: 0, flags unchanged. */
	core.r[6] = 40U;
	CHECK(mov_rsr(&core, FALSE, 5U, 1U, 6U, SRType_LSL) == STD_E_OK && core.r[5] == 0U && core.apsr == NZCV(1, 0, 0, 0), "LSL by 40");
}

/*
 * The remaining data-processing instructions: one case per line, from R[n],
 * the second operand (imm32 with the expansion's carry, or R[m] with a
 * shift) and NZCV before, to the result and NZCV after, worked out from the
 * pseudo-code (AddWithCarry; Shift_C; logical: C from the operand, V
 * unchanged; setflags FALSE: flags unchanged; TST, TEQ, CMN: no Rd).
 */
typedef struct {
	const char *name;
	DpImm imm_op;
	DpReg reg_op;
	bool setflags;
	uint32 rn;
	uint32 op2;
	SRType shift_t;
	uint32 shift_n;
	bool imm_carry;
	uint32 apsr_before;
	uint32 result;
	uint32 apsr_after;
} DpCase;

static const DpCase dp_cases[] = {
	{ "ADCS imm", arm_pseudo_adc_imm, NULL, TRUE, 0xFFFFFFFFU, 0x00000000U, SRType_LSL, 0U, FALSE, NZCV(0, 0, 1, 0), 0x00000000U, NZCV(0, 1, 1, 0) },
	{ "ADCS imm", arm_pseudo_adc_imm, NULL, TRUE, 0x7FFFFFFFU, 0x00000000U, SRType_LSL, 0U, FALSE, NZCV(0, 0, 1, 0), 0x80000000U, NZCV(1, 0, 0, 1) },
	{ "ADC imm", arm_pseudo_adc_imm, NULL, FALSE, 0x00000005U, 0x00000003U, SRType_LSL, 0U, FALSE, NZCV(1, 1, 0, 1), 0x00000008U, NZCV(1, 1, 0, 1) },
	{ "ADCS reg", NULL, arm_pseudo_adc_reg, TRUE, 0x00000001U, 0x80000000U, SRType_LSR, 31U, FALSE, NZCV(0, 0, 0, 0), 0x00000002U, NZCV(0, 0, 0, 0) },
	{ "ADCS reg", NULL, arm_pseudo_adc_reg, TRUE, 0xFFFFFFFEU, 0x00000001U, SRType_LSL, 0U, FALSE, NZCV(0, 0, 1, 0), 0x00000000U, NZCV(0, 1, 1, 0) },
	{ "SBCS imm", arm_pseudo_sbc_imm, NULL, TRUE, 0x00000005U, 0x00000003U, SRType_LSL, 0U, FALSE, NZCV(0, 0, 1, 0), 0x00000002U, NZCV(0, 0, 1, 0) },
	{ "SBCS imm", arm_pseudo_sbc_imm, NULL, TRUE, 0x00000005U, 0x00000003U, SRType_LSL, 0U, FALSE, NZCV(0, 0, 0, 0), 0x00000001U, NZCV(0, 0, 1, 0) },
	{ "SBCS imm", arm_pseudo_sbc_imm, NULL, TRUE, 0x00000000U, 0x00000000U, SRType_LSL, 0U, FALSE, NZCV(0, 0, 0, 0), 0xFFFFFFFFU, NZCV(1, 0, 0, 0) },
	{ "SBC reg", NULL, arm_pseudo_sbc_reg, FALSE, 0x0000000AU, 0x00000002U, SRType_LSL, 1U, FALSE, NZCV(1, 0, 1, 1), 0x00000006U, NZCV(1, 0, 1, 1) },
	{ "SBCS reg", NULL, arm_pseudo_sbc_reg, TRUE, 0x80000000U, 0x00000000U, SRType_LSL, 0U, FALSE, NZCV(0, 0, 0, 0), 0x7FFFFFFFU, NZCV(0, 0, 1, 1) },
	{ "RSBS imm", arm_pseudo_rsb_imm, NULL, TRUE, 0x00000003U, 0x00000000U, SRType_LSL, 0U, FALSE, NZCV(0, 0, 0, 0), 0xFFFFFFFDU, NZCV(1, 0, 0, 0) },
	{ "RSBS imm", arm_pseudo_rsb_imm, NULL, TRUE, 0x00000000U, 0x00000000U, SRType_LSL, 0U, FALSE, NZCV(0, 0, 0, 0), 0x00000000U, NZCV(0, 1, 1, 0) },
	{ "RSBS reg", NULL, arm_pseudo_rsb_reg, TRUE, 0x00000001U, 0x80000000U, SRType_LSL, 0U, FALSE, NZCV(0, 0, 0, 0), 0x7FFFFFFFU, NZCV(0, 0, 1, 1) },
	{ "RSB reg", NULL, arm_pseudo_rsb_reg, FALSE, 0x00000004U, 0x00000001U, SRType_LSL, 4U, FALSE, NZCV(0, 1, 0, 1), 0x0000000CU, NZCV(0, 1, 0, 1) },
	{ "SUBS reg", NULL, arm_pseudo_sub_reg, TRUE, 0x00000005U, 0x00000005U, SRType_LSL, 0U, FALSE, NZCV(0, 0, 0, 0), 0x00000000U, NZCV(0, 1, 1, 0) },
	{ "SUBS reg", NULL, arm_pseudo_sub_reg, TRUE, 0x00000001U, 0x00000001U, SRType_LSL, 1U, FALSE, NZCV(0, 0, 0, 0), 0xFFFFFFFFU, NZCV(1, 0, 0, 0) },
	{ "SUB reg", NULL, arm_pseudo_sub_reg, FALSE, 0x00000064U, 0x00000001U, SRType_ROR, 1U, FALSE, NZCV(1, 1, 1, 1), 0x80000064U, NZCV(1, 1, 1, 1) },
	{ "CMNS imm", arm_pseudo_cmn_imm, NULL, TRUE, 0xFFFFFFFFU, 0x00000001U, SRType_LSL, 0U, FALSE, NZCV(0, 0, 0, 0), 0x00000000U, NZCV(0, 1, 1, 0) },
	{ "CMNS reg", NULL, arm_pseudo_cmn_reg, TRUE, 0x7FFFFFFFU, 0x00000002U, SRType_LSR, 1U, FALSE, NZCV(0, 0, 0, 0), 0x80000000U, NZCV(1, 0, 0, 1) },
	{ "ORRS imm", arm_pseudo_orr_imm, NULL, TRUE, 0x0F000000U, 0x80000000U, SRType_LSL, 0U, TRUE, NZCV(0, 0, 0, 1), 0x8F000000U, NZCV(1, 0, 1, 1) },
	{ "ORRS reg", NULL, arm_pseudo_orr_reg, TRUE, 0x00000000U, 0x00000000U, SRType_LSL, 0U, FALSE, NZCV(0, 0, 1, 0), 0x00000000U, NZCV(0, 1, 1, 0) },
	{ "ORR reg", NULL, arm_pseudo_orr_reg, FALSE, 0x000000F0U, 0x00000F00U, SRType_LSL, 4U, FALSE, NZCV(1, 1, 1, 1), 0x0000F0F0U, NZCV(1, 1, 1, 1) },
	{ "ORNS imm", arm_pseudo_orn_imm, NULL, TRUE, 0x00000000U, 0xFFFFFFFFU, SRType_LSL, 0U, FALSE, NZCV(1, 0, 1, 1), 0x00000000U, NZCV(0, 1, 0, 1) },
	{ "ORNS reg", NULL, arm_pseudo_orn_reg, TRUE, 0x00000001U, 0x00000003U, SRType_LSR, 1U, FALSE, NZCV(0, 0, 0, 0), 0xFFFFFFFFU, NZCV(1, 0, 1, 0) },
	{ "EORS imm", arm_pseudo_eor_imm, NULL, TRUE, 0xFFFF0000U, 0xFFFF0000U, SRType_LSL, 0U, TRUE, NZCV(0, 0, 0, 0), 0x00000000U, NZCV(0, 1, 1, 0) },
	{ "EORS reg", NULL, arm_pseudo_eor_reg, TRUE, 0x12345678U, 0x00000001U, SRType_ROR, 1U, FALSE, NZCV(0, 0, 0, 1), 0x92345678U, NZCV(1, 0, 1, 1) },
	{ "EOR reg", NULL, arm_pseudo_eor_reg, FALSE, 0x0000000AU, 0x00000005U, SRType_LSL, 0U, FALSE, NZCV(0, 0, 1, 0), 0x0000000FU, NZCV(0, 0, 1, 0) },
	{ "BICS imm", arm_pseudo_bic_imm, NULL, TRUE, 0xFFFFFFFFU, 0x7FFFFFFFU, SRType_LSL, 0U, TRUE, NZCV(0, 0, 0, 0), 0x80000000U, NZCV(1, 0, 1, 0) },
	{ "BICS reg", NULL, arm_pseudo_bic_reg, TRUE, 0x000000FFU, 0x000000F0U, SRType_ASR, 4U, FALSE, NZCV(0, 0, 1, 0), 0x000000F0U, NZCV(0, 0, 0, 0) },
	{ "TSTS imm", arm_pseudo_tst_imm, NULL, TRUE, 0x80000000U, 0x00FF00FFU, SRType_LSL, 0U, FALSE, NZCV(1, 0, 1, 1), 0x00000000U, NZCV(0, 1, 0, 1) },
	{ "TSTS reg", NULL, arm_pseudo_tst_reg, TRUE, 0x80000000U, 0x40000000U, SRType_LSL, 1U, FALSE, NZCV(0, 1, 0, 0), 0x80000000U, NZCV(1, 0, 0, 0) },
	{ "TEQS imm", arm_pseudo_teq_imm, NULL, TRUE, 0x00000005U, 0x00000005U, SRType_LSL, 0U, TRUE, NZCV(0, 0, 0, 0), 0x00000000U, NZCV(0, 1, 1, 0) },
	{ "TEQS reg", NULL, arm_pseudo_teq_reg, TRUE, 0x00000000U, 0x80000001U, SRType_LSR, 1U, FALSE, NZCV(0, 0, 0, 1), 0x40000000U, NZCV(0, 0, 1, 1) },
	{ "MVNS imm", arm_pseudo_mvn_imm, NULL, TRUE, 0x00000000U, 0x00000000U, SRType_LSL, 0U, TRUE, NZCV(0, 0, 0, 1), 0xFFFFFFFFU, NZCV(1, 0, 1, 1) },
	{ "MVN imm", arm_pseudo_mvn_imm, NULL, FALSE, 0x00000000U, 0x00FF00FFU, SRType_LSL, 0U, FALSE, NZCV(0, 1, 0, 0), 0xFF00FF00U, NZCV(0, 1, 0, 0) },
	{ "MVNS reg", NULL, arm_pseudo_mvn_reg, TRUE, 0x00000000U, 0xFFFFFFFFU, SRType_LSL, 0U, FALSE, NZCV(0, 0, 1, 0), 0x00000000U, NZCV(0, 1, 1, 0) },
	{ "MVNS reg", NULL, arm_pseudo_mvn_reg, TRUE, 0x00000000U, 0x00000001U, SRType_ROR, 1U, FALSE, NZCV(0, 0, 0, 0), 0x7FFFFFFFU, NZCV(0, 0, 1, 0) },
};

static void test_dp_cases(void)
{
	TargetCoreType core;
	uint32 i;

	for (i = 0U; i < sizeof(dp_cases) / sizeof(dp_cases[0]); i++) {
		const DpCase *t = &dp_cases[i];
		bool writes = (t->imm_op != arm_pseudo_cmn_imm) && (t->imm_op != arm_pseudo_tst_imm) && (t->imm_op != arm_pseudo_teq_imm)
			&& (t->reg_op != arm_pseudo_cmn_reg) && (t->reg_op != arm_pseudo_tst_reg) && (t->reg_op != arm_pseudo_teq_reg);
		Std_ReturnType err;

		test_core_init(&core, TestProfile_M, PC0);
		core.r[0] = 0xDEADBEEFU;
		core.r[1] = t->rn;
		core.r[2] = t->op2;
		core.apsr = t->apsr_before;
		if (t->imm_op != NULL) {
			err = dp_imm(t->imm_op, &core, ARM_COND_AL, t->setflags, 0U, 1U, t->op2, t->imm_carry);
		}
		else {
			err = dp_reg(t->reg_op, &core, t->setflags, 0U, 1U, 2U, t->shift_t, t->shift_n);
		}
		CHECK(err == STD_E_OK, "%s case %u", t->name, i);
		if (writes) {
			CHECK(core.r[0] == t->result, "%s case %u: result %08x, expected %08x", t->name, i, core.r[0], t->result);
		}
		else {
			CHECK(core.r[0] == 0xDEADBEEFU, "%s case %u writes Rd", t->name, i);
		}
		CHECK(core.apsr == t->apsr_after, "%s case %u: NZCV %x, expected %x", t->name, i, core.apsr >> 28, t->apsr_after >> 28);
	}
}

/* Every instruction with a failing condition (NE with Z set) changes nothing. */
static void test_condition_failed(void)
{
	static const DpImm imm_ops[] = {
		arm_pseudo_add_imm, arm_pseudo_sub_imm, arm_pseudo_cmp_imm, arm_pseudo_and_imm, arm_pseudo_mov_imm,
		arm_pseudo_adc_imm, arm_pseudo_sbc_imm, arm_pseudo_rsb_imm, arm_pseudo_cmn_imm,
		arm_pseudo_orr_imm, arm_pseudo_orn_imm, arm_pseudo_eor_imm, arm_pseudo_bic_imm,
		arm_pseudo_tst_imm, arm_pseudo_teq_imm, arm_pseudo_mvn_imm,
	};
	static const DpReg reg_ops[] = {
		arm_pseudo_add_reg, arm_pseudo_cmp_reg, arm_pseudo_and_reg, arm_pseudo_mov_reg,
		arm_pseudo_adc_reg, arm_pseudo_sbc_reg, arm_pseudo_rsb_reg, arm_pseudo_sub_reg,
		arm_pseudo_cmn_reg, arm_pseudo_orr_reg, arm_pseudo_orn_reg, arm_pseudo_eor_reg,
		arm_pseudo_bic_reg, arm_pseudo_tst_reg, arm_pseudo_teq_reg, arm_pseudo_mvn_reg,
	};
	TargetCoreType core;
	TargetCoreType before;
	uint32 i;

	test_core_init(&core, TestProfile_M, PC0);
	for (i = 0U; i < 15U; i++) {
		core.r[i] = 0x01010101U * (i + 1U);
	}
	core.apsr = ARM_APSR_Z;
	before = core;
	for (i = 0U; i < sizeof(imm_ops) / sizeof(imm_ops[0]); i++) {
		CHECK(dp_imm(imm_ops[i], &core, 0x1U, TRUE, 0U, 1U, 5U, TRUE) == STD_E_OK, "imm op %u", i);
		CHECK(memcmp(&core, &before, sizeof(core)) == 0, "imm op %u with a failing condition changed the core", i);
	}
	for (i = 0U; i < sizeof(reg_ops) / sizeof(reg_ops[0]); i++) {
		CHECK(dp_reg_cond(reg_ops[i], &core, 0x1U, TRUE, 0U, 1U, 2U, SRType_LSL, 1U) == STD_E_OK, "reg op %u", i);
		CHECK(memcmp(&core, &before, sizeof(core)) == 0, "reg op %u with a failing condition changed the core", i);
	}
	CHECK(mov_rsr_cond(&core, 0x1U, TRUE, 0U, 1U, 2U, SRType_LSL) == STD_E_OK && memcmp(&core, &before, sizeof(core)) == 0, "MOV rsr skipped");
	{
		arm_ls_imm_input_type in;
		arm_ls_output_type out;
		in.instrName = "LDR";
		in.cond = 0x1U;
		ARM_SET_REG(&core, &in, Rt, 0U);
		ARM_SET_REG(&core, &in, Rn, 1U);
		in.imm32 = 0U;
		in.index = TRUE;
		in.add = TRUE;
		in.wback = TRUE;
		CHECK(arm_pseudo_ldr_imm(&core, &in, &out) == STD_E_OK && memcmp(&core, &before, sizeof(core)) == 0, "LDR skipped");
	}
}

int main(void)
{
	test_add_imm();
	test_ldr_imm();
	test_b();
	test_sub_cmp();
	test_add_reg();
	test_and();
	test_mov();
	test_dp_cases();
	test_condition_failed();
	printf("%d checks, %d failures\n", checks, failures);
	return (failures == 0) ? 0 : 1;
}
