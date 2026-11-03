/*
 * Helper functions of Arm's pseudo-code (Armv7-A/R and Armv7-M Architecture
 * Reference Manuals, "Pseudocode details") on 32-bit values. Each function
 * has the pseudo-code it implements in a comment. They read no processor
 * state: the APSR and the operands are passed in.
 */
#ifndef ARM_COMMON_ARM_HELPERS_H
#define ARM_COMMON_ARM_HELPERS_H

#include "arm_types.h"

#define ARM_BIT(x, n)	(((x) >> (n)) & 1U)

/*
 * Shifts. The pseudo-code's shift functions take any shift amount; the
 * register-shifted forms of instructions give amounts up to 255.
 */

/*
 * (bits(N), bit) LSL_C(bits(N) x, integer shift)
 *     assert shift > 0;
 *     extended_x = x : Zeros(shift);
 *     result = extended_x<N-1:0>;
 *     carry_out = extended_x<N>;
 */
static inline uint32 LSL_C(uint32 x, uint32 shift, bool *carry_out)
{
	uint32 result = 0U;
	bool carry = FALSE;

	if (shift < 32U) {
		result = x << shift;
	}
	if (shift <= 32U) {
		carry = ARM_BIT(x, 32U - shift);
	}
	if (carry_out != NULL) {
		*carry_out = carry;
	}
	return result;
}

/*
 * bits(N) LSL(bits(N) x, integer shift)
 *     if shift == 0 then result = x; else (result, -) = LSL_C(x, shift);
 */
static inline uint32 LSL(uint32 x, uint32 shift)
{
	if (shift == 0U) {
		return x;
	}
	return LSL_C(x, shift, NULL);
}

/*
 * (bits(N), bit) LSR_C(bits(N) x, integer shift)
 *     assert shift > 0;
 *     extended_x = ZeroExtend(x, shift+N);
 *     result = extended_x<shift+N-1:shift>;
 *     carry_out = extended_x<shift-1>;
 */
static inline uint32 LSR_C(uint32 x, uint32 shift, bool *carry_out)
{
	uint32 result = 0U;
	bool carry = FALSE;

	if (shift < 32U) {
		result = x >> shift;
	}
	if (shift <= 32U) {
		carry = ARM_BIT(x, shift - 1U);
	}
	if (carry_out != NULL) {
		*carry_out = carry;
	}
	return result;
}

/*
 * bits(N) LSR(bits(N) x, integer shift)
 *     if shift == 0 then result = x; else (result, -) = LSR_C(x, shift);
 */
static inline uint32 LSR(uint32 x, uint32 shift)
{
	if (shift == 0U) {
		return x;
	}
	return LSR_C(x, shift, NULL);
}

/*
 * (bits(N), bit) ASR_C(bits(N) x, integer shift)
 *     assert shift > 0;
 *     extended_x = SignExtend(x, shift+N);
 *     result = extended_x<shift+N-1:shift>;
 *     carry_out = extended_x<shift-1>;
 */
static inline uint32 ASR_C(uint32 x, uint32 shift, bool *carry_out)
{
	uint32 sign = ARM_BIT(x, 31U);
	uint32 result;
	bool carry;

	if (shift < 32U) {
		result = (uint32)((sint32)x >> shift);
		carry = ARM_BIT(x, shift - 1U);
	}
	else {
		result = (sign != 0U) ? 0xFFFFFFFFU : 0U;
		carry = sign;
	}
	if (carry_out != NULL) {
		*carry_out = carry;
	}
	return result;
}

/*
 * bits(N) ASR(bits(N) x, integer shift)
 *     if shift == 0 then result = x; else (result, -) = ASR_C(x, shift);
 */
static inline uint32 ASR(uint32 x, uint32 shift)
{
	if (shift == 0U) {
		return x;
	}
	return ASR_C(x, shift, NULL);
}

/*
 * (bits(N), bit) ROR_C(bits(N) x, integer shift)
 *     assert shift != 0;
 *     m = shift MOD N;
 *     result = LSR(x,m) OR LSL(x,N-m);
 *     carry_out = result<N-1>;
 */
static inline uint32 ROR_C(uint32 x, uint32 shift, bool *carry_out)
{
	uint32 m = shift % 32U;
	uint32 result = LSR(x, m) | LSL(x, 32U - m);

	if (carry_out != NULL) {
		*carry_out = ARM_BIT(result, 31U);
	}
	return result;
}

/*
 * bits(N) ROR(bits(N) x, integer shift)
 *     if shift == 0 then result = x; else (result, -) = ROR_C(x, shift);
 */
static inline uint32 ROR(uint32 x, uint32 shift)
{
	if (shift == 0U) {
		return x;
	}
	return ROR_C(x, shift, NULL);
}

/*
 * (bits(N), bit) RRX_C(bits(N) x, bit carry_in)
 *     result = carry_in : x<N-1:1>;
 *     carry_out = x<0>;
 */
static inline uint32 RRX_C(uint32 x, bool carry_in, bool *carry_out)
{
	uint32 result = x >> 1U;

	if (carry_in) {
		result |= 1U << 31U;
	}
	if (carry_out != NULL) {
		*carry_out = ARM_BIT(x, 0U);
	}
	return result;
}

/*
 * bits(N) RRX(bits(N) x, bit carry_in)
 *     (result, -) = RRX_C(x, carry_in);
 */
static inline uint32 RRX(uint32 x, bool carry_in)
{
	return RRX_C(x, carry_in, NULL);
}

/*
 * (bits(N), bit) Shift_C(bits(N) value, SRType type, integer amount, bit carry_in)
 *     assert !(type == SRType_RRX && amount != 1);
 *     if amount == 0 then
 *         (result, carry_out) = (value, carry_in);
 *     else
 *         case type of
 *             when SRType_LSL (result, carry_out) = LSL_C(value, amount);
 *             when SRType_LSR (result, carry_out) = LSR_C(value, amount);
 *             when SRType_ASR (result, carry_out) = ASR_C(value, amount);
 *             when SRType_ROR (result, carry_out) = ROR_C(value, amount);
 *             when SRType_RRX (result, carry_out) = RRX_C(value, carry_in);
 */
static inline uint32 Shift_C(uint32 value, SRType type, uint32 amount, bool carry_in, bool *carry_out)
{
	uint32 result;
	bool carry;

	if (amount == 0U) {
		result = value;
		carry = carry_in;
	}
	else {
		switch (type) {
		case SRType_LSL:
			result = LSL_C(value, amount, &carry);
			break;
		case SRType_LSR:
			result = LSR_C(value, amount, &carry);
			break;
		case SRType_ASR:
			result = ASR_C(value, amount, &carry);
			break;
		case SRType_ROR:
			result = ROR_C(value, amount, &carry);
			break;
		case SRType_RRX:
		default:
			result = RRX_C(value, carry_in, &carry);
			break;
		}
	}
	if (carry_out != NULL) {
		*carry_out = carry;
	}
	return result;
}

/*
 * bits(N) Shift(bits(N) value, SRType type, integer amount, bit carry_in)
 *     (result, -) = Shift_C(value, type, amount, carry_in);
 */
static inline uint32 Shift(uint32 value, SRType type, uint32 amount, bool carry_in)
{
	return Shift_C(value, type, amount, carry_in, NULL);
}

/*
 * (SRType, integer) DecodeImmShift(bits(2) type, bits(5) imm5)
 *     case type of
 *         when '00' shift_t = SRType_LSL; shift_n = UInt(imm5);
 *         when '01' shift_t = SRType_LSR; shift_n = if imm5 == '00000' then 32 else UInt(imm5);
 *         when '10' shift_t = SRType_ASR; shift_n = if imm5 == '00000' then 32 else UInt(imm5);
 *         when '11'
 *             if imm5 == '00000' then shift_t = SRType_RRX; shift_n = 1;
 *             else shift_t = SRType_ROR; shift_n = UInt(imm5);
 */
static inline void DecodeImmShift(uint32 type, uint32 imm5, SRType *shift_t, uint32 *shift_n)
{
	switch (type & 0x3U) {
	case 0x0U:
		*shift_t = SRType_LSL;
		*shift_n = imm5;
		break;
	case 0x1U:
		*shift_t = SRType_LSR;
		*shift_n = (imm5 == 0U) ? 32U : imm5;
		break;
	case 0x2U:
		*shift_t = SRType_ASR;
		*shift_n = (imm5 == 0U) ? 32U : imm5;
		break;
	default:
		if (imm5 == 0U) {
			*shift_t = SRType_RRX;
			*shift_n = 1U;
		}
		else {
			*shift_t = SRType_ROR;
			*shift_n = imm5;
		}
		break;
	}
}

/*
 * SRType DecodeRegShift(bits(2) type)
 *     case type of
 *         when '00' shift_t = SRType_LSL;
 *         when '01' shift_t = SRType_LSR;
 *         when '10' shift_t = SRType_ASR;
 *         when '11' shift_t = SRType_ROR;
 */
static inline SRType DecodeRegShift(uint32 type)
{
	switch (type & 0x3U) {
	case 0x0U:
		return SRType_LSL;
	case 0x1U:
		return SRType_LSR;
	case 0x2U:
		return SRType_ASR;
	default:
		return SRType_ROR;
	}
}

/*
 * Modified immediates.
 */

/*
 * (bits(32), bit) ARMExpandImm_C(bits(12) imm12, bit carry_in)
 *     unrotated_value = ZeroExtend(imm12<7:0>, 32);
 *     (imm32, carry_out) = Shift_C(unrotated_value, SRType_ROR, 2*UInt(imm12<11:8>), carry_in);
 */
static inline uint32 ARMExpandImm_C(uint32 imm12, bool carry_in, bool *carry_out)
{
	uint32 unrotated_value = imm12 & 0xFFU;
	uint32 amount = 2U * ((imm12 >> 8U) & 0xFU);

	return Shift_C(unrotated_value, SRType_ROR, amount, carry_in, carry_out);
}

/*
 * bits(32) ARMExpandImm(bits(12) imm12)
 *     // APSR.C argument to following function call does not affect the imm32 result.
 *     (imm32, -) = ARMExpandImm_C(imm12, APSR.C);
 */
static inline uint32 ARMExpandImm(uint32 imm12)
{
	return ARMExpandImm_C(imm12, FALSE, NULL);
}

/*
 * (bits(32), bit) ThumbExpandImm_C(bits(12) imm12, bit carry_in)
 *     if imm12<11:10> == '00' then
 *         case imm12<9:8> of
 *             when '00' imm32 = ZeroExtend(imm12<7:0>, 32);
 *             when '01' if imm12<7:0> == '00000000' then UNPREDICTABLE;
 *                       imm32 = '00000000' : imm12<7:0> : '00000000' : imm12<7:0>;
 *             when '10' if imm12<7:0> == '00000000' then UNPREDICTABLE;
 *                       imm32 = imm12<7:0> : '00000000' : imm12<7:0> : '00000000';
 *             when '11' if imm12<7:0> == '00000000' then UNPREDICTABLE;
 *                       imm32 = imm12<7:0> : imm12<7:0> : imm12<7:0> : imm12<7:0>;
 *         carry_out = carry_in;
 *     else
 *         unrotated_value = ZeroExtend('1':imm12<6:0>, 32);
 *         (imm32, carry_out) = ROR_C(unrotated_value, UInt(imm12<11:7>));
 *
 * The UNPREDICTABLE cases are left to the encoding glue
 * (ThumbExpandImmUnpredictable).
 */
static inline uint32 ThumbExpandImm_C(uint32 imm12, bool carry_in, bool *carry_out)
{
	uint32 imm8 = imm12 & 0xFFU;
	uint32 imm32;
	bool carry;

	if (((imm12 >> 10U) & 0x3U) == 0U) {
		switch ((imm12 >> 8U) & 0x3U) {
		case 0x0U:
			imm32 = imm8;
			break;
		case 0x1U:
			imm32 = (imm8 << 16U) | imm8;
			break;
		case 0x2U:
			imm32 = (imm8 << 24U) | (imm8 << 8U);
			break;
		default:
			imm32 = (imm8 << 24U) | (imm8 << 16U) | (imm8 << 8U) | imm8;
			break;
		}
		carry = carry_in;
	}
	else {
		uint32 unrotated_value = 0x80U | (imm12 & 0x7FU);
		imm32 = ROR_C(unrotated_value, (imm12 >> 7U) & 0x1FU, &carry);
	}
	if (carry_out != NULL) {
		*carry_out = carry;
	}
	return imm32;
}

/*
 * bits(32) ThumbExpandImm(bits(12) imm12)
 *     // APSR.C argument to following function call does not affect the imm32 result.
 *     (imm32, -) = ThumbExpandImm_C(imm12, APSR.C);
 */
static inline uint32 ThumbExpandImm(uint32 imm12)
{
	return ThumbExpandImm_C(imm12, FALSE, NULL);
}

/* The UNPREDICTABLE encodings of ThumbExpandImm_C: a replicated pattern of zero. */
static inline bool ThumbExpandImmUnpredictable(uint32 imm12)
{
	return (((imm12 >> 10U) & 0x3U) == 0U) && (((imm12 >> 8U) & 0x3U) != 0U) && ((imm12 & 0xFFU) == 0U);
}

/*
 * Arithmetic.
 */

/*
 * (bits(N), bit, bit) AddWithCarry(bits(N) x, bits(N) y, bit carry_in)
 *     unsigned_sum = UInt(x) + UInt(y) + UInt(carry_in);
 *     signed_sum = SInt(x) + SInt(y) + UInt(carry_in);
 *     result = unsigned_sum<N-1:0>; // same value as signed_sum<N-1:0>
 *     carry_out = if UInt(result) == unsigned_sum then '0' else '1';
 *     overflow = if SInt(result) == signed_sum then '0' else '1';
 *
 * carry_out and overflow go to flags->carry and flags->overflow.
 */
static inline uint32 AddWithCarry(uint32 x, uint32 y, bool carry_in, PseudoCodeStatusFlagType *flags)
{
	uint64 unsigned_sum = (uint64)x + (uint64)y + (carry_in ? 1U : 0U);
	sint64 signed_sum = (sint64)(sint32)x + (sint64)(sint32)y + (carry_in ? 1 : 0);
	uint32 result = (uint32)unsigned_sum;

	if (flags != NULL) {
		flags->carry = ((uint64)result != unsigned_sum);
		flags->overflow = ((sint64)(sint32)result != signed_sum);
	}
	return result;
}

/*
 * (bits(N), boolean) SignedSatQ(integer i, integer N)
 *     if i > 2^(N-1) - 1 then
 *         result = 2^(N-1) - 1; saturated = TRUE;
 *     elsif i < -(2^(N-1)) then
 *         result = -(2^(N-1)); saturated = TRUE;
 *     else
 *         result = i; saturated = FALSE;
 *     return (result<N-1:0>, saturated);
 *
 * For 1 <= N <= 32; the result is returned sign-extended to 32 bits.
 */
static inline sint32 SignedSatQ(sint64 i, uint32 N, bool *saturated)
{
	sint64 max = ((sint64)1 << (N - 1U)) - 1;
	sint64 min = -((sint64)1 << (N - 1U));
	sint64 result = i;
	bool sat = FALSE;

	if (i > max) {
		result = max;
		sat = TRUE;
	}
	else if (i < min) {
		result = min;
		sat = TRUE;
	}
	if (saturated != NULL) {
		*saturated = sat;
	}
	return (sint32)result;
}

/*
 * (bits(N), boolean) UnsignedSatQ(integer i, integer N)
 *     if i > 2^N - 1 then
 *         result = 2^N - 1; saturated = TRUE;
 *     elsif i < 0 then
 *         result = 0; saturated = TRUE;
 *     else
 *         result = i; saturated = FALSE;
 *     return (result<N-1:0>, saturated);
 *
 * For 0 <= N <= 32.
 */
static inline uint32 UnsignedSatQ(sint64 i, uint32 N, bool *saturated)
{
	sint64 max = ((sint64)1 << N) - 1;
	sint64 result = i;
	bool sat = FALSE;

	if (i > max) {
		result = max;
		sat = TRUE;
	}
	else if (i < 0) {
		result = 0;
		sat = TRUE;
	}
	if (saturated != NULL) {
		*saturated = sat;
	}
	return (uint32)result;
}

/*
 * Bit operations.
 */

/* SignExtend(x<N-1:0>, 32): the N-bit value x sign-extended (1 <= N <= 32). */
static inline uint32 SignExtend(uint32 x, uint32 N)
{
	uint32 sign = 1U << (N - 1U);
	uint32 mask = (N == 32U) ? 0xFFFFFFFFU : ((1U << N) - 1U);
	uint32 value = x & mask;

	return (value ^ sign) - sign;
}

/* ZeroExtend(x<N-1:0>, 32): the N-bit value x zero-extended (0 <= N <= 32). */
static inline uint32 ZeroExtend(uint32 x, uint32 N)
{
	if (N >= 32U) {
		return x;
	}
	return x & ((1U << N) - 1U);
}

/* BitCount(x): the number of ones in x. */
static inline uint32 BitCount(uint32 x)
{
	uint32 count = 0U;
	uint32 i;

	for (i = 0U; i < 32U; i++) {
		count += ARM_BIT(x, i);
	}
	return count;
}

/* LowestSetBit(x): the lowest bit number that is one, or N (32) if x is zero. */
static inline uint32 LowestSetBit(uint32 x)
{
	uint32 i;

	for (i = 0U; i < 32U; i++) {
		if (ARM_BIT(x, i) != 0U) {
			return i;
		}
	}
	return 32U;
}

/* HighestSetBit(x): the highest bit number that is one, or -1 if x is zero. */
static inline sint32 HighestSetBit(uint32 x)
{
	sint32 i;

	for (i = 31; i >= 0; i--) {
		if (ARM_BIT(x, (uint32)i) != 0U) {
			return i;
		}
	}
	return -1;
}

/*
 * integer CountLeadingZeroBits(bits(N) x)
 *     return N - 1 - HighestSetBit(x);
 */
static inline uint32 CountLeadingZeroBits(uint32 x)
{
	return (uint32)(31 - HighestSetBit(x));
}

/* Align(x, y): x rounded down to a multiple of y (a power of two). */
static inline uint32 Align(uint32 x, uint32 y)
{
	return x & ~(y - 1U);
}

/*
 * Conditions and flags.
 */

/*
 * boolean ConditionHolds(bits(4) cond) on the APSR flags
 * (ConditionPassed() is ConditionHolds(CurrentCond()); the current condition
 * comes from the profile):
 *     case cond<3:1> of
 *         when '000' result = (APSR.Z == '1');                          // EQ or NE
 *         when '001' result = (APSR.C == '1');                          // CS or CC
 *         when '010' result = (APSR.N == '1');                          // MI or PL
 *         when '011' result = (APSR.V == '1');                          // VS or VC
 *         when '100' result = (APSR.C == '1') && (APSR.Z == '0');       // HI or LS
 *         when '101' result = (APSR.N == APSR.V);                       // GE or LT
 *         when '110' result = (APSR.N == APSR.V) && (APSR.Z == '0');    // GT or LE
 *         when '111' result = TRUE;                                     // AL
 *     if cond<0> == '1' && cond != '1111' then result = !result;
 */
static inline bool ConditionHolds(uint32 cond, uint32 apsr)
{
	uint32 n = ARM_BIT(apsr, ARM_APSR_BITPOS_N);
	uint32 z = ARM_BIT(apsr, ARM_APSR_BITPOS_Z);
	uint32 c = ARM_BIT(apsr, ARM_APSR_BITPOS_C);
	uint32 v = ARM_BIT(apsr, ARM_APSR_BITPOS_V);
	bool result;

	switch ((cond >> 1U) & 0x7U) {
	case 0x0U:
		result = (z == 1U);
		break;
	case 0x1U:
		result = (c == 1U);
		break;
	case 0x2U:
		result = (n == 1U);
		break;
	case 0x3U:
		result = (v == 1U);
		break;
	case 0x4U:
		result = (c == 1U) && (z == 0U);
		break;
	case 0x5U:
		result = (n == v);
		break;
	case 0x6U:
		result = (n == v) && (z == 0U);
		break;
	default:
		result = TRUE;
		break;
	}
	if (((cond & 0x1U) != 0U) && ((cond & 0xFU) != 0xFU)) {
		result = !result;
	}
	return result;
}

/* The N, Z, C and V flags of an APSR value. */
static inline PseudoCodeStatusFlagType ApsrFlags(uint32 apsr)
{
	PseudoCodeStatusFlagType flags;

	flags.negative = ARM_BIT(apsr, ARM_APSR_BITPOS_N);
	flags.zero = ARM_BIT(apsr, ARM_APSR_BITPOS_Z);
	flags.carry = ARM_BIT(apsr, ARM_APSR_BITPOS_C);
	flags.overflow = ARM_BIT(apsr, ARM_APSR_BITPOS_V);
	return flags;
}

/*
 * The flag update of a flag-setting data-processing instruction:
 *     APSR.N = result<N-1>;
 *     APSR.Z = IsZeroBit(result);
 *     APSR.C = carry;
 *     APSR.V = overflow;
 * The N and Z flags written are also stored in flags.
 */
static inline uint32 ApsrUpdateNZCV(uint32 apsr, uint32 result, PseudoCodeStatusFlagType *flags)
{
	uint32 nzcv;

	flags->negative = ARM_BIT(result, 31U);
	flags->zero = (result == 0U);
	nzcv = ((uint32)(flags->negative ? 1U : 0U) << ARM_APSR_BITPOS_N)
		| ((uint32)(flags->zero ? 1U : 0U) << ARM_APSR_BITPOS_Z)
		| ((uint32)(flags->carry ? 1U : 0U) << ARM_APSR_BITPOS_C)
		| ((uint32)(flags->overflow ? 1U : 0U) << ARM_APSR_BITPOS_V);
	return (apsr & ~(ARM_APSR_N | ARM_APSR_Z | ARM_APSR_C | ARM_APSR_V)) | nzcv;
}


/*
 * The IT state of the Thumb instruction set (ITSTATE.IT<7:0>: the base
 * condition in bits 7:5, the condition's low bit and the rest of the block
 * in bits 4:0).
 */

/* InITBlock(): return (ITSTATE.IT<3:0> != '0000'); */
static inline bool ArmInITBlock(uint32 itstate)
{
	return (itstate & 0xFU) != 0U;
}

/* LastInITBlock(): return (ITSTATE.IT<3:0> == '1000'); */
static inline bool ArmLastInITBlock(uint32 itstate)
{
	return (itstate & 0xFU) == 0x8U;
}

/* The condition of an instruction in an IT block: ITSTATE.IT<7:4>. */
static inline uint32 ArmITCondition(uint32 itstate)
{
	return (itstate >> 4) & 0xFU;
}

/*
 * ITAdvance()
 *     if ITSTATE.IT<2:0> == '000' then
 *         ITSTATE.IT = '00000000';
 *     else
 *         ITSTATE.IT<4:0> = LSL(ITSTATE.IT<4:0>, 1);
 * Returns the new IT state.
 */
static inline uint32 ArmITAdvance(uint32 itstate)
{
	if ((itstate & 0x7U) == 0U) {
		return 0U;
	}
	return (itstate & 0xE0U) | ((itstate << 1) & 0x1FU);
}

#endif /* ARM_COMMON_ARM_HELPERS_H */
