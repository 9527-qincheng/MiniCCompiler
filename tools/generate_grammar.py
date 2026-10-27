#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MiniC -- generate the LL(1) grammar tables (src/parser/grammar.cpp).

Reads the challenge-edition workbook ("MiniC挑战版文法及其相关表格.xlsx") and
emits the pure-data layer declared by src/parser/grammar.h:

    g_Grammar       100 productions (index = production number, 0 = sentinel)
    g_FirstSet      FIRST(A)  for the 45 nonterminals
    g_FollowSet     FOLLOW(A) for the 45 nonterminals
    g_SelectSet     SELECT(p) for the 105 productions
    g_PredictTable  M[nonterminal][terminal] -> production number (-1 = error)

The workbook is the same artifact the design document (v4.0, sections 4.3-4.9)
is derived from, so the generated C++ tables cannot drift away from it silently.
The script re-checks the workbook against itself before writing anything:

  * production numbers, LHS/RHS codes and semantic-action positions are valid;
  * the dense prediction table agrees cell by cell with the SELECT sets, with
    exactly one deliberate difference: 否则部分 x else resolves to production 42
    (dangling-else, resolved inside the table, see design doc section 4.8).

Usage:
    python tools/generate_grammar.py [--xlsx PATH] [--outdir PATH]
"""

import argparse
import os
import sys

try:
    from openpyxl import load_workbook
except ImportError:  # pragma: no cover - environment dependent
    sys.exit("openpyxl is required to read the workbook (pip install openpyxl)")

# --------------------------------------------------------------------------
# Table shape, mirrored from src/parser/grammar.h
# --------------------------------------------------------------------------
MAX_NT = 48
MAX_TERM = 100
NTERM_CODE_BASE = 100
PROD_COUNT = 105
TERM_COUNT = 41
PROD_TABLE_SIZE = PROD_COUNT + 1

# Nonterminal code of 否则部分 / terminal code of "else": the single cell where
# the prediction table intentionally overrides the SELECT sets.
DANGLING_ELSE_NT = 120
DANGLING_ELSE_TERM = 7
DANGLING_ELSE_PROD = 42

# --------------------------------------------------------------------------
# Workbook layout (sheet names and columns are fixed by the design docs)
# --------------------------------------------------------------------------
SHEET_PROD = "终结符、非终结符编码及产生式编号"
SHEET_FIRST = "挑战版First集合"
SHEET_FOLLOW = "挑战版Follow集合"
SHEET_SELECT = "挑战版Select集合"
SHEET_PRED = "预测分析表"

EPSILON = 0


class Production(object):
    """One row of the production table."""

    def __init__(self, num, lhs, rhs, sem_pos, sem_act, select):
        self.num = num
        self.lhs = lhs
        self.rhs = rhs
        self.sem_pos = sem_pos
        self.sem_act = sem_act
        self.select = select


def fail(msg):
    sys.exit("generate_grammar: " + msg)


def parse_code_set(text):
    """'003 004 000' -> [3, 4, 0]; empty/None -> []."""
    if text is None:
        return []
    return [int(tok) for tok in str(text).split()]


def load_workbook_data(xlsx_path):
    wb = load_workbook(xlsx_path, data_only=True)
    for name in (SHEET_PROD, SHEET_FIRST, SHEET_FOLLOW, SHEET_SELECT, SHEET_PRED):
        if name not in wb.sheetnames:
            fail("workbook is missing required sheet %r" % name)

    # ---- terminal / nonterminal code tables + productions -----------------
    ws = wb[SHEET_PROD]
    rows = list(ws.iter_rows(min_row=2, max_row=1 + PROD_COUNT, values_only=True))
    if len(rows) != PROD_COUNT:
        fail("expected %d production rows, found %d" % (PROD_COUNT, len(rows)))

    nt_code_by_name = {}
    term_codes = []
    productions = []

    for row in rows:
        num = row[0]
        lhs_name = row[1]
        encoded = row[4]

        # Columns G/H = terminal code table, J/K = nonterminal code table.
        term_code, term_name = row[6], row[7]
        nt_code, nt_name = row[9], row[10]
        if term_code is not None and term_name is not None:
            term_codes.append(int(term_code))
        if nt_code is not None and nt_name is not None:
            nt_code_by_name[str(nt_name).strip()] = int(nt_code)

        if not isinstance(num, int):
            fail("production row has a non-integer number: %r" % (num,))
        if not encoded:
            fail("production %d has no encoded right-hand side" % num)

        rhs, sem_pos, sem_act = [], [], []
        for tok in str(encoded).split():
            if tok.startswith("s"):
                if len(rhs) < len(sem_pos) or len(sem_pos) != len(sem_act):
                    fail("production %d has malformed semantic actions" % num)
                sem_pos.append(len(rhs))
                sem_act.append(int(tok[1:]))
            elif tok == "000":
                if rhs:
                    fail("production %d mixes epsilon 000 with a non-empty RHS" % num)
            else:
                rhs.append(int(tok))
        productions.append(
            Production(num, int(nt_code_by_name[str(lhs_name).strip()]),
                       rhs, sem_pos, sem_act, None)
        )

    if [p.num for p in productions] != list(range(1, PROD_COUNT + 1)):
        fail("production numbers are not 1..%d in order" % PROD_COUNT)
    if len(set(nt_code_by_name.values())) != MAX_NT:
        fail("expected %d distinct nonterminals, found %d"
             % (MAX_NT, len(set(nt_code_by_name.values()))))
    if len(term_codes) != TERM_COUNT:
        fail("expected %d terminals in the code table, found %d"
             % (TERM_COUNT, len(term_codes)))

    nt_name_by_code = {v: k for k, v in nt_code_by_name.items()}

    # ---- FIRST / FOLLOW (only the nonterminal rows are kept) --------------
    first = [None] * MAX_NT
    follow = [None] * MAX_NT

    ws = wb[SHEET_FIRST]
    for row in ws.iter_rows(min_row=2, values_only=True):
        if row[0] is None:
            continue
        code = int(row[0])
        if code >= NTERM_CODE_BASE:
            first[code - NTERM_CODE_BASE] = parse_code_set(row[2])

    ws = wb[SHEET_FOLLOW]
    for row in ws.iter_rows(min_row=2, values_only=True):
        if row[0] is None:
            continue
        code = int(row[1])
        follow[code - NTERM_CODE_BASE] = parse_code_set(row[2])

    # ---- SELECT ----------------------------------------------------------
    ws = wb[SHEET_SELECT]
    for row in ws.iter_rows(min_row=2, max_row=1 + PROD_COUNT, values_only=True):
        productions[int(row[0]) - 1].select = parse_code_set(row[2])

    # ---- dense prediction table -----------------------------------------
    ws = wb[SHEET_PRED]
    header = [int(c.value) for c in ws[1][2:]]
    if header != sorted(term_codes):
        fail("prediction-table header does not match the %d terminal codes"
             % TERM_COUNT)
    pred = [[-1] * MAX_TERM for _ in range(MAX_NT)]
    for row in ws.iter_rows(min_row=3, max_row=2 + MAX_NT, values_only=True):
        idx = row[0]
        if idx is None:
            continue
        nt = int(row[1] and nt_code_by_name[str(row[1]).strip()])
        if nt - NTERM_CODE_BASE != idx:
            fail("prediction row %r is out of order" % (row[1],))
        for code, value in zip(header, row[2:]):
            pred[idx][code] = -1 if value in (None, "") else int(value)

    check_workbook(productions, first, follow, pred, nt_name_by_code)
    return productions, first, follow, pred


def check_workbook(productions, first, follow, pred, nt_name_by_code):
    """Re-derive the table from SELECT and require the single known exception."""
    for p in productions:
        if not (NTERM_CODE_BASE <= p.lhs < NTERM_CODE_BASE + MAX_NT):
            fail("production %d has an invalid LHS code %d" % (p.num, p.lhs))
        for code in p.rhs:
            is_term = 0 < code < MAX_TERM
            is_nterm = NTERM_CODE_BASE <= code < NTERM_CODE_BASE + MAX_NT
            if not (is_term or is_nterm):
                fail("production %d has an invalid RHS code %d" % (p.num, code))
        for code in p.select:
            if not (0 < code < MAX_TERM):
                fail("production %d has an invalid SELECT code %d" % (p.num, code))
        if len(p.sem_pos) != len(p.sem_act):
            fail("production %d has mismatched semantic-action vectors" % p.num)
        if p.sem_pos != sorted(p.sem_pos) or any(
                not (0 <= pos <= len(p.rhs)) for pos in p.sem_pos):
            fail("production %d has an out-of-range semantic position" % p.num)
        if not p.select:
            fail("production %d has an empty SELECT set" % p.num)

    for idx in range(MAX_NT):
        if not first[idx]:
            fail("nonterminal %d has an empty FIRST set" % (idx + NTERM_CODE_BASE))
        if not follow[idx]:
            fail("nonterminal %d has an empty FOLLOW set" % (idx + NTERM_CODE_BASE))

    unexpected = []
    for idx in range(MAX_NT):
        nt = idx + NTERM_CODE_BASE
        owners = [p for p in productions if p.lhs == nt]
        for term in range(MAX_TERM):
            hits = [p.num for p in owners if term in p.select]
            got = pred[idx][term]
            if len(hits) > 1:
                if (nt, term, got) != (DANGLING_ELSE_NT, DANGLING_ELSE_TERM,
                                       DANGLING_ELSE_PROD):
                    unexpected.append((nt, term, "conflict", hits, got))
            elif len(hits) == 1:
                if got != hits[0]:
                    unexpected.append((nt, term, hits[0], hits, got))
            elif got != -1:
                unexpected.append((nt, term, None, hits, got))

    for nt, term, expected, hits, got in unexpected:
        print("prediction mismatch: NT %d x terminal %d -> expected %s, "
              "SELECT owners %s, table says %d"
              % (nt, term, expected, hits, got))
    if unexpected:
        fail("%d mismatch(es) between the prediction table and the SELECT sets"
             % len(unexpected))

    print("workbook OK: %d productions, %d nonterminals, %d terminals; "
          "dangling-else cell %s x else -> %d"
          % (len(productions), MAX_NT, TERM_COUNT,
             nt_name_by_code[DANGLING_ELSE_NT], pred[DANGLING_ELSE_NT - NTERM_CODE_BASE][
                 DANGLING_ELSE_TERM]))


def fmt_int_list(codes, empty="{}"):
    if not codes:
        return empty
    return "{" + ", ".join(str(c) for c in codes) + "}"


def emit_grammar_cpp(productions, first, follow, pred):
    out = []
    out.append("// ============================================================================")
    out.append("// MiniC compiler -- LL(1) grammar tables (design doc v4.0, section 5.3.1)")
    out.append("//")
    out.append("// GENERATED FILE -- do not edit by hand.")
    out.append("// Regenerate with: python tools/generate_grammar.py")
    out.append("//")
    out.append("// Indexing (see the encoding tables in sections 4.1 / 4.2 of the doc):")
    out.append("//   terminal     001..054, 099  -> used directly as an index")
    out.append("//   nonterminal  100..144      -> index = code - NTERM_CODE_BASE (0..44)")
    out.append("//   production   1..105        -> index = production number (0 = sentinel)")
    out.append("//   epsilon      000           -> represented as an empty m_Rhs")
    out.append("// ============================================================================")
    out.append('#include "parser/grammar.h"')
    out.append("")
    out.append("// ----------------------------------------------------------------------------")
    out.append("// Production table: {m_iNum, m_iLhs, m_Rhs, m_SemPos, m_SemAct}")
    out.append("// The trailing comment reads: [number] LHS -> RHS | sem: action@position")
    out.append("// ----------------------------------------------------------------------------")
    out.append("const Production g_Grammar[PROD_TABLE_SIZE] =")
    out.append("{")
    out.append("    Production{0, 0, {}, {}, {}},   // [ 0] sentinel (unused)")
    for p in productions:
        rhs = " ".join(str(c) for c in p.rhs) if p.rhs else "EPS"
        acts = ", ".join("%d@%d" % (a, pos)
                         for pos, a in zip(p.sem_pos, p.sem_act)) or "-"
        out.append("    Production{%d, %d, %s, %s, %s},"
                   % (p.num, p.lhs, fmt_int_list(p.rhs),
                      fmt_int_list(p.sem_pos), fmt_int_list(p.sem_act)))
        out[-1] += "   // [%2d] %3d -> %-46s | sem: %s" % (p.num, p.lhs, rhs, acts)
    out.append("};")
    out.append("")

    for name, sets in (("g_FirstSet", first), ("g_FollowSet", follow)):
        out.append("// %s: 0 = epsilon (present only for nullable nonterminals)"
                   % name)
        out.append("const std::set<int> %s[MAX_NT] =" % name)
        out.append("{")
        for idx, codes in enumerate(sets):
            out.append("    %s,   // [%2d] %3d" % (fmt_int_list(sorted(codes)),
                                                   idx, idx + NTERM_CODE_BASE))
        out.append("};")
        out.append("")

    out.append("// g_SelectSet: index = production number (0 = sentinel, unused)")
    out.append("const std::set<int> g_SelectSet[PROD_TABLE_SIZE] =")
    out.append("{")
    out.append("    {},   // [ 0] sentinel (unused)")
    for p in productions:
        out.append("    %s,   // [%2d]" % (fmt_int_list(sorted(p.select)), p.num))
    out.append("};")
    out.append("")

    out.append("// g_PredictTable: M[nonterminal - NTERM_CODE_BASE][terminal code]")
    out.append("// Row 0..44 = nonterminal codes 100..144; column = terminal code")
    out.append("// (only 001..012, 021..028, 031..046, 051..054 and 099 are ever used).")
    out.append("// -1 = error entry. Section 4.8: 否则部分 x else = 42 (dangling else).")
    out.append("const int g_PredictTable[MAX_NT][MAX_TERM] =")
    out.append("{")
    columns = 12
    for idx in range(MAX_NT):
        cells = [("%d" % v).rjust(2) for v in pred[idx]]
        chunks = [cells[i:i + columns] for i in range(0, MAX_TERM, columns)]
        for k, chunk in enumerate(chunks):
            pad = "    /* [%2d] %3d */ {" % (idx, idx + NTERM_CODE_BASE) if k == 0 \
                else " " * 22
            suffix = " }," if k == len(chunks) - 1 else ","
            out.append(pad + ", ".join(chunk) + suffix)
    out.append("};")
    out.append("")
    return "\n".join(out)


def main():
    default_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--xlsx", default=os.path.join(
        os.path.dirname(default_root), "codex", "MiniC挑战版文法及其相关表格.xlsx"),
        help="path to the challenge-edition workbook")
    parser.add_argument("--outdir", default=os.path.join(default_root, "src", "parser"),
        help="directory that receives grammar.cpp")
    args = parser.parse_args()

    if not os.path.isfile(args.xlsx):
        fail("workbook not found: %s" % args.xlsx)

    productions, first, follow, pred = load_workbook_data(args.xlsx)
    text = emit_grammar_cpp(productions, first, follow, pred)

    destination = os.path.join(args.outdir, "grammar.cpp")
    with open(destination, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
    print("wrote %s (%d lines)" % (destination, text.count("\n") + 1))


if __name__ == "__main__":
    main()
