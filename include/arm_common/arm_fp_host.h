/*
 * The fast path of the floating-point arithmetic instructions: the host's
 * FPU computes the result where it is guaranteed to give the same bits and
 * the same FPSCR as the pseudo-code of arm_fp.h, and declines everywhere
 * else, where the caller computes with the pseudo-code.
 *
 * Why the bits are the same: IEEE 754 defines the result of +, -, *, /,
 * sqrt and the fused multiply-add as the exact result rounded once, which
 * is also what FPRound() does. The cases where the Arm pseudo-code differs
 * from a plain IEEE host are all declined:
 *   - a rounding mode other than round to nearest (FPSCR.RMode != 0);
 *   - an operand that is a NaN, an infinity or a denormal (FPUnpack's FZ,
 *     IDC, the NaN rules and DN);
 *   - an intermediate product (VMLA ...) or a result that is not a normal
 *     number or an exact zero (overflow, underflow, flush to zero and
 *     their flags);
 * so only IXC can be raised. Whether the result is inexact is worked out
 * exactly, without the host's exception flags (writing them is slow on
 * some hosts): the rounding error of a sum by TwoSum, of a product, a
 * quotient or a square root by a fused multiply-add, which are exact while
 * the error is representable (results with a small exponent are declined
 * for that); a single-precision fused multiply-add is checked in double
 * precision, where the product is exact. Double-precision fused
 * multiply-adds are declined.
 *
 * This header is included by arm_pseudo_fp_arith.h after ArmFPOp. The host
 * code must not contract a * b + c into a fused operation (VMLA rounds the
 * product, TwoSum needs every rounding): contraction is turned off here.
 * The host's fma()/fmaf() must be correctly rounded (a hardware FMA, or the
 * C library's exact one).
 */
#ifndef ARM_COMMON_ARM_FP_HOST_H
#define ARM_COMMON_ARM_FP_HOST_H

#include <math.h>
#include <string.h>
#include "arm_fp.h"

#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

#define ARM_FP_HOST_SQRT	100U	/* the operation code of VSQRT (the others are ArmFPOp) */

/* The exponent field of an N-bit value. */
static inline uint32 ArmFPHostExp(uint64 v, uint32 N)
{
	return (N == 32U) ? (uint32)((v >> 23) & 0xFFU) : (uint32)((v >> 52) & 0x7FFU);
}

/* A normal number or a zero: the operands the host computes. */
static inline bool ArmFPHostOperand(uint64 v, uint32 N)
{
	uint32 exp = ArmFPHostExp(v, N);
	uint64 frac = (N == 32U) ? (v & 0x7FFFFFU) : (v & 0xFFFFFFFFFFFFFULL);

	if (exp == ((N == 32U) ? 0xFFU : 0x7FFU)) {
		return FALSE;
	}
	return (exp != 0U) || (frac == 0U);
}

static inline bool ArmFPHostIsZero(uint64 v, uint32 N)
{
	return ((N == 32U) ? (v & 0x7FFFFFFFU) : (v & 0x7FFFFFFFFFFFFFFFULL)) == 0U;
}

/*
 * A finite result whose exponent field is at least min_exp (2 or more: a
 * normal number that is not of the smallest exponent; larger where an error
 * term must stay representable), or a zero if zero_ok.
 */
static inline bool ArmFPHostResult(uint64 v, uint32 N, uint32 min_exp, bool zero_ok)
{
	uint32 exp = ArmFPHostExp(v, N);

	if (ArmFPHostIsZero(v, N)) {
		return zero_ok;
	}
	return (exp >= min_exp) && (exp != ((N == 32U) ? 0xFFU : 0x7FFU));
}

/*
 * One generator per size: T the host type, U the unsigned type of its bits,
 * P the precision in bits, FMA and SQRT the host's functions. Each check
 * returns FALSE to decline, else sets *inexact.
 */
#define ARM_FP_HOST_DEFINE(NAME, T, U, N, P, FMA, SQRT) \
static inline U NAME##_bits(T x) \
{ \
	U u; \
	memcpy(&u, &x, sizeof(u)); \
	return u; \
} \
static inline T NAME##_value(uint64 v) \
{ \
	U u = (U)v; \
	T x; \
	memcpy(&x, &u, sizeof(x)); \
	return x; \
} \
/* a + b: TwoSum's error is exact; a zero sum is exact */ \
static inline bool NAME##_add(T a, T b, T *r, bool *inexact) \
{ \
	T s = a + b; \
	T z = s - a; \
	T e = (a - (s - z)) + (b - z); \
	\
	if (!ArmFPHostResult((uint64)NAME##_bits(s), N, 2U, TRUE)) { \
		return FALSE; \
	} \
	*inexact = *inexact || (e != 0); \
	*r = s; \
	return TRUE; \
} \
/* a * b: the error fma(a, b, -p) is exact when p's exponent is P + 2 or more */ \
static inline bool NAME##_mul(T a, T b, T *r, bool *inexact) \
{ \
	T p = a * b; \
	\
	if ((a == 0) || (b == 0)) { \
		*r = p; \
		return TRUE; \
	} \
	if (!ArmFPHostResult((uint64)NAME##_bits(p), N, (P) + 2U, FALSE)) { \
		return FALSE; \
	} \
	*inexact = *inexact || (FMA(a, b, -p) != 0); \
	*r = p; \
	return TRUE; \
} \
/* a / b: the remainder fma(-q, b, a) is exact when a's exponent is P + 2 or more */ \
static inline bool NAME##_div(T a, T b, T *r, bool *inexact) \
{ \
	T q; \
	\
	if (b == 0) { \
		return FALSE; \
	} \
	q = a / b; \
	if (a == 0) { \
		*r = q; \
		return TRUE; \
	} \
	if ((ArmFPHostExp((uint64)NAME##_bits(a), N) < ((P) + 2U)) \
		|| !ArmFPHostResult((uint64)NAME##_bits(q), N, 2U, FALSE)) { \
		return FALSE; \
	} \
	*inexact = *inexact || (FMA(-q, b, a) != 0); \
	*r = q; \
	return TRUE; \
} \
/* sqrt(a): the error fma(-s, s, a) is exact when a's exponent is P + 2 or more */ \
static inline bool NAME##_sqrt(T a, T *r, bool *inexact) \
{ \
	T s; \
	\
	if (a == 0) { \
		*r = a; \
		return TRUE; \
	} \
	if ((a < 0) || (ArmFPHostExp((uint64)NAME##_bits(a), N) < ((P) + 2U))) { \
		return FALSE; \
	} \
	s = SQRT(a); \
	*inexact = *inexact || (FMA(-s, s, a) != 0); \
	*r = s; \
	return TRUE; \
}

ARM_FP_HOST_DEFINE(ArmFPHost32, float, uint32, 32U, 24U, fmaf, sqrtf)
ARM_FP_HOST_DEFINE(ArmFPHost64, double, uint64, 64U, 53U, fma, sqrt)

/*
 * a * b + c rounded once, in single precision: the product is exact in
 * double, TwoSum gives the exact sum as s + e, and the result is exact when
 * it equals it. A zero result is declined (it may be an underflow).
 */
static inline bool ArmFPHost32_fma(float a, float b, float c, float *r, bool *inexact)
{
	double p = (double)a * (double)b;
	double s = p + (double)c;
	double z = s - p;
	double e = (p - (s - z)) + ((double)c - z);
	float f = fmaf(a, b, c);

	if (!ArmFPHostResult((uint64)ArmFPHost32_bits(f), 32U, 2U, FALSE)) {
		return FALSE;
	}
	*inexact = *inexact || (e != 0.0) || ((double)f != s);
	*r = f;
	return TRUE;
}

static inline bool ArmFPHost32_op(uint32 op, float d, float n, float m, float *r, bool *inexact)
{
	float p;

	switch (op) {
	case ArmFPOp_VADD:
		return ArmFPHost32_add(n, m, r, inexact);
	case ArmFPOp_VSUB:
		return ArmFPHost32_add(n, -m, r, inexact);
	case ArmFPOp_VMUL:
		return ArmFPHost32_mul(n, m, r, inexact);
	case ArmFPOp_VNMUL:
		if (!ArmFPHost32_mul(n, m, r, inexact)) {
			return FALSE;
		}
		*r = -*r;
		return TRUE;
	case ArmFPOp_VMLA:
		return ArmFPHost32_mul(n, m, &p, inexact) && ArmFPHost32_add(d, p, r, inexact);
	case ArmFPOp_VMLS:
		return ArmFPHost32_mul(n, m, &p, inexact) && ArmFPHost32_add(d, -p, r, inexact);
	case ArmFPOp_VNMLA:
		return ArmFPHost32_mul(n, m, &p, inexact) && ArmFPHost32_add(-d, -p, r, inexact);
	case ArmFPOp_VNMLS:
		return ArmFPHost32_mul(n, m, &p, inexact) && ArmFPHost32_add(-d, p, r, inexact);
	case ArmFPOp_VDIV:
		return ArmFPHost32_div(n, m, r, inexact);
	case ArmFPOp_VFMA:
		return ArmFPHost32_fma(n, m, d, r, inexact);
	case ArmFPOp_VFMS:
		return ArmFPHost32_fma(-n, m, d, r, inexact);
	case ArmFPOp_VFNMA:
		return ArmFPHost32_fma(-n, m, -d, r, inexact);
	case ArmFPOp_VFNMS:
		return ArmFPHost32_fma(n, m, -d, r, inexact);
	case ARM_FP_HOST_SQRT:
		return ArmFPHost32_sqrt(m, r, inexact);
	default:
		return FALSE;
	}
}

static inline bool ArmFPHost64_op(uint32 op, double d, double n, double m, double *r, bool *inexact)
{
	double p;

	switch (op) {
	case ArmFPOp_VADD:
		return ArmFPHost64_add(n, m, r, inexact);
	case ArmFPOp_VSUB:
		return ArmFPHost64_add(n, -m, r, inexact);
	case ArmFPOp_VMUL:
		return ArmFPHost64_mul(n, m, r, inexact);
	case ArmFPOp_VNMUL:
		if (!ArmFPHost64_mul(n, m, r, inexact)) {
			return FALSE;
		}
		*r = -*r;
		return TRUE;
	case ArmFPOp_VMLA:
		return ArmFPHost64_mul(n, m, &p, inexact) && ArmFPHost64_add(d, p, r, inexact);
	case ArmFPOp_VMLS:
		return ArmFPHost64_mul(n, m, &p, inexact) && ArmFPHost64_add(d, -p, r, inexact);
	case ArmFPOp_VNMLA:
		return ArmFPHost64_mul(n, m, &p, inexact) && ArmFPHost64_add(-d, -p, r, inexact);
	case ArmFPOp_VNMLS:
		return ArmFPHost64_mul(n, m, &p, inexact) && ArmFPHost64_add(-d, p, r, inexact);
	case ArmFPOp_VDIV:
		return ArmFPHost64_div(n, m, r, inexact);
	case ARM_FP_HOST_SQRT:
		return ArmFPHost64_sqrt(m, r, inexact);
	default:
		return FALSE;	/* the fused multiply-adds: the pseudo-code */
	}
}

/*
 * The result of operation op (ArmFPOp, or ARM_FP_HOST_SQRT on m) on the
 * N-bit values d (the destination's old value), n, m by the host, with IXC
 * added to *fpscr; FALSE (and *fpscr unchanged) where the pseudo-code must
 * compute it.
 */
static inline bool ArmFPHostOperation(uint32 op, uint64 d, uint64 n, uint64 m, uint32 N, uint32 *fpscr, uint64 *result)
{
	bool uses_d = (op >= ArmFPOp_VMLA) && (op <= ArmFPOp_VFNMS) && (op != ArmFPOp_VDIV);
	bool inexact = FALSE;

	if (((*fpscr >> ARM_FPSCR_BITPOS_RMODE) & 3U) != 0U) {
		return FALSE;
	}
	if (!ArmFPHostOperand(m, N) || ((op != ARM_FP_HOST_SQRT) && !ArmFPHostOperand(n, N))
		|| (uses_d && !ArmFPHostOperand(d, N))) {
		return FALSE;
	}
	if (N == 32U) {
		float r;

		if (!ArmFPHost32_op(op, ArmFPHost32_value(d), ArmFPHost32_value(n), ArmFPHost32_value(m), &r, &inexact)) {
			return FALSE;
		}
		*result = (uint64)ArmFPHost32_bits(r);
	}
	else {
		double r;

		if (!ArmFPHost64_op(op, ArmFPHost64_value(d), ArmFPHost64_value(n), ArmFPHost64_value(m), &r, &inexact)) {
			return FALSE;
		}
		*result = ArmFPHost64_bits(r);
	}
	if (inexact) {
		*fpscr |= ARM_FPSCR_IXC;
	}
	return TRUE;
}

#endif /* ARM_COMMON_ARM_FP_HOST_H */
