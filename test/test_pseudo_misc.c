/*
 * Tests of the extend, bitfield, bit-operation, multiply and divide
 * pseudo-code, and of ADR and MOVT, through the test profile.
 *
 * The case tables are printed by test/model_misc.py, an independent model
 * (Python integers), so the expected values do not come from this code.
 */
#include <stdio.h>
#include <string.h>
#include "arm_common/arm_pseudo_arith.h"
#include "arm_common/arm_pseudo_move.h"
#include "arm_common/arm_pseudo_extend.h"
#include "arm_common/arm_pseudo_bitfield.h"
#include "arm_common/arm_pseudo_multiply.h"

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

typedef Std_ReturnType (*ExtOp)(TargetCoreType *core, const arm_extend_input_type *in, arm_dp_output_type *out);
typedef Std_ReturnType (*BfxOp)(TargetCoreType *core, const arm_bfx_input_type *in, arm_dp_output_type *out);
typedef Std_ReturnType (*BfiOp)(TargetCoreType *core, const arm_bfi_input_type *in, arm_dp_output_type *out);
typedef Std_ReturnType (*UnaryOp)(TargetCoreType *core, const arm_unary_input_type *in, arm_dp_output_type *out);
typedef Std_ReturnType (*MulOp)(TargetCoreType *core, const arm_mul_input_type *in, arm_dp_output_type *out);
typedef Std_ReturnType (*MulLongOp)(TargetCoreType *core, const arm_mul_long_input_type *in, arm_mul_long_output_type *out);

typedef struct {
	const char *name;
	ExtOp op;
	uint32 rn;
	uint32 rm;
	uint32 rotation;
	uint32 result;
} ExtCase;

typedef struct {
	const char *name;
	BfxOp op;
	uint32 rn;
	uint32 lsbit;
	uint32 widthminus1;
	uint32 result;
} BfxCase;

typedef struct {
	const char *name;
	BfiOp op;
	uint32 rd;
	uint32 rn;
	uint32 lsbit;
	uint32 msbit;
	uint32 result;
} BfiCase;

typedef struct {
	const char *name;
	UnaryOp op;
	uint32 rm;
	uint32 result;
} UnaryCase;

typedef struct {
	const char *name;
	MulOp op;
	bool setflags;
	uint32 rn;
	uint32 rm;
	uint32 ra;
	uint32 apsr_before;
	uint32 result;
	uint32 apsr_after;
} MulCase;

typedef struct {
	const char *name;
	MulLongOp op;
	bool setflags;
	uint32 rn;
	uint32 rm;
	uint32 lo;
	uint32 hi;
	uint32 apsr_before;
	uint32 result_lo;
	uint32 result_hi;
	uint32 apsr_after;
} MulLongCase;

/* Printed by test/model_misc.py. */
static const ExtCase ext_cases[] = {
	{ "SXTB", arm_pseudo_sxtb, 0x35840EA8U, 0x7FAFAD48U, 0U, 0x00000048U },
	{ "SXTB", arm_pseudo_sxtb, 0x3A60F30BU, 0xA63C88CEU, 8U, 0xFFFFFF88U },
	{ "SXTB", arm_pseudo_sxtb, 0x9607AB37U, 0x9E863514U, 16U, 0xFFFFFF86U },
	{ "SXTB", arm_pseudo_sxtb, 0x12D9D65FU, 0xE0469FA2U, 24U, 0xFFFFFFE0U },
	{ "SXTH", arm_pseudo_sxth, 0xFFFFFFFFU, 0x7FFFFFFFU, 0U, 0xFFFFFFFFU },
	{ "SXTH", arm_pseudo_sxth, 0x3D43F4D0U, 0xD033997BU, 8U, 0x00003399U },
	{ "SXTH", arm_pseudo_sxth, 0xC720BBF9U, 0x000000FFU, 16U, 0x00000000U },
	{ "SXTH", arm_pseudo_sxth, 0x89ABCDEFU, 0x8CE84F81U, 24U, 0xFFFF818CU },
	{ "UXTB", arm_pseudo_uxtb, 0x7F7A0769U, 0xC7FCC6E3U, 0U, 0x000000E3U },
	{ "UXTB", arm_pseudo_uxtb, 0x8820C816U, 0x5022E8F4U, 8U, 0x000000E8U },
	{ "UXTB", arm_pseudo_uxtb, 0x00008000U, 0xFAE461BCU, 16U, 0x000000E4U },
	{ "UXTB", arm_pseudo_uxtb, 0x00000080U, 0xF590C115U, 24U, 0x000000F5U },
	{ "UXTH", arm_pseudo_uxth, 0x8858C561U, 0x7E02DD90U, 0U, 0x0000DD90U },
	{ "UXTH", arm_pseudo_uxth, 0xD0564B58U, 0x7E6EFA10U, 8U, 0x00006EFAU },
	{ "UXTH", arm_pseudo_uxth, 0x31A32939U, 0xFFFFFFFFU, 16U, 0x0000FFFFU },
	{ "UXTH", arm_pseudo_uxth, 0xFC9D2DF2U, 0xC766E390U, 24U, 0x000090C7U },
	{ "SXTAB", arm_pseudo_sxtab, 0x12345678U, 0x38FAD758U, 0U, 0x123456D0U },
	{ "SXTAB", arm_pseudo_sxtab, 0xC3E9E02AU, 0xC73E5770U, 8U, 0xC3E9E081U },
	{ "SXTAB", arm_pseudo_sxtab, 0x0A3AAE48U, 0x00000001U, 16U, 0x0A3AAE48U },
	{ "SXTAB", arm_pseudo_sxtab, 0x6F1CF302U, 0x12345678U, 24U, 0x6F1CF314U },
	{ "SXTAH", arm_pseudo_sxtah, 0xE2AD8CD6U, 0x89ABCDEFU, 0U, 0xE2AD5AC5U },
	{ "SXTAH", arm_pseudo_sxtah, 0x00007FFFU, 0x00000001U, 8U, 0x00007FFFU },
	{ "SXTAH", arm_pseudo_sxtah, 0xE342994AU, 0x4FB3D533U, 16U, 0xE342E8FDU },
	{ "SXTAH", arm_pseudo_sxtah, 0x69E39F6DU, 0x0000FFFFU, 24U, 0x69E39E6DU },
	{ "UXTAB", arm_pseudo_uxtab, 0x00000001U, 0xFFFFFFFFU, 0U, 0x00000100U },
	{ "UXTAB", arm_pseudo_uxtab, 0x388A8FCFU, 0x174E0442U, 8U, 0x388A8FD3U },
	{ "UXTAB", arm_pseudo_uxtab, 0x97368832U, 0xB915FDB3U, 16U, 0x97368847U },
	{ "UXTAB", arm_pseudo_uxtab, 0x62D615C6U, 0x9000B3EEU, 24U, 0x62D61656U },
	{ "UXTAH", arm_pseudo_uxtah, 0x9E00E0F5U, 0xC4734FE1U, 0U, 0x9E0130D6U },
	{ "UXTAH", arm_pseudo_uxtah, 0x00008000U, 0x98BB0487U, 8U, 0x00013B04U },
	{ "UXTAH", arm_pseudo_uxtah, 0xAEED9384U, 0x4D81496CU, 16U, 0xAEEDE105U },
	{ "UXTAH", arm_pseudo_uxtah, 0x3429E696U, 0xB9565AE9U, 24U, 0x342AD04FU },
	{ "SXTB16", arm_pseudo_sxtb16, 0x8E787DC3U, 0xA22D2AE4U, 0U, 0x002DFFE4U },
	{ "SXTB16", arm_pseudo_sxtb16, 0xB31460FDU, 0x80000000U, 8U, 0xFF800000U },
	{ "SXTB16", arm_pseudo_sxtb16, 0x7C80F3CFU, 0x00008000U, 16U, 0x00000000U },
	{ "SXTB16", arm_pseudo_sxtb16, 0xAF71F5E2U, 0x02A2A1C4U, 24U, 0xFFA10002U },
	{ "UXTB16", arm_pseudo_uxtb16, 0x00000000U, 0x7FFFFFFFU, 0U, 0x00FF00FFU },
	{ "UXTB16", arm_pseudo_uxtb16, 0xDC680652U, 0x0000FFFFU, 8U, 0x000000FFU },
	{ "UXTB16", arm_pseudo_uxtb16, 0x00000001U, 0xE2E6822AU, 16U, 0x002A00E6U },
	{ "UXTB16", arm_pseudo_uxtb16, 0x000000FFU, 0x6B380B17U, 24U, 0x000B006BU },
	{ "SXTAB16", arm_pseudo_sxtab16, 0x00000000U, 0x00007FFFU, 0U, 0x0000FFFFU },
	{ "SXTAB16", arm_pseudo_sxtab16, 0x000000FFU, 0x75D7D6DBU, 8U, 0x007500D5U },
	{ "SXTAB16", arm_pseudo_sxtab16, 0x321A4BF2U, 0x3B2AE0BAU, 16U, 0x31D44C1CU },
	{ "SXTAB16", arm_pseudo_sxtab16, 0x00007FFFU, 0x52619690U, 24U, 0xFF968051U },
	{ "UXTAB16", arm_pseudo_uxtab16, 0x30AF5211U, 0x00000080U, 0U, 0x30AF5291U },
	{ "UXTAB16", arm_pseudo_uxtab16, 0x73C55A00U, 0xFFFFFFFFU, 8U, 0x74C45AFFU },
	{ "UXTAB16", arm_pseudo_uxtab16, 0x00000001U, 0x2BFA8EC5U, 16U, 0x00C500FBU },
	{ "UXTAB16", arm_pseudo_uxtab16, 0xF679DC62U, 0xA9539930U, 24U, 0xF712DD0BU },
};
static const BfxCase bfx_cases[] = {
	{ "SBFX", arm_pseudo_sbfx, 0x3F23D24CU, 0U, 0U, 0x00000000U },
	{ "SBFX", arm_pseudo_sbfx, 0x7F5A9EDDU, 0U, 31U, 0x7F5A9EDDU },
	{ "SBFX", arm_pseudo_sbfx, 0x01F3594DU, 31U, 0U, 0x00000000U },
	{ "SBFX", arm_pseudo_sbfx, 0x89ABCDEFU, 4U, 7U, 0xFFFFFFDEU },
	{ "SBFX", arm_pseudo_sbfx, 0x000000FFU, 8U, 15U, 0x00000000U },
	{ "SBFX", arm_pseudo_sbfx, 0xA35FE70BU, 16U, 15U, 0xFFFFA35FU },
	{ "SBFX", arm_pseudo_sbfx, 0x63111D6DU, 3U, 1U, 0x00000001U },
	{ "SBFX", arm_pseudo_sbfx, 0xF0427513U, 5U, 16U, 0x000013A8U },
	{ "SBFX", arm_pseudo_sbfx, 0x0000007FU, 21U, 3U, 0x00000000U },
	{ "SBFX", arm_pseudo_sbfx, 0xCF0F2100U, 25U, 1U, 0xFFFFFFFFU },
	{ "SBFX", arm_pseudo_sbfx, 0x0000007FU, 4U, 6U, 0x00000007U },
	{ "SBFX", arm_pseudo_sbfx, 0x0461C833U, 9U, 18U, 0x000230E4U },
	{ "UBFX", arm_pseudo_ubfx, 0x9DCAC8DFU, 0U, 0U, 0x00000001U },
	{ "UBFX", arm_pseudo_ubfx, 0x658F40F9U, 0U, 31U, 0x658F40F9U },
	{ "UBFX", arm_pseudo_ubfx, 0x126E0D5CU, 31U, 0U, 0x00000000U },
	{ "UBFX", arm_pseudo_ubfx, 0x0000FFFFU, 4U, 7U, 0x000000FFU },
	{ "UBFX", arm_pseudo_ubfx, 0xFFFFFFFFU, 8U, 15U, 0x0000FFFFU },
	{ "UBFX", arm_pseudo_ubfx, 0xEB325F60U, 16U, 15U, 0x0000EB32U },
	{ "UBFX", arm_pseudo_ubfx, 0xA25C2C1FU, 3U, 1U, 0x00000003U },
	{ "UBFX", arm_pseudo_ubfx, 0x4F58348BU, 9U, 11U, 0x00000C1AU },
	{ "UBFX", arm_pseudo_ubfx, 0xE9477683U, 16U, 14U, 0x00006947U },
	{ "UBFX", arm_pseudo_ubfx, 0xE4C7D0E1U, 3U, 1U, 0x00000000U },
	{ "UBFX", arm_pseudo_ubfx, 0xAD52FF7AU, 10U, 7U, 0x000000BFU },
	{ "UBFX", arm_pseudo_ubfx, 0xC12FDE74U, 11U, 14U, 0x000025FBU },
};
static const BfiCase bfi_cases[] = {
	{ "BFI", arm_pseudo_bfi, 0x2B74AAB8U, 0x00008000U, 0U, 0U, 0x2B74AAB8U },
	{ "BFI", arm_pseudo_bfi, 0x8BBC5BE9U, 0x0000FFFFU, 0U, 31U, 0x0000FFFFU },
	{ "BFI", arm_pseudo_bfi, 0xA3B1111CU, 0x185CE77FU, 31U, 31U, 0xA3B1111CU },
	{ "BFI", arm_pseudo_bfi, 0x275D487AU, 0x00000080U, 4U, 11U, 0x275D480AU },
	{ "BFI", arm_pseudo_bfi, 0x2090F6F9U, 0x0000FFFFU, 16U, 31U, 0xFFFFF6F9U },
	{ "BFI", arm_pseudo_bfi, 0x82AE9F58U, 0xD7714D25U, 5U, 5U, 0x82AE9F78U },
	{ "BFI", arm_pseudo_bfi, 0x427940C6U, 0xCDA238D4U, 30U, 30U, 0x027940C6U },
	{ "BFI", arm_pseudo_bfi, 0x12345678U, 0x00000001U, 16U, 22U, 0x12015678U },
	{ "BFI", arm_pseudo_bfi, 0x3F5DD0CEU, 0x12345678U, 15U, 15U, 0x3F5D50CEU },
	{ "BFI", arm_pseudo_bfi, 0xD260750CU, 0x480E1668U, 3U, 9U, 0xD2607744U },
	{ "BFI", arm_pseudo_bfi, 0x295A8769U, 0xCE34FBC5U, 3U, 6U, 0x295A8729U },
	{ "BFC", arm_pseudo_bfc, 0x93AE6BFFU, 0xD7AC3EF1U, 0U, 0U, 0x93AE6BFEU },
	{ "BFC", arm_pseudo_bfc, 0xCAA57D72U, 0xFFFFFFFFU, 0U, 31U, 0x00000000U },
	{ "BFC", arm_pseudo_bfc, 0x0000007FU, 0x00000080U, 31U, 31U, 0x0000007FU },
	{ "BFC", arm_pseudo_bfc, 0xEFDD8C83U, 0xF07A0E05U, 4U, 11U, 0xEFDD8003U },
	{ "BFC", arm_pseudo_bfc, 0xD9AA77F8U, 0x0000FFFFU, 16U, 31U, 0x000077F8U },
	{ "BFC", arm_pseudo_bfc, 0x8382F24FU, 0x00008000U, 5U, 5U, 0x8382F24FU },
	{ "BFC", arm_pseudo_bfc, 0x89ABCDEFU, 0xFE32B650U, 8U, 12U, 0x89ABC0EFU },
	{ "BFC", arm_pseudo_bfc, 0x723B4CE0U, 0x375D30E1U, 17U, 31U, 0x00014CE0U },
	{ "BFC", arm_pseudo_bfc, 0x12345678U, 0x319DFC6EU, 10U, 28U, 0x00000278U },
	{ "BFC", arm_pseudo_bfc, 0xEFB926D6U, 0xAC5BD3ABU, 1U, 20U, 0xEFA00000U },
	{ "BFC", arm_pseudo_bfc, 0xDE688D32U, 0x95247738U, 24U, 25U, 0xDC688D32U },
};
static const UnaryCase unary_cases[] = {
	{ "REV", arm_pseudo_rev, 0x00000000U, 0x00000000U },
	{ "REV", arm_pseudo_rev, 0x00000001U, 0x01000000U },
	{ "REV", arm_pseudo_rev, 0x00000080U, 0x80000000U },
	{ "REV", arm_pseudo_rev, 0x00008000U, 0x00800000U },
	{ "REV", arm_pseudo_rev, 0x80000000U, 0x00000080U },
	{ "REV", arm_pseudo_rev, 0xFFFFFFFFU, 0xFFFFFFFFU },
	{ "REV", arm_pseudo_rev, 0x92A51779U, 0x7917A592U },
	{ "REV", arm_pseudo_rev, 0x00000001U, 0x01000000U },
	{ "REV", arm_pseudo_rev, 0xA02F5100U, 0x00512FA0U },
	{ "REV", arm_pseudo_rev, 0x000000FFU, 0xFF000000U },
	{ "REV16", arm_pseudo_rev16, 0x00000000U, 0x00000000U },
	{ "REV16", arm_pseudo_rev16, 0x00000001U, 0x00000100U },
	{ "REV16", arm_pseudo_rev16, 0x00000080U, 0x00008000U },
	{ "REV16", arm_pseudo_rev16, 0x00008000U, 0x00000080U },
	{ "REV16", arm_pseudo_rev16, 0x80000000U, 0x00800000U },
	{ "REV16", arm_pseudo_rev16, 0xFFFFFFFFU, 0xFFFFFFFFU },
	{ "REV16", arm_pseudo_rev16, 0x0BF5A185U, 0xF50B85A1U },
	{ "REV16", arm_pseudo_rev16, 0x393E68F3U, 0x3E39F368U },
	{ "REV16", arm_pseudo_rev16, 0x89ABCDEFU, 0xAB89EFCDU },
	{ "REV16", arm_pseudo_rev16, 0x12345678U, 0x34127856U },
	{ "REVSH", arm_pseudo_revsh, 0x00000000U, 0x00000000U },
	{ "REVSH", arm_pseudo_revsh, 0x00000001U, 0x00000100U },
	{ "REVSH", arm_pseudo_revsh, 0x00000080U, 0xFFFF8000U },
	{ "REVSH", arm_pseudo_revsh, 0x00008000U, 0x00000080U },
	{ "REVSH", arm_pseudo_revsh, 0x80000000U, 0x00000000U },
	{ "REVSH", arm_pseudo_revsh, 0xFFFFFFFFU, 0xFFFFFFFFU },
	{ "REVSH", arm_pseudo_revsh, 0xD8B98970U, 0x00007089U },
	{ "REVSH", arm_pseudo_revsh, 0xA19751A6U, 0xFFFFA651U },
	{ "REVSH", arm_pseudo_revsh, 0x000000FFU, 0xFFFFFF00U },
	{ "REVSH", arm_pseudo_revsh, 0x00000080U, 0xFFFF8000U },
	{ "RBIT", arm_pseudo_rbit, 0x00000000U, 0x00000000U },
	{ "RBIT", arm_pseudo_rbit, 0x00000001U, 0x80000000U },
	{ "RBIT", arm_pseudo_rbit, 0x00000080U, 0x01000000U },
	{ "RBIT", arm_pseudo_rbit, 0x00008000U, 0x00010000U },
	{ "RBIT", arm_pseudo_rbit, 0x80000000U, 0x00000001U },
	{ "RBIT", arm_pseudo_rbit, 0xFFFFFFFFU, 0xFFFFFFFFU },
	{ "RBIT", arm_pseudo_rbit, 0x00007FFFU, 0xFFFE0000U },
	{ "RBIT", arm_pseudo_rbit, 0x5C1F580BU, 0xD01AF83AU },
	{ "RBIT", arm_pseudo_rbit, 0xD8C13C11U, 0x883C831BU },
	{ "RBIT", arm_pseudo_rbit, 0x000000FFU, 0xFF000000U },
	{ "CLZ", arm_pseudo_clz, 0x00000000U, 0x00000020U },
	{ "CLZ", arm_pseudo_clz, 0x00000001U, 0x0000001FU },
	{ "CLZ", arm_pseudo_clz, 0x00000080U, 0x00000018U },
	{ "CLZ", arm_pseudo_clz, 0x00008000U, 0x00000010U },
	{ "CLZ", arm_pseudo_clz, 0x80000000U, 0x00000000U },
	{ "CLZ", arm_pseudo_clz, 0xFFFFFFFFU, 0x00000000U },
	{ "CLZ", arm_pseudo_clz, 0x00000001U, 0x0000001FU },
	{ "CLZ", arm_pseudo_clz, 0x128A91DBU, 0x00000003U },
	{ "CLZ", arm_pseudo_clz, 0x00008000U, 0x00000010U },
	{ "CLZ", arm_pseudo_clz, 0x00000001U, 0x0000001FU },
};
static const MulCase mul_cases[] = {
	{ "MUL", arm_pseudo_mul, TRUE, 0x00000000U, 0x00000005U, 0x80000000U, 0x70000000U, 0x00000000U, 0x70000000U },
	{ "MUL", arm_pseudo_mul, FALSE, 0x00000007U, 0x00000000U, 0xA884D201U, 0x90000000U, 0x00000000U, 0x90000000U },
	{ "MUL", arm_pseudo_mul, TRUE, 0x80000000U, 0xFFFFFFFFU, 0x50B9F62CU, 0xE0000000U, 0x80000000U, 0xA0000000U },
	{ "MUL", arm_pseudo_mul, FALSE, 0xFFFFFFFFU, 0xFFFFFFFFU, 0x00008000U, 0x00000000U, 0x00000001U, 0x00000000U },
	{ "MUL", arm_pseudo_mul, TRUE, 0x00010000U, 0x00010000U, 0xFFFFFFFFU, 0xE0000000U, 0x00000000U, 0x60000000U },
	{ "MUL", arm_pseudo_mul, FALSE, 0x7FFFFFFFU, 0x00000002U, 0x993641CAU, 0x60000000U, 0xFFFFFFFEU, 0x60000000U },
	{ "MUL", arm_pseudo_mul, TRUE, 0x00000000U, 0xDCC1D1D4U, 0x88622086U, 0xA0000000U, 0x00000000U, 0x60000000U },
	{ "MUL", arm_pseudo_mul, FALSE, 0x00007FFFU, 0x0000FFFFU, 0xAC8FD019U, 0xE0000000U, 0x7FFE8001U, 0xE0000000U },
	{ "MUL", arm_pseudo_mul, TRUE, 0x00000080U, 0xB78623CBU, 0x655AEF8FU, 0x80000000U, 0xC311E580U, 0x80000000U },
	{ "MUL", arm_pseudo_mul, FALSE, 0x00008000U, 0x8F6C79D3U, 0x6F574FB7U, 0xA0000000U, 0x3CE98000U, 0xA0000000U },
	{ "MUL", arm_pseudo_mul, TRUE, 0x35BF6007U, 0x6A7DCFF3U, 0x8540BFDEU, 0xC0000000U, 0x8CB8CFA5U, 0x80000000U },
	{ "MUL", arm_pseudo_mul, FALSE, 0xA1596658U, 0x66940C78U, 0x00000080U, 0x90000000U, 0x0D941940U, 0x90000000U },
	{ "MLA", arm_pseudo_mla, TRUE, 0x00000000U, 0x00000005U, 0x8622B3B8U, 0xD0000000U, 0x8622B3B8U, 0x90000000U },
	{ "MLA", arm_pseudo_mla, FALSE, 0x00000007U, 0x00000000U, 0x44380580U, 0x20000000U, 0x44380580U, 0x20000000U },
	{ "MLA", arm_pseudo_mla, TRUE, 0x80000000U, 0xFFFFFFFFU, 0x8D48CBE3U, 0x60000000U, 0x0D48CBE3U, 0x20000000U },
	{ "MLA", arm_pseudo_mla, FALSE, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xB2CEB8F5U, 0x40000000U, 0xB2CEB8F6U, 0x40000000U },
	{ "MLA", arm_pseudo_mla, TRUE, 0x00010000U, 0x00010000U, 0x00000001U, 0x60000000U, 0x00000001U, 0x20000000U },
	{ "MLA", arm_pseudo_mla, FALSE, 0x7FFFFFFFU, 0x00000002U, 0x12345678U, 0x00000000U, 0x12345676U, 0x00000000U },
	{ "MLA", arm_pseudo_mla, TRUE, 0x1E7BF0F3U, 0x00000000U, 0x6A3FF894U, 0x20000000U, 0x6A3FF894U, 0x20000000U },
	{ "MLA", arm_pseudo_mla, FALSE, 0x7FFFFFFFU, 0x06405530U, 0x4D60C3C6U, 0xF0000000U, 0x47206E96U, 0xF0000000U },
	{ "MLA", arm_pseudo_mla, TRUE, 0xCA815A2AU, 0x9B27FC0CU, 0xE20BEE09U, 0x60000000U, 0xDF438001U, 0xA0000000U },
	{ "MLA", arm_pseudo_mla, FALSE, 0x00008000U, 0x089CDA0AU, 0x813F86D4U, 0x10000000U, 0xEE4486D4U, 0x10000000U },
	{ "MLA", arm_pseudo_mla, TRUE, 0x40E369C4U, 0xA84BBA78U, 0x2545F4DFU, 0x40000000U, 0x6723F0BFU, 0x00000000U },
	{ "MLA", arm_pseudo_mla, FALSE, 0x5C91F70CU, 0x1BDC05FAU, 0x0000007FU, 0xC0000000U, 0xD2AE7E37U, 0xC0000000U },
	{ "MLS", arm_pseudo_mls, FALSE, 0x00000000U, 0x00000005U, 0x7FFFFFFFU, 0x60000000U, 0x7FFFFFFFU, 0x60000000U },
	{ "MLS", arm_pseudo_mls, FALSE, 0x00000007U, 0x00000000U, 0xA53D6962U, 0xB0000000U, 0xA53D6962U, 0xB0000000U },
	{ "MLS", arm_pseudo_mls, FALSE, 0x80000000U, 0xFFFFFFFFU, 0x80000000U, 0xC0000000U, 0x00000000U, 0xC0000000U },
	{ "MLS", arm_pseudo_mls, FALSE, 0xFFFFFFFFU, 0xFFFFFFFFU, 0x00008000U, 0x20000000U, 0x00007FFFU, 0x20000000U },
	{ "MLS", arm_pseudo_mls, FALSE, 0x00010000U, 0x00010000U, 0xB2F4E73BU, 0x30000000U, 0xB2F4E73BU, 0x30000000U },
	{ "MLS", arm_pseudo_mls, FALSE, 0x7FFFFFFFU, 0x00000002U, 0x2D487651U, 0x10000000U, 0x2D487653U, 0x10000000U },
	{ "MLS", arm_pseudo_mls, FALSE, 0x00000080U, 0x1EB01B59U, 0xEAA9F23AU, 0x70000000U, 0x929C45BAU, 0x70000000U },
	{ "MLS", arm_pseudo_mls, FALSE, 0x00000000U, 0x8AC48C37U, 0xA4B726FDU, 0xE0000000U, 0xA4B726FDU, 0xE0000000U },
	{ "MLS", arm_pseudo_mls, FALSE, 0xDA3D8622U, 0x5C256771U, 0xC42E3E3BU, 0x00000000U, 0x13245B39U, 0x00000000U },
	{ "MLS", arm_pseudo_mls, FALSE, 0x31D8E51FU, 0x00000001U, 0x82F79545U, 0x60000000U, 0x511EB026U, 0x60000000U },
	{ "MLS", arm_pseudo_mls, FALSE, 0x00008000U, 0x8C2833CDU, 0xA6AB7865U, 0xD0000000U, 0x8CC4F865U, 0xD0000000U },
	{ "MLS", arm_pseudo_mls, FALSE, 0x0000FFFFU, 0x36BC7AC3U, 0xE7BEFE88U, 0x90000000U, 0xA3B8794BU, 0x90000000U },
	{ "SDIV", arm_pseudo_sdiv, FALSE, 0x00000000U, 0x00000005U, 0x80A658E8U, 0x10000000U, 0x00000000U, 0x10000000U },
	{ "SDIV", arm_pseudo_sdiv, FALSE, 0x00000007U, 0x00000000U, 0xD41D4A24U, 0x70000000U, 0x00000000U, 0x70000000U },
	{ "SDIV", arm_pseudo_sdiv, FALSE, 0x80000000U, 0xFFFFFFFFU, 0x8C7EBEC8U, 0x20000000U, 0x80000000U, 0x20000000U },
	{ "SDIV", arm_pseudo_sdiv, FALSE, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFC423BBU, 0xA0000000U, 0x00000001U, 0xA0000000U },
	{ "SDIV", arm_pseudo_sdiv, FALSE, 0x00010000U, 0x00010000U, 0x9328624EU, 0x00000000U, 0x00000001U, 0x00000000U },
	{ "SDIV", arm_pseudo_sdiv, FALSE, 0x7FFFFFFFU, 0x00000002U, 0x6B486A53U, 0x00000000U, 0x3FFFFFFFU, 0x00000000U },
	{ "SDIV", arm_pseudo_sdiv, FALSE, 0x3F378722U, 0x062E741EU, 0xEA5AE928U, 0x10000000U, 0x0000000AU, 0x10000000U },
	{ "SDIV", arm_pseudo_sdiv, FALSE, 0xC2CE46F3U, 0x88E334B3U, 0x5659C0E7U, 0xD0000000U, 0x00000000U, 0xD0000000U },
	{ "SDIV", arm_pseudo_sdiv, FALSE, 0xE700AC28U, 0xD2B0B873U, 0x8E596939U, 0xA0000000U, 0x00000000U, 0xA0000000U },
	{ "SDIV", arm_pseudo_sdiv, FALSE, 0xBEE4C9B0U, 0x12345678U, 0xC9EBCC91U, 0x50000000U, 0xFFFFFFFDU, 0x50000000U },
	{ "SDIV", arm_pseudo_sdiv, FALSE, 0x12345678U, 0x89ABCDEFU, 0xD89BD130U, 0x20000000U, 0x00000000U, 0x20000000U },
	{ "SDIV", arm_pseudo_sdiv, FALSE, 0x159BC78EU, 0xA109D577U, 0x782844B8U, 0x10000000U, 0x00000000U, 0x10000000U },
	{ "UDIV", arm_pseudo_udiv, FALSE, 0x00000000U, 0x00000005U, 0x90C829C9U, 0x70000000U, 0x00000000U, 0x70000000U },
	{ "UDIV", arm_pseudo_udiv, FALSE, 0x00000007U, 0x00000000U, 0x00008000U, 0x30000000U, 0x00000000U, 0x30000000U },
	{ "UDIV", arm_pseudo_udiv, FALSE, 0x80000000U, 0xFFFFFFFFU, 0xFE3BC5D8U, 0xB0000000U, 0x00000000U, 0xB0000000U },
	{ "UDIV", arm_pseudo_udiv, FALSE, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xD6EAFF6CU, 0x00000000U, 0x00000001U, 0x00000000U },
	{ "UDIV", arm_pseudo_udiv, FALSE, 0x00010000U, 0x00010000U, 0x17E509C2U, 0x10000000U, 0x00000001U, 0x10000000U },
	{ "UDIV", arm_pseudo_udiv, FALSE, 0x7FFFFFFFU, 0x00000002U, 0xC3B6EDC5U, 0xF0000000U, 0x3FFFFFFFU, 0xF0000000U },
	{ "UDIV", arm_pseudo_udiv, FALSE, 0xDBB41D56U, 0x00007FFFU, 0x4E80EC0AU, 0x90000000U, 0x0001B76BU, 0x90000000U },
	{ "UDIV", arm_pseudo_udiv, FALSE, 0xFCD979F6U, 0x0F2DE894U, 0xE58B3543U, 0x10000000U, 0x00000010U, 0x10000000U },
	{ "UDIV", arm_pseudo_udiv, FALSE, 0x00008000U, 0x5BB219E9U, 0x00000001U, 0x60000000U, 0x00000000U, 0x60000000U },
	{ "UDIV", arm_pseudo_udiv, FALSE, 0x89ABCDEFU, 0x89ABCDEFU, 0xB82B428EU, 0xF0000000U, 0x00000001U, 0xF0000000U },
	{ "UDIV", arm_pseudo_udiv, FALSE, 0x2BF6335DU, 0xD5E2CE90U, 0x5297A24CU, 0xB0000000U, 0x00000000U, 0xB0000000U },
	{ "UDIV", arm_pseudo_udiv, FALSE, 0x0000FFFFU, 0x80000000U, 0xFFFFFFFFU, 0xB0000000U, 0x00000000U, 0xB0000000U },
};
static const MulLongCase mul_long_cases[] = {
	{ "SMULL", arm_pseudo_smull, TRUE, 0x00000000U, 0x00000005U, 0x833C0119U, 0xC5B8744EU, 0x40000000U, 0x00000000U, 0x00000000U, 0x40000000U },
	{ "SMULL", arm_pseudo_smull, FALSE, 0x80000000U, 0x80000000U, 0x15E46212U, 0xA38D86A6U, 0x30000000U, 0x00000000U, 0x40000000U, 0x30000000U },
	{ "SMULL", arm_pseudo_smull, TRUE, 0xFFFFFFFFU, 0xFFFFFFFFU, 0x5D688236U, 0x0000007FU, 0x70000000U, 0x00000001U, 0x00000000U, 0x30000000U },
	{ "SMULL", arm_pseudo_smull, FALSE, 0x80000000U, 0xFFFFFFFFU, 0x12345678U, 0x536A3910U, 0xD0000000U, 0x80000000U, 0x00000000U, 0xD0000000U },
	{ "SMULL", arm_pseudo_smull, TRUE, 0xFFFFFFFFU, 0x00000001U, 0x275ED040U, 0xE19147D6U, 0x80000000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0x80000000U },
	{ "SMULL", arm_pseudo_smull, FALSE, 0x00000080U, 0xFFFFFFFFU, 0x83383680U, 0xC0BB6102U, 0x20000000U, 0xFFFFFF80U, 0xFFFFFFFFU, 0x20000000U },
	{ "SMULL", arm_pseudo_smull, TRUE, 0x25AA43D0U, 0x5CB3F73DU, 0x0F67DDCFU, 0x00007FFFU, 0xF0000000U, 0x6B6FD890U, 0x0DA3AACCU, 0x30000000U },
	{ "SMULL", arm_pseudo_smull, FALSE, 0x49CE86B0U, 0x71EADEA5U, 0xF67AC2BBU, 0x80000000U, 0xF0000000U, 0x77C96F70U, 0x20D7E06BU, 0xF0000000U },
	{ "SMULL", arm_pseudo_smull, TRUE, 0x02DD559AU, 0x96898883U, 0x7FFFFFFFU, 0x100D1BB7U, 0x20000000U, 0x18269DCEU, 0xFED1E488U, 0xA0000000U },
	{ "SMULL", arm_pseudo_smull, FALSE, 0xF31CB3E7U, 0x28F8134EU, 0x9B84BEBCU, 0xE20200B6U, 0xA0000000U, 0x93E0F562U, 0xFDEFFEF1U, 0xA0000000U },
	{ "SMULL", arm_pseudo_smull, TRUE, 0xD56D1161U, 0xEE5B3C6FU, 0x80807065U, 0x0000FFFFU, 0x10000000U, 0x75D8450FU, 0x02EF287FU, 0x10000000U },
	{ "UMULL", arm_pseudo_umull, TRUE, 0x00000000U, 0x00000005U, 0x39C4BD42U, 0xCD2877BFU, 0xD0000000U, 0x00000000U, 0x00000000U, 0x50000000U },
	{ "UMULL", arm_pseudo_umull, FALSE, 0x80000000U, 0x80000000U, 0x2894A35EU, 0x74225CE9U, 0x30000000U, 0x00000000U, 0x40000000U, 0x30000000U },
	{ "UMULL", arm_pseudo_umull, TRUE, 0xFFFFFFFFU, 0xFFFFFFFFU, 0x3200DECBU, 0x5DC9DB83U, 0x40000000U, 0x00000001U, 0xFFFFFFFEU, 0x80000000U },
	{ "UMULL", arm_pseudo_umull, FALSE, 0x80000000U, 0xFFFFFFFFU, 0x0000007FU, 0x0000007FU, 0xD0000000U, 0x80000000U, 0x7FFFFFFFU, 0xD0000000U },
	{ "UMULL", arm_pseudo_umull, TRUE, 0xFFFFFFFFU, 0x00000001U, 0x4783946DU, 0xCA07D2B2U, 0x60000000U, 0xFFFFFFFFU, 0x00000000U, 0x20000000U },
	{ "UMULL", arm_pseudo_umull, FALSE, 0x882DAE3AU, 0xB7E71EFBU, 0xBD0F5254U, 0x295E33D0U, 0xE0000000U, 0x8B8A9EDEU, 0x61D3993EU, 0xE0000000U },
	{ "UMULL", arm_pseudo_umull, TRUE, 0x06B3C03CU, 0x00007FFFU, 0x8FD6FAC0U, 0xB3ACB038U, 0x30000000U, 0xD96A3FC4U, 0x00000359U, 0x30000000U },
	{ "UMULL", arm_pseudo_umull, FALSE, 0xBF64DA5CU, 0x00000000U, 0xF3F91B38U, 0x00000000U, 0x10000000U, 0x00000000U, 0x00000000U, 0x10000000U },
	{ "UMULL", arm_pseudo_umull, TRUE, 0x0000007FU, 0x0CD465AEU, 0xACDBEEACU, 0x03B2CA2DU, 0x30000000U, 0x5D5E7152U, 0x00000006U, 0x30000000U },
	{ "UMULL", arm_pseudo_umull, FALSE, 0xFFFFFFFFU, 0x37A9D065U, 0x0000FFFFU, 0x12345678U, 0x80000000U, 0xC8562F9BU, 0x37A9D064U, 0x80000000U },
	{ "UMULL", arm_pseudo_umull, TRUE, 0x67E04881U, 0x0A7080B7U, 0x1D7B1AABU, 0x04082D4CU, 0xD0000000U, 0x28045437U, 0x043C692FU, 0x10000000U },
	{ "SMLAL", arm_pseudo_smlal, TRUE, 0x00000000U, 0x00000005U, 0x00000000U, 0x00000000U, 0x70000000U, 0x00000000U, 0x00000000U, 0x70000000U },
	{ "SMLAL", arm_pseudo_smlal, FALSE, 0x80000000U, 0x80000000U, 0xA835E785U, 0x75598B11U, 0x90000000U, 0xA835E785U, 0xB5598B11U, 0x90000000U },
	{ "SMLAL", arm_pseudo_smlal, TRUE, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xC2C796AAU, 0x35E773F0U, 0xA0000000U, 0xC2C796ABU, 0x35E773F0U, 0x20000000U },
	{ "SMLAL", arm_pseudo_smlal, FALSE, 0x80000000U, 0xFFFFFFFFU, 0x000000FFU, 0x3609AEB5U, 0x30000000U, 0x800000FFU, 0x3609AEB5U, 0x30000000U },
	{ "SMLAL", arm_pseudo_smlal, TRUE, 0xFFFFFFFFU, 0x00000001U, 0x00008000U, 0xBBA0A1B4U, 0x60000000U, 0x00007FFFU, 0xBBA0A1B4U, 0xA0000000U },
	{ "SMLAL", arm_pseudo_smlal, FALSE, 0xF6E177D1U, 0x882CD6B5U, 0x9A3FDC2EU, 0x015BF170U, 0x30000000U, 0x41BE48F3U, 0x05A0A861U, 0x30000000U },
	{ "SMLAL", arm_pseudo_smlal, TRUE, 0xFFFFFFFFU, 0x00000080U, 0x0000007FU, 0x7FFFFFFFU, 0x50000000U, 0xFFFFFFFFU, 0x7FFFFFFEU, 0x10000000U },
	{ "SMLAL", arm_pseudo_smlal, FALSE, 0x1CFF2DDBU, 0x80000000U, 0x7CDAC9EAU, 0x42E7EF25U, 0x60000000U, 0xFCDAC9EAU, 0x34685837U, 0x60000000U },
	{ "SMLAL", arm_pseudo_smlal, TRUE, 0x00000001U, 0x167EF8BCU, 0x814B2BBAU, 0x872A5276U, 0x00000000U, 0x97CA2476U, 0x872A5276U, 0x80000000U },
	{ "SMLAL", arm_pseudo_smlal, FALSE, 0xD25321B3U, 0x5BB17692U, 0x41F51A3AU, 0x302C113BU, 0xE0000000U, 0x74A9D450U, 0x1FCFF488U, 0xE0000000U },
	{ "SMLAL", arm_pseudo_smlal, TRUE, 0x54EB4850U, 0x58928D97U, 0xF523F499U, 0x0000007FU, 0x50000000U, 0x605FABC9U, 0x1D617E82U, 0x10000000U },
	{ "UMLAL", arm_pseudo_umlal, TRUE, 0x00000000U, 0x00000005U, 0x00000000U, 0x00000000U, 0x80000000U, 0x00000000U, 0x00000000U, 0x40000000U },
	{ "UMLAL", arm_pseudo_umlal, FALSE, 0x80000000U, 0x80000000U, 0xC83258C9U, 0xE87DF1F7U, 0x00000000U, 0xC83258C9U, 0x287DF1F7U, 0x00000000U },
	{ "UMLAL", arm_pseudo_umlal, TRUE, 0xFFFFFFFFU, 0xFFFFFFFFU, 0x7FFFFFFFU, 0x1E4A346AU, 0x00000000U, 0x80000000U, 0x1E4A3468U, 0x00000000U },
	{ "UMLAL", arm_pseudo_umlal, FALSE, 0x80000000U, 0xFFFFFFFFU, 0x8FD89210U, 0xFF32E8BBU, 0x00000000U, 0x0FD89210U, 0x7F32E8BBU, 0x00000000U },
	{ "UMLAL", arm_pseudo_umlal, TRUE, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0xADA34265U, 0xD0000000U, 0xFFFFFFFEU, 0xADA34266U, 0x90000000U },
	{ "UMLAL", arm_pseudo_umlal, FALSE, 0xFFFFFFFFU, 0x12345678U, 0x7AC6B171U, 0xF5FE83A9U, 0x10000000U, 0x68925AF9U, 0x0832DA21U, 0x10000000U },
	{ "UMLAL", arm_pseudo_umlal, TRUE, 0x0000FFFFU, 0x056A43D2U, 0x3C971584U, 0xA1FB4E56U, 0x90000000U, 0x7AFED1B2U, 0xA1FB53C0U, 0x90000000U },
	{ "UMLAL", arm_pseudo_umlal, FALSE, 0x0C7F37C8U, 0x000000FFU, 0x00008000U, 0xF5180D21U, 0x50000000U, 0x72B91038U, 0xF5180D2DU, 0x50000000U },
	{ "UMLAL", arm_pseudo_umlal, TRUE, 0x6AF0495BU, 0x5C0649BAU, 0x12345678U, 0x57F37B2EU, 0x10000000U, 0xB5D69596U, 0x7E6475F8U, 0x10000000U },
	{ "UMLAL", arm_pseudo_umlal, FALSE, 0x82BD14F0U, 0x89ABCDEFU, 0x7FA54FDFU, 0x658258F1U, 0x60000000U, 0x64400BEFU, 0xABD1469EU, 0x60000000U },
	{ "UMLAL", arm_pseudo_umlal, TRUE, 0x00007FFFU, 0xF20B4B6BU, 0x34937887U, 0x00007FFFU, 0x20000000U, 0xE83DAD1CU, 0x0000F903U, 0x20000000U },
};

#define N_CASES(table)	(sizeof(table) / sizeof((table)[0]))

/* The operands are in r1 (Rn), r2 (Rm), r3 (Ra); the result goes to r0. */
static Std_ReturnType ext(ExtOp op, TargetCoreType *core, uint32 cond, uint32 d, uint32 n, uint32 m, uint32 rotation)
{
	arm_extend_input_type in;
	arm_dp_output_type out;

	in.instrName = "EXT";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rd, d);
	ARM_SET_REG(core, &in, Rn, n);
	ARM_SET_REG(core, &in, Rm, m);
	in.rotation = rotation;
	return op(core, &in, &out);
}

static Std_ReturnType bfx(BfxOp op, TargetCoreType *core, uint32 cond, uint32 d, uint32 lsbit, uint32 widthminus1)
{
	arm_bfx_input_type in;
	arm_dp_output_type out;

	in.instrName = "BFX";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rd, d);
	ARM_SET_REG(core, &in, Rn, 1U);
	in.lsbit = lsbit;
	in.widthminus1 = widthminus1;
	return op(core, &in, &out);
}

static Std_ReturnType bfi(BfiOp op, TargetCoreType *core, uint32 cond, uint32 d, uint32 lsbit, uint32 msbit)
{
	arm_bfi_input_type in;
	arm_dp_output_type out;

	in.instrName = "BFI";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rd, d);
	if (op == arm_pseudo_bfc) {
		ARM_NO_REG(&in, Rn);
	}
	else {
		ARM_SET_REG(core, &in, Rn, 1U);
	}
	in.lsbit = lsbit;
	in.msbit = msbit;
	return op(core, &in, &out);
}

static Std_ReturnType unary(UnaryOp op, TargetCoreType *core, uint32 cond, uint32 d)
{
	arm_unary_input_type in;
	arm_dp_output_type out;

	in.instrName = "UNARY";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rd, d);
	ARM_SET_REG(core, &in, Rm, 2U);
	return op(core, &in, &out);
}

static Std_ReturnType mul(MulOp op, TargetCoreType *core, uint32 cond, bool setflags, uint32 d)
{
	arm_mul_input_type in;
	arm_dp_output_type out;

	in.instrName = "MUL";
	in.cond = cond;
	in.setflags = setflags;
	ARM_SET_REG(core, &in, Rd, d);
	ARM_SET_REG(core, &in, Rn, 1U);
	ARM_SET_REG(core, &in, Rm, 2U);
	ARM_SET_REG(core, &in, Ra, 3U);
	return op(core, &in, &out);
}

/* RdLo r4, RdHi r5 (unless given), Rn r1, Rm r2. */
static Std_ReturnType mul_long(MulLongOp op, TargetCoreType *core, uint32 cond, bool setflags, uint32 lo, uint32 hi)
{
	arm_mul_long_input_type in;
	arm_mul_long_output_type out;

	in.instrName = "MULL";
	in.cond = cond;
	in.setflags = setflags;
	ARM_SET_REG(core, &in, RdLo, lo);
	ARM_SET_REG(core, &in, RdHi, hi);
	ARM_SET_REG(core, &in, Rn, 1U);
	ARM_SET_REG(core, &in, Rm, 2U);
	return op(core, &in, &out);
}

static void init(TargetCoreType *core, TestProfileType profile)
{
	test_core_init(core, profile, PC0);
	core->r[0] = 0xDEADBEEFU;
}

static void test_tables(void)
{
	TargetCoreType core;
	Std_ReturnType err;
	uint32 i;

	for (i = 0U; i < N_CASES(ext_cases); i++) {
		const ExtCase *t = &ext_cases[i];
		init(&core, TestProfile_M);
		core.r[1] = t->rn;
		core.r[2] = t->rm;
		err = ext(t->op, &core, ARM_COND_AL, 0U, 1U, 2U, t->rotation);
		CHECK(err == STD_E_OK && core.r[0] == t->result, "%s #%u ror %u: %08x, expected %08x", t->name, i, t->rotation, core.r[0], t->result);
	}
	for (i = 0U; i < N_CASES(bfx_cases); i++) {
		const BfxCase *t = &bfx_cases[i];
		init(&core, TestProfile_M);
		core.r[1] = t->rn;
		err = bfx(t->op, &core, ARM_COND_AL, 0U, t->lsbit, t->widthminus1);
		CHECK(err == STD_E_OK && core.r[0] == t->result, "%s #%u: %08x, expected %08x", t->name, i, core.r[0], t->result);
	}
	for (i = 0U; i < N_CASES(bfi_cases); i++) {
		const BfiCase *t = &bfi_cases[i];
		init(&core, TestProfile_M);
		core.r[0] = t->rd;
		core.r[1] = t->rn;
		err = bfi(t->op, &core, ARM_COND_AL, 0U, t->lsbit, t->msbit);
		CHECK(err == STD_E_OK && core.r[0] == t->result, "%s #%u: %08x, expected %08x", t->name, i, core.r[0], t->result);
	}
	for (i = 0U; i < N_CASES(unary_cases); i++) {
		const UnaryCase *t = &unary_cases[i];
		init(&core, TestProfile_M);
		core.r[2] = t->rm;
		err = unary(t->op, &core, ARM_COND_AL, 0U);
		CHECK(err == STD_E_OK && core.r[0] == t->result, "%s #%u: %08x, expected %08x", t->name, i, core.r[0], t->result);
	}
	for (i = 0U; i < N_CASES(mul_cases); i++) {
		const MulCase *t = &mul_cases[i];
		init(&core, TestProfile_M);
		core.r[1] = t->rn;
		core.r[2] = t->rm;
		core.r[3] = t->ra;
		core.apsr = t->apsr_before;
		err = mul(t->op, &core, ARM_COND_AL, t->setflags, 0U);
		CHECK(err == STD_E_OK && core.r[0] == t->result && core.apsr == t->apsr_after,
		      "%s #%u: %08x apsr %08x, expected %08x apsr %08x", t->name, i, core.r[0], core.apsr, t->result, t->apsr_after);
	}
	for (i = 0U; i < N_CASES(mul_long_cases); i++) {
		const MulLongCase *t = &mul_long_cases[i];
		init(&core, TestProfile_M);
		core.r[1] = t->rn;
		core.r[2] = t->rm;
		core.r[4] = t->lo;
		core.r[5] = t->hi;
		core.apsr = t->apsr_before;
		err = mul_long(t->op, &core, ARM_COND_AL, t->setflags, 4U, 5U);
		CHECK(err == STD_E_OK && core.r[4] == t->result_lo && core.r[5] == t->result_hi && core.apsr == t->apsr_after,
		      "%s #%u: %08x:%08x apsr %08x, expected %08x:%08x apsr %08x", t->name, i,
		      core.r[5], core.r[4], core.apsr, t->result_hi, t->result_lo, t->apsr_after);
	}
}

/* Operands that are also the destination are read before it is written. */
static void test_same_register(void)
{
	TargetCoreType core;

	init(&core, TestProfile_M);
	core.r[1] = 0x00000080U;
	CHECK(ext(arm_pseudo_sxtab, &core, ARM_COND_AL, 1U, 1U, 1U, 0U) == STD_E_OK && core.r[1] == 0x00000000U, "SXTAB r1, r1, r1");
	init(&core, TestProfile_M);
	core.r[1] = 3U;
	core.r[2] = 5U;
	core.r[3] = 7U;
	CHECK(mul(arm_pseudo_mla, &core, ARM_COND_AL, FALSE, 3U) == STD_E_OK && core.r[3] == 22U, "MLA r3, r1, r2, r3");
}

/* PC as the destination, an out-of-range field, RdHi == RdLo: UNPREDICTABLE. */
static void test_unpredictable(void)
{
	TargetCoreType core;

	init(&core, TestProfile_M);
	CHECK(ext(arm_pseudo_uxtb, &core, ARM_COND_AL, 15U, 1U, 2U, 0U) == STD_E_INVALID, "UXTB pc");
	core.apsr = 0U;
	CHECK(mul(arm_pseudo_mul, &core, ARM_COND_AL, TRUE, 15U) == STD_E_INVALID && core.apsr == 0U, "MULS pc");
	CHECK(bfx(arm_pseudo_sbfx, &core, ARM_COND_AL, 0U, 20U, 12U) == STD_E_INVALID, "SBFX msbit 32");
	CHECK(bfx(arm_pseudo_ubfx, &core, ARM_COND_AL, 0U, 31U, 1U) == STD_E_INVALID, "UBFX msbit 32");
	CHECK(bfi(arm_pseudo_bfi, &core, ARM_COND_AL, 0U, 5U, 4U) == STD_E_INVALID, "BFI msbit < lsbit");
	CHECK(bfi(arm_pseudo_bfc, &core, ARM_COND_AL, 0U, 5U, 4U) == STD_E_INVALID, "BFC msbit < lsbit");
	CHECK(core.r[0] == 0xDEADBEEFU, "an UNPREDICTABLE field wrote Rd");
	CHECK(mul_long(arm_pseudo_umull, &core, ARM_COND_AL, FALSE, 4U, 4U) == STD_E_INVALID, "UMULL RdHi == RdLo");
	CHECK(mul_long(arm_pseudo_smull, &core, ARM_COND_AL, FALSE, 15U, 4U) == STD_E_INVALID, "SMULL RdLo pc");
	CHECK(mul_long(arm_pseudo_smull, &core, ARM_COND_AL, FALSE, 4U, 15U) == STD_E_INVALID, "SMULL RdHi pc");
}

/* Division by zero: 0, or the trap when DIV_0_TRP is set (M profile). */
static void test_divide_by_zero(void)
{
	TargetCoreType core;

	init(&core, TestProfile_M);
	core.r[1] = 1234U;
	core.r[2] = 0U;
	CHECK(mul(arm_pseudo_sdiv, &core, ARM_COND_AL, FALSE, 0U) == STD_E_OK && core.r[0] == 0U, "SDIV by 0");
	core.r[0] = 0xDEADBEEFU;
	CHECK(mul(arm_pseudo_udiv, &core, ARM_COND_AL, FALSE, 0U) == STD_E_OK && core.r[0] == 0U, "UDIV by 0");
	core.r[0] = 0xDEADBEEFU;
	core.div_0_trp = TRUE;
	CHECK(mul(arm_pseudo_sdiv, &core, ARM_COND_AL, FALSE, 0U) == STD_E_EXEC && core.r[0] == 0xDEADBEEFU, "SDIV by 0 traps");
	CHECK(mul(arm_pseudo_udiv, &core, ARM_COND_AL, FALSE, 0U) == STD_E_EXEC && core.r[0] == 0xDEADBEEFU, "UDIV by 0 traps");
}

static Std_ReturnType adr(TargetCoreType *core, uint32 cond, uint32 d, uint32 imm32, bool add)
{
	arm_adr_input_type in;
	arm_dp_output_type out;

	in.instrName = "ADR";
	in.cond = cond;
	ARM_SET_REG(core, &in, Rd, d);
	in.imm32 = imm32;
	in.add = add;
	return arm_pseudo_adr(core, &in, &out);
}

static Std_ReturnType movt(TargetCoreType *core, uint32 cond, uint32 d, uint32 imm16)
{
	arm_dp_imm_input_type in;
	arm_dp_output_type out;

	in.instrName = "MOVT";
	in.cond = cond;
	in.setflags = FALSE;
	ARM_SET_REG(core, &in, Rd, d);
	ARM_NO_REG(&in, Rn);
	in.imm32 = imm16;
	in.carry = FALSE;
	return arm_pseudo_movt(core, &in, &out);
}

static void test_adr_movt(void)
{
	TargetCoreType core;

	/* Thumb: Align(0x08000102 + 4, 4) = 0x08000104. */
	init(&core, TestProfile_M);
	CHECK(adr(&core, ARM_COND_AL, 0U, 0x10U, TRUE) == STD_E_OK && core.r[0] == 0x08000114U, "ADR add %08x", core.r[0]);
	CHECK(adr(&core, ARM_COND_AL, 0U, 0x200U, FALSE) == STD_E_OK && core.r[0] == 0x07FFFF04U, "ADR sub %08x", core.r[0]);
	/* ARM: PC = 0x08000102 + 8, aligned 0x08000108; to PC: ALUWritePC. */
	init(&core, TestProfile_A_ARM);
	CHECK(adr(&core, ARM_COND_AL, 15U, 0x8U, TRUE) == STD_E_OK && core.next_pc == 0x08000110U, "ADR pc %08x", core.next_pc);
	init(&core, TestProfile_M);
	core.apsr = ARM_APSR_Z;
	CHECK(adr(&core, 0x1U, 0U, 0x10U, TRUE) == STD_E_OK && core.r[0] == 0xDEADBEEFU, "ADRNE skipped");

	init(&core, TestProfile_M);
	CHECK(movt(&core, ARM_COND_AL, 0U, 0x1234U) == STD_E_OK && core.r[0] == 0x1234BEEFU, "MOVT %08x", core.r[0]);
	CHECK(movt(&core, ARM_COND_AL, 15U, 0x1234U) == STD_E_INVALID, "MOVT pc");
	core.apsr = ARM_APSR_Z;
	CHECK(movt(&core, 0x1U, 0U, 0xFFFFU) == STD_E_OK && core.r[0] == 0x1234BEEFU, "MOVTNE skipped");
}

/* Every instruction with a failing condition (NE with Z set) changes nothing. */
static void test_condition_failed(void)
{
	static const ExtOp ext_ops[] = {
		arm_pseudo_sxtb, arm_pseudo_sxth, arm_pseudo_uxtb, arm_pseudo_uxth, arm_pseudo_sxtab, arm_pseudo_sxtah,
		arm_pseudo_uxtab, arm_pseudo_uxtah, arm_pseudo_sxtb16, arm_pseudo_uxtb16, arm_pseudo_sxtab16, arm_pseudo_uxtab16,
	};
	static const UnaryOp unary_ops[] = { arm_pseudo_rev, arm_pseudo_rev16, arm_pseudo_revsh, arm_pseudo_rbit, arm_pseudo_clz };
	static const MulOp mul_ops[] = { arm_pseudo_mul, arm_pseudo_mla, arm_pseudo_mls, arm_pseudo_sdiv, arm_pseudo_udiv };
	static const MulLongOp long_ops[] = { arm_pseudo_smull, arm_pseudo_umull, arm_pseudo_smlal, arm_pseudo_umlal };
	TargetCoreType core;
	TargetCoreType before;
	uint32 i;

	init(&core, TestProfile_M);
	core.r[1] = 0x12345678U;
	core.r[2] = 0x9ABCDEF0U;
	core.r[3] = 0x0F0F0F0FU;
	core.apsr = ARM_APSR_Z;
	before = core;
	for (i = 0U; i < N_CASES(ext_ops); i++) {
		CHECK(ext(ext_ops[i], &core, 0x1U, 0U, 1U, 2U, 8U) == STD_E_OK, "skip");
	}
	CHECK(bfx(arm_pseudo_sbfx, &core, 0x1U, 0U, 4U, 3U) == STD_E_OK, "skip");
	CHECK(bfx(arm_pseudo_ubfx, &core, 0x1U, 0U, 4U, 3U) == STD_E_OK, "skip");
	CHECK(bfi(arm_pseudo_bfi, &core, 0x1U, 0U, 4U, 7U) == STD_E_OK, "skip");
	CHECK(bfi(arm_pseudo_bfc, &core, 0x1U, 0U, 4U, 7U) == STD_E_OK, "skip");
	for (i = 0U; i < N_CASES(unary_ops); i++) {
		CHECK(unary(unary_ops[i], &core, 0x1U, 0U) == STD_E_OK, "skip");
	}
	for (i = 0U; i < N_CASES(mul_ops); i++) {
		CHECK(mul(mul_ops[i], &core, 0x1U, TRUE, 0U) == STD_E_OK, "skip");
	}
	for (i = 0U; i < N_CASES(long_ops); i++) {
		CHECK(mul_long(long_ops[i], &core, 0x1U, TRUE, 4U, 5U) == STD_E_OK, "skip");
	}
	CHECK(memcmp(&core, &before, sizeof(core)) == 0, "an instruction with a failing condition changed the core");
}

int main(void)
{
	test_tables();
	test_same_register();
	test_unpredictable();
	test_divide_by_zero();
	test_adr_movt();
	test_condition_failed();
	printf("%d checks, %d failures\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
