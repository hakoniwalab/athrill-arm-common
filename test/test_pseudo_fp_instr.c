/*
 * Tests of the floating-point instructions around arm_fp.h (whose functions
 * test_fp.c checks): which registers they read and write, the halves of
 * VCVTB/VCVTT, the extension of fixed-point results, VCMP's flags, the
 * rounding each takes, FPSCR's exception bits and a failing condition.
 * The values are worked out by hand.
 */
#include <stdio.h>
#include <string.h>
#include "arm_common/arm_pseudo_fp_convert.h"

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
#define ONE_F	0x3F800000U			/* 1.0f */
#define ONE_D	0x3FF0000000000000ULL		/* 1.0 */
#define RM_RZ	(3U << ARM_FPSCR_BITPOS_RMODE)

typedef Std_ReturnType (*FpOp)(TargetCoreType *, const arm_fp_input_type *, arm_fp_output_type *);
typedef Std_ReturnType (*CvtOp)(TargetCoreType *, const arm_fp_cvt_input_type *, arm_fp_output_type *);

static Std_ReturnType fp(FpOp op, TargetCoreType *core, uint32 cond, bool dp, uint32 d, uint32 n, uint32 m)
{
	arm_fp_input_type in;
	arm_fp_output_type out;

	memset(&in, 0, sizeof(in));
	in.instrName = "VFP";
	in.cond = cond;
	in.dp_operation = dp;
	in.d = d;
	in.n = n;
	in.m = m;
	return op(core, &in, &out);
}

static arm_fp_cvt_input_type cvt_in(bool dp, uint32 d, uint32 m)
{
	arm_fp_cvt_input_type in;

	memset(&in, 0, sizeof(in));
	in.instrName = "VCVT";
	in.cond = ARM_COND_AL;
	in.dp_operation = dp;
	in.d = d;
	in.m = m;
	in.size = 32U;
	in.rounding = FPRounding_ZERO;
	return in;
}

static Std_ReturnType cvt(CvtOp op, TargetCoreType *core, const arm_fp_cvt_input_type *in)
{
	arm_fp_output_type out;

	return op(core, in, &out);
}

static void init(TargetCoreType *core)
{
	test_core_init(core, TestProfile_M, PC0);
}

static void test_arith(void)
{
	TargetCoreType core;

	init(&core);
	/* VADD.F32 s2, s0, s1: 1 + 1 = 2 */
	core.s[0] = ONE_F;
	core.s[1] = ONE_F;
	CHECK(fp(arm_pseudo_vadd, &core, ARM_COND_AL, FALSE, 2U, 0U, 1U) == STD_E_OK && core.s[2] == 0x40000000U, "VADD.F32");
	/* VMLA.F32 s2, s0, s1: 2 + 1*1 = 3; VMLS: 3 - 1 = 2; VNMLA: -2 - 1 = -3; VNMLS: 3 + 1 = 4... from -(-3) */
	CHECK(fp(arm_pseudo_vmla, &core, ARM_COND_AL, FALSE, 2U, 0U, 1U) == STD_E_OK && core.s[2] == 0x40400000U, "VMLA.F32");
	CHECK(fp(arm_pseudo_vmls, &core, ARM_COND_AL, FALSE, 2U, 0U, 1U) == STD_E_OK && core.s[2] == 0x40000000U, "VMLS.F32");
	CHECK(fp(arm_pseudo_vnmla, &core, ARM_COND_AL, FALSE, 2U, 0U, 1U) == STD_E_OK && core.s[2] == 0xC0400000U, "VNMLA.F32");
	CHECK(fp(arm_pseudo_vnmls, &core, ARM_COND_AL, FALSE, 2U, 0U, 1U) == STD_E_OK && core.s[2] == 0x40800000U, "VNMLS.F32 %08x", core.s[2]);
	/* VFMA, VFMS, VFNMA, VFNMS on d2 = 4: 4+1, 5-1, -4-1, -(-5)+1 */
	core.s[0] = 0U;
	core.s[1] = 0x3FF00000U;	/* d0 = 1.0 */
	core.s[4] = 0U;
	core.s[5] = 0x40100000U;	/* d2 = 4.0 */
	CHECK(fp(arm_pseudo_vfma, &core, ARM_COND_AL, TRUE, 2U, 0U, 0U) == STD_E_OK && core.s[5] == 0x40140000U, "VFMA.F64");
	CHECK(fp(arm_pseudo_vfms, &core, ARM_COND_AL, TRUE, 2U, 0U, 0U) == STD_E_OK && core.s[5] == 0x40100000U, "VFMS.F64");
	CHECK(fp(arm_pseudo_vfnma, &core, ARM_COND_AL, TRUE, 2U, 0U, 0U) == STD_E_OK && core.s[5] == 0xC0140000U, "VFNMA.F64");
	CHECK(fp(arm_pseudo_vfnms, &core, ARM_COND_AL, TRUE, 2U, 0U, 0U) == STD_E_OK && core.s[5] == 0x40180000U, "VFNMS.F64 %08x", core.s[5]);
	/* VSUB, VMUL, VNMUL, VDIV, VSQRT on doubles: 6-1, 5*1, -(5*1), 1/(-5)... */
	CHECK(fp(arm_pseudo_vsub, &core, ARM_COND_AL, TRUE, 2U, 2U, 0U) == STD_E_OK && core.s[5] == 0x40140000U, "VSUB.F64");
	CHECK(fp(arm_pseudo_vmul, &core, ARM_COND_AL, TRUE, 3U, 2U, 0U) == STD_E_OK && core.s[7] == 0x40140000U, "VMUL.F64");
	CHECK(fp(arm_pseudo_vnmul, &core, ARM_COND_AL, TRUE, 3U, 2U, 0U) == STD_E_OK && core.s[7] == 0xC0140000U, "VNMUL.F64");
	core.s[8] = 0U;
	core.s[9] = 0x40100000U;	/* d4 = 4.0 */
	CHECK(fp(arm_pseudo_vsqrt, &core, ARM_COND_AL, TRUE, 3U, 0U, 4U) == STD_E_OK && core.s[7] == 0x40000000U, "VSQRT.F64");
	CHECK(fp(arm_pseudo_vdiv, &core, ARM_COND_AL, TRUE, 3U, 0U, 4U) == STD_E_OK && core.s[7] == 0x3FD00000U, "VDIV.F64 1/4");
	/* 1/0: infinity and DZC */
	core.s[8] = 0U;
	core.s[9] = 0U;
	CHECK(fp(arm_pseudo_vdiv, &core, ARM_COND_AL, TRUE, 3U, 0U, 4U) == STD_E_OK && core.s[7] == 0x7FF00000U
	      && core.fpscr == ARM_FPSCR_DZC, "VDIV.F64 1/0 FPSCR %08x", core.fpscr);
}

static void test_compare(void)
{
	TargetCoreType core;

	init(&core);
	core.fpscr = ARM_FPSCR_IXC;
	core.s[0] = ONE_F;
	core.s[1] = 0x40000000U;
	CHECK(fp(arm_pseudo_vcmp, &core, ARM_COND_AL, FALSE, 0U, 0U, 1U) == STD_E_OK && core.fpscr == (0x80000000U | ARM_FPSCR_IXC),
	      "VCMP 1, 2: N %08x", core.fpscr);
	CHECK(fp(arm_pseudo_vcmp_zero, &core, ARM_COND_AL, FALSE, 0U, 0U, 1U) == STD_E_OK && core.fpscr == (0x20000000U | ARM_FPSCR_IXC),
	      "VCMP 1, #0: C");
	core.s[1] = 0x7FC00000U;
	CHECK(fp(arm_pseudo_vcmp, &core, ARM_COND_AL, FALSE, 0U, 0U, 1U) == STD_E_OK && core.fpscr == (0x30000000U | ARM_FPSCR_IXC),
	      "VCMP with a quiet NaN: C V, no IOC");
	CHECK(fp(arm_pseudo_vcmpe, &core, ARM_COND_AL, FALSE, 0U, 0U, 1U) == STD_E_OK
	      && core.fpscr == (0x30000000U | ARM_FPSCR_IXC | ARM_FPSCR_IOC), "VCMPE with a quiet NaN: IOC");
	core.s[0] = 0x80000000U;	/* -0 == #0 */
	CHECK(fp(arm_pseudo_vcmpe_zero, &core, ARM_COND_AL, FALSE, 0U, 0U, 1U) == STD_E_OK && (core.fpscr >> 28) == 0x6U, "VCMPE -0, #0: Z C");
	/* VCMP.F64 d1, d2: 2.0 > 1.0 */
	core.s[2] = 0U;
	core.s[3] = 0x40000000U;
	core.s[4] = 0U;
	core.s[5] = 0x3FF00000U;
	CHECK(fp(arm_pseudo_vcmp, &core, ARM_COND_AL, TRUE, 1U, 0U, 2U) == STD_E_OK && (core.fpscr >> 28) == 0x2U, "VCMP.F64 2, 1: C");
}

static void test_convert(void)
{
	TargetCoreType core;
	arm_fp_cvt_input_type in;

	init(&core);
	/* VCVT.F64.F32 d1, s0 and back to s5 */
	core.s[0] = ONE_F;
	in = cvt_in(FALSE, 1U, 0U);
	CHECK(cvt(arm_pseudo_vcvt_fp, &core, &in) == STD_E_OK && core.s[2] == 0U && core.s[3] == 0x3FF00000U, "VCVT.F64.F32");
	in = cvt_in(TRUE, 5U, 1U);
	CHECK(cvt(arm_pseudo_vcvt_fp, &core, &in) == STD_E_OK && core.s[5] == ONE_F, "VCVT.F32.F64");
	/* VCVTT.F16.F32 s6, s0: 1.0 into the top half (0x3C00), the bottom half kept */
	core.s[6] = 0x12345678U;
	in = cvt_in(FALSE, 6U, 0U);
	in.top = TRUE;
	CHECK(cvt(arm_pseudo_vcvt_to_half, &core, &in) == STD_E_OK && core.s[6] == 0x3C005678U, "VCVTT.F16.F32 %08x", core.s[6]);
	in.top = FALSE;
	CHECK(cvt(arm_pseudo_vcvt_to_half, &core, &in) == STD_E_OK && core.s[6] == 0x3C003C00U, "VCVTB.F16.F32");
	/* VCVTB.F64.F16 d4, s7: 0xC000 = -2.0; VCVTT.F32.F16 s23, s7: 0x3800 = 0.5 */
	core.s[7] = 0x3800C000U;
	in = cvt_in(TRUE, 4U, 7U);
	CHECK(cvt(arm_pseudo_vcvt_from_half, &core, &in) == STD_E_OK && core.s[8] == 0U && core.s[9] == 0xC0000000U, "VCVTB.F64.F16");
	in = cvt_in(FALSE, 23U, 7U);
	in.top = TRUE;
	CHECK(cvt(arm_pseudo_vcvt_from_half, &core, &in) == STD_E_OK && core.s[23] == 0x3F000000U, "VCVTT.F32.F16");
	in = cvt_in(TRUE, 1U, 4U);
	CHECK(cvt(arm_pseudo_vcvt_to_half, &core, &in) == STD_E_OK && core.s[1] == 0x0000C000U, "VCVTB.F16.F64 %08x", core.s[1]);
	/* VCVT.S32.F64 s10, d4 = -2; VCVTR with FPSCR's rounding; VCVTA */
	in = cvt_in(TRUE, 10U, 4U);
	CHECK(cvt(arm_pseudo_vcvt_to_int, &core, &in) == STD_E_OK && core.s[10] == 0xFFFFFFFEU, "VCVT.S32.F64");
	core.s[11] = 0x3FC00000U;	/* 1.5f */
	in = cvt_in(FALSE, 12U, 11U);
	in.fpscr_rounding = TRUE;
	core.fpscr = 0U;
	CHECK(cvt(arm_pseudo_vcvt_to_int, &core, &in) == STD_E_OK && core.s[12] == 2U && core.fpscr == ARM_FPSCR_IXC, "VCVTR RN 1.5");
	core.fpscr = RM_RZ;
	CHECK(cvt(arm_pseudo_vcvt_to_int, &core, &in) == STD_E_OK && core.s[12] == 1U, "VCVTR RZ 1.5");
	in.fpscr_rounding = FALSE;
	in.rounding = FPRounding_TIEAWAY;
	CHECK(cvt(arm_pseudo_vcvt_to_int, &core, &in) == STD_E_OK && core.s[12] == 2U, "VCVTA 1.5");
	/* VCVT.F64.U32 d7, s13: 0xFFFFFFFF */
	core.s[13] = 0xFFFFFFFFU;
	in = cvt_in(TRUE, 7U, 13U);
	in.unsigned_int = TRUE;
	CHECK(cvt(arm_pseudo_vcvt_from_int, &core, &in) == STD_E_OK && core.s[14] == 0xFFE00000U && core.s[15] == 0x41EFFFFFU,
	      "VCVT.F64.U32 %08x:%08x", core.s[15], core.s[14]);
	/* VCVT.S16.F64 d8, d8, #1: -2.0 -> -4, sign-extended to 64 bits */
	core.s[16] = 0U;
	core.s[17] = 0xC0000000U;
	in = cvt_in(TRUE, 8U, 0U);
	in.size = 16U;
	in.fraction_bits = 1U;
	CHECK(cvt(arm_pseudo_vcvt_to_fixed, &core, &in) == STD_E_OK && core.s[16] == 0xFFFFFFFCU && core.s[17] == 0xFFFFFFFFU,
	      "VCVT.S16.F64 #1 %08x:%08x", core.s[17], core.s[16]);
	/* ... and back: -4 / 2 = -2.0 */
	CHECK(cvt(arm_pseudo_vcvt_from_fixed, &core, &in) == STD_E_OK && core.s[16] == 0U && core.s[17] == 0xC0000000U, "VCVT.F64.S16 #1");
	/* VCVT.U32.F32 s18, s18, #0 of 3.0; from fixed: round to nearest even under RZ (0x01000001 -> 0x4B800000 + ...) */
	core.s[18] = 0x40400000U;
	in = cvt_in(FALSE, 18U, 0U);
	in.unsigned_int = TRUE;
	CHECK(cvt(arm_pseudo_vcvt_to_fixed, &core, &in) == STD_E_OK && core.s[18] == 3U, "VCVT.U32.F32 #0");
	core.s[18] = 0x01000003U;	/* 2^24 + 3: rounds to 2^24 + 4 to nearest, 2^24 + 2 towards zero */
	core.fpscr = RM_RZ;
	CHECK(cvt(arm_pseudo_vcvt_from_fixed, &core, &in) == STD_E_OK && core.s[18] == 0x4B800002U, "VCVT.F32.U32 #0 to nearest %08x",
	      core.s[18]);
	/* VRINTZ.F32 s19, s11 (1.5) = 1.0; VRINTX in RN = 2.0 with IXC */
	in = cvt_in(FALSE, 19U, 11U);
	CHECK(cvt(arm_pseudo_vrint, &core, &in) == STD_E_OK && core.s[19] == ONE_F, "VRINTZ");
	in.fpscr_rounding = TRUE;
	in.exact = TRUE;
	core.fpscr = 0U;
	CHECK(cvt(arm_pseudo_vrint, &core, &in) == STD_E_OK && core.s[19] == 0x40000000U && core.fpscr == ARM_FPSCR_IXC, "VRINTX");
	/* VMAXNM, VMINNM with a quiet NaN: the number */
	core.s[20] = 0x7FC00000U;
	core.s[21] = ONE_F;
	CHECK(fp(arm_pseudo_vmaxnm, &core, ARM_COND_AL, FALSE, 22U, 20U, 21U) == STD_E_OK && core.s[22] == ONE_F, "VMAXNM");
	CHECK(fp(arm_pseudo_vminnm, &core, ARM_COND_AL, FALSE, 22U, 21U, 20U) == STD_E_OK && core.s[22] == ONE_F, "VMINNM");
	/* the other precision of each: VSQRT.F32 4 = 2, VCVT.F32.S32 -1, VCVT.S32.F32 #1 / VCVT.F32.S32 #1 in place,
	 * VRINTZ.F64 */
	core.fpscr = 0U;
	core.s[24] = 0x40800000U;
	CHECK(fp(arm_pseudo_vsqrt, &core, ARM_COND_AL, FALSE, 25U, 0U, 24U) == STD_E_OK && core.s[25] == 0x40000000U, "VSQRT.F32");
	core.s[24] = 0xFFFFFFFFU;
	in = cvt_in(FALSE, 25U, 24U);
	CHECK(cvt(arm_pseudo_vcvt_from_int, &core, &in) == STD_E_OK && core.s[25] == 0xBF800000U, "VCVT.F32.S32");
	in = cvt_in(FALSE, 25U, 0U);
	in.fraction_bits = 1U;
	CHECK(cvt(arm_pseudo_vcvt_to_fixed, &core, &in) == STD_E_OK && core.s[25] == 0xFFFFFFFEU, "VCVT.S32.F32 #1");
	CHECK(cvt(arm_pseudo_vcvt_from_fixed, &core, &in) == STD_E_OK && core.s[25] == 0xBF800000U, "VCVT.F32.S32 #1");
	core.s[26] = 0U;
	core.s[27] = 0x3FF80000U;	/* d13 = 1.5 */
	in = cvt_in(TRUE, 13U, 13U);
	CHECK(cvt(arm_pseudo_vrint, &core, &in) == STD_E_OK && core.s[26] == 0U && core.s[27] == 0x3FF00000U, "VRINTZ.F64");
	/* VMAXNM.F64 d14, d13, d0 (1.5 and 0 bits of d0) */
	core.s[0] = 0U;
	core.s[1] = 0xBFF00000U;	/* d0 = -1.0 */
	core.s[27] = 0x3FF80000U;	/* d13 = 1.5 */
	CHECK(fp(arm_pseudo_vmaxnm, &core, ARM_COND_AL, TRUE, 14U, 13U, 0U) == STD_E_OK && core.s[29] == 0x3FF80000U, "VMAXNM.F64");
	(void)ONE_D;
}

/* Every instruction with a failing condition (NE with Z set) changes nothing. */
static void test_condition_failed(void)
{
	static const FpOp fp_ops[] = {
		arm_pseudo_vadd, arm_pseudo_vsqrt, arm_pseudo_vcmp, arm_pseudo_vcmpe_zero, arm_pseudo_vmaxnm,
	};
	static const CvtOp cvt_ops[] = {
		arm_pseudo_vcvt_fp, arm_pseudo_vcvt_from_half, arm_pseudo_vcvt_to_half, arm_pseudo_vcvt_to_int,
		arm_pseudo_vcvt_from_int, arm_pseudo_vcvt_to_fixed, arm_pseudo_vcvt_from_fixed, arm_pseudo_vrint,
	};
	TargetCoreType core;
	TargetCoreType before;
	arm_fp_cvt_input_type in;
	uint32 i;

	init(&core);
	for (i = 0U; i < 32U; i++) {
		core.s[i] = 0x3F800000U + i;
	}
	core.apsr = ARM_APSR_Z;
	before = core;
	for (i = 0U; i < sizeof(fp_ops) / sizeof(fp_ops[0]); i++) {
		CHECK(fp(fp_ops[i], &core, 0x1U, FALSE, 0U, 1U, 2U) == STD_E_OK, "skip");
	}
	in = cvt_in(FALSE, 0U, 1U);
	in.cond = 0x1U;
	for (i = 0U; i < sizeof(cvt_ops) / sizeof(cvt_ops[0]); i++) {
		CHECK(cvt(cvt_ops[i], &core, &in) == STD_E_OK, "skip");
	}
	CHECK(memcmp(&core, &before, sizeof(core)) == 0, "an instruction with a failing condition changed the core");
}

int main(void)
{
	test_arith();
	test_compare();
	test_convert();
	test_condition_failed();
	printf("%d checks, %d failures\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
