/*
 * Tests of the floating-point moves, loads and stores through the test
 * profile (FPv5 with 16 double registers).
 */
#include <stdio.h>
#include <string.h>
#include "arm_common/arm_pseudo_fp_ls.h"

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

#define PC0	0x08000102U
#define N_CASES(table)	(sizeof(table) / sizeof((table)[0]))

/*
 * VFPExpandImm: a few constants written out from the definition
 * (sign : NOT(b):Replicate(b,E-3):cd : efgh:Zeros(F-4)), and their values.
 */
static void test_expand_imm(void)
{
	static const struct {
		uint32 imm8;
		uint32 single;
		uint64 dbl;
	} cases[] = {
		{ 0x70U, 0x3F800000U, 0x3FF0000000000000ULL },	/* 1.0 */
		{ 0x00U, 0x40000000U, 0x4000000000000000ULL },	/* 2.0 */
		{ 0xF8U, 0xBFC00000U, 0xBFF8000000000000ULL },	/* -1.5 */
		{ 0x3FU, 0x41F80000U, 0x403F000000000000ULL },	/* 31.0 */
		{ 0x40U, 0x3E000000U, 0x3FC0000000000000ULL },	/* 0.125 */
		{ 0x7FU, 0x3FF80000U, 0x3FFF000000000000ULL },	/* 1.9375 */
		{ 0xC0U, 0xBE000000U, 0xBFC0000000000000ULL },	/* -0.125 */
	};
	uint32 i;

	for (i = 0U; i < N_CASES(cases); i++) {
		CHECK(VFPExpandImm(cases[i].imm8, 32U) == cases[i].single, "VFPExpandImm(%02x, 32)", cases[i].imm8);
		CHECK(VFPExpandImm(cases[i].imm8, 64U) == cases[i].dbl, "VFPExpandImm(%02x, 64)", cases[i].imm8);
	}
}

static Std_ReturnType fp(Std_ReturnType (*op)(TargetCoreType *, const arm_fp_input_type *, arm_fp_output_type *),
	TargetCoreType *core, uint32 cond, bool dp, uint32 d, uint32 n, uint32 m)
{
	arm_fp_input_type in;
	arm_fp_output_type out;

	in.instrName = "VFP";
	in.cond = cond;
	in.dp_operation = dp;
	in.d = d;
	in.n = n;
	in.m = m;
	in.imm64 = 0x4000000000000000ULL;
	in.sel_cond = 0x0U;	/* VSELEQ */
	return op(core, &in, &out);
}

static void init(TargetCoreType *core)
{
	uint32 i;

	test_core_init(core, TestProfile_M, PC0);
	for (i = 0U; i < 32U; i++) {
		core->s[i] = 0x11111111U * (i % 15U) + i;
	}
}

static void test_register_ops(void)
{
	TargetCoreType core;

	init(&core);
	CHECK(fp(arm_pseudo_vmov_imm, &core, ARM_COND_AL, FALSE, 3U, 0U, 0U) == STD_E_OK && core.s[3] == 0U, "VMOV.F32 imm: low word");
	CHECK(fp(arm_pseudo_vmov_imm, &core, ARM_COND_AL, TRUE, 3U, 0U, 0U) == STD_E_OK && core.s[6] == 0U && core.s[7] == 0x40000000U,
	      "VMOV.F64 d3, imm");
	core.s[9] = 0x80000001U;
	CHECK(fp(arm_pseudo_vmov_reg, &core, ARM_COND_AL, FALSE, 0U, 0U, 9U) == STD_E_OK && core.s[0] == 0x80000001U, "VMOV.F32 s0, s9");
	CHECK(fp(arm_pseudo_vabs, &core, ARM_COND_AL, FALSE, 1U, 0U, 9U) == STD_E_OK && core.s[1] == 0x00000001U, "VABS.F32");
	CHECK(fp(arm_pseudo_vneg, &core, ARM_COND_AL, FALSE, 1U, 0U, 1U) == STD_E_OK && core.s[1] == 0x80000001U, "VNEG.F32");
	core.s[10] = 0x00000005U;
	core.s[11] = 0xFFF80000U;	/* d5: a NaN: VABS and VNEG change only the sign */
	CHECK(fp(arm_pseudo_vabs, &core, ARM_COND_AL, TRUE, 4U, 0U, 5U) == STD_E_OK && core.s[8] == 5U && core.s[9] == 0x7FF80000U, "VABS.F64");
	CHECK(fp(arm_pseudo_vneg, &core, ARM_COND_AL, TRUE, 4U, 0U, 4U) == STD_E_OK && core.s[8] == 5U && core.s[9] == 0xFFF80000U, "VNEG.F64");
	CHECK(fp(arm_pseudo_vmov_reg, &core, ARM_COND_AL, TRUE, 0U, 0U, 4U) == STD_E_OK && core.s[0] == 5U && core.s[1] == 0xFFF80000U, "VMOV.F64");
	/* VSELEQ: S[n] when Z is set, S[m] otherwise. */
	core.s[2] = 0x2U;
	core.s[3] = 0x3U;
	core.apsr = ARM_APSR_Z;
	CHECK(fp(arm_pseudo_vsel, &core, ARM_COND_AL, FALSE, 4U, 2U, 3U) == STD_E_OK && core.s[4] == 2U, "VSELEQ taken");
	core.apsr = 0U;
	CHECK(fp(arm_pseudo_vsel, &core, ARM_COND_AL, FALSE, 4U, 2U, 3U) == STD_E_OK && core.s[4] == 3U, "VSELEQ not taken");
}

static Std_ReturnType fpcore(Std_ReturnType (*op)(TargetCoreType *, const arm_fp_core_input_type *, arm_fp_output_type *),
	TargetCoreType *core, uint32 cond, bool to_arm, uint32 t, uint32 t2, uint32 fpreg, uint32 index)
{
	arm_fp_core_input_type in;
	arm_fp_output_type out;

	in.instrName = "VMOV";
	in.cond = cond;
	in.to_arm_registers = to_arm;
	ARM_SET_REG(core, &in, Rt, t);
	ARM_SET_REG(core, &in, Rt2, t2);
	in.fp = fpreg;
	in.index = index;
	return op(core, &in, &out);
}

static void test_core_moves(void)
{
	TargetCoreType core;

	init(&core);
	core.r[1] = 0xAAAA0001U;
	core.r[2] = 0xBBBB0002U;
	CHECK(fpcore(arm_pseudo_vmov_core_s, &core, ARM_COND_AL, FALSE, 1U, 0U, 5U, 0U) == STD_E_OK && core.s[5] == 0xAAAA0001U, "VMOV s5, r1");
	CHECK(fpcore(arm_pseudo_vmov_core_s, &core, ARM_COND_AL, TRUE, 3U, 0U, 5U, 0U) == STD_E_OK && core.r[3] == 0xAAAA0001U, "VMOV r3, s5");
	CHECK(fpcore(arm_pseudo_vmov_core_ss, &core, ARM_COND_AL, FALSE, 1U, 2U, 7U, 0U) == STD_E_OK
	      && core.s[7] == 0xAAAA0001U && core.s[8] == 0xBBBB0002U, "VMOV s7, s8, r1, r2");
	CHECK(fpcore(arm_pseudo_vmov_core_ss, &core, ARM_COND_AL, TRUE, 4U, 5U, 7U, 0U) == STD_E_OK
	      && core.r[4] == 0xAAAA0001U && core.r[5] == 0xBBBB0002U, "VMOV r4, r5, s7, s8");
	CHECK(fpcore(arm_pseudo_vmov_core_d, &core, ARM_COND_AL, FALSE, 1U, 2U, 6U, 0U) == STD_E_OK
	      && core.s[12] == 0xAAAA0001U && core.s[13] == 0xBBBB0002U, "VMOV d6, r1, r2");
	core.s[12] = 0x12345678U;
	CHECK(fpcore(arm_pseudo_vmov_core_d, &core, ARM_COND_AL, TRUE, 4U, 5U, 6U, 0U) == STD_E_OK
	      && core.r[4] == 0x12345678U && core.r[5] == 0xBBBB0002U, "VMOV r4, r5, d6");
	CHECK(fpcore(arm_pseudo_vmov_core_scalar, &core, ARM_COND_AL, FALSE, 1U, 0U, 6U, 1U) == STD_E_OK
	      && core.s[12] == 0x12345678U && core.s[13] == 0xAAAA0001U, "VMOV.32 d6[1], r1");
	CHECK(fpcore(arm_pseudo_vmov_core_scalar, &core, ARM_COND_AL, FALSE, 2U, 0U, 6U, 0U) == STD_E_OK
	      && core.s[12] == 0xBBBB0002U && core.s[13] == 0xAAAA0001U, "VMOV.32 d6[0], r2");
	CHECK(fpcore(arm_pseudo_vmov_core_scalar, &core, ARM_COND_AL, TRUE, 3U, 0U, 6U, 1U) == STD_E_OK && core.r[3] == 0xAAAA0001U,
	      "VMOV.32 r3, d6[1]");
	CHECK(fpcore(arm_pseudo_vmov_core_scalar, &core, ARM_COND_AL, TRUE, 3U, 0U, 6U, 0U) == STD_E_OK && core.r[3] == 0xBBBB0002U,
	      "VMOV.32 r3, d6[0]");
	/* VMSR keeps the implemented bits; VMRS to a register and to APSR_nzcv. */
	core.r[1] = 0xFFFFFFFFU;
	CHECK(fpcore(arm_pseudo_vmrs_vmsr, &core, ARM_COND_AL, FALSE, 1U, 0U, 0U, 0U) == STD_E_OK && core.fpscr == TEST_FPSCR_MASK, "VMSR");
	CHECK(fpcore(arm_pseudo_vmrs_vmsr, &core, ARM_COND_AL, TRUE, 2U, 0U, 0U, 0U) == STD_E_OK && core.r[2] == TEST_FPSCR_MASK, "VMRS r2");
	core.fpscr = 0x60000000U;
	core.apsr = 0x98050000U;	/* N V Q, GE: only N Z C V change */
	CHECK(fpcore(arm_pseudo_vmrs_vmsr, &core, ARM_COND_AL, TRUE, 15U, 0U, 0U, 0U) == STD_E_OK && core.apsr == 0x68050000U,
	      "VMRS APSR_nzcv %08x", core.apsr);
}

#define MEM(offset)	(TEST_MEM_BASE + (offset))

static Std_ReturnType fpls(Std_ReturnType (*op)(TargetCoreType *, const arm_fp_ls_input_type *, arm_fp_output_type *),
	TargetCoreType *core, uint32 cond, bool single, uint32 d, uint32 n, uint32 imm32, bool add, bool wback, uint32 regs)
{
	arm_fp_ls_input_type in;
	arm_fp_output_type out;

	in.instrName = "VLS";
	in.cond = cond;
	in.single_regs = single;
	in.d = d;
	ARM_SET_REG(core, &in, Rn, n);
	in.imm32 = imm32;
	in.add = add;
	in.wback = wback;
	in.regs = regs;
	return op(core, &in, &out);
}

static uint32 word_at(const TargetCoreType *core, uint32 offset)
{
	return (uint32)core->mem[offset] | ((uint32)core->mem[offset + 1U] << 8) | ((uint32)core->mem[offset + 2U] << 16)
		| ((uint32)core->mem[offset + 3U] << 24);
}

static void test_loads_stores(void)
{
	TargetCoreType core;
	uint32 i;

	init(&core);
	for (i = 0U; i < TEST_MEM_SIZE; i++) {
		core.mem[i] = (uint8)(i * 7U + 3U);
	}
	core.r[1] = MEM(0x20U);
	CHECK(fpls(arm_pseudo_vldr, &core, ARM_COND_AL, TRUE, 3U, 1U, 8U, TRUE, FALSE, 0U) == STD_E_OK && core.s[3] == word_at(&core, 0x28U),
	      "VLDR s3, [r1, #8]");
	CHECK(fpls(arm_pseudo_vldr, &core, ARM_COND_AL, FALSE, 2U, 1U, 8U, FALSE, FALSE, 0U) == STD_E_OK
	      && core.s[4] == word_at(&core, 0x18U) && core.s[5] == word_at(&core, 0x1CU), "VLDR d2, [r1, #-8]");
	core.s[6] = 0xCAFEF00DU;
	core.s[7] = 0x01234567U;
	CHECK(fpls(arm_pseudo_vstr, &core, ARM_COND_AL, FALSE, 3U, 1U, 0x40U, TRUE, FALSE, 0U) == STD_E_OK
	      && word_at(&core, 0x60U) == 0xCAFEF00DU && word_at(&core, 0x64U) == 0x01234567U, "VSTR d3");
	CHECK(fpls(arm_pseudo_vstr, &core, ARM_COND_AL, TRUE, 7U, 1U, 4U, FALSE, FALSE, 0U) == STD_E_OK && word_at(&core, 0x1CU) == 0x01234567U,
	      "VSTR s7, [r1, #-4]");
	/* The literal form: base Align(PC,4). */
	test_core_init(&core, TestProfile_M, TEST_MEM_BASE + 0x12U);	/* PC = 0x16, aligned 0x14 */
	for (i = 0U; i < TEST_MEM_SIZE; i++) {
		core.mem[i] = (uint8)(i * 7U + 3U);
	}
	CHECK(fpls(arm_pseudo_vldr, &core, ARM_COND_AL, TRUE, 0U, 15U, 4U, TRUE, FALSE, 0U) == STD_E_OK && core.s[0] == word_at(&core, 0x18U),
	      "VLDR s0, [pc, #4]");
	/* VLDM/VSTM, increment after with write-back, decrement before. */
	core.r[2] = MEM(0x40U);
	CHECK(fpls(arm_pseudo_vldm, &core, ARM_COND_AL, FALSE, 1U, 2U, 16U, TRUE, TRUE, 2U) == STD_E_OK && core.r[2] == MEM(0x50U)
	      && core.s[2] == word_at(&core, 0x40U) && core.s[5] == word_at(&core, 0x4CU), "VLDMIA r2!, {d1-d2}");
	core.s[10] = 0xA0A0A0A0U;
	core.s[11] = 0xB1B1B1B1U;
	core.s[12] = 0xC2C2C2C2U;
	CHECK(fpls(arm_pseudo_vstm, &core, ARM_COND_AL, TRUE, 10U, 2U, 12U, FALSE, TRUE, 3U) == STD_E_OK && core.r[2] == MEM(0x44U)
	      && word_at(&core, 0x44U) == 0xA0A0A0A0U && word_at(&core, 0x4CU) == 0xC2C2C2C2U, "VSTMDB r2!, {s10-s12}");
	CHECK(fpls(arm_pseudo_vldm, &core, ARM_COND_AL, TRUE, 20U, 2U, 12U, TRUE, FALSE, 3U) == STD_E_OK && core.r[2] == MEM(0x44U)
	      && core.s[20] == 0xA0A0A0A0U && core.s[22] == 0xC2C2C2C2U, "VLDMIA r2, {s20-s22}");
	/* Faults: an unaligned address, and memory that is not there (the second word). */
	core.r[1] = MEM(0x22U);
	CHECK(fpls(arm_pseudo_vldr, &core, ARM_COND_AL, TRUE, 0U, 1U, 0U, TRUE, FALSE, 0U) == STD_E_SEGV, "VLDR unaligned");
	core.r[1] = MEM(TEST_MEM_SIZE - 4U);
	CHECK(fpls(arm_pseudo_vldr, &core, ARM_COND_AL, FALSE, 0U, 1U, 0U, TRUE, FALSE, 0U) == STD_E_SEGV, "VLDR.64 second word");
	CHECK(fpls(arm_pseudo_vstr, &core, ARM_COND_AL, FALSE, 0U, 1U, 0U, TRUE, FALSE, 0U) == STD_E_SEGV, "VSTR.64 second word");
	core.r[1] = MEM(0x22U);
	CHECK(fpls(arm_pseudo_vstr, &core, ARM_COND_AL, FALSE, 0U, 1U, 0U, TRUE, FALSE, 0U) == STD_E_SEGV, "VSTR.64 unaligned");
	CHECK(fpls(arm_pseudo_vldm, &core, ARM_COND_AL, TRUE, 0U, 1U, 8U, TRUE, FALSE, 2U) == STD_E_SEGV, "VLDM unaligned");
	CHECK(fpls(arm_pseudo_vstm, &core, ARM_COND_AL, TRUE, 0U, 1U, 8U, TRUE, FALSE, 2U) == STD_E_SEGV, "VSTM unaligned");
}

/* Every instruction with a failing condition (NE with Z set) changes nothing. */
static void test_condition_failed(void)
{
	TargetCoreType core;
	TargetCoreType before;

	init(&core);
	core.r[1] = MEM(0x20U);
	core.apsr = ARM_APSR_Z;
	before = core;
	CHECK(fp(arm_pseudo_vmov_imm, &core, 0x1U, TRUE, 0U, 0U, 0U) == STD_E_OK, "skip");
	CHECK(fp(arm_pseudo_vmov_reg, &core, 0x1U, TRUE, 0U, 0U, 1U) == STD_E_OK, "skip");
	CHECK(fp(arm_pseudo_vabs, &core, 0x1U, FALSE, 0U, 0U, 1U) == STD_E_OK, "skip");
	CHECK(fp(arm_pseudo_vneg, &core, 0x1U, FALSE, 0U, 0U, 1U) == STD_E_OK, "skip");
	CHECK(fp(arm_pseudo_vsel, &core, 0x1U, FALSE, 0U, 2U, 1U) == STD_E_OK, "skip");
	CHECK(fpcore(arm_pseudo_vmov_core_s, &core, 0x1U, TRUE, 3U, 0U, 1U, 0U) == STD_E_OK, "skip");
	CHECK(fpcore(arm_pseudo_vmov_core_ss, &core, 0x1U, FALSE, 3U, 4U, 1U, 0U) == STD_E_OK, "skip");
	CHECK(fpcore(arm_pseudo_vmov_core_d, &core, 0x1U, TRUE, 3U, 4U, 1U, 0U) == STD_E_OK, "skip");
	CHECK(fpcore(arm_pseudo_vmov_core_scalar, &core, 0x1U, FALSE, 3U, 0U, 1U, 1U) == STD_E_OK, "skip");
	CHECK(fpcore(arm_pseudo_vmrs_vmsr, &core, 0x1U, FALSE, 3U, 0U, 0U, 0U) == STD_E_OK, "skip");
	CHECK(fpls(arm_pseudo_vldr, &core, 0x1U, TRUE, 0U, 1U, 0U, TRUE, FALSE, 0U) == STD_E_OK, "skip");
	CHECK(fpls(arm_pseudo_vstr, &core, 0x1U, TRUE, 0U, 1U, 0U, TRUE, FALSE, 0U) == STD_E_OK, "skip");
	CHECK(fpls(arm_pseudo_vldm, &core, 0x1U, TRUE, 0U, 1U, 8U, TRUE, TRUE, 2U) == STD_E_OK, "skip");
	CHECK(fpls(arm_pseudo_vstm, &core, 0x1U, TRUE, 0U, 1U, 8U, TRUE, TRUE, 2U) == STD_E_OK, "skip");
	CHECK(memcmp(&core, &before, sizeof(core)) == 0, "an instruction with a failing condition changed the core");
}

int main(void)
{
	test_expand_imm();
	test_register_ops();
	test_core_moves();
	test_loads_stores();
	test_condition_failed();
	printf("%d checks, %d failures\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
