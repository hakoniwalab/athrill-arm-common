/*
 * Tests of the floating-point functions (arm_fp.h) against the cases of
 * test/model_fp.py, an independent model of the same pseudo-code in exact
 * rational arithmetic: the result and the FPSCR after it (the cumulative
 * exception bits) for each operation, size, rounding mode, FZ and DN.
 */
#include <stdio.h>
#include "arm_common/arm_fp.h"

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

typedef enum {
	FPT_ADD = 0,
	FPT_SUB,
	FPT_MUL,
	FPT_DIV,
	FPT_SQRT,
	FPT_MULADD,
	FPT_CMP,
	FPT_CMPE,
	FPT_CVT,	/* b: the target size */
	FPT_TOFIXED,	/* b: M | fbits << 8 | unsigned << 16 | rounding << 20 */
	FPT_FROMFIXED,	/* b: M | fbits << 8 | unsigned << 16 | round_to_nearest << 17; N: the target size */
	FPT_ROUNDINT,	/* b: rounding | exact << 4 */
	FPT_MAXNUM,
	FPT_MINNUM,
} FPTOp;

static const char *const op_names[] = {
	"ADD", "SUB", "MUL", "DIV", "SQRT", "MULADD", "CMP", "CMPE", "CVT", "TOFIXED", "FROMFIXED", "ROUNDINT", "MAXNUM", "MINNUM",
};

typedef struct {
	FPTOp op;
	uint32 N;
	uint64 a;
	uint64 b;
	uint64 c;
	uint32 fpscr_before;
	uint64 result;
	uint32 fpscr_after;
} FPCase;

static const FPCase cases[] = {
#include "fp_cases.inc"
};

static uint64 run(const FPCase *t, uint32 *fpscr)
{
	switch (t->op) {
	case FPT_ADD:
		return FPAdd(t->a, t->b, t->N, fpscr);
	case FPT_SUB:
		return FPSub(t->a, t->b, t->N, fpscr);
	case FPT_MUL:
		return FPMul(t->a, t->b, t->N, fpscr);
	case FPT_DIV:
		return FPDiv(t->a, t->b, t->N, fpscr);
	case FPT_SQRT:
		return FPSqrt(t->a, t->N, fpscr);
	case FPT_MULADD:
		return FPMulAdd(t->c, t->a, t->b, t->N, fpscr);
	case FPT_CMP:
		return FPCompare(t->a, t->b, FALSE, t->N, fpscr);
	case FPT_CMPE:
		return FPCompare(t->a, t->b, TRUE, t->N, fpscr);
	case FPT_CVT:
		return FPConvert(t->a, t->N, (uint32)t->b, fpscr);
	case FPT_TOFIXED:
		return FPToFixed(t->a, t->N, (uint32)(t->b & 0xFFU), (uint32)((t->b >> 8) & 0xFFU), ((t->b >> 16) & 1U) != 0U,
			(FPRounding)((t->b >> 20) & 7U), fpscr);
	case FPT_FROMFIXED:
		return FixedToFP((uint32)t->a, (uint32)(t->b & 0xFFU), t->N, (uint32)((t->b >> 8) & 0xFFU), ((t->b >> 16) & 1U) != 0U,
			((t->b >> 17) & 1U) != 0U, fpscr);
	case FPT_ROUNDINT:
		return FPRoundInt(t->a, t->N, (FPRounding)(t->b & 7U), ((t->b >> 4) & 1U) != 0U, fpscr);
	case FPT_MAXNUM:
		return FPMaxNum(t->a, t->b, t->N, fpscr);
	default:
		return FPMinNum(t->a, t->b, t->N, fpscr);
	}
}

/* The 128-bit helpers at their edges. */
static void test_u128(void)
{
	ArmU128 a = ArmU128Make(0x8000000000000001ULL, 0x0000000000000003ULL);
	ArmU128 r;

	r = ArmU128ShiftLeft(a, 0U);
	CHECK(r.hi == a.hi && r.lo == a.lo, "<< 0");
	r = ArmU128ShiftLeft(a, 64U);
	CHECK(r.hi == 3U && r.lo == 0U, "<< 64");
	r = ArmU128ShiftLeft(a, 128U);
	CHECK(ArmU128IsZero(r), "<< 128");
	r = ArmU128ShiftRight(a, 0U);
	CHECK(r.hi == a.hi && r.lo == a.lo, ">> 0");
	r = ArmU128ShiftRight(a, 128U);
	CHECK(ArmU128IsZero(r), ">> 128");
	r = ArmU128ShiftRight(a, 64U);
	CHECK(r.hi == 0U && r.lo == a.hi, ">> 64");
	r = ArmU128ShiftRightJam(a, 0U);
	CHECK(r.hi == a.hi && r.lo == a.lo, ">> 0 jam");
	r = ArmU128ShiftRightJam(a, 1U);
	CHECK(r.hi == 0x4000000000000000ULL && r.lo == 0x8000000000000001ULL, ">> 1 jam");
	r = ArmU128ShiftRightJam(a, 64U);
	CHECK(r.hi == 0U && r.lo == 0x8000000000000001ULL, ">> 64 jam");
	r = ArmU128ShiftRightJam(ArmU128Make(1U, 0U), 65U);
	CHECK(r.hi == 0U && r.lo == 1U, ">> 65 of 2^64: the bit is lost");
	r = ArmU128ShiftRightJam(ArmU128Make(4U, 0U), 65U);
	CHECK(r.hi == 0U && r.lo == 2U, ">> 65 of 2^66: nothing lost");
	r = ArmU128ShiftRightJam(a, 200U);
	CHECK(r.hi == 0U && r.lo == 1U, ">> 200 jam");
	r = ArmU128ShiftRightJam(ArmU128Make(0U, 0U), 200U);
	CHECK(ArmU128IsZero(r), ">> 200 of 0");
	r = ArmU128Mul64(0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL);
	CHECK(r.hi == 0xFFFFFFFFFFFFFFFEULL && r.lo == 1U, "(2^64-1)^2");
	CHECK(ArmU128Msb(ArmU128Make(1U, 0U)) == 64U && ArmU128Msb(ArmU128Make(0U, 1U)) == 0U, "msb");
	CHECK(FPNeg(0x7FC00000U, 32U) == 0xFFC00000U && FPNeg(0x8000000000000000ULL, 64U) == 0U, "FPNeg");
}

int main(void)
{
	uint32 i;

	test_u128();

	for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
		const FPCase *t = &cases[i];
		uint32 fpscr = t->fpscr_before;
		uint64 result = run(t, &fpscr);

		CHECK(result == t->result && fpscr == t->fpscr_after,
		      "%s.F%u #%u (%016llx, %016llx, %016llx, FPSCR %08x): %016llx FPSCR %08x, expected %016llx FPSCR %08x",
		      op_names[t->op], t->N, i, (unsigned long long)t->a, (unsigned long long)t->b, (unsigned long long)t->c,
		      t->fpscr_before, (unsigned long long)result, fpscr, (unsigned long long)t->result, t->fpscr_after);
	}
	printf("%d checks, %d failures\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
