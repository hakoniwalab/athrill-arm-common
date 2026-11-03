"""Decoder rules for Armv7-M, as Cortex-M7 implements it (Armv7E-M with the
FPv5 floating-point unit, 16 double-precision registers). See
athrill-target-stm32's docs/decoder.md for why each rule is there.

A rules module gives, for the A-profile data:
  PROFILE            the profile's name, for messages and generated comments
  NOT_IN_16          16-bit instructions the profile does not have (undefined)
  HINTS_AS_NOP_16    whether unallocated 16-bit hints (0xBFx0) execute as NOP
  NOT_IN_GROUPS_32   32-bit groups the profile does not have (undefined)
  not_in_32(name)    whether a 32-bit instruction is not in the profile
  EXECUTED_AS_32     32-bit instructions the profile executes as another one
  FIXED_32           32-bit instructions with more fixed bits (mask, match)
"""

PROFILE = "Armv7-M"

# A-profile instructions that Armv7-M does not have.
NOT_IN_16 = {"HLT_T1", "SETEND_T1", "SETPAN_T1"}

# Armv7-M: the hint space (0xBFx0) not allocated to a v7-M hint is
# "NOP-compatible" and executes as NOP (SEVL is an Armv8 hint).
HINTS_AS_NOP_16 = True

# Groups Armv7-M does not have: Advanced SIMD (NEON) and its extensions,
# Armv8 load-acquire/store-release, and A-profile system instructions
# (32-bit CPS, BXJ, ERET, DCPS, BLX immediate).
NOT_IN_GROUPS_32 = {
    "simddp", "vldst", "simd_3sameext", "tfloatdpmac", "tsimd_dotprod",
    "ldastl", "cps", "bx_jaz", "eret", "dcps", "blx",
}

# Instructions Armv7-M does not have, in groups it does have.
NOT_IN_32 = {
    "STREXD_T1", "LDREXD_T1",
    "SRS_T1_AS", "SRS_T2_AS", "RFE_T1_AS", "RFE_T2_AS", "HVC_T1", "SMC_T1_AS", "SB_T1",
    "CRC32B_T1", "CRC32H_T1", "CRC32W_T1", "CRC32CB_T1", "CRC32CH_T1", "CRC32CW_T1",
    "PLDW_r_T1", "PLDW_i_T1", "PLDW_i_T2",
    "FSTMDBX_T1", "FSTMIAX_T1", "FLDMDBX_T1", "FLDMIAX_T1", "VDUP_r_T1_Q", "VDUP_r_T1_D",
    "VJCVT_T1", "VCVTB_T1_bfs", "VCVTT_T1_bfs", "VMOV_toh_T1", "VMOV_h_T1", "VMOVX_T1", "VINS_T1",
}


# Extra fixed bits in Armv7-M: VMOV between a core register and a scalar
# moves only a 32-bit word (U = 0, opc1<1> = 0, opc2 = 00); the 8- and 16-bit
# forms are Advanced SIMD.
FIXED_32 = {
    "VMOV_rs_T1": (0x00C00060, 0x00000000),
    "VMOV_sr_T1": (0x00C00060, 0x00000000),
}


def not_in_32(name):
    """Half-precision arithmetic (names ending _H) is Armv8.2; Cortex-M7 has
    only the half-precision conversions (VCVTB/VCVTT)."""
    return name in NOT_IN_32 or name.endswith("_H")


# Instructions Armv7-M executes as another one: unallocated hints are NOP,
# DSB with any option is DSB, and the banked-register MRS/MSR forms are
# MRS/MSR with a SYSm value (bits 7:0) Armv7-M reserves.
EXECUTED_AS_32 = {
    "MRS_br_T1_AS": "MRS_T1_AS", "MSR_br_T1_AS": "MSR_r_T1_AS",
    "SEVL_T2": "NOP_T2", "ESB_T1": "NOP_T2", "TSB_T1": "NOP_T2", "CSDB_T1": "NOP_T2",
    "CLRBHB_T1": "NOP_T2", "SSBB_T1": "DSB_T1", "PSSBB_T1": "DSB_T1",
}
