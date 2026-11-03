# Independent model of the extend, bitfield, bit-operation and multiply
# instructions (Python integers): prints the case tables of
# test/test_pseudo_misc.c (python3 test/model_misc.py).
import random
M32 = 0xFFFFFFFF
rng = random.Random(20261104)

def bits(x, hi, lo): return (x >> lo) & ((1 << (hi - lo + 1)) - 1)
def sx(x, n): x &= (1 << n) - 1; return x - (1 << n) if x >> (n - 1) else x
def ror(x, r): r %= 32; return ((x >> r) | (x << (32 - r))) & M32 if r else x

VALS = [0, 1, 0x7F, 0x80, 0xFF, 0x7FFF, 0x8000, 0xFFFF, 0x7FFFFFFF, 0x80000000, M32, 0x12345678, 0x89ABCDEF]
def val(): return rng.choice(VALS) if rng.random() < 0.3 else rng.getrandbits(32)

out = []
# extend
def ext(name, rn, rm, rot):
    r = ror(rm, rot)
    b0, b2, h = bits(r, 7, 0), bits(r, 23, 16), bits(r, 15, 0)
    f = {
        "sxtb": sx(b0, 8), "sxth": sx(h, 16), "uxtb": b0, "uxth": h,
        "sxtab": rn + sx(b0, 8), "sxtah": rn + sx(h, 16), "uxtab": rn + b0, "uxtah": rn + h,
        "sxtb16": ((sx(b2, 8) & 0xFFFF) << 16) | (sx(b0, 8) & 0xFFFF),
        "uxtb16": (b2 << 16) | b0,
        "sxtab16": (((bits(rn, 31, 16) + sx(b2, 8)) & 0xFFFF) << 16) | ((bits(rn, 15, 0) + sx(b0, 8)) & 0xFFFF),
        "uxtab16": (((bits(rn, 31, 16) + b2) & 0xFFFF) << 16) | ((bits(rn, 15, 0) + b0) & 0xFFFF),
    }[name]
    return f & M32
out.append("static const ExtCase ext_cases[] = {")
for name in ["sxtb", "sxth", "uxtb", "uxth", "sxtab", "sxtah", "uxtab", "uxtah", "sxtb16", "uxtb16", "sxtab16", "uxtab16"]:
    for rot in (0, 8, 16, 24):
        rn, rm = val(), val()
        out.append('\t{ "%s", arm_pseudo_%s, 0x%08XU, 0x%08XU, %dU, 0x%08XU },' % (name.upper(), name, rn, rm, rot, ext(name, rn, rm, rot)))
out.append("};")
# bitfield extract
out.append("static const BfxCase bfx_cases[] = {")
for name in ["sbfx", "ubfx"]:
    for lsb, wm1 in [(0, 0), (0, 31), (31, 0), (4, 7), (8, 15), (16, 15), (3, 1)] + [(l, rng.randrange(0, 32 - l)) for l in (rng.randrange(32) for _ in range(5))]:
        rn = val()
        f = bits(rn, lsb + wm1, lsb)
        r = sx(f, wm1 + 1) & M32 if name == "sbfx" else f
        out.append('\t{ "%s", arm_pseudo_%s, 0x%08XU, %dU, %dU, 0x%08XU },' % (name.upper(), name, rn, lsb, wm1, r))
out.append("};")
# bitfield insert
out.append("static const BfiCase bfi_cases[] = {")
for name in ["bfi", "bfc"]:
    for lsb, msb in [(0, 0), (0, 31), (31, 31), (4, 11), (16, 31), (5, 5)] + [(l, rng.randrange(l, 32)) for l in (rng.randrange(32) for _ in range(5))]:
        rd, rn = val(), val()
        w = msb - lsb + 1
        mask = ((1 << w) - 1) << lsb
        src = 0 if name == "bfc" else rn
        r = (rd & ~mask | ((src << lsb) & mask)) & M32
        out.append('\t{ "%s", arm_pseudo_%s, 0x%08XU, 0x%08XU, %dU, %dU, 0x%08XU },' % (name.upper(), name, rd, rn, lsb, msb, r))
out.append("};")
# unary
def un(name, m):
    bs = [bits(m, 8 * i + 7, 8 * i) for i in range(4)]
    if name == "rev": return (bs[0] << 24) | (bs[1] << 16) | (bs[2] << 8) | bs[3]
    if name == "rev16": return (bs[2] << 24) | (bs[3] << 16) | (bs[0] << 8) | bs[1]
    if name == "revsh": return ((sx(bs[0], 8) << 8) | bs[1]) & M32
    if name == "rbit": return int(format(m, "032b")[::-1], 2)
    if name == "clz": return 32 - m.bit_length()
out.append("static const UnaryCase unary_cases[] = {")
for name in ["rev", "rev16", "revsh", "rbit", "clz"]:
    for m in [0, 1, 0x80, 0x8000, 0x80000000, M32] + [val() for _ in range(4)]:
        out.append('\t{ "%s", arm_pseudo_%s, 0x%08XU, 0x%08XU },' % (name.upper(), name, m, un(name, m)))
out.append("};")
# mul / mla / mls / sdiv / udiv; flags: NZCV nibble in bits 31..28
def s32(x): return sx(x, 32)
out.append("static const MulCase mul_cases[] = {")
for name in ["mul", "mla", "mls", "sdiv", "udiv"]:
    pairs = [(0, 5), (7, 0), (0x80000000, M32), (M32, M32), (0x10000, 0x10000), (0x7FFFFFFF, 2)] + [(val(), val()) for _ in range(6)]
    for k, (n, m) in enumerate(pairs):
        a = val()
        setflags = name in ("mul", "mla") and k % 2 == 0
        if name == "mul": r = (n * m) & M32
        elif name == "mla": r = (n * m + a) & M32
        elif name == "mls": r = (a - n * m) & M32
        elif name == "udiv": r = 0 if m == 0 else n // m
        else:
            if s32(m) == 0: r = 0
            else:
                q = abs(s32(n)) // abs(s32(m))
                r = (q if (s32(n) < 0) == (s32(m) < 0) else -q) & M32
        before = rng.getrandbits(4) << 28
        after = before
        if setflags:
            after = (before & 0x30000000) | ((r >> 31) << 31) | ((1 if r == 0 else 0) << 30)
        out.append('\t{ "%s", arm_pseudo_%s, %s, 0x%08XU, 0x%08XU, 0x%08XU, 0x%08XU, 0x%08XU, 0x%08XU },'
                   % (name.upper(), name, "TRUE" if setflags else "FALSE", n, m, a, before, r, after))
out.append("};")
# long
M64 = (1 << 64) - 1
out.append("static const MulLongCase mul_long_cases[] = {")
for name in ["smull", "umull", "smlal", "umlal"]:
    pairs = [(0, 5), (0x80000000, 0x80000000), (M32, M32), (0x80000000, M32), (M32, 1)] + [(val(), val()) for _ in range(6)]
    for k, (n, m) in enumerate(pairs):
        lo, hi = val(), val()
        acc = (hi << 32) | lo
        if name[0] == "s": p = s32(n) * s32(m)
        else: p = n * m
        r = (p + (acc if name.endswith("lal") else 0)) & M64
        if name.endswith("lal") and k == 0: lo, hi = (-p) & M32, ((-p) >> 32) & M32; r = 0
        setflags = k % 2 == 0
        before = rng.getrandbits(4) << 28
        after = before
        if setflags:
            after = (before & 0x30000000) | ((r >> 63) << 31) | ((1 if r == 0 else 0) << 30)
        out.append('\t{ "%s", arm_pseudo_%s, %s, 0x%08XU, 0x%08XU, 0x%08XU, 0x%08XU, 0x%08XU, 0x%08XU, 0x%08XU, 0x%08XU },'
                   % (name.upper(), name, "TRUE" if setflags else "FALSE", n, m, lo, hi, before, r & M32, r >> 32, after))
out.append("};")
print("\n".join(out))
