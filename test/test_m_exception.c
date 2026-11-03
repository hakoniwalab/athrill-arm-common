/*
 * Tests of the M profile's exception model (arm_m_exception.h), its system
 * instructions (arm_pseudo_m_system.h) and its System Control Space
 * (arm_m_scs.h) through the test profile. The expected values are worked out
 * by hand from the pseudo-code.
 *
 * Memory (256 bytes at 0x20000000): the vector table at 0x00 (VTOR), the
 * stacks below 0x100.
 */
#include <stdio.h>
#include <string.h>
#include "arm_common/arm_pseudo_m_system.h"
#include "arm_common/arm_m_scs.h"

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

#define MEM(offset)	(TEST_MEM_BASE + (offset))
#define PC0		0x08000200U
#define HANDLER(n)	(0x08001000U + (0x10U * (n)))

static uint32 word_at(const TargetCoreType *core, uint32 offset)
{
	return (uint32)core->mem[offset] | ((uint32)core->mem[offset + 1U] << 8) | ((uint32)core->mem[offset + 2U] << 16)
		| ((uint32)core->mem[offset + 3U] << 24);
}

static void put_word(TargetCoreType *core, uint32 offset, uint32 value)
{
	core->mem[offset] = (uint8)value;
	core->mem[offset + 1U] = (uint8)(value >> 8);
	core->mem[offset + 2U] = (uint8)(value >> 16);
	core->mem[offset + 3U] = (uint8)(value >> 24);
}

/* Thread mode on the main stack at sp, the vector table at 0x20000000 (vector n = HANDLER(n) | 1). */
static void init(TargetCoreType *core, bool have_fp, uint32 sp)
{
	uint32 n;

	test_core_init(core, TestProfile_M, PC0);
	ArmMReset(&core->m, 16U, 4U, have_fp);
	core->m.vtor = MEM(0U);
	for (n = 1U; n <= 17U; n++) {
		put_word(core, 4U * n, HANDLER(n) | 1U);
	}
	core->r[13] = sp;
	core->r[0] = 0x10U;
	core->r[1] = 0x11U;
	core->r[2] = 0x12U;
	core->r[3] = 0x13U;
	core->r[12] = 0x1CU;
	core->r[14] = 0x0800011FU;
	core->apsr = ARM_APSR_N | ARM_APSR_C;
}

static void test_priorities(void)
{
	TargetCoreType core;
	ArmMStateType *m = &core.m;

	init(&core, FALSE, MEM(0x100U));
	CHECK(ArmMExecutionPriority(m) == 256, "Thread, nothing active: 256");
	CHECK(ArmMPendingToTake(m) == 0U, "nothing pending");
	/* IRQ0 at 0x40, pending but disabled; then enabled. */
	m->priority[16] = 0x40U;
	m->pending[16] = 1U;
	CHECK(ArmMPendingToTake(m) == 0U, "a disabled interrupt is not taken");
	m->enabled[16] = 1U;
	CHECK(ArmMPendingToTake(m) == 16U, "IRQ0");
	/* the masks */
	m->primask = 1U;
	CHECK(ArmMExecutionPriority(m) == 0 && ArmMPendingToTake(m) == 0U, "PRIMASK");
	m->primask = 0U;
	m->basepri = 0x40U;
	CHECK(ArmMPendingToTake(m) == 0U, "BASEPRI 0x40 masks priority 0x40");
	m->basepri = 0x50U;
	CHECK(ArmMPendingToTake(m) == 16U, "BASEPRI 0x50 does not");
	m->basepri = 0U;
	m->faultmask = 1U;
	CHECK(ArmMExecutionPriority(m) == -1, "FAULTMASK: -1");
	m->faultmask = 0U;
	/* SVCall pending at 0x40 too: the lower number first */
	m->priority[11] = 0x40U;
	m->pending[11] = 1U;
	CHECK(ArmMPendingToTake(m) == 11U, "equal priorities: the lower number");
	m->pending[11] = 0U;
	/* an active exception at 0x40, IRQ1 pending at 0x30: PRIGROUP decides preemption */
	m->pending[16] = 0U;
	m->active[16] = 1U;
	m->priority[17] = 0x30U;
	m->pending[17] = 1U;
	m->enabled[17] = 1U;
	CHECK(ArmMExecutionPriority(m) == 0x40 && ArmMPendingToTake(m) == 17U, "0x30 preempts 0x40");
	m->aircr_prigroup = 6U;	/* group: bit 7 only */
	CHECK(ArmMExecutionPriority(m) == 0x00 && ArmMPendingToTake(m) == 0U, "PRIGROUP 6: the same group, no preemption");
	m->aircr_prigroup = 4U;	/* group: bits 7:5 */
	CHECK(ArmMExecutionPriority(m) == 0x40 && ArmMPendingToTake(m) == 17U, "PRIGROUP 4: groups 0x20 and 0x40, preemption");
	/* UsageFault is taken only when enabled (SHCSR) */
	m->aircr_prigroup = 0U;
	m->active[16] = 0U;
	m->pending[17] = 0U;
	m->pending[6] = 1U;
	CHECK(ArmMPendingToTake(m) == 0U, "UsageFault disabled");
	m->enabled[6] = 1U;
	CHECK(ArmMPendingToTake(m) == 6U, "UsageFault enabled");
}

/* Entry from Thread mode on the main stack, then return. */
static void test_entry_return_main(void)
{
	TargetCoreType core;
	ArmMStateType *m = &core.m;

	init(&core, FALSE, MEM(0xFCU));	/* SP<2> set: the frame is aligned to 8 */
	core.itstate = 0xC6U;
	m->pending[16] = 1U;
	CHECK(ArmMExceptionEntry(&core, 16U, PC0) == STD_E_OK, "entry");
	/* frame at (0xFC - 0x20) & ~4 = 0xD8 */
	CHECK(core.r[13] == MEM(0xD8U), "SP_main %08x", core.r[13]);
	CHECK(word_at(&core, 0xD8U) == 0x10U && word_at(&core, 0xDCU) == 0x11U && word_at(&core, 0xE0U) == 0x12U
	      && word_at(&core, 0xE4U) == 0x13U && word_at(&core, 0xE8U) == 0x1CU && word_at(&core, 0xECU) == 0x0800011FU
	      && word_at(&core, 0xF0U) == PC0, "R0-R3, R12, LR, return address");
	/* xPSR: N C, IT 0xC6 (IT<1:0> = 10 at 26:25, IT<7:2> = 110001 at 15:10), T, the alignment bit 9 */
	CHECK(word_at(&core, 0xF4U) == 0xA500C600U, "xPSR %08x", word_at(&core, 0xF4U));
	CHECK(core.r[14] == 0xFFFFFFF9U, "EXC_RETURN %08x", core.r[14]);
	CHECK(core.next_pc == HANDLER(16U) && m->handler_mode && m->ipsr == 16U && m->active[16] == 1U && m->pending[16] == 0U
	      && core.itstate == 0U && m->epsr_t, "taken");
	/* return */
	core.r[0] = 0xDEADU;
	core.apsr = 0U;
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFF9U) == STD_E_OK, "return");
	CHECK(core.r[0] == 0x10U && core.r[3] == 0x13U && core.r[12] == 0x1CU && core.r[14] == 0x0800011FU && core.next_pc == PC0,
	      "registers restored");
	CHECK(core.r[13] == MEM(0xFCU) && core.apsr == (ARM_APSR_N | ARM_APSR_C) && core.itstate == 0xC6U, "SP %08x, APSR, IT",
	      core.r[13]);
	CHECK(!m->handler_mode && m->ipsr == 0U && m->active[16] == 0U, "back in Thread mode");
}

/* Entry from Thread mode on the process stack; a nested exception; returns. */
static void test_process_stack_and_nesting(void)
{
	TargetCoreType core;
	ArmMStateType *m = &core.m;

	init(&core, FALSE, MEM(0xC0U));
	m->control = ARM_M_CONTROL_SPSEL;
	m->sp_other = MEM(0x100U);	/* SP_main */
	CHECK(ArmMExceptionEntry(&core, ARM_M_EXC_SVCALL, PC0 + 2U) == STD_E_OK, "SVCall entry");
	CHECK(core.r[13] == MEM(0x100U) && m->sp_other == MEM(0xA0U) && core.r[14] == 0xFFFFFFFDU
	      && (m->control & ARM_M_CONTROL_SPSEL) == 0U, "the handler on the main stack; the frame on the process stack");
	CHECK(word_at(&core, 0xB8U) == PC0 + 2U, "the return address");
	/* nested: from Handler mode, on the main stack */
	CHECK(ArmMExceptionEntry(&core, 16U, HANDLER(11U) + 4U) == STD_E_OK, "nested entry");
	CHECK(core.r[13] == MEM(0xE0U) && core.r[14] == 0xFFFFFFF1U && m->ipsr == 16U && word_at(&core, 0xFCU) == 0xA100000BU,
	      "nested frame, EXC_RETURN, the stacked xPSR (N C, T, IPSR 11) %08x", word_at(&core, 0xFCU));
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFF1U) == STD_E_OK && m->handler_mode && m->ipsr == 11U && core.r[13] == MEM(0x100U)
	      && core.next_pc == HANDLER(11U) + 4U, "back to the SVCall handler");
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFFDU) == STD_E_OK && !m->handler_mode && core.r[13] == MEM(0xC0U)
	      && m->sp_other == MEM(0x100U) && (m->control & ARM_M_CONTROL_SPSEL) != 0U && core.next_pc == PC0 + 2U,
	      "back to Thread mode on the process stack");
}

static void test_invalid_returns(void)
{
	TargetCoreType core;
	ArmMStateType *m = &core.m;

	/* EXC_RETURN<3:0> = 0101: UsageFault INVPC, LR = '1111':EXC_RETURN<27:0> */
	init(&core, FALSE, MEM(0x100U));
	CHECK(ArmMExceptionEntry(&core, 16U, PC0) == STD_E_OK, "entry");
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFF5U) == STD_E_OK && m->ipsr == 6U && m->active[16] == 0U && m->active[6] == 1U
	      && (m->cfsr & (ARM_M_UFSR_INVPC << 16)) != 0U && core.r[14] == 0xFFFFFFF5U && core.next_pc == HANDLER(6U),
	      "an illegal EXC_RETURN");
	/* returning to Handler mode with one active exception */
	init(&core, FALSE, MEM(0x100U));
	(void)ArmMExceptionEntry(&core, 16U, PC0);
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFF1U) == STD_E_OK && m->ipsr == 6U, "to Handler mode from the only handler");
	/* returning to Thread mode with two active (CCR.NONBASETHRDENA 0), then with it set */
	init(&core, FALSE, MEM(0x100U));
	(void)ArmMExceptionEntry(&core, 16U, PC0);
	m->active[17] = 1U;
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFF9U) == STD_E_OK && m->ipsr == 6U, "to Thread mode with two active");
	init(&core, FALSE, MEM(0x100U));
	(void)ArmMExceptionEntry(&core, 16U, PC0);
	m->active[17] = 1U;
	m->ccr |= ARM_M_CCR_NONBASETHRDENA;
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFF9U) == STD_E_OK && !m->handler_mode && m->ipsr == 0U, "allowed with NONBASETHRDENA");
	/* returning from an exception that is not active */
	init(&core, FALSE, MEM(0x100U));
	(void)ArmMExceptionEntry(&core, 16U, PC0);
	m->active[16] = 0U;
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFF9U) == STD_E_OK && m->ipsr == 6U, "an inactive handler");
	/* EXC_RETURN<27:4> not all ones: UNPREDICTABLE */
	init(&core, FALSE, MEM(0x100U));
	(void)ArmMExceptionEntry(&core, 16U, PC0);
	CHECK(ArmMExceptionReturn(&core, 0xF00000F9U) == STD_E_INVALID, "not all ones");
	/* the stacked IPSR inconsistent with the mode returned to: INVPC after popping */
	init(&core, FALSE, MEM(0x100U));
	(void)ArmMExceptionEntry(&core, 16U, PC0);
	put_word(&core, 0xFCU, 0x01000005U);	/* stacked xPSR with IPSR 5, returning to Thread mode */
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFF9U) == STD_E_OK && m->ipsr == 6U && m->handler_mode && core.r[13] == MEM(0xE0U),
	      "IPSR != 0 in Thread mode: pushed again, UsageFault");
}

/* The floating-point context: stacked at once (LSPEN 0), and lazily (LSPEN 1). */
static void test_fp_context(void)
{
	TargetCoreType core;
	ArmMStateType *m = &core.m;
	uint32 i;

	init(&core, TRUE, MEM(0x100U));
	m->control = ARM_M_CONTROL_FPCA;
	m->fpccr = ARM_M_FPCCR_ASPEN;
	for (i = 0U; i < 32U; i++) {
		core.s[i] = 0x40000000U + i;
	}
	core.fpscr = 0x03C00000U;
	CHECK(ArmMExceptionEntry(&core, 16U, PC0) == STD_E_OK, "entry");
	/* frame of 0x68 at 0x98: S0-S15 at 0xB8, FPSCR at 0xF8 */
	CHECK(core.r[13] == MEM(0x98U) && core.r[14] == 0xFFFFFFE9U && (m->control & ARM_M_CONTROL_FPCA) == 0U, "FP frame");
	CHECK(word_at(&core, 0xB8U) == 0x40000000U && word_at(&core, 0xF4U) == 0x4000000FU && word_at(&core, 0xF8U) == 0x03C00000U,
	      "S0, S15, FPSCR stacked");
	for (i = 0U; i < 32U; i++) {
		core.s[i] = 0U;
	}
	core.fpscr = 0U;
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFE9U) == STD_E_OK && core.s[0] == 0x40000000U && core.s[15] == 0x4000000FU
	      && core.s[16] == 0U && core.fpscr == 0x03C00000U && (m->control & ARM_M_CONTROL_FPCA) != 0U && core.r[13] == MEM(0x100U),
	      "S0-S15 and FPSCR restored");

	/* lazily: the space reserved, LSPACT set, nothing written */
	init(&core, TRUE, MEM(0x100U));
	m->control = ARM_M_CONTROL_FPCA;
	m->fpdscr = 0x01000000U;
	for (i = 0U; i < 16U; i++) {
		core.s[i] = 0x50000000U + i;
	}
	core.fpscr = 0x02000000U;
	CHECK(ArmMExceptionEntry(&core, 16U, PC0) == STD_E_OK, "lazy entry");
	CHECK(m->fpcar == MEM(0xB8U) && m->fpccr == (ARM_M_FPCCR_ASPEN | ARM_M_FPCCR_LSPEN | ARM_M_FPCCR_LSPACT | ARM_M_FPCCR_THREAD
	      | ARM_M_FPCCR_HFRDY) && word_at(&core, 0xB8U) == 0U, "FPCAR, FPCCR %08x, not stacked", m->fpccr);
	/* the handler's first floating-point instruction preserves the context, then starts a new one */
	CHECK(ArmMExecuteFPCheck(&core) == STD_E_OK && word_at(&core, 0xB8U) == 0x50000000U && word_at(&core, 0xF8U) == 0x02000000U
	      && (m->fpccr & ARM_M_FPCCR_LSPACT) == 0U && core.fpscr == 0x01000000U && (m->control & ARM_M_CONTROL_FPCA) != 0U,
	      "preserved; FPSCR = FPDSCR");
	CHECK(ArmMExecuteFPCheck(&core) == STD_E_OK && core.fpscr == 0x01000000U, "once only");
	core.s[0] = 0U;
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFE9U) == STD_E_OK && core.s[0] == 0x50000000U && core.fpscr == 0x02000000U,
	      "restored from the frame");
	/* a handler without floating point: the registers are kept, LSPACT cleared */
	init(&core, TRUE, MEM(0x100U));
	m->control = ARM_M_CONTROL_FPCA;
	core.s[0] = 0x12345678U;
	(void)ArmMExceptionEntry(&core, 16U, PC0);
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFE9U) == STD_E_OK && core.s[0] == 0x12345678U && (m->fpccr & ARM_M_FPCCR_LSPACT) == 0U,
	      "lazy return");
	/* entry without an active floating-point context: the basic frame */
	init(&core, TRUE, MEM(0x100U));
	CHECK(ArmMExceptionEntry(&core, 16U, PC0) == STD_E_OK && core.r[13] == MEM(0xE0U) && core.r[14] == 0xFFFFFFF9U, "no FPCA");
	/* from Handler mode with a context: EXC_RETURN 0xFFFFFFE1 */
	m->control |= ARM_M_CONTROL_FPCA;
	CHECK(ArmMExceptionEntry(&core, 17U, HANDLER(16U)) == STD_E_OK && core.r[14] == 0xFFFFFFE1U, "Handler mode with FPCA");
}

static void test_synchronous(void)
{
	TargetCoreType core;
	ArmMStateType *m = &core.m;

	/* SVC from Thread mode: taken */
	init(&core, FALSE, MEM(0x100U));
	CHECK(ArmMTakeSynchronous(&core, ARM_M_EXC_SVCALL, PC0 + 2U) == STD_E_OK && m->ipsr == 11U, "SVCall");
	/* SVC at the priority of the running handler: HardFault, HFSR.FORCED */
	m->priority[11] = 0x80U;
	m->priority[16] = 0x40U;
	m->active[16] = 1U;
	m->active[11] = 0U;
	m->ipsr = 16U;
	CHECK(ArmMTakeSynchronous(&core, ARM_M_EXC_SVCALL, PC0) == STD_E_OK && m->ipsr == 3U && (m->hfsr & ARM_M_HFSR_FORCED) != 0U,
	      "escalated to HardFault");
	/* a disabled UsageFault escalates too */
	init(&core, FALSE, MEM(0x100U));
	CHECK(ArmMTakeSynchronous(&core, ARM_M_EXC_USAGEFAULT, PC0) == STD_E_OK && m->ipsr == 3U, "disabled UsageFault");
	/* with FAULTMASK (priority -1): lockup */
	init(&core, FALSE, MEM(0x100U));
	m->faultmask = 1U;
	CHECK(ArmMTakeSynchronous(&core, ARM_M_EXC_SVCALL, PC0) == STD_E_EXEC && m->lockup, "lockup");
	/* a fault while stacking stops the entry */
	init(&core, FALSE, MEM(0x10U));
	CHECK(ArmMExceptionEntry(&core, 16U, PC0) == STD_E_SEGV, "stacking fault");
	init(&core, FALSE, MEM(0x100U));
	m->vtor = 0x30000000U;
	CHECK(ArmMExceptionEntry(&core, 16U, PC0) == STD_E_SEGV, "vector fault");
}

static Std_ReturnType mrs(TargetCoreType *core, uint32 d, uint32 sysm)
{
	arm_mrs_input_type in;
	arm_dp_output_type out;

	in.instrName = "MRS";
	in.cond = ARM_COND_AL;
	ARM_SET_REG(core, &in, Rd, d);
	in.SYSm = sysm;
	return arm_pseudo_mrs(core, &in, &out);
}

static Std_ReturnType msr(TargetCoreType *core, uint32 n, uint32 sysm)
{
	arm_msr_input_type in;
	arm_dp_output_type out;

	in.instrName = "MSR";
	in.cond = ARM_COND_AL;
	ARM_SET_REG(core, &in, Rn, n);
	in.SYSm = sysm;
	in.mask = 2U;
	return arm_pseudo_msr(core, &in, &out);
}

static void test_special_registers(void)
{
	TargetCoreType core;
	ArmMStateType *m = &core.m;
	arm_cps_input_type cps;
	arm_svc_input_type svc;

	init(&core, TRUE, MEM(0x100U));
	m->sp_other = MEM(0x80U);
	CHECK(mrs(&core, 0U, 8U) == STD_E_OK && core.r[0] == MEM(0x100U), "MRS MSP");
	CHECK(mrs(&core, 0U, 9U) == STD_E_OK && core.r[0] == MEM(0x80U), "MRS PSP");
	core.r[1] = MEM(0x63U);
	CHECK(msr(&core, 1U, 9U) == STD_E_OK && m->sp_other == MEM(0x60U), "MSR PSP: bits 1:0 cleared");
	core.r[1] = 0xFFFFFFFFU;
	CHECK(msr(&core, 1U, 16U) == STD_E_OK && m->primask == 1U, "MSR PRIMASK");
	CHECK(mrs(&core, 0U, 16U) == STD_E_OK && core.r[0] == 1U, "MRS PRIMASK");
	CHECK(msr(&core, 1U, 17U) == STD_E_OK && m->basepri == 0xF0U, "MSR BASEPRI: 4 priority bits");
	core.r[1] = 0x80U;
	CHECK(msr(&core, 1U, 18U) == STD_E_OK && m->basepri == 0x80U, "BASEPRI_MAX: raises");
	core.r[1] = 0xC0U;
	CHECK(msr(&core, 1U, 18U) == STD_E_OK && m->basepri == 0x80U, "BASEPRI_MAX: never lowers");
	core.r[1] = 0U;
	CHECK(msr(&core, 1U, 18U) == STD_E_OK && m->basepri == 0x80U, "BASEPRI_MAX 0: ignored");
	CHECK(mrs(&core, 0U, 18U) == STD_E_OK && core.r[0] == 0x80U, "MRS BASEPRI_MAX");
	core.r[1] = 1U;
	CHECK(msr(&core, 1U, 19U) == STD_E_OK && m->faultmask == 1U, "MSR FAULTMASK");
	CHECK(mrs(&core, 0U, 19U) == STD_E_OK && core.r[0] == 1U, "MRS FAULTMASK");
	core.r[1] = 0U;
	CHECK(msr(&core, 1U, 19U) == STD_E_OK && m->faultmask == 1U, "FAULTMASK at priority -1: not written");
	m->faultmask = 0U;
	/* CONTROL: SPSEL switches R[13] in Thread mode */
	core.r[1] = ARM_M_CONTROL_SPSEL | ARM_M_CONTROL_FPCA;
	CHECK(msr(&core, 1U, 20U) == STD_E_OK && core.r[13] == MEM(0x60U) && m->sp_other == MEM(0x100U)
	      && m->control == (ARM_M_CONTROL_SPSEL | ARM_M_CONTROL_FPCA), "MSR CONTROL: process stack");
	CHECK(mrs(&core, 0U, 20U) == STD_E_OK && core.r[0] == 6U, "MRS CONTROL");
	CHECK(mrs(&core, 0U, 0U) == STD_E_OK && core.r[0] == (ARM_APSR_N | ARM_APSR_C), "MRS APSR (arm_pseudo_mrs_xpsr)");
	core.r[1] = 0xF0000000U;
	CHECK(msr(&core, 1U, 0U) == STD_E_OK && core.apsr == 0xF0000000U, "MSR APSR");
	/* unprivileged: no stack pointers, no masks, CONTROL kept */
	core.r[1] = ARM_M_CONTROL_NPRIV | ARM_M_CONTROL_SPSEL;
	CHECK(msr(&core, 1U, 20U) == STD_E_OK && !ArmMPrivileged(m), "nPRIV");
	CHECK(mrs(&core, 0U, 8U) == STD_E_OK && core.r[0] == 0U, "unprivileged MRS MSP: 0");
	core.r[1] = 1U;
	CHECK(msr(&core, 1U, 16U) == STD_E_OK && m->primask == 1U, "unprivileged MSR: ignored");
	core.r[1] = 0U;
	CHECK(msr(&core, 1U, 20U) == STD_E_OK && !ArmMPrivileged(m), "unprivileged: CONTROL kept");
	m->control = 0U;
	m->primask = 0U;
	/* in Handler mode CONTROL.SPSEL is not written */
	init(&core, TRUE, MEM(0x100U));
	m->handler_mode = TRUE;
	core.r[1] = ARM_M_CONTROL_SPSEL;
	CHECK(msr(&core, 1U, 20U) == STD_E_OK && m->control == 0U, "Handler mode: SPSEL kept");
	/* CPSID I, CPSIE I, CPSID F (not at priority -1), CPSIE F */
	init(&core, TRUE, MEM(0x100U));
	cps.instrName = "CPS";
	cps.enable = FALSE;
	cps.affectPRI = TRUE;
	cps.affectFAULT = FALSE;
	CHECK(arm_pseudo_cps(&core, &cps) == STD_E_OK && m->primask == 1U, "CPSID I");
	cps.affectPRI = FALSE;
	cps.affectFAULT = TRUE;
	CHECK(arm_pseudo_cps(&core, &cps) == STD_E_OK && m->faultmask == 1U, "CPSID F");
	cps.enable = TRUE;
	cps.affectPRI = TRUE;
	CHECK(arm_pseudo_cps(&core, &cps) == STD_E_OK && m->primask == 0U && m->faultmask == 0U, "CPSIE IF");
	m->control = ARM_M_CONTROL_NPRIV;
	cps.enable = FALSE;
	CHECK(arm_pseudo_cps(&core, &cps) == STD_E_OK && m->primask == 0U, "unprivileged CPSID: ignored");
	m->control = 0U;
	m->active[3] = 1U;	/* HardFault active: priority -1 */
	cps.affectPRI = FALSE;
	CHECK(arm_pseudo_cps(&core, &cps) == STD_E_OK && m->faultmask == 0U, "CPSID F at -1: ignored");
	/* SVC: CallSupervisor() when its condition passes */
	svc.instrName = "SVC";
	svc.cond = ARM_COND_AL;
	svc.imm32 = 5U;
	CHECK(arm_pseudo_svc(&core, &svc) == STD_E_OK && core.supervisor_calls == 1U, "SVC");
	svc.cond = 0x0U;	/* EQ, Z clear */
	CHECK(arm_pseudo_svc(&core, &svc) == STD_E_OK && core.supervisor_calls == 1U, "SVCEQ skipped");
}

static uint32 scs_read(TargetCoreType *core, ArmMSysTickType *st, ArmMScsConfigType *cfg, uint32 offset, uint32 size)
{
	uint32 value = ArmMScsRead(&core->m, st, cfg, offset, size);

	ArmMScsReadSideEffects(st, offset);
	return value;
}

static void test_scs(void)
{
	TargetCoreType core;
	ArmMStateType *m = &core.m;
	ArmMSysTickType st;
	ArmMScsConfigType cfg;
	uint32 i;

	init(&core, TRUE, MEM(0x100U));
	memset(&st, 0, sizeof(st));
	cfg.cpuid = 0x411FC272U;
	cfg.ccr_writable = 0x0000031BU;
	cfg.reset_requested = FALSE;
	CHECK(scs_read(&core, &st, &cfg, 0xD00U, 4U) == 0x411FC272U && scs_read(&core, &st, &cfg, 0x004U, 4U) == 0U, "CPUID, ICTR");
	/* NVIC: enable, pend, priority (4 bits), active */
	ArmMScsWrite(&core, m, &st, &cfg, 0x100U, 4U, 0x00000003U);
	CHECK(m->enabled[16] == 1U && m->enabled[17] == 1U && scs_read(&core, &st, &cfg, 0x180U, 4U) == 3U, "ISER, read by ICER");
	ArmMScsWrite(&core, m, &st, &cfg, 0x180U, 4U, 0x00000001U);
	CHECK(m->enabled[16] == 0U && m->enabled[17] == 1U, "ICER");
	ArmMScsWrite(&core, m, &st, &cfg, 0x200U, 4U, 0xFFFF0002U);
	CHECK(m->pending[17] == 1U && m->pending[16] == 0U && scs_read(&core, &st, &cfg, 0x280U, 4U) == 2U, "ISPR (16 interrupts)");
	ArmMScsWrite(&core, m, &st, &cfg, 0x280U, 4U, 2U);
	CHECK(m->pending[17] == 0U, "ICPR");
	ArmMScsWrite(&core, m, &st, &cfg, 0x401U, 1U, 0xFFU);
	CHECK(m->priority[17] == 0xF0U && scs_read(&core, &st, &cfg, 0x400U, 4U) == 0x0000F000U && scs_read(&core, &st, &cfg, 0x401U, 1U) == 0xF0U,
	      "IPR byte write, 4 bits");
	ArmMScsWrite(&core, m, &st, &cfg, 0xF00U, 4U, 1U);
	CHECK(m->pending[17] == 1U, "STIR");
	ArmMScsWrite(&core, m, &st, &cfg, 0xF00U, 4U, 40U);
	CHECK(m->pending[16 + 40 - 24] == 0U, "STIR beyond the interrupts: ignored");
	m->active[16] = 1U;
	CHECK(scs_read(&core, &st, &cfg, 0x300U, 4U) == 1U, "IABR");
	/* ICSR */
	m->ipsr = 16U;
	CHECK(scs_read(&core, &st, &cfg, 0xD04U, 4U) == (16U | (17U << 12) | ARM_M_ICSR_ISRPENDING | ARM_M_ICSR_RETTOBASE),
	      "ICSR %08x", scs_read(&core, &st, &cfg, 0xD04U, 4U));
	ArmMScsWrite(&core, m, &st, &cfg, 0xD04U, 4U, ARM_M_ICSR_PENDSVSET | ARM_M_ICSR_PENDSTSET | ARM_M_ICSR_NMIPENDSET);
	CHECK(m->pending[14] == 1U && m->pending[15] == 1U && m->pending[2] == 1U
	      && (scs_read(&core, &st, &cfg, 0xD04U, 4U) & 0x9C000000U) == 0x94000000U, "ICSR set: NMI, PendSV, SysTick");
	CHECK(((scs_read(&core, &st, &cfg, 0xD04U, 4U) >> 12) & 0x1FFU) == 2U, "VECTPENDING: NMI");
	ArmMScsWrite(&core, m, &st, &cfg, 0xD04U, 4U, ARM_M_ICSR_PENDSVCLR | ARM_M_ICSR_PENDSTCLR);
	CHECK(m->pending[14] == 0U && m->pending[15] == 0U, "ICSR clear");
	m->active[17] = 1U;
	CHECK((scs_read(&core, &st, &cfg, 0xD04U, 4U) & ARM_M_ICSR_RETTOBASE) == 0U, "RETTOBASE with two active");
	/* VTOR, AIRCR (with the key only), SCR, CCR */
	ArmMScsWrite(&core, m, &st, &cfg, 0xD08U, 4U, 0x08008004U);
	CHECK(m->vtor == 0x08008000U, "VTOR");
	ArmMScsWrite(&core, m, &st, &cfg, 0xD0CU, 4U, 0x00000500U);
	CHECK(m->aircr_prigroup == 0U && scs_read(&core, &st, &cfg, 0xD0CU, 4U) == 0xFA050000U, "AIRCR without the key");
	ArmMScsWrite(&core, m, &st, &cfg, 0xD0CU, 4U, 0x05FA0504U);
	CHECK(m->aircr_prigroup == 5U && cfg.reset_requested && scs_read(&core, &st, &cfg, 0xD0CU, 4U) == 0xFA050500U, "AIRCR");
	ArmMScsWrite(&core, m, &st, &cfg, 0xD10U, 4U, 0xFFFFFFFFU);
	CHECK(m->scr == 0x16U, "SCR");
	ArmMScsWrite(&core, m, &st, &cfg, 0xD14U, 4U, 0xFFFFFFFFU);
	CHECK(m->ccr == 0x31BU, "CCR");
	/* SHPR: SVCall, PendSV, SysTick; exception 7 is reserved */
	ArmMScsWrite(&core, m, &st, &cfg, 0xD1FU, 1U, 0x80U);
	ArmMScsWrite(&core, m, &st, &cfg, 0xD20U, 4U, 0xE0D0FFFFU);
	CHECK(m->priority[11] == 0x80U && m->priority[14] == 0xD0U && m->priority[15] == 0xE0U
	      && scs_read(&core, &st, &cfg, 0xD1CU, 4U) == 0x80000000U && scs_read(&core, &st, &cfg, 0xD20U, 4U) == 0xE0D000F0U, "SHPR2, SHPR3 (byte 0: DebugMonitor)");
	ArmMScsWrite(&core, m, &st, &cfg, 0xD18U, 4U, 0x40302010U);
	CHECK(m->priority[4] == 0x10U && m->priority[6] == 0x30U && m->priority[7] == 0U && scs_read(&core, &st, &cfg, 0xD18U, 4U) == 0x00302010U,
	      "SHPR1");
	/* SHCSR: enables, active and pending bits */
	ArmMScsWrite(&core, m, &st, &cfg, 0xD24U, 4U, (1U << 18) | (1U << 16) | (1U << 15) | (1U << 7));
	CHECK(m->enabled[6] == 1U && m->enabled[4] == 1U && m->pending[11] == 1U && m->active[11] == 1U
	      && scs_read(&core, &st, &cfg, 0xD24U, 4U) == ((1U << 18) | (1U << 16) | (1U << 15) | (1U << 7)), "SHCSR");
	ArmMScsWrite(&core, m, &st, &cfg, 0xD26U, 1U, 0x02U);	/* byte 2: BUSFAULTENA only */
	CHECK(m->enabled[5] == 1U && m->enabled[6] == 0U && m->enabled[4] == 0U && m->active[11] == 1U, "SHCSR byte write");
	m->pending[6] = 1U;
	m->pending[4] = 1U;
	m->pending[5] = 1U;
	m->active[4] = 1U;
	m->active[5] = 1U;
	m->active[6] = 1U;
	CHECK((scs_read(&core, &st, &cfg, 0xD24U, 4U) & 0x700BU) == 0x700BU, "SHCSR: the fault bits");
	/* CFSR, HFSR: write 1 to clear */
	m->cfsr = 0x00040001U;
	m->hfsr = ARM_M_HFSR_FORCED;
	ArmMScsWrite(&core, m, &st, &cfg, 0xD2AU, 2U, 0x0004U);
	ArmMScsWrite(&core, m, &st, &cfg, 0xD2CU, 4U, ARM_M_HFSR_FORCED);
	CHECK(m->cfsr == 1U && m->hfsr == 0U && scs_read(&core, &st, &cfg, 0xD28U, 4U) == 1U && scs_read(&core, &st, &cfg, 0xD2CU, 4U) == 0U,
	      "CFSR (UFSR halfword), HFSR");
	/* CPACR, FPCCR, FPCAR, FPDSCR */
	ArmMScsWrite(&core, m, &st, &cfg, 0xD88U, 4U, 0xFFFFFFFFU);
	ArmMScsWrite(&core, m, &st, &cfg, 0xF34U, 4U, 0U);
	ArmMScsWrite(&core, m, &st, &cfg, 0xF38U, 4U, 0x20000047U);
	ArmMScsWrite(&core, m, &st, &cfg, 0xF3CU, 4U, 0xFFFFFFFFU);
	CHECK(scs_read(&core, &st, &cfg, 0xD88U, 4U) == 0x00F00000U && m->fpccr == 0U && m->fpcar == 0x20000040U
	      && scs_read(&core, &st, &cfg, 0xF3CU, 4U) == 0x07C00000U && scs_read(&core, &st, &cfg, 0xF34U, 4U) == 0U
	      && scs_read(&core, &st, &cfg, 0xF38U, 4U) == 0x20000040U, "CPACR, FPCCR, FPCAR, FPDSCR");
	CHECK(scs_read(&core, &st, &cfg, 0xE00U, 4U) == 0U, "reserved: 0");
	/* without the FP extension the FP registers are not written */
	m->have_fp = FALSE;
	ArmMScsWrite(&core, m, &st, &cfg, 0xF34U, 4U, ARM_M_FPCCR_ASPEN);
	ArmMScsWrite(&core, m, &st, &cfg, 0xF38U, 4U, 0x20000080U);
	ArmMScsWrite(&core, m, &st, &cfg, 0xF3CU, 4U, 0x01000000U);
	CHECK(m->fpccr == 0U && m->fpcar == 0x20000040U && m->fpdscr == 0x07C00000U, "no FP extension");
	/* SysTick: RVR 2, enabled with TICKINT: 0 -> reload 2 -> 1 -> 0 (COUNTFLAG, pending) -> reload */
	m->pending[15] = 0U;
	ArmMScsWrite(&core, m, &st, &cfg, 0x014U, 4U, 0xFF000002U);
	ArmMScsWrite(&core, m, &st, &cfg, 0x018U, 4U, 0x1234U);
	CHECK(st.rvr == 2U && st.cvr == 0U, "RVR (24 bits), CVR cleared");
	ArmMSysTickClock(m, &st, 3U);
	CHECK(st.cvr == 0U && m->pending[15] == 0U && (st.csr & ARM_M_SYST_COUNTFLAG) == 0U, "disabled: no count");
	ArmMScsWrite(&core, m, &st, &cfg, 0x010U, 4U, ARM_M_SYST_ENABLE | ARM_M_SYST_TICKINT | ARM_M_SYST_CLKSOURCE | ARM_M_SYST_COUNTFLAG);
	CHECK(st.csr == 0x7U, "CSR: COUNTFLAG not written");
	ArmMSysTickClock(m, &st, 1U);
	CHECK(st.cvr == 2U, "reload");
	ArmMSysTickClock(m, &st, 1U);
	CHECK(st.cvr == 1U && m->pending[15] == 0U, "count");
	ArmMSysTickClock(m, &st, 1U);
	CHECK(st.cvr == 0U && m->pending[15] == 1U && (st.csr & ARM_M_SYST_COUNTFLAG) != 0U, "zero: COUNTFLAG, SysTick pending");
	CHECK(scs_read(&core, &st, &cfg, 0x010U, 4U) == 0x10007U && scs_read(&core, &st, &cfg, 0x010U, 4U) == 0x7U, "COUNTFLAG cleared by reading");
	ArmMSysTickClock(m, &st, 1U);
	CHECK(st.cvr == 2U && scs_read(&core, &st, &cfg, 0x018U, 4U) == 2U && scs_read(&core, &st, &cfg, 0x014U, 4U) == 2U, "reload again");
	st.calib = 0x40000000U;
	CHECK(scs_read(&core, &st, &cfg, 0x01CU, 4U) == 0x40000000U, "CALIB");
	/* many ticks at once: as one at a time */
	{
		ArmMSysTickType a;
		ArmMSysTickType b;
		ArmMStateType ma;
		ArmMStateType mb;
		uint32 n;
		uint32 k;

		for (n = 0U; n < 40U; n++) {
			for (k = 0U; k < 4U; k++) {
				a.csr = ARM_M_SYST_ENABLE | ARM_M_SYST_TICKINT;
				a.rvr = k * 3U;
				a.cvr = (k == 0U) ? 0U : (k + 1U);
				a.calib = 0U;
				b = a;
				ma = core.m;
				ma.pending[15] = 0U;
				mb = ma;
				ArmMSysTickClock(&ma, &a, n);
				for (i = 0U; i < n; i++) {
					ArmMSysTickClock(&mb, &b, 1U);
				}
				CHECK(a.cvr == b.cvr && a.csr == b.csr && ma.pending[15] == mb.pending[15],
				      "%u ticks, RVR %u: CVR %u/%u", n, a.rvr, a.cvr, b.cvr);
			}
		}
		a.csr = ARM_M_SYST_ENABLE | ARM_M_SYST_TICKINT;
		a.rvr = 5U;
		a.cvr = 3U;
		CHECK(ArmMSysTickTicksToInterrupt(&a) == 3U, "3 ticks to the interrupt");
		a.cvr = 0U;
		CHECK(ArmMSysTickTicksToInterrupt(&a) == 6U, "a reload and 5");
		a.rvr = 0U;
		CHECK(ArmMSysTickTicksToInterrupt(&a) == 0U, "RVR 0: never");
		a.csr = ARM_M_SYST_ENABLE;
		CHECK(ArmMSysTickTicksToInterrupt(&a) == 0U, "no TICKINT: never");
	}
	/* without TICKINT: COUNTFLAG only */
	m->pending[15] = 0U;
	ArmMScsWrite(&core, m, &st, &cfg, 0x010U, 1U, ARM_M_SYST_ENABLE);
	for (i = 0U; i < 2U; i++) {
		ArmMSysTickClock(m, &st, 1U);
	}
	CHECK(st.cvr == 0U && m->pending[15] == 0U && (st.csr & ARM_M_SYST_COUNTFLAG) != 0U, "no TICKINT");
}

/* Faults while stacking or unstacking, and the remaining reads and writes. */
static void test_edges(void)
{
	TargetCoreType core;
	ArmMStateType *m = &core.m;
	ArmMSysTickType st;
	ArmMScsConfigType cfg;
	uint32 i;

	/* the FP part of a frame beyond memory: SP at 0x20000020, frame from 0x1FFFFFB8 */
	init(&core, TRUE, MEM(0x20U));
	m->control = ARM_M_CONTROL_FPCA;
	m->fpccr = 0U;
	CHECK(ArmMExceptionEntry(&core, 16U, PC0) == STD_E_SEGV, "FP frame fault");
	/* S registers fit, FPSCR does not: frame at MEM - 0x68 + ... */
	init(&core, TRUE, MEM(0x100U) + 0x60U - 0x60U);
	m->control = ARM_M_CONTROL_FPCA;
	m->fpccr = 0U;
	core.r[13] = MEM(0x100U) + 0x68U;	/* frame at 0x100: its first words beyond memory */
	CHECK(ArmMExceptionEntry(&core, 16U, PC0) == STD_E_SEGV, "frame beyond memory");
	core.r[13] = MEM(0xE0U) + 0x68U;	/* frame at 0xE0: the basic frame fits, S0 at 0x100 does not */
	CHECK(ArmMExceptionEntry(&core, 16U, PC0) == STD_E_SEGV, "S registers beyond memory");
	core.r[13] = MEM(0xA0U) + 0x68U;	/* frame at 0xA0: S registers at 0xC0-0xFF, FPSCR at 0x100 */
	CHECK(ArmMExceptionEntry(&core, 16U, PC0) == STD_E_SEGV, "FPSCR beyond memory");
	/* lazy preservation to FPCAR beyond memory: S registers, then FPSCR */
	init(&core, TRUE, MEM(0x100U));
	m->fpccr = ARM_M_FPCCR_LSPACT;
	m->fpcar = MEM(0xF8U);
	CHECK(ArmMExecuteFPCheck(&core) == STD_E_SEGV, "preserve: S registers");
	m->fpcar = MEM(0xC0U);
	CHECK(ArmMExecuteFPCheck(&core) == STD_E_SEGV, "preserve: FPSCR");
	/* unprivileged, with MemManage and BusFault enabled at 0: FPCCR.USER, MMRDY, BFRDY */
	init(&core, TRUE, MEM(0x100U));
	m->control = ARM_M_CONTROL_FPCA | ARM_M_CONTROL_NPRIV;
	m->enabled[4] = 1U;
	m->enabled[5] = 1U;
	(void)ArmMExceptionEntry(&core, 16U, PC0);
	CHECK(m->fpccr == (ARM_M_FPCCR_ASPEN | ARM_M_FPCCR_LSPEN | ARM_M_FPCCR_LSPACT | ARM_M_FPCCR_USER | ARM_M_FPCCR_THREAD
	      | ARM_M_FPCCR_HFRDY | ARM_M_FPCCR_MMRDY | ARM_M_FPCCR_BFRDY), "FPCCR %08x", m->fpccr);
	/* returns whose frame is beyond memory: the basic part, S registers, FPSCR */
	init(&core, TRUE, MEM(0x100U));
	(void)ArmMExceptionEntry(&core, 16U, PC0);
	core.r[13] = MEM(0xF0U);
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFF9U) == STD_E_SEGV, "pop fault");
	init(&core, TRUE, MEM(0x100U));
	(void)ArmMExceptionEntry(&core, 16U, PC0);
	core.r[13] = MEM(0xD0U);
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFE9U) == STD_E_SEGV, "pop fault: S registers");
	init(&core, TRUE, MEM(0x100U));
	(void)ArmMExceptionEntry(&core, 16U, PC0);
	core.r[13] = MEM(0xA0U);	/* S0-S15 at 0xC0-0xFF, FPSCR at 0x100 */
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFE9U) == STD_E_SEGV, "pop fault: FPSCR");
	/* an inconsistent IPSR whose new frame does not fit */
	init(&core, FALSE, MEM(0x20U));
	(void)ArmMExceptionEntry(&core, 16U, PC0);
	put_word(&core, 0x1CU, 0x01000005U);
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFF9U) == STD_E_OK && core.r[13] == MEM(0x00U), "IPSR inconsistent: pushed again");
	init(&core, FALSE, MEM(0x20U));
	(void)ArmMExceptionEntry(&core, 16U, PC0);
	put_word(&core, 0x1CU, 0x01000005U);
	put_word(&core, 0x18U, PC0);
	m->vtor = 0x30000000U;
	CHECK(ArmMExceptionReturn(&core, 0xFFFFFFF9U) == STD_E_SEGV, "IPSR inconsistent: vector fault");
	/* the exception list, Reset's priority, SP_main written on the process stack */
	init(&core, FALSE, MEM(0x100U));
	CHECK(ArmMExceptionExists(m, 2U) && ArmMExceptionExists(m, 3U) && !ArmMExceptionExists(m, 7U) && ArmMExceptionPriority(m, 1U) == -3,
	      "exceptions");
	m->control = ARM_M_CONTROL_SPSEL;
	ArmMSPMainWrite(&core, MEM(0x40U));
	CHECK(m->sp_other == MEM(0x40U), "SP_main while the process stack is selected");
	/* MRS BASEPRI, a failing condition, MSR MSP */
	init(&core, FALSE, MEM(0x100U));
	m->basepri = 0x40U;
	CHECK(mrs(&core, 0U, 17U) == STD_E_OK && core.r[0] == 0x40U, "MRS BASEPRI");
	core.apsr = ARM_APSR_Z;
	{
		arm_mrs_input_type in;
		arm_dp_output_type out;

		in.instrName = "MRS";
		in.cond = 0x1U;
		ARM_SET_REG(&core, &in, Rd, 0U);
		in.SYSm = 16U;
		core.r[0] = 0x55U;
		CHECK(arm_pseudo_mrs(&core, &in, &out) == STD_E_OK && core.r[0] == 0x55U, "MRSNE skipped");
	}
	core.r[1] = MEM(0x83U);
	CHECK(msr(&core, 1U, 8U) == STD_E_OK && core.r[13] == MEM(0x80U), "MSR MSP");
	/* the remaining SCS reads, and a write to a reserved word */
	memset(&st, 0, sizeof(st));
	cfg.cpuid = 0U;
	cfg.ccr_writable = 0x31BU;
	cfg.reset_requested = FALSE;
	m->enabled[16] = 1U;
	m->pending[17] = 1U;
	m->vtor = 0x08000000U;
	m->scr = 0x2U;
	CHECK(ArmMScsRead(m, &st, &cfg, 0x100U, 4U) == 1U && ArmMScsRead(m, &st, &cfg, 0x200U, 4U) == 2U
	      && ArmMScsRead(m, &st, &cfg, 0xD08U, 4U) == 0x08000000U && ArmMScsRead(m, &st, &cfg, 0xD10U, 4U) == 2U
	      && ArmMScsRead(m, &st, &cfg, 0xD14U, 4U) == ARM_M_CCR_STKALIGN && ArmMScsRead(m, &st, &cfg, 0xD0EU, 2U) == 0xFA05U,
	      "ISER, ISPR, VTOR, SCR, CCR, AIRCR halfword");
	for (i = 0U; i < 4U; i++) {
		ArmMScsWrite(&core, m, &st, &cfg, 0xE00U + (4U * i), 4U, 0xFFFFFFFFU);
	}
	CHECK(ArmMScsRead(m, &st, &cfg, 0xE00U, 4U) == 0U, "reserved: WI");
}

/* The summary: set by a pend, cleared when nothing is pending, kept while one is masked. */
static void test_pending_summary(void)
{
	TargetCoreType core;
	ArmMStateType *m = &core.m;

	init(&core, FALSE, MEM(0x100U));
	CHECK(!m->pending_any && ArmMPendingToTakeFast(m) == 0U, "nothing pending");
	m->enabled[16] = 1U;
	ArmMSetPending(m, 16U);
	CHECK(m->pending_any && ArmMPendingToTakeFast(m) == 16U, "pended");
	m->primask = 1U;
	CHECK(ArmMPendingToTakeFast(m) == 0U && m->pending_any, "masked: kept");
	m->pending[16] = 0U;
	CHECK(ArmMPendingToTakeFast(m) == 0U && !m->pending_any, "none: cleared");
}

int main(void)
{
	test_priorities();
	test_entry_return_main();
	test_process_stack_and_nesting();
	test_invalid_returns();
	test_fp_context();
	test_synchronous();
	test_special_registers();
	test_scs();
	test_edges();
	test_pending_summary();
	printf("%d checks, %d failures\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
