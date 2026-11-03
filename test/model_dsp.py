# Independent model of the DSP instructions (Python integers): prints the
# case tables of test/test_pseudo_dsp.c (python3 test/model_dsp.py).
import random
M32 = 0xFFFFFFFF
M64 = (1 << 64) - 1
rng = random.Random(20261105)
EDGE = [0, 1, 0x7F, 0x80, 0xFF, 0x7FFF, 0x8000, 0xFFFF, 0x7FFFFFFF, 0x80000000, M32,
        0x7F7F7F7F, 0x80808080, 0x7FFF7FFF, 0x80008000, 0x00FF00FF]


def val():
    return rng.choice(EDGE) if rng.random() < 0.35 else rng.getrandbits(32)


def sx(x, n):
    x &= (1 << n) - 1
    return x - (1 << n) if x >> (n - 1) else x


def lane(x, i, w, signed):
    v = (x >> (i * w)) & ((1 << w) - 1)
    return sx(v, w) if signed else v


def ssat(v, n):
    hi, lo = (1 << (n - 1)) - 1, -(1 << (n - 1))
    return (min(max(v, lo), hi), v > hi or v < lo)


def usat(v, n):
    hi = (1 << n) - 1
    return (min(max(v, 0), hi), v > hi or v < 0)


def parallel(prefix, op, n, m):
    signed = prefix in ("S", "Q", "SH")
    w = 8 if op.endswith("8") else 16
    k = 32 // w
    res, ge = 0, 0
    for i in range(k):
        if op in ("ADD16", "ADD8"):
            v, sub = lane(n, i, w, signed) + lane(m, i, w, signed), False
        elif op in ("SUB16", "SUB8"):
            v, sub = lane(n, i, w, signed) - lane(m, i, w, signed), True
        elif op == "ASX":   # lo: n.lo - m.hi, hi: n.hi + m.lo
            v, sub = (lane(n, 0, 16, signed) - lane(m, 1, 16, signed), True) if i == 0 else \
                     (lane(n, 1, 16, signed) + lane(m, 0, 16, signed), False)
        else:               # SAX: lo: n.lo + m.hi, hi: n.hi - m.lo
            v, sub = (lane(n, 0, 16, signed) + lane(m, 1, 16, signed), False) if i == 0 else \
                     (lane(n, 1, 16, signed) - lane(m, 0, 16, signed), True)
        if prefix == "Q":
            r = ssat(v, w)[0]
        elif prefix == "UQ":
            r = usat(v, w)[0]
        elif prefix in ("SH", "UH"):
            r = v >> 1
        else:
            r = v
        res |= (r & ((1 << w) - 1)) << (i * w)
        g = (v >= 0) if (signed or sub) else (v >= (1 << w))
        if g:
            ge |= (3 if w == 16 else 1) << (i * (w // 8))
    return res, ge if prefix in ("S", "U") else None


out = []
out.append("static const ParallelCase parallel_cases[] = {")
for op in ("ADD16", "ASX", "SAX", "SUB16", "ADD8", "SUB8"):
    for prefix in ("S", "U", "Q", "UQ", "SH", "UH"):
        for _ in range(4):
            n, m, ge0 = val(), val(), rng.getrandbits(4)
            r, ge = parallel(prefix, op, n, m)
            ge = ge0 if ge is None else ge
            name = prefix + op
            out.append('\t{ "%s", arm_pseudo_%s, 0x%08XU, 0x%08XU, 0x%XU, 0x%08XU, 0x%XU },' % (name, name.lower(), n, m, ge0, r, ge))
out.append("};")

out.append("static const SelCase sel_cases[] = {")
for _ in range(8):
    n, m, ge = val(), val(), rng.getrandbits(4)
    r = 0
    for i in range(4):
        r |= ((n if ge >> i & 1 else m) >> (8 * i) & 0xFF) << (8 * i)
    out.append('\t{ 0x%08XU, 0x%08XU, 0x%XU, 0x%08XU },' % (n, m, ge, r))
out.append("};")


def shift(x, t, n):
    if t == "LSL":
        return (x << n) & M32
    return (sx(x, 32) >> n) & M32   # ASR (n = 1..31)


out.append("static const SatCase sat_cases[] = {")
for name in ("ssat", "usat", "ssat16", "usat16"):
    for _ in range(10):
        x = val()
        if name == "ssat":
            to, t, sh = rng.randrange(1, 33), rng.choice(["LSL", "ASR"]), rng.randrange(0, 32)
            if t == "ASR" and sh == 0:
                t = "LSL"
            r, q = ssat(sx(shift(x, t, sh), 32), to)
        elif name == "usat":
            to, t, sh = rng.randrange(0, 32), rng.choice(["LSL", "ASR"]), rng.randrange(0, 32)
            if t == "ASR" and sh == 0:
                t = "LSL"
            r, q = usat(sx(shift(x, t, sh), 32), to)
        elif name == "ssat16":
            to, t, sh = rng.randrange(1, 17), "LSL", 0
            (a, q1), (b, q2) = ssat(lane(x, 0, 16, True), to), ssat(lane(x, 1, 16, True), to)
            r, q = ((b & 0xFFFF) << 16) | (a & 0xFFFF), q1 or q2
        else:
            to, t, sh = rng.randrange(0, 16), "LSL", 0
            (a, q1), (b, q2) = usat(lane(x, 0, 16, True), to), usat(lane(x, 1, 16, True), to)
            r, q = (b << 16) | a, q1 or q2
        out.append('\t{ "%s", arm_pseudo_%s, 0x%08XU, %dU, SRType_%s, %dU, 0x%08XU, %s },'
                   % (name.upper(), name, x, to, t, sh, r & M32, "TRUE" if q else "FALSE"))
out.append("};")

out.append("static const QCase q_cases[] = {")
for name in ("qadd", "qsub", "qdadd", "qdsub"):
    for k in range(8):
        n, m = val(), val()
        sn, sm = sx(n, 32), sx(m, 32)
        if name == "qadd":
            r, q = ssat(sm + sn, 32)
        elif name == "qsub":
            r, q = ssat(sm - sn, 32)
        else:
            d, q1 = ssat(2 * sn, 32)
            r, q2 = ssat(sm + d if name == "qdadd" else sm - d, 32)
            q = q1 or q2
        out.append('\t{ "%s", arm_pseudo_%s, 0x%08XU, 0x%08XU, 0x%08XU, %s },' % (name.upper(), name, n, m, r & M32, "TRUE" if q else "FALSE"))
out.append("};")

out.append("static const PkhCase pkh_cases[] = {")
for tb in (False, True):
    for _ in range(6):
        n, m = val(), val()
        t, sh = ("ASR", rng.randrange(1, 32)) if tb else ("LSL", rng.randrange(0, 32))
        o2 = shift(m, t, sh)
        r = ((n & 0xFFFF0000) | (o2 & 0xFFFF)) if tb else ((o2 & 0xFFFF0000) | (n & 0xFFFF))
        out.append('\t{ "%s", arm_pseudo_%s, 0x%08XU, 0x%08XU, SRType_%s, %dU, 0x%08XU },'
                   % ("PKHTB" if tb else "PKHBT", "pkhtb" if tb else "pkhbt", n, m, t, sh, r))
out.append("};")


def half(x, high):
    return lane(x, 1 if high else 0, 16, True)


def ror16(x):
    return ((x >> 16) | (x << 16)) & M32


out.append("static const DspMulCase dsp_mul_cases[] = {")
for name in ("smulxy", "smlaxy", "smulwy", "smlawy", "smuad", "smlad", "smusd", "smlsd",
             "smmul", "smmla", "smmls", "usad8", "usada8"):
    for k in range(8):
        n, m, a = val(), val(), val()
        nh, mh, sw, rd = bool(rng.getrandbits(1)), bool(rng.getrandbits(1)), bool(rng.getrandbits(1)), bool(rng.getrandbits(1))
        sn, sm, sa = sx(n, 32), sx(m, 32), sx(a, 32)
        q = False
        if name == "smulxy":
            r = half(n, nh) * half(m, mh)
        elif name == "smlaxy":
            r = half(n, nh) * half(m, mh) + sa
            q = r != sx(r & M32, 32)
        elif name == "smulwy":
            r = (sn * half(m, mh)) >> 16
        elif name == "smlawy":
            r = (sn * half(m, mh) + (sa << 16)) >> 16
            q = r != sx(r & M32, 32)
        elif name in ("smuad", "smlad", "smusd", "smlsd"):
            o2 = ror16(m) if sw else m
            p1, p2 = half(n, False) * half(o2, False), half(n, True) * half(o2, True)
            r = {"smuad": p1 + p2, "smlad": p1 + p2 + sa, "smusd": p1 - p2, "smlsd": p1 - p2 + sa}[name]
            q = name != "smusd" and r != sx(r & M32, 32)
        elif name in ("smmul", "smmla", "smmls"):
            p = sn * sm
            r = {"smmul": p, "smmla": (sa << 32) + p, "smmls": (sa << 32) - p}[name]
            if rd:
                r += 0x80000000
            r = (r >> 32)
        else:
            r = sum(abs(((n >> (8 * i)) & 0xFF) - ((m >> (8 * i)) & 0xFF)) for i in range(4))
            if name == "usada8":
                r += a
        out.append('\t{ "%s", arm_pseudo_%s, 0x%08XU, 0x%08XU, 0x%08XU, %s, %s, %s, %s, 0x%08XU, %s },'
                   % (name.upper(), name, n, m, a, *["TRUE" if b else "FALSE" for b in (nh, mh, sw, rd)], r & M32,
                      "TRUE" if q else "FALSE"))
out.append("};")

out.append("static const DspLongCase dsp_long_cases[] = {")
for name in ("smlalxy", "smlald", "smlsld", "umaal"):
    for k in range(8):
        n, m, lo, hi = val(), val(), val(), val()
        nh, mh, sw = bool(rng.getrandbits(1)), bool(rng.getrandbits(1)), bool(rng.getrandbits(1))
        acc = sx((hi << 32) | lo, 64)
        if name == "smlalxy":
            r = half(n, nh) * half(m, mh) + acc
        elif name in ("smlald", "smlsld"):
            o2 = ror16(m) if sw else m
            p1, p2 = half(n, False) * half(o2, False), half(n, True) * half(o2, True)
            r = (p1 + p2 if name == "smlald" else p1 - p2) + acc
        else:
            r = n * m + hi + lo
        r &= M64
        out.append('\t{ "%s", arm_pseudo_%s, 0x%08XU, 0x%08XU, 0x%08XU, 0x%08XU, %s, %s, %s, 0x%08XU, 0x%08XU },'
                   % (name.upper(), name, n, m, lo, hi, *["TRUE" if b else "FALSE" for b in (nh, mh, sw)], r & M32, r >> 32))
out.append("};")
print("\n".join(out))
