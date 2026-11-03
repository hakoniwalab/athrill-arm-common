"""Reading Arm's machine-readable architecture data (AARCHMRS,
Instructions.json) for the T32 decoders: encodings, fields, decode
conditions (evaluated in Python, or turned into C), and the licence notice
of files derived from the data. Nothing here depends on a profile.
"""
import importlib
import json

NOTICE = """\
 * Derived from Arm's Architecture Machine Readable Specification (AARCHMRS),
 * package {package} (Instructions.json, SHA-256 {sha256}).
 *
 * Copyright 2010-2026 Arm Limited or its affiliates.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the copyright holder nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE."""


def load_rules(name):
    """The profile's rules module, rules_<name>.py next to this file."""
    return importlib.import_module("rules_" + name)


def pattern_matches(pattern, value, width):
    """'1x0' style pattern against the width-bit value (MSB first)."""
    for i, ch in enumerate(pattern):
        bit = (value >> (width - 1 - i)) & 1
        if ch in "01" and int(ch) != bit:
            return False
    return True


def field_value(insn, field):
    start, width = field
    return (insn >> start) & ((1 << width) - 1)


def strip(value):
    return value.strip("'")


class Condition:
    """Evaluates a decode condition (AST) over the fields of a node."""

    def __init__(self, fields):
        self.fields = fields

    def eval(self, node, insn):
        kind = node["_type"]
        if kind == "AST.Bool":
            return node["value"]
        if kind == "AST.UnaryOp" and node["op"] == "!":
            return not self.eval(node["expr"], insn)
        if kind == "AST.Function" and node["name"] == "IsFeatureImplemented":
            return False
        if kind == "AST.BinaryOp":
            op = node["op"]
            if op == "&&":
                return self.eval(node["left"], insn) and self.eval(node["right"], insn)
            if op == "||":
                return self.eval(node["left"], insn) or self.eval(node["right"], insn)
            name = node["left"]["value"]
            field = self.fields[name]
            value = field_value(insn, field)
            if op in ("==", "!="):
                same = pattern_matches(strip(node["right"]["value"]), value, field[1])
                return same if op == "==" else not same
            if op == "IN":
                return any(pattern_matches(strip(v["value"]), value, field[1])
                           for v in node["right"]["values"])
        raise ValueError("unsupported condition: " + json.dumps(node)[:200])


def decode_pattern(value):
    """The value's pattern with its should-be bits (should_be_mask '1') as 'x':
    those bits should have the given value, but the instruction decodes as
    this one whatever they are (otherwise UNPREDICTABLE), as the hardware and
    the disassemblers do."""
    pattern = strip(value["value"]["value"])
    should_be = strip(value["should_be_mask"]["value"])
    return "".join("x" if sb == "1" else ch for ch, sb in zip(pattern, should_be))


def encoding_matches(encoding, insn):
    for value in encoding["values"]:
        start = value["range"]["start"]
        width = value["range"]["width"]
        if not pattern_matches(decode_pattern(value), field_value(insn, (start, width)), width):
            return False
    return True


def encoding_fields(encoding):
    return {v["name"]: (v["range"]["start"], v["range"]["width"])
            for v in encoding["values"] if v["_type"] == "Instruction.Encodeset.Field"}


class Condition32(Condition):
    """Condition with the slices (field<3:2>, field<0>) 32-bit encodings use."""

    def eval(self, node, insn):
        if node["_type"] == "AST.BinaryOp" and node["left"]["_type"] == "AST.SquareOp":
            square = node["left"]
            arg = square["arguments"][0]
            start, _ = self.fields[square["var"]["value"]]
            high, low = (arg["left"]["value"], arg["right"]["value"]) if arg["_type"] == "AST.Slice" \
                else (arg["value"], arg["value"])
            width = high - low + 1
            value = (insn >> (start + low)) & ((1 << width) - 1)
            values = [node["right"]] if node["op"] in ("==", "!=") else node["right"]["values"]
            same = any(pattern_matches(strip(v["value"]), value, width) for v in values)
            return not same if node["op"] == "!=" else same
        if node["_type"] in ("AST.UnaryOp", "AST.BinaryOp") and node.get("op") in ("!", "&&", "||"):
            if node["op"] == "!":
                return not self.eval(node["expr"], insn)
            left = self.eval(node["left"], insn)
            return (left and self.eval(node["right"], insn)) if node["op"] == "&&" \
                else (left or self.eval(node["right"], insn))
        return super().eval(node, insn)


def mask_match(pattern, start):
    mask = match = 0
    width = len(pattern)
    for i, ch in enumerate(pattern):
        bit = start + width - 1 - i
        if ch in "01":
            mask |= 1 << bit
            match |= (ch == "1") << bit
    return mask, match


def encoding_test(encoding):
    mask = match = 0
    for value in encoding["values"]:
        m, v = mask_match(decode_pattern(value), value["range"]["start"])
        mask |= m
        match |= v
    if mask == 0:
        return None
    return "((insn & 0x%08XU) == 0x%08XU)" % (mask, match)


class ConditionToC:
    """A decode condition (AST) as a C expression over insn."""

    def __init__(self, fields):
        self.fields = fields

    def field(self, name, high=None, low=None):
        start, width = self.fields[name]
        if high is not None:
            start, width = start + low, high - low + 1
        return start, width

    def compare(self, start, width, pattern):
        m, v = mask_match(pattern, start)
        assert len(pattern) == width, (pattern, width)
        if m == 0:
            return "1"
        return "((insn & 0x%08XU) == 0x%08XU)" % (m, v)

    def operand(self, node):
        if node["_type"] == "AST.Identifier":
            return self.field(node["value"])
        if node["_type"] == "AST.SquareOp":
            arg = node["arguments"][0]
            if arg["_type"] == "AST.Slice":
                return self.field(node["var"]["value"], arg["left"]["value"], arg["right"]["value"])
            return self.field(node["var"]["value"], arg["value"], arg["value"])
        raise ValueError("unsupported operand: " + json.dumps(node)[:200])

    def c(self, node):
        kind = node["_type"]
        if kind == "AST.Bool":
            return "1" if node["value"] else "0"
        if kind == "AST.UnaryOp" and node["op"] == "!":
            return "!" + self.c(node["expr"])
        if kind == "AST.Function" and node["name"] == "IsFeatureImplemented":
            return "0 /* %s */" % node["arguments"][0]["value"]
        if kind == "AST.BinaryOp":
            op = node["op"]
            if op in ("&&", "||"):
                return "(%s %s %s)" % (self.c(node["left"]), op, self.c(node["right"]))
            start, width = self.operand(node["left"])
            if op in ("==", "!="):
                test = self.compare(start, width, strip(node["right"]["value"]))
                return test if op == "==" else "!" + test
            if op == "IN":
                tests = [self.compare(start, width, strip(v["value"])) for v in node["right"]["values"]]
                return "(" + " || ".join(tests) + ")"
        raise ValueError("unsupported condition: " + json.dumps(node)[:200])


