/*
 * Tests of the host fast path (arm_fp_host.h) against the pseudo-code
 * (arm_fp.h): for random operands of every arithmetic operation, both sizes
 * and the FPSCR settings, where the fast path accepts, its result and FPSCR
 * must be the pseudo-code's bit for bit; where it declines, FPSCR must be
 * unchanged. The operands are drawn to reach the edges: exponents near the
 * smallest and largest normal numbers, operands close to each other
 * (cancellation), zeros, denormals, infinities and NaNs.
 */
#include <stdio.h>
#include <string.h>
#include "arm_common/arm_pseudo_fp_arith.h"

static int failures;
static long checks;

static uint64 rng_state = 0x9E3779B97F4A7C15ULL;

static uint64 rnd(void)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 7;
	rng_state ^= rng_state << 17;
	return rng_state;
}

/* An operand of N bits; near: a value close to it is drawn sometimes. */
static uint64 operand(uint32 N, uint64 near)
{
	uint32 ebits = (N == 32U) ? 8U : 11U;
	uint32 fbits = (N == 32U) ? 23U : 52U;
	uint64 emax = ((uint64)1U << ebits) - 1U;
	uint64 sign = rnd() & 1U;
	uint64 exp;
	uint64 frac = rnd() & (((uint64)1U << fbits) - 1U);
	uint32 kind = (uint32)(rnd() % 16U);

	switch (kind) {
	case 0U:	/* zero */
		exp = 0U;
		frac = 0U;
		break;
	case 1U:	/* denormal */
		exp = 0U;
		break;
	case 2U:	/* infinity or NaN */
		exp = emax;
		frac = ((rnd() & 1U) != 0U) ? 0U : frac;
		break;
	case 3U:	/* near the smallest normal */
		exp = 1U + (rnd() % 30U);
		break;
	case 4U:	/* near the largest */
		exp = emax - 1U - (rnd() % 30U);
		break;
	case 7U:
	case 8U:	/* few significant bits: exact results */
		exp = (emax / 2U) - 8U + (rnd() % 16U);
		frac &= ~(((uint64)1U << (fbits - 4U)) - 1U);
		break;
	case 5U:
	case 6U:	/* close to near: cancellation */
		return near ^ (rnd() & 0xFFU) ^ (((rnd() & 3U) == 0U) ? ((uint64)1U << (N - 1U)) : 0U);
	default:	/* ordinary values */
		exp = (emax / 2U) - 40U + (rnd() % 80U);
		break;
	}
	return (sign << (N - 1U)) | (exp << fbits) | frac;
}

static uint64 strict(uint32 op, uint64 d, uint64 n, uint64 m, uint32 N, uint32 *fpscr)
{
	if (op == ARM_FP_HOST_SQRT) {
		return FPSqrt(m, N, fpscr);
	}
	return ArmFPOperation((ArmFPOp)op, d, n, m, N, fpscr);
}

int main(void)
{
	static const uint32 ops[] = {
		ArmFPOp_VADD, ArmFPOp_VSUB, ArmFPOp_VMUL, ArmFPOp_VNMUL, ArmFPOp_VMLA, ArmFPOp_VMLS,
		ArmFPOp_VNMLA, ArmFPOp_VNMLS, ArmFPOp_VDIV, ArmFPOp_VFMA, ArmFPOp_VFMS, ArmFPOp_VFNMA,
		ArmFPOp_VFNMS, ARM_FP_HOST_SQRT,
	};
	long accepted = 0;
	long declined = 0;
	long inexact = 0;
	long exact = 0;
	uint32 i;
	uint32 k;

	for (i = 0U; i < (sizeof(ops) / sizeof(ops[0])); i++) {
		for (k = 0U; k < 200000U; k++) {
			uint32 N = ((k & 1U) != 0U) ? 64U : 32U;
			/* RMode mostly round to nearest; FZ, DN and earlier cumulative bits at random */
			uint32 fpscr0 = (((rnd() % 8U) == 0U) ? ((uint32)(rnd() % 4U) << 22) : 0U)
				| (((rnd() & 1U) != 0U) ? ARM_FPSCR_FZ : 0U) | (((rnd() & 1U) != 0U) ? ARM_FPSCR_DN : 0U)
				| ((uint32)rnd() & 0x9FU);
			uint64 n = operand(N, 0U);
			uint64 m = operand(N, n);
			uint64 d = operand(N, (rnd() & 1U) ? n : m);
			uint32 f_strict = fpscr0;
			uint32 f_host = fpscr0;
			uint64 r_strict = strict(ops[i], d, n, m, N, &f_strict);
			uint64 r_host = 0U;

			checks++;
			if (ArmFPHostOperation(ops[i], d, n, m, N, &f_host, &r_host)) {
				accepted++;
				if (!(fpscr0 & ARM_FPSCR_IXC)) {
					if (f_host & ARM_FPSCR_IXC) {
						inexact++;
					}
					else {
						exact++;
					}
				}
				if ((r_host != r_strict) || (f_host != f_strict)) {
					failures++;
					if (failures < 20) {
						printf("FAIL op %u N %u d %llx n %llx m %llx fpscr %x: host %llx/%x strict %llx/%x\n",
							ops[i], N, (unsigned long long)d, (unsigned long long)n, (unsigned long long)m,
							fpscr0, (unsigned long long)r_host, f_host, (unsigned long long)r_strict, f_strict);
					}
				}
			}
			else {
				declined++;
				if (f_host != fpscr0) {
					failures++;
					printf("FAIL op %u: declined but FPSCR changed %x -> %x\n", ops[i], fpscr0, f_host);
				}
			}
		}
	}
	printf("accepted %ld (inexact %ld, exact %ld, where IXC was clear), declined %ld\n", accepted, inexact, exact, declined);
	printf("%ld checks, %d failures\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
