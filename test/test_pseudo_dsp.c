/*
 * Tests of the DSP pseudo-code (parallel addition and subtraction, SEL,
 * saturation, PKH, DSP multiplies) through the test profile.
 *
 * The case tables are printed by test/model_dsp.py, an independent model
 * (Python integers), so the expected values do not come from this code.
 */
#include <stdio.h>
#include <string.h>
#include "arm_common/arm_pseudo_dsp_multiply.h"

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
#define N_CASES(table)	(sizeof(table) / sizeof((table)[0]))
#define GE(apsr)	(((apsr) >> ARM_APSR_BITPOS_GE) & 0xFU)

typedef Std_ReturnType (*DspOp)(TargetCoreType *core, const arm_dsp_input_type *in, arm_dp_output_type *out);
typedef Std_ReturnType (*SatOp)(TargetCoreType *core, const arm_sat_input_type *in, arm_dp_output_type *out);
typedef Std_ReturnType (*PkhOp)(TargetCoreType *core, const arm_dp_reg_input_type *in, arm_dp_output_type *out);
typedef Std_ReturnType (*DspLongOp)(TargetCoreType *core, const arm_dsp_long_input_type *in, arm_mul_long_output_type *out);

typedef struct {
	const char *name;
	DspOp op;
	uint32 rn;
	uint32 rm;
	uint32 ge_before;
	uint32 result;
	uint32 ge_after;
} ParallelCase;

typedef struct {
	uint32 rn;
	uint32 rm;
	uint32 ge;
	uint32 result;
} SelCase;

typedef struct {
	const char *name;
	SatOp op;
	uint32 rn;
	uint32 saturate_to;
	SRType shift_t;
	uint32 shift_n;
	uint32 result;
	bool q;
} SatCase;

typedef struct {
	const char *name;
	DspOp op;
	uint32 rn;
	uint32 rm;
	uint32 result;
	bool q;
} QCase;

typedef struct {
	const char *name;
	PkhOp op;
	uint32 rn;
	uint32 rm;
	SRType shift_t;
	uint32 shift_n;
	uint32 result;
} PkhCase;

typedef struct {
	const char *name;
	DspOp op;
	uint32 rn;
	uint32 rm;
	uint32 ra;
	bool n_high;
	bool m_high;
	bool m_swap;
	bool round;
	uint32 result;
	bool q;
} DspMulCase;

typedef struct {
	const char *name;
	DspLongOp op;
	uint32 rn;
	uint32 rm;
	uint32 lo;
	uint32 hi;
	bool n_high;
	bool m_high;
	bool m_swap;
	uint32 result_lo;
	uint32 result_hi;
} DspLongCase;

/* Printed by test/model_dsp.py. */
static const ParallelCase parallel_cases[] = {
	{ "SADD16", arm_pseudo_sadd16, 0x000000FFU, 0x9AC80625U, 0x1U, 0x9AC80724U, 0x3U },
	{ "SADD16", arm_pseudo_sadd16, 0xA21AD572U, 0x80000000U, 0x2U, 0x221AD572U, 0x0U },
	{ "SADD16", arm_pseudo_sadd16, 0xBFF48056U, 0x74C3FBF9U, 0x9U, 0x34B77C4FU, 0xCU },
	{ "SADD16", arm_pseudo_sadd16, 0x2F36CC0CU, 0x80000000U, 0x4U, 0xAF36CC0CU, 0x0U },
	{ "UADD16", arm_pseudo_uadd16, 0x565FBD13U, 0x7C410181U, 0xDU, 0xD2A0BE94U, 0x0U },
	{ "UADD16", arm_pseudo_uadd16, 0x00007FFFU, 0xD5823127U, 0x8U, 0xD582B126U, 0x0U },
	{ "UADD16", arm_pseudo_uadd16, 0x7F6AC7B1U, 0x7FFF7FFFU, 0x0U, 0xFF6947B0U, 0x3U },
	{ "UADD16", arm_pseudo_uadd16, 0xB3D72116U, 0xBEA7852DU, 0xCU, 0x727EA643U, 0xCU },
	{ "QADD16", arm_pseudo_qadd16, 0x781E32FDU, 0x7918D093U, 0x7U, 0x7FFF0390U, 0x7U },
	{ "QADD16", arm_pseudo_qadd16, 0x58D21A48U, 0x209F61D4U, 0xFU, 0x79717C1CU, 0xFU },
	{ "QADD16", arm_pseudo_qadd16, 0x0000FFFFU, 0x00000001U, 0xBU, 0x00000000U, 0xBU },
	{ "QADD16", arm_pseudo_qadd16, 0x33C5787DU, 0xD9883972U, 0x6U, 0x0D4D7FFFU, 0x6U },
	{ "UQADD16", arm_pseudo_uqadd16, 0x80808080U, 0x80008000U, 0x8U, 0xFFFFFFFFU, 0x8U },
	{ "UQADD16", arm_pseudo_uqadd16, 0x84915E98U, 0xF3F93B8DU, 0xEU, 0xFFFF9A25U, 0xEU },
	{ "UQADD16", arm_pseudo_uqadd16, 0x7FFFFFFFU, 0xA43A366BU, 0xBU, 0xFFFFFFFFU, 0xBU },
	{ "UQADD16", arm_pseudo_uqadd16, 0x00008000U, 0x7FFF7FFFU, 0x0U, 0x7FFFFFFFU, 0x0U },
	{ "SHADD16", arm_pseudo_shadd16, 0x24D99976U, 0x7FFFFFFFU, 0x7U, 0x526CCCBAU, 0x7U },
	{ "SHADD16", arm_pseudo_shadd16, 0x7FFF7FFFU, 0x0BD7CBBDU, 0xDU, 0x45EB25DEU, 0xDU },
	{ "SHADD16", arm_pseudo_shadd16, 0x241E9A47U, 0xFC17BDA8U, 0x0U, 0x101AABF7U, 0x0U },
	{ "SHADD16", arm_pseudo_shadd16, 0x00000000U, 0x7E27A420U, 0x6U, 0x3F13D210U, 0x6U },
	{ "UHADD16", arm_pseudo_uhadd16, 0x5E3E5392U, 0xBE4562BCU, 0xCU, 0x8E415B27U, 0xCU },
	{ "UHADD16", arm_pseudo_uhadd16, 0xF19025FCU, 0x983C21AFU, 0x1U, 0xC4E623D5U, 0x1U },
	{ "UHADD16", arm_pseudo_uhadd16, 0x000000FFU, 0x4383F6CEU, 0xEU, 0x21C17BE6U, 0xEU },
	{ "UHADD16", arm_pseudo_uhadd16, 0xD9EF48F8U, 0x76EE64A2U, 0xFU, 0xA86E56CDU, 0xFU },
	{ "SASX", arm_pseudo_sasx, 0x8BE21FC7U, 0x6B3F9FA8U, 0x4U, 0x2B8AB488U, 0x0U },
	{ "SASX", arm_pseudo_sasx, 0x80008000U, 0xF5DC278CU, 0xAU, 0xA78C8A24U, 0x0U },
	{ "SASX", arm_pseudo_sasx, 0x7F7F7F7FU, 0x00000001U, 0xDU, 0x7F807F7FU, 0xFU },
	{ "SASX", arm_pseudo_sasx, 0x6D5A9419U, 0xE6E5AC49U, 0x9U, 0x19A3AD34U, 0xCU },
	{ "UASX", arm_pseudo_uasx, 0x4921E210U, 0x72701F3CU, 0xCU, 0x685D6FA0U, 0x3U },
	{ "UASX", arm_pseudo_uasx, 0x8B6D5877U, 0x55309A8EU, 0x0U, 0x25FB0347U, 0xFU },
	{ "UASX", arm_pseudo_uasx, 0x86A4616BU, 0xFABC19B0U, 0xBU, 0xA05466AFU, 0x0U },
	{ "UASX", arm_pseudo_uasx, 0xC54B9C2CU, 0x000000FFU, 0x0U, 0xC64A9C2CU, 0x3U },
	{ "QASX", arm_pseudo_qasx, 0x0000FFFFU, 0x964E3F98U, 0xEU, 0x3F9869B1U, 0xEU },
	{ "QASX", arm_pseudo_qasx, 0x00000000U, 0x98088FDFU, 0x6U, 0x8FDF67F8U, 0x6U },
	{ "QASX", arm_pseudo_qasx, 0xFFFFFFFFU, 0x00000001U, 0x8U, 0x0000FFFFU, 0x8U },
	{ "QASX", arm_pseudo_qasx, 0xD01A4449U, 0xFFFFFFFFU, 0x5U, 0xD019444AU, 0x5U },
	{ "UQASX", arm_pseudo_uqasx, 0x00FF00FFU, 0x80000000U, 0x6U, 0x00FF0000U, 0x6U },
	{ "UQASX", arm_pseudo_uqasx, 0x17AE815BU, 0x225BDE06U, 0x4U, 0xF5B45F00U, 0x4U },
	{ "UQASX", arm_pseudo_uqasx, 0x9EB0B535U, 0xBD2129A2U, 0x8U, 0xC8520000U, 0x8U },
	{ "UQASX", arm_pseudo_uqasx, 0x67B8A427U, 0x9D432FCCU, 0x6U, 0x978406E4U, 0x6U },
	{ "SHASX", arm_pseudo_shasx, 0xB67B4CFFU, 0x5D1CEC80U, 0x2U, 0xD17DF7F1U, 0x2U },
	{ "SHASX", arm_pseudo_shasx, 0xD6973727U, 0x80008000U, 0x7U, 0xAB4B5B93U, 0x7U },
	{ "SHASX", arm_pseudo_shasx, 0xD099B530U, 0x80808080U, 0x2U, 0xA88C1A58U, 0x2U },
	{ "SHASX", arm_pseudo_shasx, 0x7FFFFFFFU, 0x80000000U, 0x2U, 0x3FFF3FFFU, 0x2U },
	{ "UHASX", arm_pseudo_uhasx, 0x36D0F190U, 0x00008000U, 0xFU, 0x5B6878C8U, 0xFU },
	{ "UHASX", arm_pseudo_uhasx, 0xDD42003AU, 0x95AA6622U, 0x8U, 0xA1B2B548U, 0x8U },
	{ "UHASX", arm_pseudo_uhasx, 0x80808080U, 0x2ADC8518U, 0xBU, 0x82CC2AD2U, 0xBU },
	{ "UHASX", arm_pseudo_uhasx, 0x442186DFU, 0xFFFFFFFFU, 0x9U, 0xA210C370U, 0x9U },
	{ "SSAX", arm_pseudo_ssax, 0x7F7F7F7FU, 0xAA1F1EDAU, 0x8U, 0x60A5299EU, 0xFU },
	{ "SSAX", arm_pseudo_ssax, 0x00007FFFU, 0x00000001U, 0x1U, 0xFFFF7FFFU, 0x3U },
	{ "SSAX", arm_pseudo_ssax, 0x15D89263U, 0xFCDF7813U, 0x0U, 0x9DC58F42U, 0x0U },
	{ "SSAX", arm_pseudo_ssax, 0x7057F0DCU, 0x000000FFU, 0xBU, 0x6F58F0DCU, 0xCU },
	{ "USAX", arm_pseudo_usax, 0x000000FFU, 0x98A210A4U, 0x6U, 0xEF5C99A1U, 0x0U },
	{ "USAX", arm_pseudo_usax, 0xD9EBB6DBU, 0xE1334DA5U, 0xCU, 0x8C46980EU, 0xFU },
	{ "USAX", arm_pseudo_usax, 0x80008000U, 0xEEE15F80U, 0x0U, 0x20806EE1U, 0xFU },
	{ "USAX", arm_pseudo_usax, 0x00000080U, 0x642E8B24U, 0x8U, 0x74DC64AEU, 0x0U },
	{ "QSAX", arm_pseudo_qsax, 0x000000FFU, 0x80008000U, 0x7U, 0x7FFF80FFU, 0x7U },
	{ "QSAX", arm_pseudo_qsax, 0x0000007FU, 0x7F7F7F7FU, 0x5U, 0x80817FFEU, 0x5U },
	{ "QSAX", arm_pseudo_qsax, 0xC0F66CA1U, 0x00008000U, 0xDU, 0x40F66CA1U, 0xDU },
	{ "QSAX", arm_pseudo_qsax, 0x0000007FU, 0x43DCD12CU, 0xCU, 0x2ED4445BU, 0xCU },
	{ "UQSAX", arm_pseudo_uqsax, 0x7760E88AU, 0x0000FFFFU, 0xDU, 0x0000E88AU, 0xDU },
	{ "UQSAX", arm_pseudo_uqsax, 0xDE10F9ADU, 0x516D3084U, 0xDU, 0xAD8CFFFFU, 0xDU },
	{ "UQSAX", arm_pseudo_uqsax, 0xB6C17C5FU, 0x7D345B91U, 0x3U, 0x5B30F993U, 0x3U },
	{ "UQSAX", arm_pseudo_uqsax, 0xEF544D46U, 0x00000001U, 0xCU, 0xEF534D46U, 0xCU },
	{ "SHSAX", arm_pseudo_shsax, 0x53392827U, 0x00000080U, 0x3U, 0x295C1413U, 0x3U },
	{ "SHSAX", arm_pseudo_shsax, 0xFD6DF662U, 0x00FF00FFU, 0x4U, 0xFE37FBB0U, 0x4U },
	{ "SHSAX", arm_pseudo_shsax, 0x9624357AU, 0x80008000U, 0x2U, 0x0B12DABDU, 0x2U },
	{ "SHSAX", arm_pseudo_shsax, 0x8726BB1DU, 0x00000080U, 0xFU, 0xC353DD8EU, 0xFU },
	{ "UHSAX", arm_pseudo_uhsax, 0x5F939DFAU, 0x000000FFU, 0x1U, 0x2F4A4EFDU, 0x1U },
	{ "UHSAX", arm_pseudo_uhsax, 0x00FF00FFU, 0xED5D26C0U, 0x3U, 0xED1F772EU, 0x3U },
	{ "UHSAX", arm_pseudo_uhsax, 0x86ED60ABU, 0x00000001U, 0x7U, 0x43763055U, 0x7U },
	{ "UHSAX", arm_pseudo_uhsax, 0x0000FFFFU, 0x35958EB8U, 0x2U, 0xB8A49ACAU, 0x2U },
	{ "SSUB16", arm_pseudo_ssub16, 0x00007FFFU, 0x7FFF7FFFU, 0xFU, 0x80010000U, 0x3U },
	{ "SSUB16", arm_pseudo_ssub16, 0x80808080U, 0x00FF00FFU, 0x2U, 0x7F817F81U, 0x0U },
	{ "SSUB16", arm_pseudo_ssub16, 0xF245256EU, 0x00000000U, 0x0U, 0xF245256EU, 0x3U },
	{ "SSUB16", arm_pseudo_ssub16, 0xBDA1BDA0U, 0xA41A7A7BU, 0xDU, 0x19874325U, 0xCU },
	{ "USUB16", arm_pseudo_usub16, 0x0000FFFFU, 0x2476C17EU, 0x1U, 0xDB8A3E81U, 0x3U },
	{ "USUB16", arm_pseudo_usub16, 0xBA365792U, 0x00000080U, 0x7U, 0xBA365712U, 0xFU },
	{ "USUB16", arm_pseudo_usub16, 0x6049EFC6U, 0xAF8B829FU, 0x6U, 0xB0BE6D27U, 0x3U },
	{ "USUB16", arm_pseudo_usub16, 0x00000000U, 0xB58A6859U, 0xEU, 0x4A7697A7U, 0x0U },
	{ "QSUB16", arm_pseudo_qsub16, 0x5EA0D14EU, 0xA6E01EC7U, 0xDU, 0x7FFFB287U, 0xDU },
	{ "QSUB16", arm_pseudo_qsub16, 0x0E6F5D62U, 0x00FF00FFU, 0x1U, 0x0D705C63U, 0x1U },
	{ "QSUB16", arm_pseudo_qsub16, 0xAAC9B52EU, 0x5B8B7B59U, 0xDU, 0x80008000U, 0xDU },
	{ "QSUB16", arm_pseudo_qsub16, 0x78024E4FU, 0x80008000U, 0x6U, 0x7FFF7FFFU, 0x6U },
	{ "UQSUB16", arm_pseudo_uqsub16, 0x000000FFU, 0x9DD40A0CU, 0x1U, 0x00000000U, 0x1U },
	{ "UQSUB16", arm_pseudo_uqsub16, 0x80808080U, 0x0000007FU, 0x2U, 0x80808001U, 0x2U },
	{ "UQSUB16", arm_pseudo_uqsub16, 0x7A960D65U, 0xA8589135U, 0xEU, 0x00000000U, 0xEU },
	{ "UQSUB16", arm_pseudo_uqsub16, 0x80008000U, 0x5101BC0DU, 0xFU, 0x2EFF0000U, 0xFU },
	{ "SHSUB16", arm_pseudo_shsub16, 0x00FF00FFU, 0x000000FFU, 0x4U, 0x007F0000U, 0x4U },
	{ "SHSUB16", arm_pseudo_shsub16, 0x97F3F835U, 0x2713B65AU, 0xFU, 0xB87020EDU, 0xFU },
	{ "SHSUB16", arm_pseudo_shsub16, 0x3175A8E9U, 0xDD244E41U, 0xAU, 0x2A28AD54U, 0xAU },
	{ "SHSUB16", arm_pseudo_shsub16, 0x7FFF7FFFU, 0x7FFF7FFFU, 0x7U, 0x00000000U, 0x7U },
	{ "UHSUB16", arm_pseudo_uhsub16, 0xDAB72845U, 0x7BB9F0C3U, 0x4U, 0x2F7F9BC1U, 0x4U },
	{ "UHSUB16", arm_pseudo_uhsub16, 0x2A1D17F9U, 0xFCB468D7U, 0xAU, 0x96B4D791U, 0xAU },
	{ "UHSUB16", arm_pseudo_uhsub16, 0x0000FFFFU, 0xE10FDE6CU, 0xDU, 0x8F7810C9U, 0xDU },
	{ "UHSUB16", arm_pseudo_uhsub16, 0x017DFDE2U, 0x00000001U, 0x7U, 0x00BE7EF0U, 0x7U },
	{ "SADD8", arm_pseudo_sadd8, 0x00008000U, 0xDAFA16BCU, 0xDU, 0xDAFA96BCU, 0x0U },
	{ "SADD8", arm_pseudo_sadd8, 0xDA81DD21U, 0x00000001U, 0x5U, 0xDA81DD22U, 0x1U },
	{ "SADD8", arm_pseudo_sadd8, 0x00000000U, 0x00000080U, 0x1U, 0x00000080U, 0xEU },
	{ "SADD8", arm_pseudo_sadd8, 0x51808829U, 0x47838FB4U, 0xAU, 0x980317DDU, 0x8U },
	{ "UADD8", arm_pseudo_uadd8, 0x8634864AU, 0x0000007FU, 0x7U, 0x863486C9U, 0x0U },
	{ "UADD8", arm_pseudo_uadd8, 0x61FEC7D5U, 0x20BE3E90U, 0x9U, 0x81BC0565U, 0x7U },
	{ "UADD8", arm_pseudo_uadd8, 0x00000000U, 0x80000000U, 0xEU, 0x80000000U, 0x0U },
	{ "UADD8", arm_pseudo_uadd8, 0x00000000U, 0xFFFFFFFFU, 0x9U, 0xFFFFFFFFU, 0x0U },
	{ "QADD8", arm_pseudo_qadd8, 0x00007FFFU, 0x0000FFFFU, 0x5U, 0x00007EFEU, 0x5U },
	{ "QADD8", arm_pseudo_qadd8, 0xF56849CBU, 0x80808080U, 0x6U, 0x80E8C980U, 0x6U },
	{ "QADD8", arm_pseudo_qadd8, 0x4633DBA2U, 0x00000001U, 0x3U, 0x4633DBA3U, 0x3U },
	{ "QADD8", arm_pseudo_qadd8, 0x10869515U, 0x88F3D0A9U, 0x5U, 0x988080BEU, 0x5U },
	{ "UQADD8", arm_pseudo_uqadd8, 0x5C0E55D3U, 0x812FA0D8U, 0xDU, 0xDD3DF5FFU, 0xDU },
	{ "UQADD8", arm_pseudo_uqadd8, 0x23B38E35U, 0x32AAB0EAU, 0x4U, 0x55FFFFFFU, 0x4U },
	{ "UQADD8", arm_pseudo_uqadd8, 0x9E74FAF1U, 0x7FFFFFFFU, 0xCU, 0xFFFFFFFFU, 0xCU },
	{ "UQADD8", arm_pseudo_uqadd8, 0x7B2DE803U, 0x3DFC2094U, 0x0U, 0xB8FFFF97U, 0x0U },
	{ "SHADD8", arm_pseudo_shadd8, 0xFFFFFFFFU, 0x8D6DFD5CU, 0x7U, 0xC636FE2DU, 0x7U },
	{ "SHADD8", arm_pseudo_shadd8, 0x80808080U, 0x267CC205U, 0x3U, 0xD3FEA1C2U, 0x3U },
	{ "SHADD8", arm_pseudo_shadd8, 0x00007FFFU, 0x00000080U, 0xFU, 0x00003FBFU, 0xFU },
	{ "SHADD8", arm_pseudo_shadd8, 0xFFF6B6F8U, 0x80008000U, 0xEU, 0xBFFB9BFCU, 0xEU },
	{ "UHADD8", arm_pseudo_uhadd8, 0x7FFF7FFFU, 0x0930F2F7U, 0x2U, 0x4497B8FBU, 0x2U },
	{ "UHADD8", arm_pseudo_uhadd8, 0x8D3DE1A7U, 0x7E43CC16U, 0x5U, 0x8540D65EU, 0x5U },
	{ "UHADD8", arm_pseudo_uhadd8, 0x4158A97FU, 0x00008000U, 0x8U, 0x202C943FU, 0x8U },
	{ "UHADD8", arm_pseudo_uhadd8, 0x00000000U, 0x00FF00FFU, 0xDU, 0x007F007FU, 0xDU },
	{ "SSUB8", arm_pseudo_ssub8, 0x2AF6094AU, 0x80008000U, 0x7U, 0xAAF6894AU, 0xBU },
	{ "SSUB8", arm_pseudo_ssub8, 0xC3D32310U, 0xCD4FF656U, 0xCU, 0xF6842DBAU, 0x2U },
	{ "SSUB8", arm_pseudo_ssub8, 0xFFFFFFFFU, 0x64FA8F5AU, 0xAU, 0x9B0570A5U, 0x6U },
	{ "SSUB8", arm_pseudo_ssub8, 0x00008000U, 0x00007FFFU, 0x8U, 0x00000101U, 0xDU },
	{ "USUB8", arm_pseudo_usub8, 0xAB69E52FU, 0x038850EFU, 0xEU, 0xA8E19540U, 0xAU },
	{ "USUB8", arm_pseudo_usub8, 0xC8EA3D50U, 0x25C4C821U, 0x7U, 0xA326752FU, 0xDU },
	{ "USUB8", arm_pseudo_usub8, 0x7B85C8A7U, 0x4009DF84U, 0x3U, 0x3B7CE923U, 0xDU },
	{ "USUB8", arm_pseudo_usub8, 0x0000007FU, 0x00007FFFU, 0x4U, 0x00008180U, 0xCU },
	{ "QSUB8", arm_pseudo_qsub8, 0x7795ED15U, 0x41782649U, 0x3U, 0x3680C7CCU, 0x3U },
	{ "QSUB8", arm_pseudo_qsub8, 0xDB914DF8U, 0x1C061108U, 0x3U, 0xBF8B3CF0U, 0x3U },
	{ "QSUB8", arm_pseudo_qsub8, 0x00FF00FFU, 0xFFFFFFFFU, 0x4U, 0x01000100U, 0x4U },
	{ "QSUB8", arm_pseudo_qsub8, 0x3D69FE29U, 0xAF0B9541U, 0x3U, 0x7F5E69E8U, 0x3U },
	{ "UQSUB8", arm_pseudo_uqsub8, 0xB59A74E1U, 0x80808080U, 0xEU, 0x351A0061U, 0xEU },
	{ "UQSUB8", arm_pseudo_uqsub8, 0x5F8A6BEBU, 0x0000FFFFU, 0x5U, 0x5F8A0000U, 0x5U },
	{ "UQSUB8", arm_pseudo_uqsub8, 0x533CCAA6U, 0x80000000U, 0x7U, 0x003CCAA6U, 0x7U },
	{ "UQSUB8", arm_pseudo_uqsub8, 0x3C0F73ADU, 0x0000007FU, 0xEU, 0x3C0F732EU, 0xEU },
	{ "SHSUB8", arm_pseudo_shsub8, 0x23664C11U, 0x69DD2279U, 0xCU, 0xDD4415CCU, 0xCU },
	{ "SHSUB8", arm_pseudo_shsub8, 0x00007FFFU, 0x3DED23B7U, 0xEU, 0xE1092E24U, 0xEU },
	{ "SHSUB8", arm_pseudo_shsub8, 0x2258BC52U, 0xD20AFA71U, 0xAU, 0x2827E1F0U, 0xAU },
	{ "SHSUB8", arm_pseudo_shsub8, 0x0000FFFFU, 0x7F7F7F7FU, 0x6U, 0xC0C0C0C0U, 0x6U },
	{ "UHSUB8", arm_pseudo_uhsub8, 0x0B0733D6U, 0xE93F4D81U, 0xFU, 0x91E4F32AU, 0xFU },
	{ "UHSUB8", arm_pseudo_uhsub8, 0x87E8A7DEU, 0x7FFFFFFFU, 0x1U, 0x04F4D4EFU, 0x1U },
	{ "UHSUB8", arm_pseudo_uhsub8, 0x9263C3CBU, 0x83447245U, 0x3U, 0x070F2843U, 0x3U },
	{ "UHSUB8", arm_pseudo_uhsub8, 0x00FF00FFU, 0x00008000U, 0xFU, 0x007FC07FU, 0xFU },
};
static const SelCase sel_cases[] = {
	{ 0x00007FFFU, 0xC8ABB352U, 0xBU, 0x00AB7FFFU },
	{ 0x32017ED4U, 0xA51BEF58U, 0x9U, 0x321BEFD4U },
	{ 0xA1063595U, 0x3E2A8C21U, 0xBU, 0xA12A3595U },
	{ 0x7FFFFFFFU, 0x0000007FU, 0xFU, 0x7FFFFFFFU },
	{ 0xC5E39205U, 0xE29FA371U, 0x8U, 0xC59FA371U },
	{ 0xFE5ECE21U, 0xB0F7261FU, 0xBU, 0xFEF7CE21U },
	{ 0x80000000U, 0x102E9B2DU, 0xCU, 0x80009B2DU },
	{ 0x89D1D332U, 0x3FBC2ACFU, 0x3U, 0x3FBCD332U },
};
static const SatCase sat_cases[] = {
	{ "SSAT", arm_pseudo_ssat, 0x80808080U, 23U, SRType_ASR, 30U, 0xFFFFFFFEU, FALSE },
	{ "SSAT", arm_pseudo_ssat, 0x41AD9D34U, 24U, SRType_LSL, 25U, 0x007FFFFFU, TRUE },
	{ "SSAT", arm_pseudo_ssat, 0xDEAFE9FEU, 16U, SRType_ASR, 9U, 0xFFFF8000U, TRUE },
	{ "SSAT", arm_pseudo_ssat, 0x6B174475U, 5U, SRType_LSL, 15U, 0xFFFFFFF0U, TRUE },
	{ "SSAT", arm_pseudo_ssat, 0x0000007FU, 11U, SRType_LSL, 7U, 0x000003FFU, TRUE },
	{ "SSAT", arm_pseudo_ssat, 0x80808080U, 16U, SRType_LSL, 2U, 0x00007FFFU, TRUE },
	{ "SSAT", arm_pseudo_ssat, 0x5BD66D88U, 15U, SRType_LSL, 8U, 0xFFFFC000U, TRUE },
	{ "SSAT", arm_pseudo_ssat, 0x107AA9D5U, 26U, SRType_ASR, 7U, 0x0020F553U, FALSE },
	{ "SSAT", arm_pseudo_ssat, 0x1DBF9F46U, 9U, SRType_ASR, 10U, 0x000000FFU, TRUE },
	{ "SSAT", arm_pseudo_ssat, 0xCA783E31U, 3U, SRType_LSL, 25U, 0x00000003U, TRUE },
	{ "USAT", arm_pseudo_usat, 0x075BC931U, 24U, SRType_ASR, 18U, 0x000001D6U, FALSE },
	{ "USAT", arm_pseudo_usat, 0xE0ADDA12U, 25U, SRType_ASR, 2U, 0x00000000U, TRUE },
	{ "USAT", arm_pseudo_usat, 0x6111523AU, 12U, SRType_ASR, 11U, 0x00000FFFU, TRUE },
	{ "USAT", arm_pseudo_usat, 0x4153064BU, 8U, SRType_LSL, 6U, 0x000000FFU, TRUE },
	{ "USAT", arm_pseudo_usat, 0x5F0214CCU, 3U, SRType_LSL, 18U, 0x00000007U, TRUE },
	{ "USAT", arm_pseudo_usat, 0xBE295218U, 10U, SRType_LSL, 1U, 0x000003FFU, TRUE },
	{ "USAT", arm_pseudo_usat, 0x121E801EU, 30U, SRType_LSL, 25U, 0x3C000000U, FALSE },
	{ "USAT", arm_pseudo_usat, 0x85C829B3U, 17U, SRType_ASR, 24U, 0x00000000U, TRUE },
	{ "USAT", arm_pseudo_usat, 0x2B5BCC04U, 13U, SRType_ASR, 12U, 0x00001FFFU, TRUE },
	{ "USAT", arm_pseudo_usat, 0x08DFF628U, 8U, SRType_LSL, 23U, 0x000000FFU, TRUE },
	{ "SSAT16", arm_pseudo_ssat16, 0xA03FB853U, 10U, SRType_LSL, 0U, 0xFE00FE00U, TRUE },
	{ "SSAT16", arm_pseudo_ssat16, 0x7551FDF0U, 9U, SRType_LSL, 0U, 0x00FFFF00U, TRUE },
	{ "SSAT16", arm_pseudo_ssat16, 0x3380FC3CU, 5U, SRType_LSL, 0U, 0x000FFFF0U, TRUE },
	{ "SSAT16", arm_pseudo_ssat16, 0x6078B1D9U, 8U, SRType_LSL, 0U, 0x007FFF80U, TRUE },
	{ "SSAT16", arm_pseudo_ssat16, 0xBD40A49FU, 4U, SRType_LSL, 0U, 0xFFF8FFF8U, TRUE },
	{ "SSAT16", arm_pseudo_ssat16, 0x9C1D3366U, 10U, SRType_LSL, 0U, 0xFE0001FFU, TRUE },
	{ "SSAT16", arm_pseudo_ssat16, 0x00008000U, 12U, SRType_LSL, 0U, 0x0000F800U, TRUE },
	{ "SSAT16", arm_pseudo_ssat16, 0xEF5AA7FAU, 10U, SRType_LSL, 0U, 0xFE00FE00U, TRUE },
	{ "SSAT16", arm_pseudo_ssat16, 0x0000007FU, 13U, SRType_LSL, 0U, 0x0000007FU, FALSE },
	{ "SSAT16", arm_pseudo_ssat16, 0x2F0A8EFDU, 7U, SRType_LSL, 0U, 0x003FFFC0U, TRUE },
	{ "USAT16", arm_pseudo_usat16, 0xDC5F7D5DU, 10U, SRType_LSL, 0U, 0x000003FFU, TRUE },
	{ "USAT16", arm_pseudo_usat16, 0x7D2EC82EU, 7U, SRType_LSL, 0U, 0x007F0000U, TRUE },
	{ "USAT16", arm_pseudo_usat16, 0x732AA896U, 12U, SRType_LSL, 0U, 0x0FFF0000U, TRUE },
	{ "USAT16", arm_pseudo_usat16, 0x6FACC238U, 7U, SRType_LSL, 0U, 0x007F0000U, TRUE },
	{ "USAT16", arm_pseudo_usat16, 0xF22BEAE5U, 14U, SRType_LSL, 0U, 0x00000000U, TRUE },
	{ "USAT16", arm_pseudo_usat16, 0x67F5966EU, 1U, SRType_LSL, 0U, 0x00010000U, TRUE },
	{ "USAT16", arm_pseudo_usat16, 0xB2FD47D3U, 12U, SRType_LSL, 0U, 0x00000FFFU, TRUE },
	{ "USAT16", arm_pseudo_usat16, 0x80808080U, 0U, SRType_LSL, 0U, 0x00000000U, TRUE },
	{ "USAT16", arm_pseudo_usat16, 0x0000FFFFU, 12U, SRType_LSL, 0U, 0x00000000U, TRUE },
	{ "USAT16", arm_pseudo_usat16, 0x6EDDF719U, 9U, SRType_LSL, 0U, 0x01FF0000U, TRUE },
};
static const QCase q_cases[] = {
	{ "QADD", arm_pseudo_qadd, 0x00008000U, 0xB29AA306U, 0xB29B2306U, FALSE },
	{ "QADD", arm_pseudo_qadd, 0x14A78792U, 0x133E8786U, 0x27E60F18U, FALSE },
	{ "QADD", arm_pseudo_qadd, 0x80008000U, 0x38DFBE8EU, 0xB8E03E8EU, FALSE },
	{ "QADD", arm_pseudo_qadd, 0x5711057BU, 0x4E5A8CA2U, 0x7FFFFFFFU, TRUE },
	{ "QADD", arm_pseudo_qadd, 0x00000001U, 0x2B2E72BEU, 0x2B2E72BFU, FALSE },
	{ "QADD", arm_pseudo_qadd, 0x80008000U, 0x6E7A10CFU, 0xEE7A90CFU, FALSE },
	{ "QADD", arm_pseudo_qadd, 0xDAB2D4C3U, 0x5346A153U, 0x2DF97616U, FALSE },
	{ "QADD", arm_pseudo_qadd, 0x000000FFU, 0x07016480U, 0x0701657FU, FALSE },
	{ "QSUB", arm_pseudo_qsub, 0x84C085BFU, 0x00000080U, 0x7B3F7AC1U, FALSE },
	{ "QSUB", arm_pseudo_qsub, 0x08EDD642U, 0x00FF00FFU, 0xF8112ABDU, FALSE },
	{ "QSUB", arm_pseudo_qsub, 0x2B4430A6U, 0x80008000U, 0x80000000U, TRUE },
	{ "QSUB", arm_pseudo_qsub, 0x0000FFFFU, 0xE42E22BEU, 0xE42D22BFU, FALSE },
	{ "QSUB", arm_pseudo_qsub, 0x7FFFFFFFU, 0x1742E7DFU, 0x9742E7E0U, FALSE },
	{ "QSUB", arm_pseudo_qsub, 0x7C2D0AFDU, 0x51009B74U, 0xD4D39077U, FALSE },
	{ "QSUB", arm_pseudo_qsub, 0x056496D4U, 0x7F7F7F7FU, 0x7A1AE8ABU, FALSE },
	{ "QSUB", arm_pseudo_qsub, 0x0000007FU, 0x00008000U, 0x00007F81U, FALSE },
	{ "QDADD", arm_pseudo_qdadd, 0x70A92707U, 0x4FF06FBCU, 0x7FFFFFFFU, TRUE },
	{ "QDADD", arm_pseudo_qdadd, 0xD5DFB862U, 0x3227576BU, 0xDDE6C82FU, FALSE },
	{ "QDADD", arm_pseudo_qdadd, 0x4733013FU, 0xE10BA0DAU, 0x610BA0D9U, TRUE },
	{ "QDADD", arm_pseudo_qdadd, 0x7185BDB8U, 0x77FA1077U, 0x7FFFFFFFU, TRUE },
	{ "QDADD", arm_pseudo_qdadd, 0x3E4C0BBFU, 0x80008000U, 0xFC98977EU, FALSE },
	{ "QDADD", arm_pseudo_qdadd, 0x000000FFU, 0x55380B6AU, 0x55380D68U, FALSE },
	{ "QDADD", arm_pseudo_qdadd, 0x000000FFU, 0x0000007FU, 0x0000027DU, FALSE },
	{ "QDADD", arm_pseudo_qdadd, 0x4EC76C95U, 0x80808080U, 0x0080807FU, TRUE },
	{ "QDSUB", arm_pseudo_qdsub, 0x80000000U, 0x411150D4U, 0x7FFFFFFFU, TRUE },
	{ "QDSUB", arm_pseudo_qdsub, 0x00000080U, 0x7F7F7F7FU, 0x7F7F7E7FU, FALSE },
	{ "QDSUB", arm_pseudo_qdsub, 0xC2CA8D79U, 0xF5A32DE2U, 0x700E12F0U, FALSE },
	{ "QDSUB", arm_pseudo_qdsub, 0x000000FFU, 0x1FC450BAU, 0x1FC44EBCU, FALSE },
	{ "QDSUB", arm_pseudo_qdsub, 0x00FF00FFU, 0x0BCCA13FU, 0x09CE9F41U, FALSE },
	{ "QDSUB", arm_pseudo_qdsub, 0xE1A0DC06U, 0x47D1CDD0U, 0x7FFFFFFFU, TRUE },
	{ "QDSUB", arm_pseudo_qdsub, 0x80808080U, 0x00000001U, 0x7FFFFFFFU, TRUE },
	{ "QDSUB", arm_pseudo_qdsub, 0x00FF00FFU, 0x00000000U, 0xFE01FE02U, FALSE },
};
static const PkhCase pkh_cases[] = {
	{ "PKHBT", arm_pseudo_pkhbt, 0x7FFF7FFFU, 0x0000007FU, SRType_LSL, 20U, 0x07F07FFFU },
	{ "PKHBT", arm_pseudo_pkhbt, 0x86093F71U, 0x12D4B005U, SRType_LSL, 29U, 0xA0003F71U },
	{ "PKHBT", arm_pseudo_pkhbt, 0xD23D7534U, 0x7F7F7F7FU, SRType_LSL, 20U, 0xF7F07534U },
	{ "PKHBT", arm_pseudo_pkhbt, 0xBD8509F1U, 0x9DF02DABU, SRType_LSL, 26U, 0xAC0009F1U },
	{ "PKHBT", arm_pseudo_pkhbt, 0xFE0F64A1U, 0xD7FA010BU, SRType_LSL, 6U, 0xFE8064A1U },
	{ "PKHBT", arm_pseudo_pkhbt, 0x000000FFU, 0x48D0CA07U, SRType_LSL, 29U, 0xE00000FFU },
	{ "PKHTB", arm_pseudo_pkhtb, 0xB152F54DU, 0xFFFFFFFFU, SRType_ASR, 13U, 0xB152FFFFU },
	{ "PKHTB", arm_pseudo_pkhtb, 0x00008000U, 0x80000000U, SRType_ASR, 9U, 0x00000000U },
	{ "PKHTB", arm_pseudo_pkhtb, 0x15835BBCU, 0x9BC988ACU, SRType_ASR, 17U, 0x1583CDE4U },
	{ "PKHTB", arm_pseudo_pkhtb, 0xCED6818DU, 0x80008000U, SRType_ASR, 13U, 0xCED60004U },
	{ "PKHTB", arm_pseudo_pkhtb, 0x80000000U, 0x0000FFFFU, SRType_ASR, 27U, 0x80000000U },
	{ "PKHTB", arm_pseudo_pkhtb, 0xD5A0C23CU, 0xF69CA5C0U, SRType_ASR, 26U, 0xD5A0FFFDU },
};
static const DspMulCase dsp_mul_cases[] = {
	{ "SMULXY", arm_pseudo_smulxy, 0xEDF5048FU, 0x47CCF2CCU, 0x00008000U, FALSE, FALSE, FALSE, TRUE, 0xFFC3CFF4U, FALSE },
	{ "SMULXY", arm_pseudo_smulxy, 0x00000000U, 0x4454DA42U, 0x91ED2664U, TRUE, FALSE, TRUE, FALSE, 0x00000000U, FALSE },
	{ "SMULXY", arm_pseudo_smulxy, 0x00000000U, 0xF5D4A9DBU, 0x9090791FU, TRUE, FALSE, TRUE, TRUE, 0x00000000U, FALSE },
	{ "SMULXY", arm_pseudo_smulxy, 0x17D5A934U, 0x484CE082U, 0x9F2A6547U, TRUE, FALSE, FALSE, FALSE, 0xFD117A2AU, FALSE },
	{ "SMULXY", arm_pseudo_smulxy, 0x80008000U, 0x00000080U, 0x251B1322U, TRUE, FALSE, FALSE, FALSE, 0xFFC00000U, FALSE },
	{ "SMULXY", arm_pseudo_smulxy, 0x80008000U, 0xBC195FCBU, 0x7F7F7F7FU, TRUE, FALSE, FALSE, FALSE, 0xD01A8000U, FALSE },
	{ "SMULXY", arm_pseudo_smulxy, 0x0000007FU, 0x73C3B7C0U, 0x32FDF8A8U, TRUE, FALSE, TRUE, FALSE, 0x00000000U, FALSE },
	{ "SMULXY", arm_pseudo_smulxy, 0x9AF6C236U, 0xE89556B4U, 0x00008000U, FALSE, FALSE, TRUE, FALSE, 0xEB12B1F8U, FALSE },
	{ "SMLAXY", arm_pseudo_smlaxy, 0x0000007FU, 0x7FFF7FFFU, 0x1A7A2935U, TRUE, TRUE, TRUE, FALSE, 0x1A7A2935U, FALSE },
	{ "SMLAXY", arm_pseudo_smlaxy, 0x00000001U, 0x7677D3A5U, 0x00000080U, FALSE, FALSE, FALSE, FALSE, 0xFFFFD425U, FALSE },
	{ "SMLAXY", arm_pseudo_smlaxy, 0x0000FFFFU, 0x00007FFFU, 0x3A9E7F4EU, FALSE, FALSE, TRUE, FALSE, 0x3A9DFF4FU, FALSE },
	{ "SMLAXY", arm_pseudo_smlaxy, 0x00008000U, 0x1805624CU, 0x7FFFFFFFU, TRUE, FALSE, TRUE, TRUE, 0x7FFFFFFFU, FALSE },
	{ "SMLAXY", arm_pseudo_smlaxy, 0xFFFFFFFFU, 0x72A47272U, 0x72E43D17U, FALSE, FALSE, FALSE, FALSE, 0x72E3CAA5U, FALSE },
	{ "SMLAXY", arm_pseudo_smlaxy, 0x2660EC75U, 0x28E82AABU, 0x5FA8E792U, FALSE, FALSE, TRUE, TRUE, 0x5C670BB9U, FALSE },
	{ "SMLAXY", arm_pseudo_smlaxy, 0x80000000U, 0x3AFFCDAFU, 0x540CC4B1U, FALSE, TRUE, TRUE, FALSE, 0x540CC4B1U, FALSE },
	{ "SMLAXY", arm_pseudo_smlaxy, 0x78DD3349U, 0x744D18F3U, 0x9FB17FB1U, TRUE, TRUE, FALSE, FALSE, 0xD699FE2AU, FALSE },
	{ "SMULWY", arm_pseudo_smulwy, 0x0000FFFFU, 0x2205C238U, 0x6383FF72U, FALSE, TRUE, FALSE, FALSE, 0x00002204U, FALSE },
	{ "SMULWY", arm_pseudo_smulwy, 0xE6CE855DU, 0x4CBD480FU, 0x71AA6F55U, TRUE, TRUE, TRUE, TRUE, 0xF872B610U, FALSE },
	{ "SMULWY", arm_pseudo_smulwy, 0x720A98EEU, 0xE9E849FCU, 0x075C3A53U, TRUE, TRUE, TRUE, FALSE, 0xF62865DDU, FALSE },
	{ "SMULWY", arm_pseudo_smulwy, 0x00000000U, 0x499C29A7U, 0x00FF00FFU, FALSE, TRUE, TRUE, FALSE, 0x00000000U, FALSE },
	{ "SMULWY", arm_pseudo_smulwy, 0x00007FFFU, 0xF1D1E27EU, 0xB7F956A8U, TRUE, TRUE, TRUE, TRUE, 0xFFFFF8E8U, FALSE },
	{ "SMULWY", arm_pseudo_smulwy, 0x7FFF7FFFU, 0x71C24BF4U, 0x08FA5E94U, TRUE, TRUE, FALSE, TRUE, 0x38E0C71EU, FALSE },
	{ "SMULWY", arm_pseudo_smulwy, 0x214E5F9AU, 0x1C6115C2U, 0x84108C43U, FALSE, TRUE, FALSE, FALSE, 0x03B13127U, FALSE },
	{ "SMULWY", arm_pseudo_smulwy, 0x00000001U, 0x80008000U, 0x0890CDD8U, TRUE, FALSE, FALSE, FALSE, 0xFFFFFFFFU, FALSE },
	{ "SMLAWY", arm_pseudo_smlawy, 0x6BBDEF5EU, 0x80000000U, 0x104DF8B3U, FALSE, TRUE, TRUE, TRUE, 0xDA6F0104U, FALSE },
	{ "SMLAWY", arm_pseudo_smlawy, 0x1B7D6F2DU, 0x00000000U, 0x00000000U, TRUE, TRUE, FALSE, FALSE, 0x00000000U, FALSE },
	{ "SMLAWY", arm_pseudo_smlawy, 0xC8AF4471U, 0xDA5969C7U, 0x80000000U, FALSE, TRUE, FALSE, TRUE, 0x8822C0C6U, FALSE },
	{ "SMLAWY", arm_pseudo_smlawy, 0x000000FFU, 0x000000FFU, 0x80008000U, FALSE, FALSE, FALSE, FALSE, 0x80008000U, FALSE },
	{ "SMLAWY", arm_pseudo_smlawy, 0x000000FFU, 0x80808080U, 0xF207944AU, FALSE, FALSE, TRUE, TRUE, 0xF20793CAU, FALSE },
	{ "SMLAWY", arm_pseudo_smlawy, 0x235D0413U, 0x778E7E08U, 0x000000FFU, TRUE, TRUE, TRUE, FALSE, 0x1083DB7CU, FALSE },
	{ "SMLAWY", arm_pseudo_smlawy, 0x80000000U, 0x61DA6739U, 0x000000FFU, TRUE, FALSE, FALSE, FALSE, 0xCC6380FFU, FALSE },
	{ "SMLAWY", arm_pseudo_smlawy, 0x80008000U, 0xF9ED88C7U, 0x990A53EAU, FALSE, TRUE, FALSE, FALSE, 0x9C13D0E0U, FALSE },
	{ "SMUAD", arm_pseudo_smuad, 0xA553604DU, 0xEB53BF84U, 0x00000001U, FALSE, TRUE, TRUE, FALSE, 0x0F1013C3U, FALSE },
	{ "SMUAD", arm_pseudo_smuad, 0xCE66473AU, 0x010CFA5AU, 0x2B5AFDA9U, TRUE, TRUE, TRUE, TRUE, 0x0162BC94U, FALSE },
	{ "SMUAD", arm_pseudo_smuad, 0xBB20064FU, 0xA1D7DF52U, 0x70EAFE1BU, TRUE, TRUE, FALSE, TRUE, 0x18871E2EU, FALSE },
	{ "SMUAD", arm_pseudo_smuad, 0xABDEABDDU, 0x0000FFFFU, 0x80808080U, FALSE, TRUE, FALSE, TRUE, 0x00005423U, FALSE },
	{ "SMUAD", arm_pseudo_smuad, 0x07B5E9FFU, 0xA619C605U, 0x7F7F7F7FU, TRUE, TRUE, FALSE, FALSE, 0x0246EAA8U, FALSE },
	{ "SMUAD", arm_pseudo_smuad, 0xE4CEF0ABU, 0x00000001U, 0x28F43EC8U, FALSE, TRUE, TRUE, FALSE, 0xFFFFE4CEU, FALSE },
	{ "SMUAD", arm_pseudo_smuad, 0x7822EB71U, 0x26158FC2U, 0x9DAE9224U, FALSE, FALSE, TRUE, TRUE, 0xC8451E09U, FALSE },
	{ "SMUAD", arm_pseudo_smuad, 0xFFFFFFFFU, 0x7FFF7FFFU, 0x00000080U, TRUE, FALSE, TRUE, FALSE, 0xFFFF0002U, FALSE },
	{ "SMLAD", arm_pseudo_smlad, 0xA5DA1B91U, 0x2CA65424U, 0x5483D8F5U, TRUE, FALSE, TRUE, FALSE, 0x3BB17FA3U, FALSE },
	{ "SMLAD", arm_pseudo_smlad, 0xB8F514B2U, 0xB8B7E332U, 0xDEF86A1BU, TRUE, FALSE, TRUE, FALSE, 0xE1338433U, FALSE },
	{ "SMLAD", arm_pseudo_smlad, 0xB05649F9U, 0x00007FFFU, 0xFC84ECD1U, FALSE, TRUE, FALSE, TRUE, 0x218122D8U, FALSE },
	{ "SMLAD", arm_pseudo_smlad, 0x00008000U, 0x9C3155DBU, 0x00007FFFU, FALSE, FALSE, TRUE, FALSE, 0x31E7FFFFU, FALSE },
	{ "SMLAD", arm_pseudo_smlad, 0x32DB4323U, 0x00008000U, 0x97C10030U, FALSE, TRUE, TRUE, FALSE, 0x7E538030U, TRUE },
	{ "SMLAD", arm_pseudo_smlad, 0x8B2C5647U, 0x00007FFFU, 0xF881C431U, TRUE, FALSE, TRUE, FALSE, 0xBE183905U, FALSE },
	{ "SMLAD", arm_pseudo_smlad, 0xD17DAD1DU, 0xD87B433EU, 0x00000000U, TRUE, FALSE, FALSE, TRUE, 0xF168A315U, FALSE },
	{ "SMLAD", arm_pseudo_smlad, 0x7F7F7F7FU, 0x00000000U, 0x8925EC7FU, TRUE, FALSE, FALSE, FALSE, 0x8925EC7FU, FALSE },
	{ "SMUSD", arm_pseudo_smusd, 0x0000FFFFU, 0x00FF00FFU, 0xFC7D88C5U, FALSE, TRUE, FALSE, TRUE, 0xFFFFFF01U, FALSE },
	{ "SMUSD", arm_pseudo_smusd, 0x00FF00FFU, 0x3E3AC012U, 0x8B17305BU, TRUE, TRUE, TRUE, FALSE, 0x007DA9D8U, FALSE },
	{ "SMUSD", arm_pseudo_smusd, 0x3AD8B24AU, 0x9578BB76U, 0x2D1D7F1EU, FALSE, TRUE, FALSE, FALSE, 0x2D4AEEDCU, FALSE },
	{ "SMUSD", arm_pseudo_smusd, 0x893FFB94U, 0xB9F984D6U, 0xEFC79D09U, FALSE, TRUE, TRUE, FALSE, 0xC813704AU, FALSE },
	{ "SMUSD", arm_pseudo_smusd, 0x000000FFU, 0x7FFF7FFFU, 0xBAF038D1U, TRUE, FALSE, TRUE, TRUE, 0x007F7F01U, FALSE },
	{ "SMUSD", arm_pseudo_smusd, 0xFA28B1D4U, 0x00FF00FFU, 0x0000007FU, TRUE, TRUE, TRUE, FALSE, 0xFFB7F454U, FALSE },
	{ "SMUSD", arm_pseudo_smusd, 0x7F7F7F7FU, 0x57635296U, 0x00FF00FFU, FALSE, TRUE, TRUE, FALSE, 0x026414B3U, FALSE },
	{ "SMUSD", arm_pseudo_smusd, 0x00000001U, 0x0255A273U, 0x0000007FU, TRUE, TRUE, FALSE, FALSE, 0xFFFFA273U, FALSE },
	{ "SMLSD", arm_pseudo_smlsd, 0xFFBFE28FU, 0x24339EDDU, 0x00007FFFU, FALSE, FALSE, FALSE, TRUE, 0x0B358865U, FALSE },
	{ "SMLSD", arm_pseudo_smlsd, 0xE9176078U, 0x00000000U, 0x7693FB5EU, FALSE, TRUE, TRUE, FALSE, 0x7693FB5EU, FALSE },
	{ "SMLSD", arm_pseudo_smlsd, 0x6AE1101FU, 0xB17B1AE8U, 0xEED9765DU, TRUE, TRUE, FALSE, TRUE, 0x11534D5AU, FALSE },
	{ "SMLSD", arm_pseudo_smlsd, 0x80008000U, 0xB92946C5U, 0xFF0C08F7U, FALSE, FALSE, TRUE, TRUE, 0x45DA08F7U, FALSE },
	{ "SMLSD", arm_pseudo_smlsd, 0x5EFB1AFDU, 0x63E3FAA2U, 0x0215566BU, TRUE, TRUE, TRUE, FALSE, 0x0E9AE2ECU, FALSE },
	{ "SMLSD", arm_pseudo_smlsd, 0x5A3A9356U, 0x5836C907U, 0xDC1A49C8U, FALSE, TRUE, TRUE, TRUE, 0xCA08EC56U, FALSE },
	{ "SMLSD", arm_pseudo_smlsd, 0x394DFA82U, 0xF16D4FD1U, 0xCDEC7C4EU, TRUE, TRUE, FALSE, TRUE, 0xCF793BA7U, FALSE },
	{ "SMLSD", arm_pseudo_smlsd, 0x197D5C8BU, 0x00008000U, 0x0E7C0621U, TRUE, FALSE, FALSE, FALSE, 0xE0368621U, FALSE },
	{ "SMMUL", arm_pseudo_smmul, 0x85E32B33U, 0x75CEDEF9U, 0x5E3E6FEDU, TRUE, TRUE, FALSE, TRUE, 0xC7CE252FU, FALSE },
	{ "SMMUL", arm_pseudo_smmul, 0xF95E8E27U, 0x00000080U, 0x000000FFU, TRUE, TRUE, FALSE, TRUE, 0xFFFFFFFDU, FALSE },
	{ "SMMUL", arm_pseudo_smmul, 0x00007FFFU, 0x4B1E75F6U, 0x00FF00FFU, TRUE, FALSE, FALSE, FALSE, 0x0000258EU, FALSE },
	{ "SMMUL", arm_pseudo_smmul, 0x065B4461U, 0x0000007FU, 0xE52AE199U, FALSE, TRUE, FALSE, TRUE, 0x00000003U, FALSE },
	{ "SMMUL", arm_pseudo_smmul, 0x89D1397FU, 0x7FFFFFFFU, 0xFE8BC9A1U, FALSE, FALSE, TRUE, TRUE, 0xC4E89CC0U, FALSE },
	{ "SMMUL", arm_pseudo_smmul, 0xA5E1A850U, 0xE16C71B0U, 0xE88E09B1U, TRUE, TRUE, FALSE, TRUE, 0x0AC37FCCU, FALSE },
	{ "SMMUL", arm_pseudo_smmul, 0x000000FFU, 0xD1E76E36U, 0x4F2B0D1CU, TRUE, FALSE, TRUE, TRUE, 0xFFFFFFD2U, FALSE },
	{ "SMMUL", arm_pseudo_smmul, 0x00FF00FFU, 0x7F7F7F7FU, 0x2C6D0925U, TRUE, FALSE, TRUE, FALSE, 0x007F007EU, FALSE },
	{ "SMMLA", arm_pseudo_smmla, 0xB0ECFA9BU, 0x0734C6C6U, 0x00000000U, TRUE, FALSE, FALSE, FALSE, 0xFDC62D97U, FALSE },
	{ "SMMLA", arm_pseudo_smmla, 0x00000001U, 0x0E7312E2U, 0x807CDBFEU, FALSE, TRUE, FALSE, TRUE, 0x807CDBFEU, FALSE },
	{ "SMMLA", arm_pseudo_smmla, 0x125C68B3U, 0xFFFFFFFFU, 0xB43D4CF8U, TRUE, FALSE, FALSE, TRUE, 0xB43D4CF8U, FALSE },
	{ "SMMLA", arm_pseudo_smmla, 0x80008000U, 0x00008000U, 0x27BF0C6DU, FALSE, FALSE, FALSE, FALSE, 0x27BECC6DU, FALSE },
	{ "SMMLA", arm_pseudo_smmla, 0x43E252E8U, 0x00000000U, 0xA6523654U, TRUE, FALSE, FALSE, TRUE, 0xA6523654U, FALSE },
	{ "SMMLA", arm_pseudo_smmla, 0x26C9255DU, 0x0000007FU, 0x2AFDA469U, FALSE, FALSE, TRUE, FALSE, 0x2AFDA47CU, FALSE },
	{ "SMMLA", arm_pseudo_smmla, 0xF5447F66U, 0x2D16BD62U, 0xFF306687U, FALSE, TRUE, TRUE, TRUE, 0xFD4C7CDEU, FALSE },
	{ "SMMLA", arm_pseudo_smmla, 0xFE8EE225U, 0x00007FFFU, 0x00000001U, FALSE, FALSE, TRUE, FALSE, 0xFFFFFF48U, FALSE },
	{ "SMMLS", arm_pseudo_smmls, 0x80808080U, 0x51E14D94U, 0x985D11D2U, FALSE, FALSE, FALSE, TRUE, 0xC1249EDCU, FALSE },
	{ "SMMLS", arm_pseudo_smmls, 0xB0C8BAADU, 0x6CDC3429U, 0x01987AFEU, FALSE, FALSE, FALSE, FALSE, 0x2347EFE0U, FALSE },
	{ "SMMLS", arm_pseudo_smmls, 0x00007FFFU, 0x7FFF7FFFU, 0x73BAC615U, FALSE, TRUE, TRUE, FALSE, 0x73BA8615U, FALSE },
	{ "SMMLS", arm_pseudo_smmls, 0x80008000U, 0x00008000U, 0x05AB7724U, TRUE, FALSE, FALSE, TRUE, 0x05ABB724U, FALSE },
	{ "SMMLS", arm_pseudo_smmls, 0x4B893522U, 0xFFFFFFFFU, 0xAC6DBFEFU, FALSE, TRUE, FALSE, FALSE, 0xAC6DBFEFU, FALSE },
	{ "SMMLS", arm_pseudo_smmls, 0x7A5D5B0EU, 0x73CC2ED1U, 0x7FFF7FFFU, FALSE, FALSE, TRUE, FALSE, 0x48A5F752U, FALSE },
	{ "SMMLS", arm_pseudo_smmls, 0x00000080U, 0xA5540FBDU, 0x80000000U, FALSE, FALSE, TRUE, TRUE, 0x8000002DU, FALSE },
	{ "SMMLS", arm_pseudo_smmls, 0x00FF00FFU, 0x0CA40F4EU, 0xFFFFFFFFU, FALSE, FALSE, FALSE, FALSE, 0xFFF36887U, FALSE },
	{ "USAD8", arm_pseudo_usad8, 0x80000000U, 0x562C4499U, 0xBF6F3929U, TRUE, FALSE, TRUE, TRUE, 0x00000133U, FALSE },
	{ "USAD8", arm_pseudo_usad8, 0xD572A0F2U, 0x7FFF7FFFU, 0x66120192U, TRUE, FALSE, FALSE, TRUE, 0x00000111U, FALSE },
	{ "USAD8", arm_pseudo_usad8, 0xDBDC1EF8U, 0x7F7F7F7FU, 0xDAE1F665U, FALSE, TRUE, TRUE, TRUE, 0x00000193U, FALSE },
	{ "USAD8", arm_pseudo_usad8, 0x72B0F8A1U, 0xD83B2217U, 0x0000FFFFU, TRUE, TRUE, TRUE, FALSE, 0x0000023BU, FALSE },
	{ "USAD8", arm_pseudo_usad8, 0x80000000U, 0x00008000U, 0x7F7F7F7FU, FALSE, TRUE, FALSE, FALSE, 0x00000100U, FALSE },
	{ "USAD8", arm_pseudo_usad8, 0x134A7957U, 0xADABB5E6U, 0x10F9D686U, TRUE, TRUE, TRUE, TRUE, 0x000001C6U, FALSE },
	{ "USAD8", arm_pseudo_usad8, 0x5ECA297BU, 0xB6533882U, 0x5BE26BECU, TRUE, FALSE, TRUE, FALSE, 0x000000E5U, FALSE },
	{ "USAD8", arm_pseudo_usad8, 0x00FF00FFU, 0x00000000U, 0x00FF00FFU, FALSE, TRUE, TRUE, FALSE, 0x000001FEU, FALSE },
	{ "USADA8", arm_pseudo_usada8, 0xDBB0507FU, 0x16F4C70BU, 0x79D42DB0U, FALSE, TRUE, TRUE, TRUE, 0x79D42FA4U, FALSE },
	{ "USADA8", arm_pseudo_usada8, 0xEC6C9ACAU, 0x94F25FF1U, 0x20CAC6EFU, TRUE, FALSE, TRUE, TRUE, 0x20CAC82FU, FALSE },
	{ "USADA8", arm_pseudo_usada8, 0x80808080U, 0xFFFFFFFFU, 0x00007FFFU, FALSE, FALSE, FALSE, TRUE, 0x000081FBU, FALSE },
	{ "USADA8", arm_pseudo_usada8, 0x6E10CC99U, 0x062C8016U, 0x6E0AB5BFU, TRUE, TRUE, FALSE, TRUE, 0x6E0AB712U, FALSE },
	{ "USADA8", arm_pseudo_usada8, 0x1E23C257U, 0xFFFFFFFFU, 0xC6AD574DU, TRUE, FALSE, TRUE, FALSE, 0xC6AD59EFU, FALSE },
	{ "USADA8", arm_pseudo_usada8, 0x29E92312U, 0xDBC9D592U, 0x00007FFFU, FALSE, FALSE, FALSE, TRUE, 0x00008203U, FALSE },
	{ "USADA8", arm_pseudo_usada8, 0x00007FFFU, 0x00000080U, 0xE82E9FCCU, FALSE, TRUE, TRUE, TRUE, 0xE82EA0CAU, FALSE },
	{ "USADA8", arm_pseudo_usada8, 0x00000001U, 0xE025C373U, 0xF18F89BCU, FALSE, FALSE, FALSE, TRUE, 0xF18F8BF6U, FALSE },
};
static const DspLongCase dsp_long_cases[] = {
	{ "SMLALXY", arm_pseudo_smlalxy, 0x68A1296EU, 0xEB1E02C9U, 0x00000001U, 0x4CE28EBFU, TRUE, FALSE, TRUE, 0x0123686AU, 0x4CE28EBFU },
	{ "SMLALXY", arm_pseudo_smlalxy, 0x6527E961U, 0x5FD09343U, 0xE780016FU, 0x00000001U, TRUE, TRUE, TRUE, 0x0D5BAA1FU, 0x00000002U },
	{ "SMLALXY", arm_pseudo_smlalxy, 0x80DAB9CBU, 0x000000FFU, 0x0517B435U, 0x36D8812EU, FALSE, FALSE, FALSE, 0x04D1C56AU, 0x36D8812EU },
	{ "SMLALXY", arm_pseudo_smlalxy, 0x00007FFFU, 0x52536A37U, 0xAB2AF498U, 0x00000001U, TRUE, TRUE, FALSE, 0xAB2AF498U, 0x00000001U },
	{ "SMLALXY", arm_pseudo_smlalxy, 0xB1A79DE1U, 0x69753DDAU, 0x00FF00FFU, 0x5F39A2AEU, FALSE, TRUE, TRUE, 0xD89371D4U, 0x5F39A2ADU },
	{ "SMLALXY", arm_pseudo_smlalxy, 0x2D5624F5U, 0x00000000U, 0x25D63C67U, 0x7C2D58F7U, FALSE, TRUE, FALSE, 0x25D63C67U, 0x7C2D58F7U },
	{ "SMLALXY", arm_pseudo_smlalxy, 0x3064863CU, 0x50656712U, 0xE35A55B2U, 0xF0D98287U, TRUE, FALSE, FALSE, 0xF6D5F8BAU, 0xF0D98287U },
	{ "SMLALXY", arm_pseudo_smlalxy, 0xF51D1A78U, 0x00000000U, 0x2104745BU, 0x379DE4BCU, TRUE, TRUE, FALSE, 0x2104745BU, 0x379DE4BCU },
	{ "SMLALD", arm_pseudo_smlald, 0x562638F0U, 0x8F0C0B97U, 0x53ED309FU, 0xA7DB6C2DU, TRUE, TRUE, TRUE, 0x3EB45E49U, 0xA7DB6C2DU },
	{ "SMLALD", arm_pseudo_smlald, 0xA1A2E69FU, 0x9B5E7303U, 0x5BF3538EU, 0x33035668U, TRUE, FALSE, FALSE, 0x75A4E3E7U, 0x33035668U },
	{ "SMLALD", arm_pseudo_smlald, 0x3502BF5FU, 0x00008000U, 0x80000000U, 0xB41B51D0U, FALSE, TRUE, TRUE, 0x657F0000U, 0xB41B51D0U },
	{ "SMLALD", arm_pseudo_smlald, 0x6E07A7E3U, 0x00007FFFU, 0xD60BE914U, 0x00000001U, TRUE, FALSE, FALSE, 0xA9FDC131U, 0x00000001U },
	{ "SMLALD", arm_pseudo_smlald, 0x78754B63U, 0xFFFFFFFFU, 0x00008000U, 0xE2BA34E3U, TRUE, TRUE, TRUE, 0xFFFFBC28U, 0xE2BA34E2U },
	{ "SMLALD", arm_pseudo_smlald, 0x921B6BA9U, 0x0000007FU, 0x80000000U, 0x52EFB6E3U, TRUE, FALSE, TRUE, 0x7FC97B65U, 0x52EFB6E3U },
	{ "SMLALD", arm_pseudo_smlald, 0x80008000U, 0x00000001U, 0x5C454C10U, 0xDC88CDBAU, FALSE, TRUE, FALSE, 0x5C44CC10U, 0xDC88CDBAU },
	{ "SMLALD", arm_pseudo_smlald, 0x00FF00FFU, 0x8967B064U, 0x92F4AAAFU, 0x00000000U, FALSE, FALSE, TRUE, 0x922F3BE4U, 0x00000000U },
	{ "SMLSLD", arm_pseudo_smlsld, 0x2CC158DCU, 0x00000000U, 0xED9DC3CBU, 0x85778AB6U, TRUE, TRUE, FALSE, 0xED9DC3CBU, 0x85778AB6U },
	{ "SMLSLD", arm_pseudo_smlsld, 0xEEA1C618U, 0x08EB87D4U, 0x2473368FU, 0xBB3B9B1BU, TRUE, TRUE, TRUE, 0x1A474A43U, 0xBB3B9B1BU },
	{ "SMLSLD", arm_pseudo_smlsld, 0x8B0B21C1U, 0x67786E20U, 0x7FFFFFFFU, 0x754EC4BFU, TRUE, FALSE, FALSE, 0xBDCA8BF7U, 0x754EC4BFU },
	{ "SMLSLD", arm_pseudo_smlsld, 0x67CEDD40U, 0x999FEE64U, 0x00000080U, 0xFFFFFFFFU, FALSE, TRUE, TRUE, 0x15099AC8U, 0xFFFFFFFFU },
	{ "SMLSLD", arm_pseudo_smlsld, 0x00008000U, 0xDEBAC7BDU, 0x0556E092U, 0x00007FFFU, TRUE, TRUE, TRUE, 0x15F9E092U, 0x00007FFFU },
	{ "SMLSLD", arm_pseudo_smlsld, 0xC4883CA5U, 0x0AF5D3AAU, 0x00000000U, 0x0000007FU, TRUE, FALSE, FALSE, 0xF80ADE6AU, 0x0000007EU },
	{ "SMLSLD", arm_pseudo_smlsld, 0xB3D87349U, 0x00000001U, 0x7FFFFFFFU, 0x00008000U, FALSE, FALSE, TRUE, 0x80004C27U, 0x00008000U },
	{ "SMLSLD", arm_pseudo_smlsld, 0x0000FFFFU, 0x698B05A8U, 0x8ACC9F3DU, 0x4CEF6764U, TRUE, FALSE, TRUE, 0x8ACC35B2U, 0x4CEF6764U },
	{ "UMAAL", arm_pseudo_umaal, 0x2230AEC0U, 0xFFFFFFFFU, 0xC3AC55C9U, 0x3C15B281U, FALSE, TRUE, TRUE, 0xDD91598AU, 0x2230AEC0U },
	{ "UMAAL", arm_pseudo_umaal, 0x917343DFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0x91482416U, FALSE, TRUE, TRUE, 0xFFD4E036U, 0x917343DFU },
	{ "UMAAL", arm_pseudo_umaal, 0x6F0C4EC6U, 0xF45BFA23U, 0xF54EFA56U, 0xFFFFFFFFU, TRUE, FALSE, FALSE, 0xE14D1B67U, 0x69FFA0F7U },
	{ "UMAAL", arm_pseudo_umaal, 0xFFFFFFFFU, 0x5AE979F2U, 0x80008000U, 0xC558EC73U, FALSE, FALSE, TRUE, 0xEA6FF281U, 0x5AE979F2U },
	{ "UMAAL", arm_pseudo_umaal, 0xDEC4D74FU, 0x88DC35DFU, 0xA02DD14DU, 0xCFCA2B49U, TRUE, TRUE, TRUE, 0x3DE6E567U, 0x7718326DU },
	{ "UMAAL", arm_pseudo_umaal, 0x9C5ED06AU, 0x113F22BEU, 0x32F38A1FU, 0xFFFFFFFFU, FALSE, TRUE, TRUE, 0x2D164CCAU, 0x0A88DC66U },
	{ "UMAAL", arm_pseudo_umaal, 0x7F7F7F7FU, 0xD9776B98U, 0x61E276E1U, 0xA79F9C2DU, TRUE, FALSE, FALSE, 0xF288DB76U, 0x6C4E8CEDU },
	{ "UMAAL", arm_pseudo_umaal, 0x51B52FC1U, 0x25D940A8U, 0x134976F7U, 0xA798C9F5U, FALSE, FALSE, TRUE, 0x0552D794U, 0x0C148720U },
};

/* Rd r0, Rn r1, Rm r2, Ra r3. */
static Std_ReturnType dsp(DspOp op, TargetCoreType *core, uint32 cond, uint32 d, bool n_high, bool m_high, bool m_swap, bool round)
{
	arm_dsp_input_type in;
	arm_dp_output_type out;

	in.instrName = "DSP";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rd, d);
	ARM_SET_REG(core, &in, Rn, 1U);
	ARM_SET_REG(core, &in, Rm, 2U);
	ARM_SET_REG(core, &in, Ra, 3U);
	in.n_high = n_high;
	in.m_high = m_high;
	in.m_swap = m_swap;
	in.round = round;
	return op(core, &in, &out);
}

static Std_ReturnType sat(SatOp op, TargetCoreType *core, uint32 cond, uint32 d, uint32 saturate_to, SRType shift_t, uint32 shift_n)
{
	arm_sat_input_type in;
	arm_dp_output_type out;

	in.instrName = "SAT";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rd, d);
	ARM_SET_REG(core, &in, Rn, 1U);
	in.saturate_to = saturate_to;
	in.shift_t = shift_t;
	in.shift_n = shift_n;
	return op(core, &in, &out);
}

static Std_ReturnType pkh(PkhOp op, TargetCoreType *core, uint32 cond, uint32 d, SRType shift_t, uint32 shift_n)
{
	arm_dp_reg_input_type in;
	arm_dp_output_type out;

	in.instrName = "PKH";
	in.cond = cond;
	in.setflags = FALSE;
	ARM_SET_REG(core, &in, Rd, d);
	ARM_SET_REG(core, &in, Rn, 1U);
	ARM_SET_REG(core, &in, Rm, 2U);
	in.shift_t = shift_t;
	in.shift_n = shift_n;
	return op(core, &in, &out);
}

/* RdLo r4, RdHi r5 (unless given), Rn r1, Rm r2. */
static Std_ReturnType dsp_long(DspLongOp op, TargetCoreType *core, uint32 cond, uint32 lo, uint32 hi, bool n_high, bool m_high, bool m_swap)
{
	arm_dsp_long_input_type in;
	arm_mul_long_output_type out;

	in.instrName = "DSPL";
	in.cond = cond;
	ARM_SET_REG(core, &in, RdLo, lo);
	ARM_SET_REG(core, &in, RdHi, hi);
	ARM_SET_REG(core, &in, Rn, 1U);
	ARM_SET_REG(core, &in, Rm, 2U);
	in.n_high = n_high;
	in.m_high = m_high;
	in.m_swap = m_swap;
	return op(core, &in, &out);
}

static void init(TargetCoreType *core)
{
	test_core_init(core, TestProfile_M, PC0);
	core->r[0] = 0xDEADBEEFU;
}

static void test_tables(void)
{
	TargetCoreType core;
	Std_ReturnType err;
	uint32 i;

	for (i = 0U; i < N_CASES(parallel_cases); i++) {
		const ParallelCase *t = &parallel_cases[i];
		init(&core);
		core.r[1] = t->rn;
		core.r[2] = t->rm;
		core.apsr = ARM_APSR_N | ARM_APSR_Q | (t->ge_before << ARM_APSR_BITPOS_GE);
		err = dsp(t->op, &core, ARM_COND_AL, 0U, FALSE, FALSE, FALSE, FALSE);
		CHECK(err == STD_E_OK && core.r[0] == t->result && GE(core.apsr) == t->ge_after
		      && (core.apsr & ~ARM_APSR_GE) == (ARM_APSR_N | ARM_APSR_Q),
		      "%s #%u: %08x GE %x, expected %08x GE %x", t->name, i, core.r[0], GE(core.apsr), t->result, t->ge_after);
	}
	for (i = 0U; i < N_CASES(sel_cases); i++) {
		const SelCase *t = &sel_cases[i];
		init(&core);
		core.r[1] = t->rn;
		core.r[2] = t->rm;
		core.apsr = t->ge << ARM_APSR_BITPOS_GE;
		err = dsp(arm_pseudo_sel, &core, ARM_COND_AL, 0U, FALSE, FALSE, FALSE, FALSE);
		CHECK(err == STD_E_OK && core.r[0] == t->result, "SEL #%u: %08x, expected %08x", i, core.r[0], t->result);
	}
	for (i = 0U; i < N_CASES(sat_cases); i++) {
		const SatCase *t = &sat_cases[i];
		init(&core);
		core.r[1] = t->rn;
		err = sat(t->op, &core, ARM_COND_AL, 0U, t->saturate_to, t->shift_t, t->shift_n);
		CHECK(err == STD_E_OK && core.r[0] == t->result && (ARM_BIT(core.apsr, ARM_APSR_BITPOS_Q) != 0U) == t->q,
		      "%s #%u: %08x Q %u, expected %08x Q %u", t->name, i, core.r[0], ARM_BIT(core.apsr, ARM_APSR_BITPOS_Q), t->result, t->q);
	}
	for (i = 0U; i < N_CASES(q_cases); i++) {
		const QCase *t = &q_cases[i];
		init(&core);
		core.r[1] = t->rn;
		core.r[2] = t->rm;
		err = dsp(t->op, &core, ARM_COND_AL, 0U, FALSE, FALSE, FALSE, FALSE);
		CHECK(err == STD_E_OK && core.r[0] == t->result && (ARM_BIT(core.apsr, ARM_APSR_BITPOS_Q) != 0U) == t->q,
		      "%s #%u: %08x Q %u, expected %08x Q %u", t->name, i, core.r[0], ARM_BIT(core.apsr, ARM_APSR_BITPOS_Q), t->result, t->q);
	}
	for (i = 0U; i < N_CASES(pkh_cases); i++) {
		const PkhCase *t = &pkh_cases[i];
		init(&core);
		core.r[1] = t->rn;
		core.r[2] = t->rm;
		err = pkh(t->op, &core, ARM_COND_AL, 0U, t->shift_t, t->shift_n);
		CHECK(err == STD_E_OK && core.r[0] == t->result, "%s #%u: %08x, expected %08x", t->name, i, core.r[0], t->result);
	}
	for (i = 0U; i < N_CASES(dsp_mul_cases); i++) {
		const DspMulCase *t = &dsp_mul_cases[i];
		init(&core);
		core.r[1] = t->rn;
		core.r[2] = t->rm;
		core.r[3] = t->ra;
		if (t->op == arm_pseudo_usad8) {
			core.r[3] = 0U;	/* USAD8 has no Ra: the glue leaves it unset (0) */
		}
		err = dsp(t->op, &core, ARM_COND_AL, 0U, t->n_high, t->m_high, t->m_swap, t->round);
		CHECK(err == STD_E_OK && core.r[0] == t->result && (ARM_BIT(core.apsr, ARM_APSR_BITPOS_Q) != 0U) == t->q,
		      "%s #%u: %08x Q %u, expected %08x Q %u", t->name, i, core.r[0], ARM_BIT(core.apsr, ARM_APSR_BITPOS_Q), t->result, t->q);
	}
	for (i = 0U; i < N_CASES(dsp_long_cases); i++) {
		const DspLongCase *t = &dsp_long_cases[i];
		init(&core);
		core.r[1] = t->rn;
		core.r[2] = t->rm;
		core.r[4] = t->lo;
		core.r[5] = t->hi;
		err = dsp_long(t->op, &core, ARM_COND_AL, 4U, 5U, t->n_high, t->m_high, t->m_swap);
		CHECK(err == STD_E_OK && core.r[4] == t->result_lo && core.r[5] == t->result_hi && core.apsr == 0U,
		      "%s #%u: %08x:%08x, expected %08x:%08x", t->name, i, core.r[5], core.r[4], t->result_hi, t->result_lo);
	}
}

/* Q is sticky: an instruction that does not saturate leaves it set. */
static void test_q_sticky(void)
{
	TargetCoreType core;

	init(&core);
	core.apsr = ARM_APSR_Q;
	core.r[1] = 1U;
	core.r[2] = 2U;
	CHECK(dsp(arm_pseudo_qadd, &core, ARM_COND_AL, 0U, FALSE, FALSE, FALSE, FALSE) == STD_E_OK && core.r[0] == 3U
	      && core.apsr == ARM_APSR_Q, "QADD keeps Q");
}

/* The 16-bit saturations: only the top halfword saturating still sets Q. */
static void test_sat16_halves(void)
{
	TargetCoreType core;

	init(&core);
	core.r[1] = 0x00050003U;
	CHECK(sat(arm_pseudo_usat16, &core, ARM_COND_AL, 0U, 8U, SRType_LSL, 0U) == STD_E_OK && core.r[0] == 0x00050003U
	      && core.apsr == 0U, "USAT16 #8 in range");
	core.r[1] = 0x01000003U;	/* 256 > 255 in the top halfword */
	CHECK(sat(arm_pseudo_usat16, &core, ARM_COND_AL, 0U, 8U, SRType_LSL, 0U) == STD_E_OK && core.r[0] == 0x00FF0003U
	      && core.apsr == ARM_APSR_Q, "USAT16 #8 top saturates");
	init(&core);
	core.r[1] = 0x01000003U;	/* 256 > 127 */
	CHECK(sat(arm_pseudo_ssat16, &core, ARM_COND_AL, 0U, 8U, SRType_LSL, 0U) == STD_E_OK && core.r[0] == 0x007F0003U
	      && core.apsr == ARM_APSR_Q, "SSAT16 #8 top saturates");
}

static void test_unpredictable(void)
{
	TargetCoreType core;

	init(&core);
	CHECK(dsp(arm_pseudo_sadd16, &core, ARM_COND_AL, 15U, FALSE, FALSE, FALSE, FALSE) == STD_E_INVALID, "SADD16 pc");
	CHECK(dsp_long(arm_pseudo_umaal, &core, ARM_COND_AL, 4U, 4U, FALSE, FALSE, FALSE) == STD_E_INVALID, "UMAAL RdHi == RdLo");
	CHECK(dsp_long(arm_pseudo_smlald, &core, ARM_COND_AL, 15U, 4U, FALSE, FALSE, FALSE) == STD_E_INVALID, "SMLALD RdLo pc");
	CHECK(dsp_long(arm_pseudo_smlald, &core, ARM_COND_AL, 4U, 15U, FALSE, FALSE, FALSE) == STD_E_INVALID, "SMLALD RdHi pc");
}

/* Every instruction with a failing condition (NE with Z set) changes nothing. */
static void test_condition_failed(void)
{
	static const DspOp dsp_ops[] = {
		arm_pseudo_sadd16, arm_pseudo_uqsub8, arm_pseudo_shasx, arm_pseudo_sel, arm_pseudo_qadd, arm_pseudo_qsub,
		arm_pseudo_qdadd, arm_pseudo_qdsub, arm_pseudo_smulxy, arm_pseudo_smlaxy, arm_pseudo_smulwy, arm_pseudo_smlawy,
		arm_pseudo_smuad, arm_pseudo_smlad, arm_pseudo_smusd, arm_pseudo_smlsd, arm_pseudo_smmul, arm_pseudo_smmla,
		arm_pseudo_smmls, arm_pseudo_usad8, arm_pseudo_usada8,
	};
	static const SatOp sat_ops[] = { arm_pseudo_ssat, arm_pseudo_usat, arm_pseudo_ssat16, arm_pseudo_usat16 };
	static const DspLongOp long_ops[] = { arm_pseudo_smlalxy, arm_pseudo_smlald, arm_pseudo_smlsld, arm_pseudo_umaal };
	TargetCoreType core;
	TargetCoreType before;
	uint32 i;

	init(&core);
	core.r[1] = 0x80000000U;
	core.r[2] = 0x7FFFFFFFU;
	core.r[3] = 0x7FFFFFFFU;
	core.apsr = ARM_APSR_Z;
	before = core;
	for (i = 0U; i < N_CASES(dsp_ops); i++) {
		CHECK(dsp(dsp_ops[i], &core, 0x1U, 0U, TRUE, TRUE, TRUE, TRUE) == STD_E_OK, "skip");
	}
	for (i = 0U; i < N_CASES(sat_ops); i++) {
		CHECK(sat(sat_ops[i], &core, 0x1U, 0U, 8U, SRType_LSL, 0U) == STD_E_OK, "skip");
	}
	CHECK(pkh(arm_pseudo_pkhbt, &core, 0x1U, 0U, SRType_LSL, 4U) == STD_E_OK, "skip");
	CHECK(pkh(arm_pseudo_pkhtb, &core, 0x1U, 0U, SRType_ASR, 4U) == STD_E_OK, "skip");
	for (i = 0U; i < N_CASES(long_ops); i++) {
		CHECK(dsp_long(long_ops[i], &core, 0x1U, 4U, 5U, TRUE, TRUE, TRUE) == STD_E_OK, "skip");
	}
	CHECK(memcmp(&core, &before, sizeof(core)) == 0, "an instruction with a failing condition changed the core");
}

int main(void)
{
	test_tables();
	test_q_sticky();
	test_sat16_halves();
	test_unpredictable();
	test_condition_failed();
	printf("%d checks, %d failures\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
