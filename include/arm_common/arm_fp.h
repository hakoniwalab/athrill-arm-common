/*
 * The floating-point functions of the Arm pseudo-code (FPUnpack, FPRound,
 * the NaN handling, FPAdd, FPSub, FPMul, FPDiv, FPSqrt, FPMulAdd,
 * FPCompare) for N = 16, 32 and 64, in integer arithmetic: the host's
 * floating point is not used, so the results and the exception flags are
 * the same on every host.
 *
 * The pseudo-code computes an exact real result and rounds it once
 * (FPRound). Here a real value is an integer significand of up to 128 bits
 * times a power of two. Where an exact result needs more bits (an addition
 * of far apart values, a quotient, a square root), the bits that do not fit
 * are folded into the lowest bit ("sticky"): it lies well below the
 * rounding position, so the rounding is that of the exact value.
 *
 * fpscr is FPSCR: its control bits (AHP, DN, FZ, RMode) are read and the
 * cumulative exception bits are set (FPProcessException; floating-point
 * exceptions are never trapped here).
 */
#ifndef ARM_COMMON_ARM_FP_H
#define ARM_COMMON_ARM_FP_H

#include "arm_helpers.h"

#define ARM_FPSCR_IOC		(1U << 0)	/* invalid operation */
#define ARM_FPSCR_DZC		(1U << 1)	/* division by zero */
#define ARM_FPSCR_OFC		(1U << 2)	/* overflow */
#define ARM_FPSCR_UFC		(1U << 3)	/* underflow */
#define ARM_FPSCR_IXC		(1U << 4)	/* inexact */
#define ARM_FPSCR_IDC		(1U << 7)	/* input denormal */
#define ARM_FPSCR_BITPOS_RMODE	22U
#define ARM_FPSCR_FZ		(1U << 24)
#define ARM_FPSCR_DN		(1U << 25)
#define ARM_FPSCR_AHP		(1U << 26)

typedef enum {
	FPRounding_TIEEVEN = 0,	/* RN */
	FPRounding_POSINF,	/* RP */
	FPRounding_NEGINF,	/* RM */
	FPRounding_ZERO,	/* RZ */
	FPRounding_TIEAWAY,	/* VCVTA, VRINTA */
} FPRounding;

typedef enum {
	FPType_Nonzero = 0,
	FPType_Zero,
	FPType_Infinity,
	FPType_QNaN,
	FPType_SNaN,
} FPType;

/* A 128-bit unsigned integer. */
typedef struct {
	uint64 hi;
	uint64 lo;
} ArmU128;

/* An unpacked operand: for FPType_Nonzero, its value is mant * 2^exp. */
typedef struct {
	FPType type;
	bool sign;
	uint64 mant;
	sint32 exp;
} ArmFPValue;

/*
 * 128-bit integer operations.
 */

static inline ArmU128 ArmU128Make(uint64 hi, uint64 lo)
{
	ArmU128 r;

	r.hi = hi;
	r.lo = lo;
	return r;
}

static inline bool ArmU128IsZero(ArmU128 a)
{
	return (a.hi == 0U) && (a.lo == 0U);
}

static inline bool ArmU128Less(ArmU128 a, ArmU128 b)
{
	return (a.hi < b.hi) || ((a.hi == b.hi) && (a.lo < b.lo));
}

static inline ArmU128 ArmU128Add(ArmU128 a, ArmU128 b)
{
	ArmU128 r;

	r.lo = a.lo + b.lo;
	r.hi = a.hi + b.hi + ((r.lo < a.lo) ? 1U : 0U);
	return r;
}

/* a - b for a >= b. */
static inline ArmU128 ArmU128Sub(ArmU128 a, ArmU128 b)
{
	ArmU128 r;

	r.lo = a.lo - b.lo;
	r.hi = a.hi - b.hi - ((a.lo < b.lo) ? 1U : 0U);
	return r;
}

static inline ArmU128 ArmU128ShiftLeft(ArmU128 a, uint32 n)
{
	if (n == 0U) {
		return a;
	}
	if (n >= 128U) {
		return ArmU128Make(0U, 0U);
	}
	if (n >= 64U) {
		return ArmU128Make(a.lo << (n - 64U), 0U);
	}
	return ArmU128Make((a.hi << n) | (a.lo >> (64U - n)), a.lo << n);
}

static inline ArmU128 ArmU128ShiftRight(ArmU128 a, uint32 n)
{
	if (n == 0U) {
		return a;
	}
	if (n >= 128U) {
		return ArmU128Make(0U, 0U);
	}
	if (n >= 64U) {
		return ArmU128Make(0U, a.hi >> (n - 64U));
	}
	return ArmU128Make(a.hi >> n, (a.lo >> n) | (a.hi << (64U - n)));
}

/* a >> n, with the bits shifted out ORed into bit 0 (sticky). */
static inline ArmU128 ArmU128ShiftRightJam(ArmU128 a, uint32 n)
{
	ArmU128 r;
	bool lost;

	if (n == 0U) {
		return a;
	}
	if (n >= 128U) {
		return ArmU128Make(0U, ArmU128IsZero(a) ? 0U : 1U);
	}
	if (n >= 64U) {
		lost = (a.lo != 0U) || ((n > 64U) && ((a.hi << (128U - n)) != 0U));
		r = ArmU128Make(0U, a.hi >> (n - 64U));
	}
	else {
		lost = (a.lo << (64U - n)) != 0U;
		r = ArmU128Make(a.hi >> n, (a.lo >> n) | (a.hi << (64U - n)));
	}
	if (lost) {
		r.lo |= 1U;
	}
	return r;
}

/* The position of the highest set bit of x (x != 0). */
static inline uint32 ArmMsb64(uint64 x)
{
	uint32 n = 0U;
	uint32 step;

	for (step = 32U; step != 0U; step >>= 1) {
		if ((x >> step) != 0U) {
			x >>= step;
			n += step;
		}
	}
	return n;
}

/* The position of the highest set bit (a != 0). */
static inline uint32 ArmU128Msb(ArmU128 a)
{
	if (a.hi != 0U) {
		return 64U + ArmMsb64(a.hi);
	}
	return ArmMsb64(a.lo);
}

/* a * b, exactly. */
static inline ArmU128 ArmU128Mul64(uint64 a, uint64 b)
{
	uint64 a0 = a & 0xFFFFFFFFU;
	uint64 a1 = a >> 32;
	uint64 b0 = b & 0xFFFFFFFFU;
	uint64 b1 = b >> 32;
	uint64 p00 = a0 * b0;
	uint64 p01 = a0 * b1;
	uint64 p10 = a1 * b0;
	uint64 p11 = a1 * b1;
	uint64 middle = (p00 >> 32) + (p01 & 0xFFFFFFFFU) + (p10 & 0xFFFFFFFFU);

	return ArmU128Make(p11 + (p01 >> 32) + (p10 >> 32) + (middle >> 32), (middle << 32) | (p00 & 0xFFFFFFFFU));
}

/*
 * The formats: E exponent bits, F fraction bits.
 */
static inline uint32 ArmFPExpBits(uint32 N)
{
	return (N == 16U) ? 5U : ((N == 32U) ? 8U : 11U);
}

static inline uint32 ArmFPFracBits(uint32 N)
{
	return N - ArmFPExpBits(N) - 1U;
}

/* Ones(n), 0 <= n <= 63. */
static inline uint64 ArmFPOnes(uint32 n)
{
	return ((uint64)1U << n) - 1U;
}

/* FPZero(sign, N) = sign : Zeros(E) : Zeros(F) */
static inline uint64 FPZero(bool sign, uint32 N)
{
	return (uint64)(sign ? 1U : 0U) << (N - 1U);
}

/* FPInfinity(sign, N) = sign : Ones(E) : Zeros(F) */
static inline uint64 FPInfinity(bool sign, uint32 N)
{
	return FPZero(sign, N) | (ArmFPOnes(ArmFPExpBits(N)) << ArmFPFracBits(N));
}

/* FPMaxNormal(sign, N) = sign : Ones(E-1):'0' : Ones(F) */
static inline uint64 FPMaxNormal(bool sign, uint32 N)
{
	return FPZero(sign, N) | ((ArmFPOnes(ArmFPExpBits(N)) - 1U) << ArmFPFracBits(N)) | ArmFPOnes(ArmFPFracBits(N));
}

/* FPDefaultNaN(N) = '0' : Ones(E) : '1':Zeros(F-1) */
static inline uint64 FPDefaultNaN(uint32 N)
{
	return FPInfinity(FALSE, N) | ((uint64)1U << (ArmFPFracBits(N) - 1U));
}

/* FPNeg, FPAbs: the sign bit only. */
static inline uint64 FPNeg(uint64 op, uint32 N)
{
	return op ^ ((uint64)1U << (N - 1U));
}

/* FPProcessException(exception, fpscr_val): the cumulative bit (no traps). */
static inline void FPProcessException(uint32 exception, uint32 *fpscr)
{
	*fpscr |= exception;
}

/*
 * (FPType, bit, real) FPUnpack(bits(N) fpval, bits(32) fpscr_val)
 *     sign = fpval<N-1>;
 *     exp = fpval<N-2:F>; frac = fpval<F-1:0>;
 *     if IsZero(exp) then
 *         // Produce zero if value is zero or flush-to-zero is selected (not for N == 16)
 *         if IsZero(frac) || (N != 16 && fpscr_val<24> == '1') then
 *             type = FPType_Zero; value = 0.0;
 *             if !IsZero(frac) then  // Denormalized input flushed to zero
 *                 FPProcessException(FPExc_InputDenorm, fpscr_val);
 *         else
 *             type = FPType_Nonzero; value = 2.0^(1-bias-F) * UInt(frac);
 *     elsif IsOnes(exp) && !(N == 16 && fpscr_val<26> == '1') then
 *         if IsZero(frac) then
 *             type = FPType_Infinity; value = 2.0^1000000;
 *         else
 *             type = if frac<F-1> == '1' then FPType_QNaN else FPType_SNaN;
 *             value = 0.0;
 *     else
 *         type = FPType_Nonzero; value = 2.0^(UInt(exp)-bias-F) * (2^F + UInt(frac));
 *     if sign == '1' then value = -value;
 * (16-bit values with FPSCR.AHP set are the alternative half-precision
 * format, without infinities and NaNs.)
 */
static inline ArmFPValue FPUnpack(uint64 fpval, uint32 N, uint32 *fpscr)
{
	uint32 F = ArmFPFracBits(N);
	uint32 E = ArmFPExpBits(N);
	sint32 bias = (sint32)ArmFPOnes(E - 1U);
	uint64 exp = (fpval >> F) & ArmFPOnes(E);
	uint64 frac = fpval & ArmFPOnes(F);
	ArmFPValue v;

	v.sign = ((fpval >> (N - 1U)) & 1U) != 0U;
	v.mant = 0U;
	v.exp = 0;
	if (exp == 0U) {
		if ((frac == 0U) || ((N != 16U) && ((*fpscr & ARM_FPSCR_FZ) != 0U))) {
			v.type = FPType_Zero;
			if (frac != 0U) {
				FPProcessException(ARM_FPSCR_IDC, fpscr);
			}
		}
		else {
			v.type = FPType_Nonzero;
			v.mant = frac;
			v.exp = 1 - bias - (sint32)F;
		}
	}
	else if ((exp == ArmFPOnes(E)) && !((N == 16U) && ((*fpscr & ARM_FPSCR_AHP) != 0U))) {
		if (frac == 0U) {
			v.type = FPType_Infinity;
		}
		else {
			v.type = (((frac >> (F - 1U)) & 1U) != 0U) ? FPType_QNaN : FPType_SNaN;
		}
	}
	else {
		v.type = FPType_Nonzero;
		v.mant = ((uint64)1U << F) | frac;
		v.exp = (sint32)exp - bias - (sint32)F;
	}
	return v;
}

static inline bool ArmFPIsNaN(FPType type)
{
	return (type == FPType_QNaN) || (type == FPType_SNaN);
}

/*
 * bits(N) FPProcessNaN(FPType type, bits(N) op, bits(32) fpscr_val)
 *     topfrac = if N == 64 then 51 else if N == 32 then 22 else 9;
 *     result = op;
 *     if type == FPType_SNaN then
 *         result<topfrac> = '1';
 *         FPProcessException(FPExc_InvalidOp, fpscr_val);
 *     if fpscr_val<25> == '1' then  // DefaultNaN requested
 *         result = FPDefaultNaN();
 */
static inline uint64 FPProcessNaN(FPType type, uint64 op, uint32 N, uint32 *fpscr)
{
	uint64 result = op;

	if (type == FPType_SNaN) {
		result |= (uint64)1U << (ArmFPFracBits(N) - 1U);
		FPProcessException(ARM_FPSCR_IOC, fpscr);
	}
	if ((*fpscr & ARM_FPSCR_DN) != 0U) {
		result = FPDefaultNaN(N);
	}
	return result;
}

/*
 * (boolean, bits(N)) FPProcessNaNs(type1, type2, op1, op2, fpscr_val)
 *     if type1 == FPType_SNaN then done = TRUE; result = FPProcessNaN(type1, op1, fpscr_val);
 *     elsif type2 == FPType_SNaN then done = TRUE; result = FPProcessNaN(type2, op2, fpscr_val);
 *     elsif type1 == FPType_QNaN then done = TRUE; result = FPProcessNaN(type1, op1, fpscr_val);
 *     elsif type2 == FPType_QNaN then done = TRUE; result = FPProcessNaN(type2, op2, fpscr_val);
 *     else done = FALSE; result = Zeros(N);  // 'Don't care' result
 */
static inline bool FPProcessNaNs(FPType type1, FPType type2, uint64 op1, uint64 op2, uint32 N, uint32 *fpscr, uint64 *result)
{
	if (type1 == FPType_SNaN) {
		*result = FPProcessNaN(type1, op1, N, fpscr);
	}
	else if (type2 == FPType_SNaN) {
		*result = FPProcessNaN(type2, op2, N, fpscr);
	}
	else if (type1 == FPType_QNaN) {
		*result = FPProcessNaN(type1, op1, N, fpscr);
	}
	else if (type2 == FPType_QNaN) {
		*result = FPProcessNaN(type2, op2, N, fpscr);
	}
	else {
		*result = 0U;
		return FALSE;
	}
	return TRUE;
}

/*
 * FPProcessNaNs3(type1, type2, type3, op1, op2, op3, fpscr_val): as
 * FPProcessNaNs, the signalling NaNs of op1, op2, op3 first, then the
 * quiet ones.
 */
static inline bool FPProcessNaNs3(FPType type1, FPType type2, FPType type3, uint64 op1, uint64 op2, uint64 op3,
	uint32 N, uint32 *fpscr, uint64 *result)
{
	if (type1 == FPType_SNaN) {
		*result = FPProcessNaN(type1, op1, N, fpscr);
	}
	else if (type2 == FPType_SNaN) {
		*result = FPProcessNaN(type2, op2, N, fpscr);
	}
	else if (type3 == FPType_SNaN) {
		*result = FPProcessNaN(type3, op3, N, fpscr);
	}
	else if (type1 == FPType_QNaN) {
		*result = FPProcessNaN(type1, op1, N, fpscr);
	}
	else if (type2 == FPType_QNaN) {
		*result = FPProcessNaN(type2, op2, N, fpscr);
	}
	else if (type3 == FPType_QNaN) {
		*result = FPProcessNaN(type3, op3, N, fpscr);
	}
	else {
		*result = 0U;
		return FALSE;
	}
	return TRUE;
}

/* The rounding mode of FPSCR.RMode. */
static inline FPRounding ArmFPSCRRounding(uint32 fpscr)
{
	return (FPRounding)((fpscr >> ARM_FPSCR_BITPOS_RMODE) & 0x3U);
}

/*
 * bits(N) FPRound(real value, bits(32) fpscr_val), with the rounding of
 * FPSCR.RMode (or the one given), for a value mant * 2^exp != 0:
 *     // Obtain sign, exponent and mantissa (1.0 <= mantissa < 2.0)
 *     if fpscr_val<24> == '1' && N != 16 && exponent < minimum_exp then
 *         result = FPZero(sign);
 *         FPSCR.UFC = '1';  // Flush-to-zero never generates a trapped exception
 *     else
 *         // Start creating the exponent value for the result.
 *         biased_exp = Max(exponent - minimum_exp + 1, 0);
 *         if biased_exp == 0 then mantissa = mantissa / 2^(minimum_exp - exponent);
 *         // Get the unrounded mantissa as an integer, and the "units in last place" rounding error.
 *         int_mant = RoundDown(mantissa * 2^F);  // < 2^F if biased_exp == 0, >= 2^F if not
 *         error = mantissa * 2^F - int_mant;
 *         // Underflow occurs if exponent is too small before rounding, and result is inexact.
 *         if biased_exp == 0 && (error != 0.0 || fpscr_val<11> == '1') then
 *             FPProcessException(FPExc_Underflow, fpscr_val);
 *         // Round result according to rounding mode.
 *         case rounding of
 *             when FPRounding_TIEEVEN
 *                 round_up = (error > 0.5 || (error == 0.5 && int_mant<0> == '1'));
 *                 overflow_to_inf = TRUE;
 *             when FPRounding_POSINF
 *                 round_up = (error != 0.0 && sign == '0'); overflow_to_inf = (sign == '0');
 *             when FPRounding_NEGINF
 *                 round_up = (error != 0.0 && sign == '1'); overflow_to_inf = (sign == '1');
 *             when FPRounding_ZERO
 *                 round_up = FALSE; overflow_to_inf = FALSE;
 *         if round_up then
 *             int_mant = int_mant + 1;
 *             if int_mant == 2^F then  // Rounded up from denormalized to normalized
 *                 biased_exp = 1;
 *             if int_mant == 2^(F+1) then  // Rounded up to next exponent
 *                 biased_exp = biased_exp + 1; int_mant = int_mant DIV 2;
 *         // Deal with overflow and generate result.
 *         if N != 16 || fpscr_val<26> == '0' then  // Single, double or IEEE half precision
 *             if biased_exp >= 2^E - 1 then
 *                 result = if overflow_to_inf then FPInfinity(sign) else FPMaxNormal(sign);
 *                 FPProcessException(FPExc_Overflow, fpscr_val);
 *                 error = 1.0;  // Ensure that an Inexact exception occurs
 *             else
 *                 result = sign : biased_exp<E-1:0> : int_mant<F-1:0>;
 *         else  // Alternative half precision
 *             if biased_exp >= 2^E then
 *                 result = sign : Ones(N-1);
 *                 FPProcessException(FPExc_InvalidOp, fpscr_val);
 *                 error = 0.0;  // Ensure that an Inexact exception does not occur
 *             else
 *                 result = sign : biased_exp<E-1:0> : int_mant<F-1:0>;
 *         // Deal with Inexact exception.
 *         if error != 0.0 then
 *             FPProcessException(FPExc_Inexact, fpscr_val);
 * (UFE, FPSCR bit 11, is 0: exceptions are not trapped.)
 */
static inline uint64 FPRoundMode(bool sign, ArmU128 mant, sint32 exp, uint32 N, FPRounding rounding, uint32 *fpscr)
{
	uint32 E = ArmFPExpBits(N);
	uint32 F = ArmFPFracBits(N);
	sint32 minimum_exp = 2 - (sint32)((uint64)1U << (E - 1U));
	uint32 msb = ArmU128Msb(mant);
	sint32 exponent = exp + (sint32)msb;
	sint32 biased_exp;
	sint32 shift;
	uint64 int_mant;
	bool half;
	bool rest;
	bool round_up;
	bool overflow_to_inf;
	uint64 result;

	if (((*fpscr & ARM_FPSCR_FZ) != 0U) && (N != 16U) && (exponent < minimum_exp)) {
		FPProcessException(ARM_FPSCR_UFC, fpscr);
		return FPZero(sign, N);
	}
	biased_exp = exponent - minimum_exp + 1;
	if (biased_exp < 0) {
		biased_exp = 0;
	}
	/* int_mant = mant >> shift; error from the bits shifted out. */
	shift = (sint32)msb - (sint32)F;
	if (biased_exp == 0) {
		shift += minimum_exp - exponent;
	}
	if (shift <= 0) {
		int_mant = mant.lo << (uint32)(-shift);
		half = FALSE;
		rest = FALSE;
	}
	else if (shift > 128) {
		int_mant = 0U;
		half = FALSE;
		rest = TRUE;
	}
	else {
		/* the bits shifted out, moved to the top: error = below / 2^128 */
		ArmU128 below = ArmU128ShiftLeft(mant, 128U - (uint32)shift);

		int_mant = ArmU128ShiftRight(mant, (uint32)shift).lo;
		half = (below.hi >> 63) != 0U;
		rest = ((below.hi << 1) != 0U) || (below.lo != 0U);
	}
	if ((biased_exp == 0) && (half || rest)) {
		FPProcessException(ARM_FPSCR_UFC, fpscr);
	}
	switch (rounding) {
	case FPRounding_TIEEVEN:
		round_up = (half && rest) || (half && ((int_mant & 1U) != 0U));
		overflow_to_inf = TRUE;
		break;
	case FPRounding_POSINF:
		round_up = (half || rest) && !sign;
		overflow_to_inf = !sign;
		break;
	case FPRounding_NEGINF:
		round_up = (half || rest) && sign;
		overflow_to_inf = sign;
		break;
	default:
		round_up = FALSE;
		overflow_to_inf = FALSE;
		break;
	}
	if (round_up) {
		int_mant++;
		if (int_mant == ((uint64)1U << F)) {
			biased_exp = 1;
		}
		if (int_mant == ((uint64)1U << (F + 1U))) {
			biased_exp++;
			int_mant >>= 1;
		}
	}
	if ((N != 16U) || ((*fpscr & ARM_FPSCR_AHP) == 0U)) {
		if ((uint64)biased_exp >= ArmFPOnes(E)) {
			result = overflow_to_inf ? FPInfinity(sign, N) : FPMaxNormal(sign, N);
			FPProcessException(ARM_FPSCR_OFC, fpscr);
			half = TRUE;
		}
		else {
			result = FPZero(sign, N) | ((uint64)biased_exp << F) | (int_mant & ArmFPOnes(F));
		}
	}
	else {
		if ((uint64)biased_exp > ArmFPOnes(E)) {
			result = FPZero(sign, N) | ArmFPOnes(N - 1U);
			FPProcessException(ARM_FPSCR_IOC, fpscr);
			half = FALSE;
			rest = FALSE;
		}
		else {
			result = FPZero(sign, N) | ((uint64)biased_exp << F) | (int_mant & ArmFPOnes(F));
		}
	}
	if (half || rest) {
		FPProcessException(ARM_FPSCR_IXC, fpscr);
	}
	return result;
}

static inline uint64 FPRound(bool sign, ArmU128 mant, sint32 exp, uint32 N, uint32 *fpscr)
{
	return FPRoundMode(sign, mant, exp, N, ArmFPSCRRounding(*fpscr), fpscr);
}

/*
 * Exact operations on values mant * 2^exp (with the sticky bit where the
 * result does not fit), for FPRound.
 */

/* mant * 2^exp with mant's top bit moved to bit 125 (two bits of headroom). */
static inline ArmU128 ArmFPAlign125(ArmU128 mant, sint32 *exp)
{
	uint32 msb = ArmU128Msb(mant);

	*exp -= 125 - (sint32)msb;
	return ArmU128ShiftLeft(mant, 125U - msb);
}

/*
 * (signA, A) + (signB, B), both nonzero, as (sign, mant, exp); returns FALSE
 * for an exact zero. The smaller is shifted right with the sticky bit; when
 * it is shifted by more than one place the difference loses at most one
 * leading bit, so the sticky bit stays far below the rounding position.
 */
static inline bool ArmFPAddExact(bool signA, ArmU128 a, sint32 expA, bool signB, ArmU128 b, sint32 expB,
	bool *sign, ArmU128 *mant, sint32 *exp)
{
	ArmU128 big;
	ArmU128 small;
	sint32 exp_big;
	sint32 exp_small;
	bool sign_big;
	bool sign_small;

	a = ArmFPAlign125(a, &expA);
	b = ArmFPAlign125(b, &expB);
	if ((expA > expB) || ((expA == expB) && !ArmU128Less(a, b))) {
		big = a; exp_big = expA; sign_big = signA;
		small = b; exp_small = expB; sign_small = signB;
	}
	else {
		big = b; exp_big = expB; sign_big = signB;
		small = a; exp_small = expA; sign_small = signA;
	}
	small = ArmU128ShiftRightJam(small, ((exp_big - exp_small) > 127) ? 128U : (uint32)(exp_big - exp_small));
	*sign = sign_big;
	*exp = exp_big;
	if (sign_big == sign_small) {
		*mant = ArmU128Add(big, small);
	}
	else {
		*mant = ArmU128Sub(big, small);
	}
	return !ArmU128IsZero(*mant);
}

static inline ArmU128 ArmFPMant128(const ArmFPValue *v)
{
	return ArmU128Make(0U, v->mant);
}

/* The sign of an exact zero sum: '1' when rounding towards minus infinity. */
static inline uint64 ArmFPZeroSum(uint32 N, uint32 fpscr)
{
	return FPZero(ArmFPSCRRounding(fpscr) == FPRounding_NEGINF, N);
}

/*
 * bits(N) FPAdd(bits(N) op1, bits(N) op2, boolean fpscr_controlled)
 *     (type1,sign1,value1) = FPUnpack(op1, fpscr_val);
 *     (type2,sign2,value2) = FPUnpack(op2, fpscr_val);
 *     (done,result) = FPProcessNaNs(type1, type2, op1, op2, fpscr_val);
 *     if !done then
 *         inf1 = (type1 == FPType_Infinity); inf2 = (type2 == FPType_Infinity);
 *         zero1 = (type1 == FPType_Zero);    zero2 = (type2 == FPType_Zero);
 *         if inf1 && inf2 && sign1 == NOT(sign2) then
 *             result = FPDefaultNaN(); FPProcessException(FPExc_InvalidOp, fpscr_val);
 *         elsif (inf1 && sign1 == '0') || (inf2 && sign2 == '0') then result = FPInfinity('0');
 *         elsif (inf1 && sign1 == '1') || (inf2 && sign2 == '1') then result = FPInfinity('1');
 *         elsif zero1 && zero2 && sign1 == sign2 then result = FPZero(sign1);
 *         else
 *             result_value = value1 + value2;
 *             if result_value == 0.0 then  // Sign of exact zero result depends on rounding mode
 *                 result_sign = if fpscr_val<23:22> == '10' then '1' else '0';
 *                 result = FPZero(result_sign);
 *             else
 *                 result = FPRound(result_value, fpscr_val);
 * FPSub is FPAdd of value1 - value2 (sign2 inverted, but not op2 for the NaNs).
 */
static inline uint64 ArmFPAddSub(uint64 op1, uint64 op2, bool subtract, uint32 N, uint32 *fpscr)
{
	ArmFPValue v1 = FPUnpack(op1, N, fpscr);
	ArmFPValue v2 = FPUnpack(op2, N, fpscr);
	uint64 result;
	bool sign2;
	bool sign;
	ArmU128 mant;
	sint32 exp;

	if (FPProcessNaNs(v1.type, v2.type, op1, op2, N, fpscr, &result)) {
		return result;
	}
	sign2 = subtract ? !v2.sign : v2.sign;
	if ((v1.type == FPType_Infinity) && (v2.type == FPType_Infinity) && (v1.sign != sign2)) {
		FPProcessException(ARM_FPSCR_IOC, fpscr);
		return FPDefaultNaN(N);
	}
	if (((v1.type == FPType_Infinity) && !v1.sign) || ((v2.type == FPType_Infinity) && !sign2)) {
		return FPInfinity(FALSE, N);
	}
	if (((v1.type == FPType_Infinity) && v1.sign) || ((v2.type == FPType_Infinity) && sign2)) {
		return FPInfinity(TRUE, N);
	}
	if ((v1.type == FPType_Zero) && (v2.type == FPType_Zero) && (v1.sign == sign2)) {
		return FPZero(v1.sign, N);
	}
	if ((v1.type == FPType_Zero) && (v2.type == FPType_Zero)) {
		return ArmFPZeroSum(N, *fpscr);
	}
	if (v1.type == FPType_Zero) {
		return FPRound(sign2, ArmFPMant128(&v2), v2.exp, N, fpscr);
	}
	if (v2.type == FPType_Zero) {
		return FPRound(v1.sign, ArmFPMant128(&v1), v1.exp, N, fpscr);
	}
	if (!ArmFPAddExact(v1.sign, ArmFPMant128(&v1), v1.exp, sign2, ArmFPMant128(&v2), v2.exp, &sign, &mant, &exp)) {
		return ArmFPZeroSum(N, *fpscr);
	}
	return FPRound(sign, mant, exp, N, fpscr);
}

static inline uint64 FPAdd(uint64 op1, uint64 op2, uint32 N, uint32 *fpscr)
{
	return ArmFPAddSub(op1, op2, FALSE, N, fpscr);
}

static inline uint64 FPSub(uint64 op1, uint64 op2, uint32 N, uint32 *fpscr)
{
	return ArmFPAddSub(op1, op2, TRUE, N, fpscr);
}

/*
 * bits(N) FPMul(bits(N) op1, bits(N) op2, boolean fpscr_controlled)
 *     (type1,sign1,value1) = FPUnpack(op1, fpscr_val);
 *     (type2,sign2,value2) = FPUnpack(op2, fpscr_val);
 *     (done,result) = FPProcessNaNs(type1, type2, op1, op2, fpscr_val);
 *     if !done then
 *         inf1 = (type1 == FPType_Infinity); inf2 = (type2 == FPType_Infinity);
 *         zero1 = (type1 == FPType_Zero);    zero2 = (type2 == FPType_Zero);
 *         if (inf1 && zero2) || (zero1 && inf2) then
 *             result = FPDefaultNaN(); FPProcessException(FPExc_InvalidOp, fpscr_val);
 *         elsif inf1 || inf2 then result = FPInfinity(sign1 EOR sign2);
 *         elsif zero1 || zero2 then result = FPZero(sign1 EOR sign2);
 *         else result = FPRound(value1*value2, fpscr_val);
 */
static inline uint64 FPMul(uint64 op1, uint64 op2, uint32 N, uint32 *fpscr)
{
	ArmFPValue v1 = FPUnpack(op1, N, fpscr);
	ArmFPValue v2 = FPUnpack(op2, N, fpscr);
	bool sign = v1.sign != v2.sign;
	uint64 result;

	if (FPProcessNaNs(v1.type, v2.type, op1, op2, N, fpscr, &result)) {
		return result;
	}
	if (((v1.type == FPType_Infinity) && (v2.type == FPType_Zero)) || ((v1.type == FPType_Zero) && (v2.type == FPType_Infinity))) {
		FPProcessException(ARM_FPSCR_IOC, fpscr);
		return FPDefaultNaN(N);
	}
	if ((v1.type == FPType_Infinity) || (v2.type == FPType_Infinity)) {
		return FPInfinity(sign, N);
	}
	if ((v1.type == FPType_Zero) || (v2.type == FPType_Zero)) {
		return FPZero(sign, N);
	}
	return FPRound(sign, ArmU128Mul64(v1.mant, v2.mant), v1.exp + v2.exp, N, fpscr);
}

/*
 * bits(N) FPDiv(bits(N) op1, bits(N) op2, boolean fpscr_controlled)
 *     (type1,sign1,value1) = FPUnpack(op1, fpscr_val);
 *     (type2,sign2,value2) = FPUnpack(op2, fpscr_val);
 *     (done,result) = FPProcessNaNs(type1, type2, op1, op2, fpscr_val);
 *     if !done then
 *         inf1 = (type1 == FPType_Infinity); inf2 = (type2 == FPType_Infinity);
 *         zero1 = (type1 == FPType_Zero);    zero2 = (type2 == FPType_Zero);
 *         if (inf1 && inf2) || (zero1 && zero2) then
 *             result = FPDefaultNaN(); FPProcessException(FPExc_InvalidOp, fpscr_val);
 *         elsif inf1 || zero2 then
 *             result = FPInfinity(sign1 EOR sign2);
 *             if !inf1 then FPProcessException(FPExc_DivideByZero, fpscr_val);
 *         elsif zero1 || inf2 then result = FPZero(sign1 EOR sign2);
 *         else result = FPRound(value1/value2, fpscr_val);
 * The quotient: 64 bits by long division, the remainder as the sticky bit.
 */
static inline uint64 FPDiv(uint64 op1, uint64 op2, uint32 N, uint32 *fpscr)
{
	ArmFPValue v1 = FPUnpack(op1, N, fpscr);
	ArmFPValue v2 = FPUnpack(op2, N, fpscr);
	bool sign = v1.sign != v2.sign;
	uint64 result;
	uint64 a;
	uint64 b;
	uint64 q;
	uint32 i;
	sint32 exp;

	if (FPProcessNaNs(v1.type, v2.type, op1, op2, N, fpscr, &result)) {
		return result;
	}
	if (((v1.type == FPType_Infinity) && (v2.type == FPType_Infinity)) || ((v1.type == FPType_Zero) && (v2.type == FPType_Zero))) {
		FPProcessException(ARM_FPSCR_IOC, fpscr);
		return FPDefaultNaN(N);
	}
	if ((v1.type == FPType_Infinity) || (v2.type == FPType_Zero)) {
		if (v1.type != FPType_Infinity) {
			FPProcessException(ARM_FPSCR_DZC, fpscr);
		}
		return FPInfinity(sign, N);
	}
	if ((v1.type == FPType_Zero) || (v2.type == FPType_Infinity)) {
		return FPZero(sign, N);
	}
	/* a / b with both top bits at 62 and a >= b: the quotient is in [1, 2). */
	a = v1.mant << (62U - ArmMsb64(v1.mant));
	b = v2.mant << (62U - ArmMsb64(v2.mant));
	exp = (v1.exp - (sint32)(62U - ArmMsb64(v1.mant))) - (v2.exp - (sint32)(62U - ArmMsb64(v2.mant)));
	if (a < b) {
		a <<= 1;
		exp--;
	}
	q = 0U;
	for (i = 0U; i < 64U; i++) {
		q <<= 1;
		if (a >= b) {
			a -= b;
			q |= 1U;
		}
		a <<= 1;
	}
	/* q = quotient * 2^63; the remainder as the sticky bit below q */
	return FPRound(sign, ArmU128Make(q, (a != 0U) ? 1U : 0U), exp - 63 - 64, N, fpscr);
}

/*
 * bits(N) FPSqrt(bits(N) operand)
 *     (type,sign,value) = FPUnpack(operand, FPSCR);
 *     if type == FPType_SNaN || type == FPType_QNaN then
 *         result = FPProcessNaN(type, operand, FPSCR);
 *     elsif type == FPType_Zero then result = FPZero(sign);
 *     elsif type == FPType_Infinity && sign == '0' then result = FPInfinity(sign);
 *     elsif sign == '1' then
 *         result = FPDefaultNaN(); FPProcessException(FPExc_InvalidOp, FPSCR);
 *     else result = FPRound(Sqrt(value), FPSCR);
 * The root: of the significand shifted to 125 or 126 bits (an even exponent
 * left), digit by digit, the remainder as the sticky bit.
 */
static inline uint64 FPSqrt(uint64 operand, uint32 N, uint32 *fpscr)
{
	ArmFPValue v = FPUnpack(operand, N, fpscr);
	ArmU128 num;
	ArmU128 res;
	ArmU128 bit;
	uint32 shift;
	sint32 exp;

	if (ArmFPIsNaN(v.type)) {
		return FPProcessNaN(v.type, operand, N, fpscr);
	}
	if (v.type == FPType_Zero) {
		return FPZero(v.sign, N);
	}
	if ((v.type == FPType_Infinity) && !v.sign) {
		return FPInfinity(FALSE, N);
	}
	if (v.sign) {
		FPProcessException(ARM_FPSCR_IOC, fpscr);
		return FPDefaultNaN(N);
	}
	shift = 125U - ArmMsb64(v.mant);
	exp = v.exp - (sint32)shift;
	if ((exp & 1) != 0) {
		shift++;
		exp--;
	}
	num = ArmU128ShiftLeft(ArmU128Make(0U, v.mant), shift);
	res = ArmU128Make(0U, 0U);
	bit = ArmU128Make((uint64)1U << 62, 0U);	/* 2^126 */
	while (ArmU128Less(num, bit)) {
		bit = ArmU128ShiftRight(bit, 2U);
	}
	while (!ArmU128IsZero(bit)) {
		ArmU128 trial = ArmU128Add(res, bit);

		if (!ArmU128Less(num, trial)) {
			num = ArmU128Sub(num, trial);
			res = ArmU128Add(ArmU128ShiftRight(res, 1U), bit);
		}
		else {
			res = ArmU128ShiftRight(res, 1U);
		}
		bit = ArmU128ShiftRight(bit, 2U);
	}
	/* sqrt(num0 * 2^exp) = res * 2^(exp/2); the remainder as a sticky bit below res */
	res = ArmU128ShiftLeft(res, 1U);
	if (!ArmU128IsZero(num)) {
		res.lo |= 1U;
	}
	return FPRound(FALSE, res, exp / 2 - 1, N, fpscr);
}

/*
 * bits(N) FPMulAdd(bits(N) addend, bits(N) op1, bits(N) op2, boolean fpscr_controlled)
 *     (typeA,signA,valueA) = FPUnpack(addend, fpscr_val);
 *     (type1,sign1,value1) = FPUnpack(op1, fpscr_val);
 *     (type2,sign2,value2) = FPUnpack(op2, fpscr_val);
 *     inf1 = (type1 == FPType_Infinity); zero1 = (type1 == FPType_Zero);
 *     inf2 = (type2 == FPType_Infinity); zero2 = (type2 == FPType_Zero);
 *     (done,result) = FPProcessNaNs3(typeA, type1, type2, addend, op1, op2, fpscr_val);
 *     if typeA == FPType_QNaN && ((inf1 && zero2) || (zero1 && inf2)) then
 *         result = FPDefaultNaN(); FPProcessException(FPExc_InvalidOp, fpscr_val);
 *     if !done then
 *         infA = (typeA == FPType_Infinity); zeroA = (typeA == FPType_Zero);
 *         // Determine sign and type product will have if it does not cause an Invalid Operation.
 *         signP = sign1 EOR sign2; infP = inf1 || inf2; zeroP = zero1 || zero2;
 *         // Non SNaN-generated Invalid Operation cases are multiplies of zero by infinity and
 *         // additions of opposite-signed infinities.
 *         if (inf1 && zero2) || (zero1 && inf2) || (infA && infP && signA == NOT(signP)) then
 *             result = FPDefaultNaN(); FPProcessException(FPExc_InvalidOp, fpscr_val);
 *         // Other cases involving infinities produce an infinity of the same sign.
 *         elsif (infA && signA == '0') || (infP && signP == '0') then result = FPInfinity('0');
 *         elsif (infA && signA == '1') || (infP && signP == '1') then result = FPInfinity('1');
 *         // Cases where the result is exactly zero and its sign is not determined by the rounding mode
 *         // are additions of same-signed zeros.
 *         elsif zeroA && zeroP && signA == signP then result = FPZero(signA);
 *         // Otherwise calculate numerical result and round it.
 *         else
 *             result_value = valueA + (value1 * value2);
 *             if result_value == 0.0 then  // Sign of exact zero result depends on rounding mode
 *                 result_sign = if fpscr_val<23:22> == '10' then '1' else '0';
 *                 result = FPZero(result_sign);
 *             else
 *                 result = FPRound(result_value, fpscr_val);
 */
static inline uint64 FPMulAdd(uint64 addend, uint64 op1, uint64 op2, uint32 N, uint32 *fpscr)
{
	ArmFPValue va = FPUnpack(addend, N, fpscr);
	ArmFPValue v1 = FPUnpack(op1, N, fpscr);
	ArmFPValue v2 = FPUnpack(op2, N, fpscr);
	bool inf_times_zero = ((v1.type == FPType_Infinity) && (v2.type == FPType_Zero))
		|| ((v1.type == FPType_Zero) && (v2.type == FPType_Infinity));
	bool signP = v1.sign != v2.sign;
	bool infP = (v1.type == FPType_Infinity) || (v2.type == FPType_Infinity);
	bool zeroP = (v1.type == FPType_Zero) || (v2.type == FPType_Zero);
	uint64 result;
	bool sign;
	ArmU128 mant;
	sint32 exp;

	if (FPProcessNaNs3(va.type, v1.type, v2.type, addend, op1, op2, N, fpscr, &result)) {
		if ((va.type == FPType_QNaN) && inf_times_zero) {
			FPProcessException(ARM_FPSCR_IOC, fpscr);
			return FPDefaultNaN(N);
		}
		return result;
	}
	if (inf_times_zero || ((va.type == FPType_Infinity) && infP && (va.sign != signP))) {
		FPProcessException(ARM_FPSCR_IOC, fpscr);
		return FPDefaultNaN(N);
	}
	if (((va.type == FPType_Infinity) && !va.sign) || (infP && !signP)) {
		return FPInfinity(FALSE, N);
	}
	if (((va.type == FPType_Infinity) && va.sign) || (infP && signP)) {
		return FPInfinity(TRUE, N);
	}
	if ((va.type == FPType_Zero) && zeroP && (va.sign == signP)) {
		return FPZero(va.sign, N);
	}
	if (zeroP && (va.type == FPType_Zero)) {
		return ArmFPZeroSum(N, *fpscr);
	}
	if (zeroP) {
		return FPRound(va.sign, ArmFPMant128(&va), va.exp, N, fpscr);
	}
	if (va.type == FPType_Zero) {
		return FPRound(signP, ArmU128Mul64(v1.mant, v2.mant), v1.exp + v2.exp, N, fpscr);
	}
	if (!ArmFPAddExact(va.sign, ArmFPMant128(&va), va.exp, signP, ArmU128Mul64(v1.mant, v2.mant), v1.exp + v2.exp,
		&sign, &mant, &exp)) {
		return ArmFPZeroSum(N, *fpscr);
	}
	return FPRound(sign, mant, exp, N, fpscr);
}

/*
 * bits(4) FPCompare(bits(N) op1, bits(N) op2, boolean quiet_nan_exc, boolean fpscr_controlled)
 *     (type1,sign1,value1) = FPUnpack(op1, fpscr_val);
 *     (type2,sign2,value2) = FPUnpack(op2, fpscr_val);
 *     if type1 == FPType_SNaN || type1 == FPType_QNaN || type2 == FPType_SNaN || type2 == FPType_QNaN then
 *         result = '0011';
 *         if type1 == FPType_SNaN || type2 == FPType_SNaN || quiet_nan_exc then
 *             FPProcessException(FPExc_InvalidOp, fpscr_val);
 *     else
 *         // All non-NaN cases can be evaluated on the values produced by FPUnpack()
 *         if value1 == value2 then result = '0110';
 *         elsif value1 < value2 then result = '1000';
 *         else result = '0010';
 * Returns NZCV in bits 3:0.
 */
static inline sint32 ArmFPOrder(const ArmFPValue *a, const ArmFPValue *b)
{
	ArmU128 ma;
	ArmU128 mb;
	sint32 ea;
	sint32 eb;
	sint32 magnitude;

	/* the magnitudes: zero < nonzero < infinity */
	if ((a->type != FPType_Nonzero) || (b->type != FPType_Nonzero)) {
		magnitude = (sint32)(a->type == FPType_Infinity ? 2 : (a->type == FPType_Zero ? 0 : 1))
			- (sint32)(b->type == FPType_Infinity ? 2 : (b->type == FPType_Zero ? 0 : 1));
	}
	else {
		ea = a->exp;
		eb = b->exp;
		ma = ArmFPAlign125(ArmU128Make(0U, a->mant), &ea);
		mb = ArmFPAlign125(ArmU128Make(0U, b->mant), &eb);
		if (ea != eb) {
			magnitude = (ea > eb) ? 1 : -1;
		}
		else if (ArmU128Less(ma, mb)) {
			magnitude = -1;
		}
		else {
			magnitude = ArmU128Less(mb, ma) ? 1 : 0;
		}
	}
	if ((a->type == FPType_Zero) && (b->type == FPType_Zero)) {
		return 0;
	}
	if (a->sign != b->sign) {
		return a->sign ? -1 : 1;
	}
	return a->sign ? -magnitude : magnitude;
}

static inline uint32 FPCompare(uint64 op1, uint64 op2, bool quiet_nan_exc, uint32 N, uint32 *fpscr)
{
	ArmFPValue v1 = FPUnpack(op1, N, fpscr);
	ArmFPValue v2 = FPUnpack(op2, N, fpscr);
	sint32 order;

	if (ArmFPIsNaN(v1.type) || ArmFPIsNaN(v2.type)) {
		if ((v1.type == FPType_SNaN) || (v2.type == FPType_SNaN) || quiet_nan_exc) {
			FPProcessException(ARM_FPSCR_IOC, fpscr);
		}
		return 0x3U;
	}
	order = ArmFPOrder(&v1, &v2);
	if (order == 0) {
		return 0x6U;
	}
	return (order < 0) ? 0x8U : 0x2U;
}

/*
 * bits(N) FPConvert(bits(M) operand, integer N), the conversions between
 * the formats (FPSingleToDouble, FPDoubleToSingle, FPHalfToSingle,
 * FPSingleToHalf, ...):
 *     (type,sign,value) = FPUnpack(operand, fpscr_val);
 *     alt_hp = (N == 16) && (fpscr_val<26> == '1');
 *     if type == FPType_SNaN || type == FPType_QNaN then
 *         if alt_hp then result = FPZero(sign);
 *         elsif fpscr_val<25> == '1' then result = FPDefaultNaN();
 *         else result = sign : Ones(E) : '1' : operand<F_M-2:F_M-F>;  // the top fraction bits, quietened
 *         if type == FPType_SNaN || alt_hp then FPProcessException(FPExc_InvalidOp, fpscr_val);
 *     elsif type == FPType_Infinity then
 *         if alt_hp then result = sign:Ones(N-1); FPProcessException(FPExc_InvalidOp, fpscr_val);
 *         else result = FPInfinity(sign);
 *     elsif type == FPType_Zero then result = FPZero(sign);
 *     else result = FPRound(value, fpscr_val);
 */
static inline uint64 FPConvert(uint64 operand, uint32 M, uint32 N, uint32 *fpscr)
{
	ArmFPValue v = FPUnpack(operand, M, fpscr);
	bool alt_hp = (N == 16U) && ((*fpscr & ARM_FPSCR_AHP) != 0U);
	uint32 FM = ArmFPFracBits(M);
	uint32 FN = ArmFPFracBits(N);
	uint64 payload;
	uint64 result;

	if (ArmFPIsNaN(v.type)) {
		if (alt_hp) {
			result = FPZero(v.sign, N);
		}
		else if ((*fpscr & ARM_FPSCR_DN) != 0U) {
			result = FPDefaultNaN(N);
		}
		else {
			/* the fraction below the quiet bit, its top bits kept */
			payload = operand & ArmFPOnes(FM - 1U);
			payload = (FN >= FM) ? (payload << (FN - FM)) : (payload >> (FM - FN));
			result = FPInfinity(v.sign, N) | ((uint64)1U << (FN - 1U)) | payload;
		}
		if ((v.type == FPType_SNaN) || alt_hp) {
			FPProcessException(ARM_FPSCR_IOC, fpscr);
		}
		return result;
	}
	if (v.type == FPType_Infinity) {
		if (alt_hp) {
			FPProcessException(ARM_FPSCR_IOC, fpscr);
			return FPZero(v.sign, N) | ArmFPOnes(N - 1U);
		}
		return FPInfinity(v.sign, N);
	}
	if (v.type == FPType_Zero) {
		return FPZero(v.sign, N);
	}
	return FPRound(v.sign, ArmU128Make(0U, v.mant), v.exp, N, fpscr);
}

/*
 * The integer part of mant * 2^shift (a nonnegative value) and the error
 * of its fraction (half: >= 0.5, rest: the bits below); FALSE when it does
 * not fit in 64 bits.
 */
static inline bool ArmFPIntegerPart(uint64 mant, sint32 shift, uint64 *int_part, bool *half, bool *rest)
{
	*half = FALSE;
	*rest = FALSE;
	if (shift >= 0) {
		if ((shift >= 64) || ((ArmMsb64(mant) + (uint32)shift) >= 64U)) {
			return FALSE;
		}
		*int_part = mant << (uint32)shift;
		return TRUE;
	}
	if (shift <= -64) {
		/* below one half: mant (at most 53 bits, nonzero) times 2^-64 or less */
		*int_part = 0U;
		*rest = TRUE;
		return TRUE;
	}
	*int_part = mant >> (uint32)(-shift);
	*half = ((mant >> (uint32)(-shift - 1)) & 1U) != 0U;
	*rest = (mant & ArmFPOnes((uint32)(-shift - 1))) != 0U;
	return TRUE;
}

/*
 * Round a magnitude with its error (half, rest) to an integer, for a value
 * of the given sign: the pseudo-code's
 *     int_result = RoundDown(value); error = value - int_result;
 *     case rounding of
 *         when FPRounding_TIEEVEN round_up = (error > 0.5 || (error == 0.5 && int_result<0> == '1'));
 *         when FPRounding_POSINF  round_up = (error != 0.0);
 *         when FPRounding_NEGINF  round_up = FALSE;
 *         when FPRounding_ZERO    round_up = (error != 0.0 && int_result < 0);
 *         when FPRounding_TIEAWAY round_up = (error > 0.5 || (error == 0.5 && int_result >= 0));
 *     if round_up then int_result = int_result + 1;
 * on the magnitude (RoundDown of a negative value rounds its magnitude up).
 */
static inline uint64 ArmFPRoundMagnitude(uint64 magnitude, bool half, bool rest, bool sign, FPRounding rounding)
{
	bool inexact = half || rest;
	bool up;

	switch (rounding) {
	case FPRounding_TIEEVEN:
		up = half && (rest || ((magnitude & 1U) != 0U));
		break;
	case FPRounding_POSINF:
		up = inexact && !sign;
		break;
	case FPRounding_NEGINF:
		up = inexact && sign;
		break;
	case FPRounding_TIEAWAY:
		up = half;
		break;
	default:
		up = FALSE;
		break;
	}
	return magnitude + (up ? 1U : 0U);
}

/*
 * bits(M) FPToFixed(bits(N) operand, integer M, integer fraction_bits, boolean unsigned,
 *                   FPRounding rounding)
 *     (type,sign,value) = FPUnpack(operand, fpscr_val);
 *     // For NaNs and infinities, FPUnpack() has produced a value that will round to the
 *     // required result of the conversion. Also, the value produced for infinities will
 *     // cause the conversion to overflow and signal an Invalid Operation floating-point
 *     // exception as required. NaNs must also generate such a floating-point exception.
 *     if type == FPType_SNaN || type == FPType_QNaN then
 *         FPProcessException(FPExc_InvalidOp, fpscr_val);
 *     // Scale value by specified number of fraction bits, then start rounding to an integer
 *     // and determine the rounding error.
 *     value = value * 2^fraction_bits;
 *     int_result = RoundDown(value);
 *     error = value - int_result;
 *     // Apply the specified rounding mode.
 *     (round_up as ArmFPRoundMagnitude)
 *     if round_up then int_result = int_result + 1;
 *     // Bitstring result is the integer result saturated to the destination size, with
 *     // saturation indicating overflow of the conversion (signaled as an Invalid
 *     // Operation floating-point exception).
 *     (result, overflow) = SatQ(int_result, M, unsigned);
 *     if overflow then
 *         FPProcessException(FPExc_InvalidOp, fpscr_val);
 *     elsif error != 0 then
 *         FPProcessException(FPExc_Inexact, fpscr_val);
 * (NaNs have the value 0, infinities the value 2^1000000.)
 */
static inline uint32 FPToFixed(uint64 operand, uint32 N, uint32 M, uint32 fraction_bits, bool unsigned_result,
	FPRounding rounding, uint32 *fpscr)
{
	ArmFPValue v = FPUnpack(operand, N, fpscr);
	uint64 magnitude = 0U;
	bool half = FALSE;
	bool rest = FALSE;
	bool fits = TRUE;
	sint64 int_result;
	bool overflow;
	uint32 result;

	if (ArmFPIsNaN(v.type)) {
		FPProcessException(ARM_FPSCR_IOC, fpscr);
	}
	else if (v.type == FPType_Infinity) {
		fits = FALSE;
	}
	else if (v.type == FPType_Nonzero) {
		fits = ArmFPIntegerPart(v.mant, v.exp + (sint32)fraction_bits, &magnitude, &half, &rest);
		if (fits) {
			magnitude = ArmFPRoundMagnitude(magnitude, half, rest, v.sign, rounding);
			fits = magnitude <= ((uint64)1U << 62);
		}
	}
	if (!fits) {
		/* far out of range: saturate by the sign */
		int_result = v.sign ? -((sint64)1 << 62) : ((sint64)1 << 62);
	}
	else {
		int_result = v.sign ? -(sint64)magnitude : (sint64)magnitude;
	}
	if (unsigned_result) {
		result = UnsignedSatQ(int_result, M, &overflow);
	}
	else {
		result = (uint32)SignedSatQ(int_result, M, &overflow) & (uint32)ArmFPOnes(M);
	}
	if (overflow) {
		FPProcessException(ARM_FPSCR_IOC, fpscr);
	}
	else if (half || rest) {
		FPProcessException(ARM_FPSCR_IXC, fpscr);
	}
	return result;
}

/*
 * bits(N) FixedToFP(bits(M) operand, integer N, integer fraction_bits, boolean unsigned,
 *                   boolean round_to_nearest)
 *     if round_to_nearest then fpscr_val<23:22> = '00';
 *     int_operand = if unsigned then UInt(operand) else SInt(operand);
 *     real_operand = int_operand / 2^fraction_bits;
 *     if real_operand == 0.0 then
 *         result = FPZero('0');
 *     else
 *         result = FPRound(real_operand, fpscr_val);
 */
static inline uint64 FixedToFP(uint32 operand, uint32 M, uint32 N, uint32 fraction_bits, bool unsigned_operand,
	bool round_to_nearest, uint32 *fpscr)
{
	uint64 value = operand & ArmFPOnes(M);
	bool sign = FALSE;

	if (!unsigned_operand && (((value >> (M - 1U)) & 1U) != 0U)) {
		sign = TRUE;
		value = ((~value) + 1U) & ArmFPOnes(M);	/* -2^(M-1) stays 2^(M-1) */
	}
	if (value == 0U) {
		return FPZero(FALSE, N);
	}
	return FPRoundMode(sign, ArmU128Make(0U, value), -(sint32)fraction_bits, N,
		round_to_nearest ? FPRounding_TIEEVEN : ArmFPSCRRounding(*fpscr), fpscr);
}

/*
 * bits(N) FPRoundInt(bits(N) op, FPRounding rounding, boolean exact)
 *     (type,sign,value) = FPUnpack(op, fpscr_val);
 *     if type == FPType_SNaN || type == FPType_QNaN then
 *         result = FPProcessNaN(type, op, fpscr_val);
 *     elsif type == FPType_Infinity then
 *         result = FPInfinity(sign);
 *     elsif type == FPType_Zero then
 *         result = FPZero(sign);
 *     else
 *         // extract integer component
 *         int_result = RoundDown(value);
 *         error = value - int_result;
 *         (round_up as ArmFPRoundMagnitude)
 *         if round_up then int_result = int_result + 1;
 *         // Convert integer value into an equivalent real value
 *         real_result = int_result;
 *         // Re-encode as a floating-point value, result is always exact
 *         if real_result == 0.0 then
 *             result = FPZero(sign);
 *         else
 *             result = FPRound(real_result, fpscr_val, FPRounding_ZERO);
 *         // Generate inexact exceptions
 *         if error != 0.0 && exact then
 *             FPProcessException(FPExc_Inexact, fpscr_val);
 */
static inline uint64 FPRoundInt(uint64 op, uint32 N, FPRounding rounding, bool exact, uint32 *fpscr)
{
	ArmFPValue v = FPUnpack(op, N, fpscr);
	uint64 magnitude;
	bool half;
	bool rest;
	uint64 result;

	if (ArmFPIsNaN(v.type)) {
		return FPProcessNaN(v.type, op, N, fpscr);
	}
	if (v.type == FPType_Infinity) {
		return FPInfinity(v.sign, N);
	}
	if (v.type == FPType_Zero) {
		return FPZero(v.sign, N);
	}
	if (v.exp >= 0) {
		/* already an integer */
		return op;
	}
	(void)ArmFPIntegerPart(v.mant, v.exp, &magnitude, &half, &rest);
	magnitude = ArmFPRoundMagnitude(magnitude, half, rest, v.sign, rounding);
	if (magnitude == 0U) {
		result = FPZero(v.sign, N);
	}
	else {
		result = FPRoundMode(v.sign, ArmU128Make(0U, magnitude), 0, N, FPRounding_ZERO, fpscr);
	}
	if ((half || rest) && exact) {
		FPProcessException(ARM_FPSCR_IXC, fpscr);
	}
	return result;
}

/*
 * bits(N) FPMax(bits(N) op1, bits(N) op2), FPMin
 *     (type1,sign1,value1) = FPUnpack(op1, fpscr_val);
 *     (type2,sign2,value2) = FPUnpack(op2, fpscr_val);
 *     (done,result) = FPProcessNaNs(type1, type2, op1, op2, fpscr_val);
 *     if !done then
 *         if value1 > value2 then (type,sign,value) = (type1,sign1,value1);  // FPMin: value1 < value2
 *         else (type,sign,value) = (type2,sign2,value2);
 *         if type == FPType_Infinity then result = FPInfinity(sign);
 *         elsif type == FPType_Zero then
 *             sign = sign1 AND sign2;  // Use most positive sign (FPMin: sign1 OR sign2)
 *             result = FPZero(sign);
 *         else
 *             result = FPRound(value, fpscr_val);
 */
static inline uint64 ArmFPMaxMin(uint64 op1, uint64 op2, bool max, uint32 N, uint32 *fpscr)
{
	ArmFPValue v1 = FPUnpack(op1, N, fpscr);
	ArmFPValue v2 = FPUnpack(op2, N, fpscr);
	const ArmFPValue *v;
	sint32 order;
	uint64 result;

	if (FPProcessNaNs(v1.type, v2.type, op1, op2, N, fpscr, &result)) {
		return result;
	}
	order = ArmFPOrder(&v1, &v2);
	v = (max ? (order > 0) : (order < 0)) ? &v1 : &v2;
	if (v->type == FPType_Infinity) {
		return FPInfinity(v->sign, N);
	}
	if (v->type == FPType_Zero) {
		return FPZero(max ? (v1.sign && v2.sign) : (v1.sign || v2.sign), N);
	}
	return FPRound(v->sign, ArmU128Make(0U, v->mant), v->exp, N, fpscr);
}

/*
 * bits(N) FPMaxNum(bits(N) op1, bits(N) op2), FPMinNum
 *     type1 = FPUnpack(op1, fpscr_val); type2 = FPUnpack(op2, fpscr_val);
 *     // treat a single quiet-NaN as -Infinity (FPMinNum: +Infinity)
 *     if type1 == FPType_QNaN && type2 != FPType_QNaN then op1 = FPInfinity('1');
 *     elsif type1 != FPType_QNaN && type2 == FPType_QNaN then op2 = FPInfinity('1');
 *     return FPMax(op1, op2, fpscr_val);
 */
static inline uint64 ArmFPMaxMinNum(uint64 op1, uint64 op2, bool max, uint32 N, uint32 *fpscr)
{
	FPType type1 = FPUnpack(op1, N, fpscr).type;
	FPType type2 = FPUnpack(op2, N, fpscr).type;

	if ((type1 == FPType_QNaN) && (type2 != FPType_QNaN)) {
		op1 = FPInfinity(max, N);
	}
	else if ((type1 != FPType_QNaN) && (type2 == FPType_QNaN)) {
		op2 = FPInfinity(max, N);
	}
	return ArmFPMaxMin(op1, op2, max, N, fpscr);
}

static inline uint64 FPMaxNum(uint64 op1, uint64 op2, uint32 N, uint32 *fpscr)
{
	return ArmFPMaxMinNum(op1, op2, TRUE, N, fpscr);
}

static inline uint64 FPMinNum(uint64 op1, uint64 op2, uint32 N, uint32 *fpscr)
{
	return ArmFPMaxMinNum(op1, op2, FALSE, N, fpscr);
}

#endif /* ARM_COMMON_ARM_FP_H */
