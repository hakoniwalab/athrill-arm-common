/*
 * Tests of arm_helpers.h.
 *
 * The expected values come from a second implementation of Arm's
 * pseudo-code written on bitstrings (arrays of bits, as the pseudo-code
 * writes them: x : Zeros(shift), extended_x<shift-1>, ...) and from values
 * worked out by hand from the pseudo-code, not from the helpers themselves.
 */
#include <stdio.h>
#include "arm_common/arm_helpers.h"

static int failures;
static int checks;

#define CHECK(cond, ...) \
	do { \
		checks++; \
		if (!(cond)) { \
			failures++; \
			if (failures <= 20) { \
				printf("FAIL %s:%d: ", __FILE__, __LINE__); \
				printf(__VA_ARGS__); \
				printf("\n"); \
			} \
		} \
	} while (0)

/*
 * Bitstrings: bits[i] is bit i; up to 32 + 255 bits.
 */
#define BITS_MAX (32 + 256)

typedef struct {
	int len;
	int bit[BITS_MAX];
} Bits;

static Bits bits_of(uint32 x)
{
	Bits b;
	int i;

	b.len = 32;
	for (i = 0; i < 32; i++) {
		b.bit[i] = (x >> i) & 1;
	}
	return b;
}

static uint32 value_of(const Bits *b, int high, int low)
{
	uint32 v = 0;
	int i;

	for (i = high; i >= low; i--) {
		v = (v << 1) | (uint32)b->bit[i];
	}
	return v;
}

/* x : Zeros(n) */
static Bits append_zeros(Bits x, int n)
{
	Bits r;
	int i;

	r.len = x.len + n;
	for (i = 0; i < n; i++) {
		r.bit[i] = 0;
	}
	for (i = 0; i < x.len; i++) {
		r.bit[i + n] = x.bit[i];
	}
	return r;
}

/* ZeroExtend(x, n) or SignExtend(x, n) */
static Bits extend(Bits x, int n, int sign)
{
	int top = x.bit[x.len - 1];
	int i;

	for (i = x.len; i < n; i++) {
		x.bit[i] = sign ? top : 0;
	}
	x.len = n;
	return x;
}

static uint32 ref_lsl_c(uint32 x, int shift, int *carry)
{
	Bits e = append_zeros(bits_of(x), shift);

	*carry = e.bit[32];
	return value_of(&e, 31, 0);
}

static uint32 ref_shift_right_c(uint32 x, int shift, int sign, int *carry)
{
	Bits e = extend(bits_of(x), shift + 32, sign);

	*carry = e.bit[shift - 1];
	return value_of(&e, shift + 31, shift);
}

static uint32 ref_ror_c(uint32 x, int shift, int *carry)
{
	Bits b = bits_of(x);
	Bits r;
	int m = shift % 32;
	int i;

	r.len = 32;
	for (i = 0; i < 32; i++) {
		r.bit[i] = b.bit[(i + m) % 32];
	}
	*carry = r.bit[31];
	return value_of(&r, 31, 0);
}

static const uint32 samples[] = {
	0x00000000U, 0x00000001U, 0x00000002U, 0x7FFFFFFFU, 0x80000000U, 0x80000001U,
	0xFFFFFFFFU, 0xFFFFFFFEU, 0x12345678U, 0x87654321U, 0xA5A5A5A5U, 0x5A5A5A5AU,
	0x0000FFFFU, 0xFFFF0000U, 0x00010000U, 0x40000000U, 0xC0000000U, 0xDEADBEEFU,
};
#define SAMPLES ((int)(sizeof(samples) / sizeof(samples[0])))

static void test_shifts(void)
{
	int s;
	int shift;

	for (s = 0; s < SAMPLES; s++) {
		uint32 x = samples[s];
		for (shift = 1; shift <= 255; shift++) {
			bool carry = 2;
			int ref_carry;
			uint32 ref;
			uint32 got;

			ref = ref_lsl_c(x, shift, &ref_carry);
			got = LSL_C(x, (uint32)shift, &carry);
			CHECK(got == ref && carry == (bool)ref_carry, "LSL_C(%08x, %d) = %08x/%d, expected %08x/%d", x, shift, got, carry, ref, ref_carry);

			ref = ref_shift_right_c(x, shift, 0, &ref_carry);
			got = LSR_C(x, (uint32)shift, &carry);
			CHECK(got == ref && carry == (bool)ref_carry, "LSR_C(%08x, %d) = %08x/%d, expected %08x/%d", x, shift, got, carry, ref, ref_carry);

			ref = ref_shift_right_c(x, shift, 1, &ref_carry);
			got = ASR_C(x, (uint32)shift, &carry);
			CHECK(got == ref && carry == (bool)ref_carry, "ASR_C(%08x, %d) = %08x/%d, expected %08x/%d", x, shift, got, carry, ref, ref_carry);

			ref = ref_ror_c(x, shift, &ref_carry);
			got = ROR_C(x, (uint32)shift, &carry);
			CHECK(got == ref && carry == (bool)ref_carry, "ROR_C(%08x, %d) = %08x/%d, expected %08x/%d", x, shift, got, carry, ref, ref_carry);
		}
		/* Shift amount 0: value and carry_in unchanged (Shift_C). */
		{
			bool carry = 2;
			CHECK(Shift_C(x, SRType_LSL, 0U, TRUE, &carry) == x && carry == TRUE, "Shift_C amount 0");
			CHECK(Shift_C(x, SRType_ASR, 0U, FALSE, &carry) == x && carry == FALSE, "Shift_C amount 0");
			CHECK(LSL(x, 0U) == x && LSR(x, 0U) == x && ASR(x, 0U) == x && ROR(x, 0U) == x, "shift by 0");
			CHECK(LSL(x, 5U) == LSL_C(x, 5U, NULL) && LSR(x, 5U) == LSR_C(x, 5U, NULL) &&
			      ASR(x, 5U) == ASR_C(x, 5U, NULL) && ROR(x, 5U) == ROR_C(x, 5U, NULL) &&
			      RRX(x, TRUE) == RRX_C(x, TRUE, NULL), "shift without carry");
		}
		/* RRX: carry_in : x<31:1>, carry_out = x<0>. */
		{
			bool carry = 2;
			uint32 got = RRX_C(x, TRUE, &carry);
			CHECK(got == ((x >> 1) | 0x80000000U) && carry == (bool)(x & 1U), "RRX_C(%08x, 1)", x);
			got = Shift_C(x, SRType_RRX, 1U, FALSE, &carry);
			CHECK(got == (x >> 1) && carry == (bool)(x & 1U), "Shift_C RRX(%08x, 0)", x);
		}
	}
	/* By hand. */
	{
		bool carry = 2;
		CHECK(LSL_C(0x40000001U, 2U, &carry) == 0x00000004U && carry == TRUE, "LSL_C bit 30 out");
		CHECK(LSR_C(0x00000006U, 2U, &carry) == 0x00000001U && carry == TRUE, "LSR_C bit 1 out");
		CHECK(ASR_C(0x80000004U, 3U, &carry) == 0xF0000000U && carry == TRUE, "ASR_C bit 2 out");
		CHECK(LSL_C(0x00000001U, 32U, &carry) == 0U && carry == TRUE, "LSL_C by 32");
		CHECK(LSL_C(0xFFFFFFFFU, 33U, &carry) == 0U && carry == FALSE, "LSL_C by 33");
		CHECK(LSR_C(0x80000000U, 32U, &carry) == 0U && carry == TRUE, "LSR_C by 32");
		CHECK(ASR_C(0x80000000U, 40U, &carry) == 0xFFFFFFFFU && carry == TRUE, "ASR_C by 40");
		CHECK(ROR_C(0x00000001U, 1U, &carry) == 0x80000000U && carry == TRUE, "ROR_C by 1");
		CHECK(ROR_C(0x80000000U, 32U, &carry) == 0x80000000U && carry == TRUE, "ROR_C by 32");
	}
}

static void test_decode_shift(void)
{
	SRType t;
	uint32 n;

	DecodeImmShift(0U, 0U, &t, &n);
	CHECK(t == SRType_LSL && n == 0U, "LSL #0");
	DecodeImmShift(0U, 31U, &t, &n);
	CHECK(t == SRType_LSL && n == 31U, "LSL #31");
	DecodeImmShift(1U, 0U, &t, &n);
	CHECK(t == SRType_LSR && n == 32U, "LSR #32");
	DecodeImmShift(2U, 0U, &t, &n);
	CHECK(t == SRType_ASR && n == 32U, "ASR #32");
	DecodeImmShift(2U, 5U, &t, &n);
	CHECK(t == SRType_ASR && n == 5U, "ASR #5");
	DecodeImmShift(3U, 0U, &t, &n);
	CHECK(t == SRType_RRX && n == 1U, "RRX");
	DecodeImmShift(3U, 7U, &t, &n);
	CHECK(t == SRType_ROR && n == 7U, "ROR #7");
	CHECK(DecodeRegShift(0U) == SRType_LSL && DecodeRegShift(1U) == SRType_LSR &&
	      DecodeRegShift(2U) == SRType_ASR && DecodeRegShift(3U) == SRType_ROR, "DecodeRegShift");
}

static void test_expand_imm(void)
{
	bool carry = 2;
	uint32 imm12;

	/* ARMExpandImm: imm8 rotated right by 2 * rotate. */
	CHECK(ARMExpandImm_C(0x0FFU, TRUE, &carry) == 0x000000FFU && carry == TRUE, "ARM rot 0 keeps carry");
	CHECK(ARMExpandImm_C(0x1FFU, FALSE, &carry) == 0xC000003FU && carry == TRUE, "ARM rot 2");
	CHECK(ARMExpandImm_C(0x4FFU, TRUE, &carry) == 0xFF000000U && carry == TRUE, "ARM rot 8");
	CHECK(ARMExpandImm_C(0xF01U, TRUE, &carry) == 0x00000004U && carry == FALSE, "ARM rot 30");

	/* ThumbExpandImm: replicated patterns keep carry_in. */
	CHECK(ThumbExpandImm_C(0x0ABU, TRUE, &carry) == 0x000000ABU && carry == TRUE, "Thumb 00");
	CHECK(ThumbExpandImm_C(0x1ABU, FALSE, &carry) == 0x00AB00ABU && carry == FALSE, "Thumb 01");
	CHECK(ThumbExpandImm_C(0x2ABU, TRUE, &carry) == 0xAB00AB00U && carry == TRUE, "Thumb 10");
	CHECK(ThumbExpandImm_C(0x3ABU, FALSE, &carry) == 0xABABABABU && carry == FALSE, "Thumb 11");
	/* Rotated: '1':imm12<6:0> rotated right by imm12<11:7>; carry = bit 31. */
	CHECK(ThumbExpandImm_C(0x400U, FALSE, &carry) == 0x80000000U && carry == TRUE, "Thumb rot 8");
	CHECK(ThumbExpandImm_C(0xFFFU, FALSE, &carry) == 0x000001FEU && carry == FALSE, "Thumb rot 31");
	CHECK(ThumbExpandImm_C(0x47FU, FALSE, &carry) == 0xFF000000U && carry == TRUE, "Thumb rot 8 0xFF");
	/* Every rotated form against the bitstring ROR. */
	for (imm12 = 0x400U; imm12 <= 0xFFFU; imm12++) {
		int ref_carry;
		uint32 ref = ref_ror_c(0x80U | (imm12 & 0x7FU), (int)(imm12 >> 7), &ref_carry);
		uint32 got = ThumbExpandImm_C(imm12, FALSE, &carry);
		CHECK(got == ref && carry == (bool)ref_carry, "ThumbExpandImm_C(%03x)", imm12);
	}
	CHECK(ThumbExpandImmUnpredictable(0x100U) && ThumbExpandImmUnpredictable(0x300U) &&
	      !ThumbExpandImmUnpredictable(0x000U) && !ThumbExpandImmUnpredictable(0x400U), "Thumb UNPREDICTABLE");
	CHECK(ARMExpandImm(0x2FFU) == 0xF000000FU && ThumbExpandImm(0x1FFU) == 0x00FF00FFU, "without carry");
}

static void test_add_with_carry(void)
{
	int a;
	int b;
	int c;

	/* By the definition: carry = unsigned sum does not fit; overflow = signed sum does not fit. */
	for (a = 0; a < SAMPLES; a++) {
		for (b = 0; b < SAMPLES; b++) {
			for (c = 0; c < 2; c++) {
				uint32 x = samples[a];
				uint32 y = samples[b];
				PseudoCodeStatusFlagType f;
				uint64 us = (uint64)x + y + (uint64)c;
				sint64 ss = (sint64)(sint32)x + (sint32)y + c;
				uint32 r = AddWithCarry(x, y, (bool)c, &f);
				CHECK(r == (uint32)us, "AddWithCarry result");
				CHECK(f.carry == (us > 0xFFFFFFFFULL), "AddWithCarry(%08x, %08x, %d) carry", x, y, c);
				CHECK(f.overflow == (ss > 0x7FFFFFFFLL || ss < -0x80000000LL), "AddWithCarry(%08x, %08x, %d) overflow", x, y, c);
			}
		}
	}
	/* SUBS r, x, y is AddWithCarry(x, NOT(y), '1'). */
	{
		PseudoCodeStatusFlagType f;
		CHECK(AddWithCarry(5U, ~3U, TRUE, &f) == 2U && f.carry == TRUE && f.overflow == FALSE, "5 - 3");
		CHECK(AddWithCarry(3U, ~5U, TRUE, &f) == 0xFFFFFFFEU && f.carry == FALSE && f.overflow == FALSE, "3 - 5");
		CHECK(AddWithCarry(0x80000000U, ~1U, TRUE, &f) == 0x7FFFFFFFU && f.carry == TRUE && f.overflow == TRUE, "INT_MIN - 1");
		CHECK(AddWithCarry(0x7FFFFFFFU, 1U, FALSE, &f) == 0x80000000U && f.carry == FALSE && f.overflow == TRUE, "INT_MAX + 1");
	}
}

static void test_saturation(void)
{
	bool sat = 2;

	CHECK(SignedSatQ(127, 8U, &sat) == 127 && sat == FALSE, "SSAT 8 in");
	CHECK(SignedSatQ(128, 8U, &sat) == 127 && sat == TRUE, "SSAT 8 high");
	CHECK(SignedSatQ(-128, 8U, &sat) == -128 && sat == FALSE, "SSAT 8 low in");
	CHECK(SignedSatQ(-129, 8U, &sat) == -128 && sat == TRUE, "SSAT 8 low");
	CHECK(SignedSatQ(0x100000000LL, 32U, &sat) == 0x7FFFFFFF && sat == TRUE, "SSAT 32");
	CHECK(SignedSatQ(-1, 1U, &sat) == -1 && sat == FALSE, "SSAT 1");
	CHECK(SignedSatQ(1, 1U, &sat) == 0 && sat == TRUE, "SSAT 1 high");
	CHECK(UnsignedSatQ(255, 8U, &sat) == 255U && sat == FALSE, "USAT 8 in");
	CHECK(UnsignedSatQ(256, 8U, &sat) == 255U && sat == TRUE, "USAT 8 high");
	CHECK(UnsignedSatQ(-1, 8U, &sat) == 0U && sat == TRUE, "USAT 8 low");
	CHECK(UnsignedSatQ(5, 0U, &sat) == 0U && sat == TRUE, "USAT 0");
	CHECK(UnsignedSatQ(0x1FFFFFFFFLL, 32U, &sat) == 0xFFFFFFFFU && sat == TRUE, "USAT 32");
}

static void test_bits(void)
{
	int s;

	CHECK(SignExtend(0x80U, 8U) == 0xFFFFFF80U && SignExtend(0x7FU, 8U) == 0x7FU, "SignExtend 8");
	CHECK(SignExtend(0x1FF000U, 21U) == 0xFFFFF000U && SignExtend(0xFFFFFFFFU, 32U) == 0xFFFFFFFFU, "SignExtend 21/32");
	CHECK(SignExtend(0x3U, 1U) == 0xFFFFFFFFU && SignExtend(0x2U, 1U) == 0U, "SignExtend 1");
	CHECK(ZeroExtend(0xFFFFFFFFU, 12U) == 0xFFFU && ZeroExtend(0xFFFFFFFFU, 32U) == 0xFFFFFFFFU && ZeroExtend(7U, 0U) == 0U, "ZeroExtend");
	CHECK(Align(0x1007U, 4U) == 0x1004U && Align(0x1007U, 2U) == 0x1006U, "Align");
	for (s = 0; s < SAMPLES; s++) {
		Bits b = bits_of(samples[s]);
		uint32 ones = 0U;
		int high = -1;
		int low = 32;
		int i;
		for (i = 0; i < 32; i++) {
			if (b.bit[i] != 0) {
				ones++;
				high = i;
				if (low == 32) {
					low = i;
				}
			}
		}
		CHECK(BitCount(samples[s]) == ones, "BitCount(%08x)", samples[s]);
		CHECK(HighestSetBit(samples[s]) == high, "HighestSetBit(%08x)", samples[s]);
		CHECK(LowestSetBit(samples[s]) == (uint32)low, "LowestSetBit(%08x)", samples[s]);
		CHECK(CountLeadingZeroBits(samples[s]) == (uint32)(31 - high), "CountLeadingZeroBits(%08x)", samples[s]);
	}
	CHECK(CountLeadingZeroBits(0x00010000U) == 15U && CountLeadingZeroBits(0U) == 32U && CountLeadingZeroBits(1U) == 31U, "CLZ by hand");
}

static void test_conditions(void)
{
	/* Expected by hand for every condition and every NZCV value. */
	uint32 nzcv;
	uint32 cond;

	for (nzcv = 0U; nzcv < 16U; nzcv++) {
		uint32 apsr = (nzcv << 28) | 0x0000ABCDU;
		int n = (nzcv >> 3) & 1;
		int z = (nzcv >> 2) & 1;
		int c = (nzcv >> 1) & 1;
		int v = nzcv & 1;
		int expected[16];
		expected[0] = z;
		expected[1] = !z;
		expected[2] = c;
		expected[3] = !c;
		expected[4] = n;
		expected[5] = !n;
		expected[6] = v;
		expected[7] = !v;
		expected[8] = c && !z;
		expected[9] = !c || z;
		expected[10] = n == v;
		expected[11] = n != v;
		expected[12] = !z && n == v;
		expected[13] = z || n != v;
		expected[14] = 1;
		expected[15] = 1;
		for (cond = 0U; cond < 16U; cond++) {
			CHECK(ConditionHolds(cond, apsr) == (bool)expected[cond], "ConditionHolds(%u) NZCV=%x", cond, nzcv);
		}
		{
			PseudoCodeStatusFlagType f = ApsrFlags(apsr);
			CHECK(f.negative == (bool)n && f.zero == (bool)z && f.carry == (bool)c && f.overflow == (bool)v, "ApsrFlags");
		}
	}
	{
		PseudoCodeStatusFlagType f;
		uint32 apsr;
		f.carry = TRUE;
		f.overflow = FALSE;
		apsr = ApsrUpdateNZCV(0x1800ABCDU, 0x80000000U, &f);
		CHECK(apsr == 0xA800ABCDU && f.negative == TRUE && f.zero == FALSE, "ApsrUpdateNZCV keeps Q, GE and low bits");
		f.carry = FALSE;
		f.overflow = TRUE;
		apsr = ApsrUpdateNZCV(0xF0000000U, 0U, &f);
		CHECK(apsr == 0x50000000U && f.zero == TRUE, "ApsrUpdateNZCV zero");
	}
}

int main(void)
{
	test_shifts();
	test_decode_shift();
	test_expand_imm();
	test_add_with_carry();
	test_saturation();
	test_bits();
	test_conditions();
	printf("%d checks, %d failures\n", checks, failures);
	return (failures == 0) ? 0 : 1;
}
