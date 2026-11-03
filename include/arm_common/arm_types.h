/*
 * Types of Arm's pseudo-code shared by Athrill's Arm targets.
 *
 * The APSR flags (N, Z, C, V, Q and GE) have the same bit positions in the A
 * profile's CPSR and the M profile's xPSR.
 */
#ifndef ARM_COMMON_ARM_TYPES_H
#define ARM_COMMON_ARM_TYPES_H

#include "std_types.h"

#define ARM_APSR_BITPOS_N	31U
#define ARM_APSR_BITPOS_Z	30U
#define ARM_APSR_BITPOS_C	29U
#define ARM_APSR_BITPOS_V	28U
#define ARM_APSR_BITPOS_Q	27U
#define ARM_APSR_BITPOS_GE	16U

#define ARM_APSR_N		(1U << ARM_APSR_BITPOS_N)
#define ARM_APSR_Z		(1U << ARM_APSR_BITPOS_Z)
#define ARM_APSR_C		(1U << ARM_APSR_BITPOS_C)
#define ARM_APSR_V		(1U << ARM_APSR_BITPOS_V)
#define ARM_APSR_Q		(1U << ARM_APSR_BITPOS_Q)
#define ARM_APSR_GE		(0xFU << ARM_APSR_BITPOS_GE)

/* Condition field value meaning "always" (AL). */
#define ARM_COND_AL		0xEU

/* Shift types of the pseudo-code. */
typedef enum {
	SRType_LSL = 0,
	SRType_LSR,
	SRType_ASR,
	SRType_ROR,
	SRType_RRX,
} SRType;

/* Condition flags an instruction computes. */
typedef struct {
	bool negative;
	bool zero;
	bool carry;
	bool overflow;
} PseudoCodeStatusFlagType;

/* A register operand of an instruction: its name in the encoding, number and value. */
typedef struct {
	const char *name;
	uint32 regId;
	uint32 regData;
} PseudoCodeRegisterDataType;

typedef uint32 PseudoCodeImmediateData32Type;	/* imm32 */
typedef uint32 PseudoCodeConditionDataType;	/* cond */
typedef bool PseudoCodeUpdateStatusFlagType;	/* setflags */
typedef SRType PseudoCodeSRType;		/* shift_t */
typedef uint32 PseudoCodeShiftNType;		/* shift_n */

#endif /* ARM_COMMON_ARM_TYPES_H */
