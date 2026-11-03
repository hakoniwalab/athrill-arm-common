# Design

athrill-arm-common holds what Athrill's Arm targets share: the meaning of
each instruction as Arm's pseudo-code gives it, the pseudo-code's helper
functions, and the decoder generator. A target is then a combination: its
decoders, the glue from each encoding to the pseudo-code, a profile (A or M)
and its SoC's devices.

## Where it comes from

The structure is the one of
[athrill-target-ARMv7-A](https://github.com/toppers/athrill-target-ARMv7-A),
which splits an instruction into three layers:

1. the decoder, one entry per encoding (ADD immediate A1, ...);
2. the encoding glue, which turns the encoding's fields into the
   pseudo-code's inputs (ARMExpandImm, DecodeImmShift, register reads);
3. the pseudo-code function, the instruction's meaning, with its inputs and
   outputs in structures that the trace output also prints; and the helper
   library under it (AddWithCarry, Shift, ConditionPassed, MemA/MemU, ...).

Layer 3 does not depend on the encoding: ADD immediate means the same in A32
(A1) and in T32 (T3, T4). It is the common part. Code taken from
athrill-target-ARMv7-A is used under this repository's MIT licence with the
permission of its author.

## What is common

| Part | Common | Per target |
|---|---|---|
| Decoder generator (AARCHMRS) | the generator and its Armv7 rules | the generated decoders, the profile's rules (for example Cortex-M7) |
| Encoding glue | ExpandImm, DecodeImmShift and other decode helpers | the glue functions, one per encoding |
| Instruction meaning | pseudo-code functions with input/output structures | |
| Helpers | shifts, AddWithCarry, saturation, bit counts, condition test, flag update | |
| Floating point (VFP) | the VFP pseudo-code | which instructions the FPU has (FPv5 adds VSEL, VRINT, ...) |
| Profile | the profile interface (below) | its implementation: registers, PC, exceptions, memory |
| Devices | | interrupt controller (GIC, NVIC), SoC peripherals |

## Profile interface

The pseudo-code reaches the processor only through these operations, which
each target defines (static inline, so the compiler folds them into the
pseudo-code). Everything that differs between the A and M profiles, or
between the ARM and Thumb states, is behind them.

| Operation | A profile (ARMv7-A) | M profile (Cortex-M) |
|---|---|---|
| Read and write R0-R14 | banked by mode | SP is MSP or PSP |
| APSR flags (N Z C V Q, GE) | in CPSR | in xPSR; same bit positions |
| Address of this instruction, its length | length 4 (ARM) or 2/4 (Thumb) | length 2 or 4 |
| Value read from PC | this + 8 (ARM), this + 4 (Thumb) | this + 4 |
| BranchWritePC | aligns by the state | clears bit 0 |
| BXWritePC, LoadWritePC | selects ARM or Thumb by bit 0 | bit 0 = 0 is a UsageFault (INVSTATE); EXC_RETURN in handler mode returns from the exception |
| ALUWritePC | BXWritePC in ARM, BranchWritePC in Thumb | BranchWritePC |
| Condition of this instruction | the cond field (ARM), the IT state (Thumb) | the IT state, or the cond field of B |
| MemA, MemU (with privilege) | MMU or MPU, alignment rules | MPU, alignment rules |
| Undefined, UNPREDICTABLE, faults | Undefined exception | UsageFault (UNDEFINSTR, NOCP, INVSTATE) |
| Status registers by instruction (MRS, MSR, CPS) | CPSR, SPSR, modes | SYSm registers, PRIMASK, BASEPRI, CONTROL |
| Exception entry and return | modes, vectors | stacking, NVIC, EXC_RETURN |

In C (`include/arm_common/arm_profile.h` lists the signatures): `ArmR`,
`ArmRWrite`, `ArmPC`, `ArmAPSR`, `ArmAPSRWrite`, `ArmBranchWritePC`,
`ArmBXWritePC`, `ArmLoadWritePC`, `ArmALUWritePC`, `ArmMemU_R/W` and
`ArmMemA_R/W`, defined by the target's `arm_target_profile.h`. A write to PC
sets the address of the next instruction; without one the target goes on to
the following instruction. An operation returns an error for a fault, and the
pseudo-code stops and returns it; the pseudo-code itself returns
`STD_E_INVALID` for an UNPREDICTABLE case it detects. The condition is not an
operation: the encoding glue puts it in the input structure (the cond field,
or the IT state in Thumb state), and the pseudo-code tests it with
`ConditionHolds` on the APSR.

The last two rows are not pseudo-code the profiles share: each profile has
its own instructions for them (MRS/MSR differ in encoding and meaning) and
implements them in the target.

## Layout

```
include/arm_common/
  arm_types.h          register, flag and shift types of the pseudo-code
  arm_helpers.h        pure helpers (shifts, AddWithCarry, ExpandImm, ...)
  arm_profile.h        the profile interface: what a target must define
  arm_pseudo_*.h       pseudo-code functions, by group (arithmetic, load, ...)
tools/                 decoder generator (AARCHMRS), cross-check tools
test/                  helper and pseudo-code tests
test/profile/          the test profile (A profile in ARM state, or M profile)
```

A target finds this repository next to its checkout (`../athrill-arm-common`),
as it finds the Athrill core, and includes `arm_profile.h` after defining its
`TargetCore`.

## Rules

- The pseudo-code functions follow Arm's pseudo-code line by line, with the
  pseudo-code in a comment above the C, as athrill-target-ARMv7-A does. One
  statement per line.
- A pseudo-code function takes the core, its input structure and its output
  structure. The trace prints both; with trace off they cost nothing, since
  the functions are inline.
- A behaviour that only one profile has does not go into a common function
  as a branch on the profile; it goes behind the profile interface.
- Instruction tests check results against values worked out from Arm's
  pseudo-code, not against the implementation's own output.

## Helpers

`arm_helpers.h` has the pseudo-code's helpers on 32-bit values: the shifts
(LSL_C ... RRX, Shift_C, DecodeImmShift, DecodeRegShift), the modified
immediates (ARMExpandImm_C, ThumbExpandImm_C), AddWithCarry, SignedSatQ and
UnsignedSatQ, the bit functions (SignExtend, ZeroExtend, BitCount,
LowestSetBit, HighestSetBit, CountLeadingZeroBits, Align), ConditionHolds and
the APSR flag update. They read no processor state: the operands and the APSR
are passed in, so both profiles use them as they are. The width N of the
pseudo-code is always 32 and is not a parameter.

They were rewritten from the pseudo-code rather than copied from
athrill-target-ARMv7-A, whose versions differ from the pseudo-code in some
cases (found while writing the tests below):

| Helper | athrill-target-ARMv7-A | Pseudo-code |
|---|---|---|
| LSL_C carry | x<31> | x<32-shift> |
| LSR_C, ASR_C carry | x<0> | x<shift-1> |
| ROR_C by 32 or more | asserts shift < 32 | any amount (register-shifted forms give up to 255) |
| CountLeadingZeroBits | trailing zeros | leading zeros |
| OpSignExtend | sets one bit | sign-extends |

`test/test_helpers.c` checks them against a second implementation of the
pseudo-code written on bitstrings (`x : Zeros(shift)`, `extended_x<shift-1>`)
for every shift amount from 1 to 255, and against values worked out by hand
(conditions for all NZCV values, expanded immediates, saturation, flags).

## Decoder tools

`tools/` generates a target's T32 decoders from Arm's machine-readable
architecture data (AARCHMRS) and cross-checks them against GNU objdump and
llvm-objdump:

| File | What it does |
|---|---|
| `aarchmrs.py` | reads the data: encodings, fields, decode conditions (in Python or as C), the licence notice of derived files |
| `rules_<profile>.py` | the profile's rules: what it does not have, what it executes as another instruction, extra fixed bits |
| `generate_t32_decoder16.py` | a two-level table for every 16-bit encoding |
| `generate_t32_decoder32.py` | C code from the 32-bit tree, one function per group |
| `crosscheck_t32_decoder16.py`, `crosscheck_t32_decoder32.py` | the comparison with the disassemblers, every difference put in an explained class |

A target runs them with its rules and its output directory, for example
`generate_t32_decoder32.py armv7m Instructions.json <package> src/cpu/decoder`,
and keeps the generated files. `rules_armv7m.py` is Cortex-M7's (Armv7E-M,
FPv5-D16); athrill-target-stm32's `docs/decoder.md` explains each rule and
has the cross-check results. A32 (ARMv7-A) is in the same data and would take
an A32 generator and an A-profile rules module.

## First steps

1. Helpers and types from athrill-target-ARMv7-A, made independent of the
   A profile (no CPSR modes, no instruction-set state inside them), with
   tests.
2. Three instructions through the STM32 target: ADD (immediate), LDR
   (immediate) and B, each in its 16- and 32-bit encodings. Done:
   `arm_pseudo_arith.h`, `arm_pseudo_load.h` and `arm_pseudo_branch.h`, tested
   here through a test profile that behaves as the A profile (ARM state) or
   the M profile (`test/profile`, `test/test_pseudo.c`), and in
   athrill-target-stm32 by a bare-metal program run on Athrill.
3. The decoder generator moves here from athrill-target-stm32. Done: the
   regenerated decoders are identical to the ones before the move (but for
   the "Generated by" line) and the cross-checks give the same results.
4. athrill-target-ARMv7-A on the common part, when its maintainers decide to
   take it in.
