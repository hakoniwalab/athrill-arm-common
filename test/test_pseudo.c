/*
 * Tests of the pseudo-code functions through the test profile, in the A
 * profile's ARM state and in the M profile. Expected values are worked out
 * by hand from the pseudo-code.
 */
#include <stdio.h>
#include "arm_common/arm_pseudo_arith.h"
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
	arm_add_imm_input_type in;
	arm_add_imm_output_type out;

	in.instrName = "ADD";
	in.cond = cond;
	in.setflags = setflags;
	ARM_SET_REG(core, &in, Rd, d);
	ARM_SET_REG(core, &in, Rn, n);
	in.imm32 = imm32;
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
	arm_ldr_imm_input_type in;
	arm_ldr_imm_output_type out;

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

int main(void)
{
	test_add_imm();
	test_ldr_imm();
	test_b();
	printf("%d checks, %d failures\n", checks, failures);
	return (failures == 0) ? 0 : 1;
}
