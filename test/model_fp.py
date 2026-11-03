# Independent model of the Arm floating-point pseudo-code (FPUnpack,
# FPRound, NaN handling, FPAdd, FPSub, FPMul, FPDiv, FPSqrt, FPMulAdd,
# FPCompare) in exact rational arithmetic (fractions), following the
# manual's text: prints the case table of test/test_fp.c
# (python3 test/model_fp.py > test/fp_cases.inc).
import math
import random
from fractions import Fraction

IOC, DZC, OFC, UFC, IXC, IDC = 1, 2, 4, 8, 16, 128
FZ, DN, AHP = 1 << 24, 1 << 25, 1 << 26
ZERO, NONZERO, INF, QNAN, SNAN = "Zero", "Nonzero", "Infinity", "QNaN", "SNaN"


class Ctx:
    def __init__(self, fpscr):
        self.fpscr = fpscr

    def exc(self, bit):
        self.fpscr |= bit


def fmt(N):
    E = {16: 5, 32: 8, 64: 11}[N]
    return E, N - E - 1


def unpack(x, N, ctx):
    E, F = fmt(N)
    bias = 2 ** (E - 1) - 1
    sign = (x >> (N - 1)) & 1
    exp = (x >> F) & (2 ** E - 1)
    frac = x & (2 ** F - 1)
    if exp == 0:
        if frac == 0 or (N != 16 and ctx.fpscr & FZ):
            if frac != 0:
                ctx.exc(IDC)
            return ZERO, sign, Fraction(0)
        value = Fraction(2) ** (1 - bias - F) * frac
    elif exp == 2 ** E - 1 and not (N == 16 and ctx.fpscr & AHP):
        if frac == 0:
            return INF, sign, None
        return (QNAN if frac >> (F - 1) else SNAN), sign, None
    else:
        value = Fraction(2) ** (exp - bias - F) * (2 ** F + frac)
    return NONZERO, sign, -value if sign else value


def zero(s, N):
    return s << (N - 1)


def inf(s, N):
    E, F = fmt(N)
    return zero(s, N) | ((2 ** E - 1) << F)


def default_nan(N):
    E, F = fmt(N)
    return inf(0, N) | (1 << (F - 1))


def max_normal(s, N):
    E, F = fmt(N)
    return zero(s, N) | ((2 ** E - 2) << F) | (2 ** F - 1)


def neg(x, N):
    return x ^ (1 << (N - 1))


def process_nan(t, op, N, ctx):
    E, F = fmt(N)
    r = op
    if t == SNAN:
        r |= 1 << (F - 1)
        ctx.exc(IOC)
    if ctx.fpscr & DN:
        r = default_nan(N)
    return r


def process_nans(types, ops, N, ctx):
    for want in (SNAN, QNAN):
        for t, op in zip(types, ops):
            if t == want:
                return True, process_nan(t, op, N, ctx)
    return False, 0


def floor_log2(v):
    # largest e with 2^e <= v (v > 0)
    e = v.numerator.bit_length() - v.denominator.bit_length()
    while Fraction(2) ** e > v:
        e -= 1
    while Fraction(2) ** (e + 1) <= v:
        e += 1
    return e


def fpround(value, N, ctx, rounding=None):
    E, F = fmt(N)
    if rounding is None:
        rounding = (ctx.fpscr >> 22) & 3
    minimum_exp = 2 - 2 ** (E - 1)
    sign = 1 if value < 0 else 0
    mantissa = abs(value)
    exponent = floor_log2(mantissa)
    mantissa = mantissa / Fraction(2) ** exponent
    if ctx.fpscr & FZ and N != 16 and exponent < minimum_exp:
        ctx.exc(UFC)
        return zero(sign, N)
    biased_exp = max(exponent - minimum_exp + 1, 0)
    if biased_exp == 0:
        mantissa = mantissa / Fraction(2) ** (minimum_exp - exponent)
    int_mant = math.floor(mantissa * 2 ** F)
    error = mantissa * 2 ** F - int_mant
    if biased_exp == 0 and error != 0:
        ctx.exc(UFC)
    if rounding == 0:
        round_up = error > Fraction(1, 2) or (error == Fraction(1, 2) and int_mant & 1)
        overflow_to_inf = True
    elif rounding == 1:
        round_up = error != 0 and sign == 0
        overflow_to_inf = sign == 0
    elif rounding == 2:
        round_up = error != 0 and sign == 1
        overflow_to_inf = sign == 1
    else:
        round_up = False
        overflow_to_inf = False
    if round_up:
        int_mant += 1
        if int_mant == 2 ** F:
            biased_exp = 1
        if int_mant == 2 ** (F + 1):
            biased_exp += 1
            int_mant //= 2
    if N != 16 or not ctx.fpscr & AHP:
        if biased_exp >= 2 ** E - 1:
            result = inf(sign, N) if overflow_to_inf else max_normal(sign, N)
            ctx.exc(OFC)
            error = 1
        else:
            result = zero(sign, N) | (biased_exp << F) | (int_mant & (2 ** F - 1))
    else:
        if biased_exp >= 2 ** E:
            result = zero(sign, N) | (2 ** (N - 1) - 1)
            ctx.exc(IOC)
            error = 0
        else:
            result = zero(sign, N) | (biased_exp << F) | (int_mant & (2 ** F - 1))
    if error != 0:
        ctx.exc(IXC)
    return result


def exact_zero(N, ctx):
    return zero(1 if (ctx.fpscr >> 22) & 3 == 2 else 0, N)


def fp_add(op1, op2, N, ctx, sub=False):
    t1, s1, v1 = unpack(op1, N, ctx)
    t2, s2, v2 = unpack(op2, N, ctx)
    done, r = process_nans((t1, t2), (op1, op2), N, ctx)
    if done:
        return r
    if sub:
        s2 ^= 1
        v2 = -v2 if v2 is not None else None
    if t1 == INF and t2 == INF and s1 != s2:
        ctx.exc(IOC)
        return default_nan(N)
    if (t1 == INF and s1 == 0) or (t2 == INF and s2 == 0):
        return inf(0, N)
    if (t1 == INF and s1 == 1) or (t2 == INF and s2 == 1):
        return inf(1, N)
    if t1 == ZERO and t2 == ZERO and s1 == s2:
        return zero(s1, N)
    v = v1 + v2
    return exact_zero(N, ctx) if v == 0 else fpround(v, N, ctx)


def fp_mul(op1, op2, N, ctx):
    t1, s1, v1 = unpack(op1, N, ctx)
    t2, s2, v2 = unpack(op2, N, ctx)
    done, r = process_nans((t1, t2), (op1, op2), N, ctx)
    if done:
        return r
    if (t1 == INF and t2 == ZERO) or (t1 == ZERO and t2 == INF):
        ctx.exc(IOC)
        return default_nan(N)
    if INF in (t1, t2):
        return inf(s1 ^ s2, N)
    if ZERO in (t1, t2):
        return zero(s1 ^ s2, N)
    return fpround(v1 * v2, N, ctx)


def fp_div(op1, op2, N, ctx):
    t1, s1, v1 = unpack(op1, N, ctx)
    t2, s2, v2 = unpack(op2, N, ctx)
    done, r = process_nans((t1, t2), (op1, op2), N, ctx)
    if done:
        return r
    if (t1 == INF and t2 == INF) or (t1 == ZERO and t2 == ZERO):
        ctx.exc(IOC)
        return default_nan(N)
    if t1 == INF or t2 == ZERO:
        if t1 != INF:
            ctx.exc(DZC)
        return inf(s1 ^ s2, N)
    if t1 == ZERO or t2 == INF:
        return zero(s1 ^ s2, N)
    return fpround(v1 / v2, N, ctx)


def fp_sqrt(op, N, ctx):
    t, s, v = unpack(op, N, ctx)
    if t in (QNAN, SNAN):
        return process_nan(t, op, N, ctx)
    if t == ZERO:
        return zero(s, N)
    if t == INF and s == 0:
        return inf(0, N)
    if s == 1:
        ctx.exc(IOC)
        return default_nan(N)
    K = 1200
    n = v * Fraction(2) ** (2 * K)
    assert n.denominator == 1
    r = math.isqrt(n.numerator)
    root = Fraction(r, 2 ** K)
    if r * r != n.numerator:
        root += Fraction(1, 2 ** (K + 1))   # far below any rounding position
    return fpround(root, N, ctx)


def fp_muladd(addend, op1, op2, N, ctx):
    ta, sa, va = unpack(addend, N, ctx)
    t1, s1, v1 = unpack(op1, N, ctx)
    t2, s2, v2 = unpack(op2, N, ctx)
    inf1, zero1, inf2, zero2 = t1 == INF, t1 == ZERO, t2 == INF, t2 == ZERO
    done, r = process_nans((ta, t1, t2), (addend, op1, op2), N, ctx)
    if ta == QNAN and ((inf1 and zero2) or (zero1 and inf2)):
        ctx.exc(IOC)
        r = default_nan(N)
    if done:
        return r
    infa, zeroa = ta == INF, ta == ZERO
    sp, infp, zerop = s1 ^ s2, inf1 or inf2, zero1 or zero2
    if (inf1 and zero2) or (zero1 and inf2) or (infa and infp and sa != sp):
        ctx.exc(IOC)
        return default_nan(N)
    if (infa and sa == 0) or (infp and sp == 0):
        return inf(0, N)
    if (infa and sa == 1) or (infp and sp == 1):
        return inf(1, N)
    if zeroa and zerop and sa == sp:
        return zero(sa, N)
    v = va + (v1 * v2 if not zerop else 0)
    return exact_zero(N, ctx) if v == 0 else fpround(v, N, ctx)


def fp_compare(op1, op2, quiet_nan_exc, N, ctx):
    t1, s1, v1 = unpack(op1, N, ctx)
    t2, s2, v2 = unpack(op2, N, ctx)
    if {t1, t2} & {QNAN, SNAN}:
        if SNAN in (t1, t2) or quiet_nan_exc:
            ctx.exc(IOC)
        return 0x3

    def key(t, s, v):
        if t == INF:
            return Fraction(-1 if s else 1) * Fraction(10) ** 400
        return v
    a, b = key(t1, s1, v1), key(t2, s2, v2)
    return 0x6 if a == b else (0x8 if a < b else 0x2)


BIG = Fraction(2) ** 100000   # the value of an infinity


def fp_convert(op, M, N, ctx):
    t, s, v = unpack(op, M, ctx)
    alt = N == 16 and ctx.fpscr & AHP
    if t in (QNAN, SNAN):
        Fm, Fn = fmt(M)[1], fmt(N)[1]
        if alt:
            r = zero(s, N)
        elif ctx.fpscr & DN:
            r = default_nan(N)
        else:
            payload = op & (2 ** (Fm - 1) - 1)
            payload = payload << (Fn - Fm) if Fn >= Fm else payload >> (Fm - Fn)
            r = inf(s, N) | (1 << (Fn - 1)) | payload
        if t == SNAN or alt:
            ctx.exc(IOC)
        return r
    if t == INF:
        if alt:
            ctx.exc(IOC)
            return zero(s, N) | (2 ** (N - 1) - 1)
        return inf(s, N)
    if t == ZERO:
        return zero(s, N)
    return fpround(v, N, ctx)


def round_up(value, rounding):
    int_result = math.floor(value)
    error = value - int_result
    half = Fraction(1, 2)
    if rounding == 0:
        up = error > half or (error == half and int_result & 1)
    elif rounding == 1:
        up = error != 0
    elif rounding == 2:
        up = False
    elif rounding == 3:
        up = error != 0 and int_result < 0
    else:
        up = error > half or (error == half and int_result >= 0)
    return int_result + (1 if up else 0), error


def fp_to_fixed(op, N, M, fbits, unsigned, rounding, ctx):
    t, s, v = unpack(op, N, ctx)
    if t in (QNAN, SNAN):
        ctx.exc(IOC)
        v = Fraction(0)
    elif t == INF:
        v = -BIG if s else BIG
    elif t == ZERO:
        v = Fraction(0)
    int_result, error = round_up(v * 2 ** fbits, rounding)
    lo, hi = (0, 2 ** M - 1) if unsigned else (-(2 ** (M - 1)), 2 ** (M - 1) - 1)
    result = min(max(int_result, lo), hi)
    if result != int_result:
        ctx.exc(IOC)
    elif error != 0:
        ctx.exc(IXC)
    return result & (2 ** M - 1)


def fixed_to_fp(op, M, N, fbits, unsigned, round_to_nearest, ctx):
    x = op & (2 ** M - 1)
    if not unsigned and x >> (M - 1):
        x -= 2 ** M
    real = Fraction(x, 2 ** fbits)
    return zero(0, N) if real == 0 else fpround(real, N, ctx, rounding=0 if round_to_nearest else None)


def fp_round_int(op, N, rounding, exact, ctx):
    t, s, v = unpack(op, N, ctx)
    if t in (QNAN, SNAN):
        return process_nan(t, op, N, ctx)
    if t == INF:
        return inf(s, N)
    if t == ZERO:
        return zero(s, N)
    int_result, error = round_up(v, rounding)
    r = zero(s, N) if int_result == 0 else fpround(Fraction(int_result), N, ctx, rounding=3)
    if error != 0 and exact:
        ctx.exc(IXC)
    return r


def fp_max_min(op1, op2, N, ctx, is_max):
    t1, s1, v1 = unpack(op1, N, ctx)
    t2, s2, v2 = unpack(op2, N, ctx)
    done, r = process_nans((t1, t2), (op1, op2), N, ctx)
    if done:
        return r
    val1 = (-BIG if s1 else BIG) if t1 == INF else v1
    val2 = (-BIG if s2 else BIG) if t2 == INF else v2
    pick1 = val1 > val2 if is_max else val1 < val2
    t, s, v = (t1, s1, v1) if pick1 else (t2, s2, v2)
    if t == INF:
        return inf(s, N)
    if t == ZERO:
        return zero((s1 & s2) if is_max else (s1 | s2), N)
    return fpround(v, N, ctx)


def fp_max_min_num(op1, op2, N, ctx, is_max):
    t1 = unpack(op1, N, ctx)[0]
    t2 = unpack(op2, N, ctx)[0]
    if t1 == QNAN and t2 != QNAN:
        op1 = inf(1 if is_max else 0, N)
    elif t1 != QNAN and t2 == QNAN:
        op2 = inf(1 if is_max else 0, N)
    return fp_max_min(op1, op2, N, ctx, is_max)


OPS = ["ADD", "SUB", "MUL", "DIV", "SQRT", "MULADD", "CMP", "CMPE",
       "CVT", "TOFIXED", "FROMFIXED", "ROUNDINT", "MAXNUM", "MINNUM"]


def run(op, N, a, b, c, fpscr):
    ctx = Ctx(fpscr)
    if op == "ADD":
        r = fp_add(a, b, N, ctx)
    elif op == "SUB":
        r = fp_add(a, b, N, ctx, sub=True)
    elif op == "MUL":
        r = fp_mul(a, b, N, ctx)
    elif op == "DIV":
        r = fp_div(a, b, N, ctx)
    elif op == "SQRT":
        r = fp_sqrt(a, N, ctx)
    elif op == "MULADD":
        r = fp_muladd(c, a, b, N, ctx)
    elif op in ("CMP", "CMPE"):
        r = fp_compare(a, b, op == "CMPE", N, ctx)
    elif op == "CVT":                       # b: the target size
        r = fp_convert(a, N, b, ctx)
    elif op == "TOFIXED":                   # b: M | fbits << 8 | unsigned << 16 | rounding << 20
        r = fp_to_fixed(a, N, b & 0xFF, (b >> 8) & 0xFF, (b >> 16) & 1, (b >> 20) & 7, ctx)
    elif op == "FROMFIXED":                 # b: M | fbits << 8 | unsigned << 16 | round_to_nearest << 17
        r = fixed_to_fp(a, b & 0xFF, N, (b >> 8) & 0xFF, (b >> 16) & 1, (b >> 17) & 1, ctx)
    elif op == "ROUNDINT":                  # b: rounding | exact << 4
        r = fp_round_int(a, N, b & 7, (b >> 4) & 1, ctx)
    else:
        r = fp_max_min_num(a, b, N, ctx, op == "MAXNUM")
    return r, ctx.fpscr


rng = random.Random(20261106)
SPECIAL = {32: [0, 0x80000000, 0x7F800000, 0xFF800000, 0x7FC00000, 0xFFC00001, 0x7F800001, 0xFFA00000, 0x00000001,
                0x807FFFFF, 0x00800000, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0xFF7FFFFF, 0x00400000, 0x3F000000],
           64: [0, 1 << 63, 0x7FF << 52, 0xFFF << 52, 0x7FF8 << 48, (0xFFF8 << 48) | 1, (0x7FF << 52) | 1,
                0xFFF4 << 48, 1, (1 << 63) | ((1 << 52) - 1), 1 << 52, 0x3FF << 52, 0xBFF << 52,
                (0x7FE << 52) | ((1 << 52) - 1), 0x8000 << 48, 0x3FE << 52]}


def operand(N, near=None):
    E, F = fmt(N)
    k = rng.random()
    if k < 0.25:
        return rng.choice(SPECIAL[N])
    if near is not None and k < 0.55:
        # close to another operand: cancellation, ties, equal values
        x = near ^ (rng.getrandbits(1) << (N - 1)) if rng.random() < 0.5 else near
        return (x + rng.randrange(-3, 4)) & ((1 << N) - 1)
    if k < 0.7:
        e = rng.choice([rng.randrange(0, 2 ** E), rng.randrange(1, 4), rng.randrange(2 ** E - 4, 2 ** E - 1),
                        2 ** (E - 1) + rng.randrange(-30, 30)])
        e = max(0, min(e, 2 ** E - 1))
        return (rng.getrandbits(1) << (N - 1)) | (e << F) | rng.getrandbits(F)
    return rng.getrandbits(N)


def fpscr_random():
    return (rng.randrange(4) << 22) | (FZ if rng.random() < 0.3 else 0) | (DN if rng.random() < 0.3 else 0)


lines = []
for op in OPS[:8]:
    for N in (32, 64):
        for _ in range(250 if op != "SQRT" else 150):
            a = operand(N)
            b = operand(N, near=a)
            c = operand(N, near=None if rng.random() < 0.5 else (a if rng.random() < 0.5 else b))
            if op == "MULADD" and rng.random() < 0.3:
                # c close to -(a*b): cancellation in the fused sum
                ctx = Ctx(0)
                p = fp_mul(a, b, N, ctx)
                c = neg(p, N) ^ rng.randrange(0, 4)
            f = fpscr_random()
            r, f2 = run(op, N, a, b, c, f)
            lines.append("\t{ FPT_%s, %d, 0x%016XULL, 0x%016XULL, 0x%016XULL, 0x%08XU, 0x%016XULL, 0x%08XU },"
                         % (op, N, a, b, c, f, r, f2))
# The conversions, round to integral, max/min.
SPECIAL[16] = [0, 0x8000, 0x7C00, 0xFC00, 0x7E00, 0xFE01, 0x7C01, 0x0001, 0x03FF, 0x0400, 0x3C00, 0xBC00, 0x7BFF, 0xFBFF]


def fpscr_cvt():
    return fpscr_random() | (AHP if rng.random() < 0.3 else 0)


for _ in range(300):
    M, N = rng.choice([(32, 64), (64, 32), (16, 32), (32, 16), (16, 64), (64, 16)])
    a = operand(M)
    f = fpscr_cvt()
    r, f2 = run("CVT", M, a, N, 0, f)
    lines.append("\t{ FPT_CVT, %d, 0x%016XULL, 0x%016XULL, 0x%016XULL, 0x%08XU, 0x%016XULL, 0x%08XU },"
                 % (M, a, N, 0, f, r, f2))
for N in (32, 64):
    for _ in range(300):
        a = operand(N)
        if rng.random() < 0.4:
            # values near integers and halves
            E, F = fmt(N)
            e = rng.randrange(-2, 34)
            a = (rng.getrandbits(1) << (N - 1)) | ((2 ** (E - 1) - 1 + e) << F) | (rng.getrandbits(F) & ~((1 << max(F - e - 2, 0)) - 1))
        M = rng.choice([16, 32])
        params = M | (rng.choice([0, 0, rng.randrange(M + 1)]) << 8) | (rng.getrandbits(1) << 16) | (rng.randrange(5) << 20)
        f = fpscr_random()
        r, f2 = run("TOFIXED", N, a, params, 0, f)
        lines.append("\t{ FPT_TOFIXED, %d, 0x%016XULL, 0x%016XULL, 0x%016XULL, 0x%08XU, 0x%016XULL, 0x%08XU },"
                     % (N, a, params, 0, f, r, f2))
        x = rng.choice([0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x8000, 0xFFFF, rng.getrandbits(32), rng.getrandbits(32)])
        M = rng.choice([16, 32])
        params = M | (rng.choice([0, 0, rng.randrange(M + 1)]) << 8) | (rng.getrandbits(1) << 16) | (rng.getrandbits(1) << 17)
        r, f2 = run("FROMFIXED", N, x, params, 0, f)
        lines.append("\t{ FPT_FROMFIXED, %d, 0x%016XULL, 0x%016XULL, 0x%016XULL, 0x%08XU, 0x%016XULL, 0x%08XU },"
                     % (N, x, params, 0, f, r, f2))
        params = rng.randrange(5) | (rng.getrandbits(1) << 4)
        r, f2 = run("ROUNDINT", N, a, params, 0, f)
        lines.append("\t{ FPT_ROUNDINT, %d, 0x%016XULL, 0x%016XULL, 0x%016XULL, 0x%08XU, 0x%016XULL, 0x%08XU },"
                     % (N, a, params, 0, f, r, f2))
        b = operand(N, near=a)
        for op in ("MAXNUM", "MINNUM"):
            r, f2 = run(op, N, a, b, 0, f)
            lines.append("\t{ FPT_%s, %d, 0x%016XULL, 0x%016XULL, 0x%016XULL, 0x%08XU, 0x%016XULL, 0x%08XU },"
                         % (op, N, a, b, 0, f, r, f2))

# Directed cases: a denormal result rounding up to the smallest normal, and
# a quiet NaN addend with infinity times zero.
for f in (0, 1 << 22, 2 << 22, 3 << 22):
    for op, N, a, b, c in (("MUL", 32, 0x007FFFFF, 0x3F800001, 0), ("MUL", 32, 0x807FFFFF, 0x3F800001, 0),
                           ("MUL", 64, 0x000FFFFFFFFFFFFF, 0x3FF0000000000001, 0),
                           ("MULADD", 32, 0x7F800000, 0x00000000, 0x7FC00000),
                           ("MULADD", 64, 0x0000000000000000, 0xFFF0000000000000, 0x7FF8000000000000),
                           # square roots whose sticky bit decides the rounding (a tie without it)
                           ("SQRT", 64, 0x438BD6B8E8F6E0BD, 0, 0), ("SQRT", 64, 0x428B05F935FFED04, 0, 0),
                           ("SQRT", 64, 0x4296E401B9E25E6C, 0, 0),
                           # exact ties
                           ("ADD", 32, 0x3F800000, 0x33800000, 0), ("ADD", 32, 0x3F800001, 0x33800000, 0),
                           ("ADD", 64, 0x3FF0000000000000, 0x3CA0000000000000, 0),
                           ("ADD", 64, 0x3FF0000000000001, 0x3CA0000000000000, 0)):
        r, f2 = run(op, N, a, b, c, f)
        lines.append("\t{ FPT_%s, %d, 0x%016XULL, 0x%016XULL, 0x%016XULL, 0x%08XU, 0x%016XULL, 0x%08XU },"
                     % (op, N, a, b, c, f, r, f2))
print("/* Printed by test/model_fp.py. */")
print("\n".join(lines))
