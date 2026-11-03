/*
 * Tests of the IT state helpers, IT, and the M profile's MRS and MSR of the
 * program status registers, through the test profile. The expected values
 * are worked out here from the manual's definitions.
 */
#include <stdio.h>
#include <string.h>
#include "arm_common/arm_pseudo_system.h"

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

/*
 * An IT block's instructions run under the conditions of its mnemonic:
 * firstcond for 'T', its inverse for 'E'. mask (firstcond<0> and the
 * pattern) is from the manual's table of IT encodings.
 */
typedef struct {
	const char *name;
	uint32 firstcond;
	uint32 mask;
	const char *pattern;	/* T or E per instruction after the first */
} ItCase;

static const ItCase it_cases[] = {
	{ "IT EQ", 0x0U, 0x8U, "" },
	{ "ITT NE", 0x1U, 0xCU, "T" },	/* firstcond<0> = 1: mask<3> = 1 for T */
	{ "ITE EQ", 0x0U, 0xCU, "E" },	/* firstcond<0> = 0: mask<3> = 1 for E */
	{ "ITTE GT", 0xCU, 0x6U, "TE" },
	{ "ITETT LS", 0x9U, 0x7U, "ETT" },	/* firstcond<0> = 1: E = 0, T = 1 */
	{ "ITEET LS", 0x9U, 0x3U, "EET" },
	{ "ITEEE HI", 0x8U, 0xFU, "EEE" },
	{ "ITTTT AL", 0xEU, 0x1U, "TTT" },
};

static void test_it_advance(void)
{
	uint32 i;
	uint32 k;

	for (i = 0U; i < sizeof(it_cases) / sizeof(it_cases[0]); i++) {
		const ItCase *t = &it_cases[i];
		uint32 n = (uint32)strlen(t->pattern) + 1U;
		uint32 it = (t->firstcond << 4) | t->mask;

		for (k = 0U; k < n; k++) {
			uint32 expected = t->firstcond;

			if ((k > 0U) && (t->pattern[k - 1U] == 'E')) {
				expected ^= 1U;
			}
			CHECK(ArmInITBlock(it), "%s: instruction %u in the block", t->name, k);
			CHECK(ArmITCondition(it) == expected, "%s: instruction %u cond %x, expected %x", t->name, k, ArmITCondition(it), expected);
			CHECK(ArmLastInITBlock(it) == (k == n - 1U), "%s: instruction %u last", t->name, k);
			it = ArmITAdvance(it);
		}
		CHECK(it == 0U && !ArmInITBlock(it), "%s: block ends (%02x)", t->name, it);
	}
}

static void test_it(void)
{
	TargetCoreType core;
	arm_it_input_type in;

	test_core_init(&core, TestProfile_M, PC0);
	in.instrName = "IT";
	in.firstcond = 0xCU;
	in.mask = 0x6U;
	CHECK(arm_pseudo_it(&core, &in) == STD_E_OK && core.itstate == 0xC6U, "ITTE GT %02x", core.itstate);
}

static Std_ReturnType mrs(TargetCoreType *core, uint32 cond, uint32 d, uint32 sysm)
{
	arm_mrs_input_type in;
	arm_dp_output_type out;

	in.instrName = "MRS";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rd, d);
	in.SYSm = sysm;
	return arm_pseudo_mrs_xpsr(core, &in, &out);
}

static Std_ReturnType msr(TargetCoreType *core, uint32 cond, uint32 n, uint32 sysm, uint32 mask)
{
	arm_msr_input_type in;
	arm_dp_output_type out;

	in.instrName = "MSR";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rn, n);
	in.SYSm = sysm;
	in.mask = mask;
	return arm_pseudo_msr_xpsr(core, &in, &out);
}

#define APSR_ALL	0xF80F0000U	/* N Z C V Q and GE<3:0> set */

static void test_mrs(void)
{
	/* SYSm: APSR 0, IAPSR 1, EAPSR 2, XPSR 3, IPSR 5, EPSR 6, IEPSR 7 */
	static const struct {
		uint32 sysm;
		uint32 dsp;
		uint32 no_dsp;
	} cases[] = {
		{ 0U, APSR_ALL, 0xF8000000U },
		{ 1U, APSR_ALL | 0x0BU, 0xF8000000U | 0x0BU },
		{ 2U, APSR_ALL, 0xF8000000U },
		{ 3U, APSR_ALL | 0x0BU, 0xF8000000U | 0x0BU },
		{ 5U, 0x0BU, 0x0BU },
		{ 6U, 0U, 0U },
		{ 7U, 0x0BU, 0x0BU },
	};
	TargetCoreType core;
	uint32 i;

	for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
		test_core_init(&core, TestProfile_M, PC0);
		core.apsr = APSR_ALL;
		core.ipsr = 0x0BU;	/* SVCall */
		core.r[0] = 0xDEADBEEFU;
		CHECK(mrs(&core, ARM_COND_AL, 0U, cases[i].sysm) == STD_E_OK && core.r[0] == cases[i].dsp,
		      "MRS SYSm %u: %08x, expected %08x", cases[i].sysm, core.r[0], cases[i].dsp);
		core.no_dsp = TRUE;
		CHECK(mrs(&core, ARM_COND_AL, 0U, cases[i].sysm) == STD_E_OK && core.r[0] == cases[i].no_dsp,
		      "MRS SYSm %u without DSP: %08x, expected %08x", cases[i].sysm, core.r[0], cases[i].no_dsp);
	}
	test_core_init(&core, TestProfile_M, PC0);
	core.apsr = APSR_ALL;
	CHECK(mrs(&core, ARM_COND_AL, 15U, 0U) == STD_E_INVALID, "MRS pc");
	core.r[0] = 0x1234U;
	CHECK(mrs(&core, 0x1U, 0U, 0U) == STD_E_OK && core.r[0] == 0x1234U, "MRSNE with Z set: skipped");
}

static void test_msr(void)
{
	TargetCoreType core;
	TargetCoreType before;

	/* mask '10': N Z C V Q only; the other bits of R[n] are ignored. */
	test_core_init(&core, TestProfile_M, PC0);
	core.apsr = 0x00050000U;
	core.r[1] = 0xA80AFFFFU;
	CHECK(msr(&core, ARM_COND_AL, 1U, 0U, 0x2U) == STD_E_OK && core.apsr == 0xA8050000U, "MSR APSR_nzcvq %08x", core.apsr);
	/* mask '01': GE only. */
	core.r[1] = 0x000A0000U;
	CHECK(msr(&core, ARM_COND_AL, 1U, 0U, 0x1U) == STD_E_OK && core.apsr == 0xA80A0000U, "MSR APSR_g %08x", core.apsr);
	/* mask '11' through XPSR: both. */
	core.r[1] = 0x50030000U;
	CHECK(msr(&core, ARM_COND_AL, 1U, 3U, 0x3U) == STD_E_OK && core.apsr == 0x50030000U, "MSR XPSR_nzcvqg %08x", core.apsr);
	/* IPSR, EPSR, IEPSR (SYSm<2> = 1): nothing written. */
	before = core;
	core.r[1] = 0xFFFFFFFFU;
	before.r[1] = 0xFFFFFFFFU;
	CHECK(msr(&core, ARM_COND_AL, 1U, 5U, 0x2U) == STD_E_OK && memcmp(&core, &before, sizeof(core)) == 0, "MSR IPSR");
	CHECK(msr(&core, ARM_COND_AL, 1U, 6U, 0x2U) == STD_E_OK && memcmp(&core, &before, sizeof(core)) == 0, "MSR EPSR");
	/* GE without the DSP extension: UNPREDICTABLE. */
	core.no_dsp = TRUE;
	CHECK(msr(&core, ARM_COND_AL, 1U, 0U, 0x1U) == STD_E_INVALID && core.apsr == 0x50030000U, "MSR APSR_g without DSP");
	/* Failing condition. */
	core.no_dsp = FALSE;
	core.apsr = ARM_APSR_Z;
	CHECK(msr(&core, 0x1U, 1U, 0U, 0x3U) == STD_E_OK && core.apsr == ARM_APSR_Z, "MSRNE with Z set: skipped");
}

int main(void)
{
	test_it_advance();
	test_it();
	test_mrs();
	test_msr();
	printf("%d checks, %d failures\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
