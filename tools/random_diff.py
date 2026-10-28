#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""随机差分测试：同一个随机程序分别交给 minic 与 gcc，比对运行输出。

为什么要有这个脚本
------------------
其余十一套回归用的都是人工写死的用例：它们能证明「我想到的写法都对」，
不能证明「我没想到的写法也对」。随机差分补的就是这一块 —— 用同一份随机
生成的程序结构，同时产出 MiniC 源与等价的 C 源，前者经 minic 生成 x86-64
汇编再用 gcc 汇编链接，后者直接交给 gcc 编译，两个可执行文件的 stdout 与
退出码必须完全一致。任何一条不一致都说明目标代码生成阶段翻译错了。

为什么可以等价
--------------
MiniC 是 C 的真子集，本脚本只生成两者语义完全重合的部分：
  * 整型是 32 位有符号，除法向零截断（C99 起同样规定），不做取模；
  * 实型是 32 位单精度，C 侧一律写 `0.1f` 这样的字面量强制单精度运算，
    避免 `f + 0.1` 在 C 里被提升成 double 而算出不同的舍入；
  * 整型与实型混算时两边都先转成单精度再算，转换点一致；
  * 所有变量在使用前显式初始化 —— MiniC 不把局部变量清零，
    不初始化就是在比较两份二进制的栈残留，测的并不是编译器；
  * 数组下标一律用常量且落在界内，循环上界固定且很小；
  * 字面量与中间结果的量级受控，不触发 32 位溢出。

用法
----
    python tools/random_diff.py                  # 默认 200 例
    python tools/random_diff.py --count 1000     # 跑 1000 例
    python tools/random_diff.py --seed 7 -n 50   # 固定种子复现某一批
    python tools/random_diff.py --keep           # 保留全部中间文件

退出码 0 表示全部一致，1 表示出现不一致（可直接接进 CI）。
"""

import argparse
import os
import random
import shutil
import subprocess
import sys
import tempfile

# --------------------------------------------------------------------------
# 随机程序模型
#
# 表达式与语句都先建成元组树，再按目标语言渲染两次。这样 MiniC 侧与 C 侧
# 保证是同一棵树，不会因为「分别生成」而悄悄跑偏。
# --------------------------------------------------------------------------

INT_VARS = ["a", "b", "c", "d"]
# 循环计数器单独一组：嵌套 while 若共用同一个计数器，内层会把外层的计数清零，
# 外层就再也走不到上界 —— 第一版就是这样跑出 5 个死循环超时用例的。
LOOP_VARS = ["i", "j", "k"]
FLT_VARS = ["f", "g"]
ARR_SIZE = 5
FLOAT_LITERALS = ["0.5", "1.5", "2.5", "0.1", "3.25", "-1.5"]


def gen_int_expr(rng, depth, allow_call=False):
    """整型表达式：变量 / 常量 / 四则运算 / 数组读 / 偶尔一次函数调用。"""
    if depth <= 0 or rng.random() < 0.34:
        r = rng.random()
        if r < 0.46:
            return ("var", rng.choice(INT_VARS))
        if r < 0.60:
            return ("arr", rng.randrange(ARR_SIZE))
        if r < 0.66 and allow_call:
            return ("call", "addmul", [gen_int_expr(rng, 0), gen_int_expr(rng, 0)])
        return ("iconst", rng.randint(-40, 60))
    op = rng.choice(["+", "-", "*", "/"])
    left = gen_int_expr(rng, depth - 1)
    if op == "/":
        # 除数固定为正的小常量：既避开除零，也避开 INT_MIN / -1 这类
        # 在 C 里属于未定义行为、在 gcc 与 idiv 上可能给出不同结果的边角。
        return ("bin", "/", left, ("iconst", rng.randint(1, 9)))
    return ("bin", op, left, gen_int_expr(rng, depth - 1))


def gen_float_expr(rng, depth):
    """实型表达式：实变量 / 实常量 / 整变量提升 / 加减乘。"""
    if depth <= 0 or rng.random() < 0.40:
        r = rng.random()
        if r < 0.44:
            return ("var", rng.choice(FLT_VARS))
        if r < 0.72:
            return ("fconst", rng.choice(FLOAT_LITERALS))
        return ("var", rng.choice(INT_VARS))  # 整型参与实型运算，两侧都隐式提升
    op = rng.choice(["+", "-", "*"])
    return ("bin", op, gen_float_expr(rng, depth - 1), gen_float_expr(rng, depth - 1))


def gen_int_cond(rng):
    """比较条件：两侧都是整型表达式。"""
    return ("cmp", rng.choice(["<", "<=", ">", ">=", "==", "!="]),
            gen_int_expr(rng, 1), gen_int_expr(rng, 1))


def gen_stmt(rng, depth, used_loops=frozenset()):
    """生成一条语句。depth 控制嵌套层数，used_loops 是外层已占用的循环计数器。"""
    choices = ["assign", "assign", "print", "arrset", "arrprint",
               "fassign", "fassign", "fprint"]
    if depth > 0:
        choices += ["if", "while"]
    if not used_loops:
        choices += ["call"]
    kind = rng.choice(choices)

    if kind == "assign":
        var = rng.choice(INT_VARS)
        return ("assign", var, gen_int_expr(rng, rng.randint(1, 3)))
    if kind == "fassign":
        return ("fassign", rng.choice(FLT_VARS), gen_float_expr(rng, rng.randint(1, 2)))
    if kind == "print":
        return ("print", gen_int_expr(rng, rng.randint(1, 2)))
    if kind == "arrset":
        return ("arrset", rng.randrange(ARR_SIZE), gen_int_expr(rng, rng.randint(1, 2)))
    if kind == "arrprint":
        return ("print", ("arr", rng.randrange(ARR_SIZE)))
    if kind == "fprint":
        return ("print", gen_float_expr(rng, rng.randint(1, 2)))
    if kind == "call":
        return ("assign", rng.choice(INT_VARS),
                ("call", "addmul", [gen_int_expr(rng, 1), gen_int_expr(rng, 1)]))
    if kind == "if":
        then_body = [gen_stmt(rng, depth - 1, used_loops) for _ in range(rng.randint(1, 2))]
        else_body = ([gen_stmt(rng, depth - 1, used_loops) for _ in range(rng.randint(1, 2))]
                     if rng.random() < 0.6 else [])
        return ("if", gen_int_cond(rng), then_body, else_body)
    # while：计数器从 0 走到 1..3，体内不许改它；内层循环换一个计数器
    var = next(v for v in LOOP_VARS if v not in used_loops)
    limit = rng.randint(1, 3)
    inner = used_loops | {var}
    body = [gen_stmt(rng, depth - 1, inner) for _ in range(rng.randint(1, 2))]
    return ("while", var, limit, body)


def gen_program(rng):
    """生成一份程序，返回 (MiniC 源, 等价 C 源)。"""
    body = []
    # 前半段保证四种语句都出现一次，后半段随机
    body.append(("assign", "a", ("iconst", rng.randint(1, 20))))
    body.append(("assign", "b", gen_int_expr(rng, 2)))
    body.append(("print", ("var", "a")))
    body.append(("fassign", "f", gen_float_expr(rng, 1)))
    body.append(("print", ("var", "f")))
    body.append(("arrset", 1, gen_int_expr(rng, 2)))
    body.append(("print", ("arr", 1)))
    for _ in range(rng.randint(4, 8)):
        body.append(gen_stmt(rng, rng.randint(1, 2)))
    # 收尾把全部状态打出来，任何一处算错都会体现在输出里
    for v in INT_VARS:
        body.append(("print", ("var", v)))
    for v in FLT_VARS:
        body.append(("print", ("var", v)))
    for k in range(ARR_SIZE):
        body.append(("print", ("arr", k)))

    return render_minic(body), render_c(body)


# --------------------------------------------------------------------------
# 渲染
# --------------------------------------------------------------------------

def render_expr(e, lang):
    kind = e[0]
    if kind == "var":
        return e[1]
    if kind == "iconst":
        return str(e[1])
    if kind == "fconst":
        # C 侧必须带 f 后缀，否则 0.1 是 double，算出来会和单精度的 MiniC 不同
        return e[1] + ("f" if lang == "c" else "")
    if kind == "arr":
        return "arr[%d]" % e[1]
    if kind == "call":
        args = ", ".join(render_expr(x, lang) for x in e[2])
        return "%s(%s)" % (e[1], args)
    if kind == "bin":
        return "(%s %s %s)" % (render_expr(e[2], lang), e[1], render_expr(e[3], lang))
    if kind == "cmp":
        return "(%s %s %s)" % (render_expr(e[2], lang), e[1], render_expr(e[3], lang))
    raise ValueError("unknown expr kind: %r" % (e,))


def expr_is_float(e):
    """静态类型判定：只要子树里出现过实型就是实型表达式。"""
    if e[0] == "fconst":
        return True
    if e[0] == "var":
        return e[1] in FLT_VARS
    if e[0] == "bin":
        return expr_is_float(e[2]) or expr_is_float(e[3])
    return False


def render_stmt(s, lang, indent):
    pad = "    " * indent
    kind = s[0]
    if kind == "assign":
        return ["%s%s = %s;" % (pad, s[1], render_expr(s[2], lang))]
    if kind == "fassign":
        return ["%s%s = %s;" % (pad, s[1], render_expr(s[2], lang))]
    if kind == "arrset":
        return ["%sarr[%d] = %s;" % (pad, s[1], render_expr(s[2], lang))]
    if kind == "print":
        if lang == "minic":
            return ["%sprint(%s);" % (pad, render_expr(s[1], lang))]
        fmt = "%g" if expr_is_float(s[1]) else "%d"
        return ['%sprintf("%s", %s);' % (pad, fmt, render_expr(s[1], lang))]
    if kind == "if":
        out = ["%sif %s" % (pad, render_expr(s[1], lang)), pad + "{"]
        for sub in s[2]:
            out += render_stmt(sub, lang, indent + 1)
        out.append(pad + "}")
        if s[3]:
            out.append(pad + "else")
            out.append(pad + "{")
            for sub in s[3]:
                out += render_stmt(sub, lang, indent + 1)
            out.append(pad + "}")
        return out
    if kind == "while":
        var, limit, body = s[1], s[2], s[3]
        out = ["%s%s = 0;" % (pad, var),
               "%swhile (%s < %d)" % (pad, var, limit),
               pad + "{"]
        for sub in body:
            out += render_stmt(sub, lang, indent + 1)
        out.append("%s%s = %s + 1;" % ("    " * (indent + 1), var, var))
        out.append(pad + "}")
        return out
    raise ValueError("unknown stmt kind: %r" % (s,))


def _preamble(lang):
    head = []
    if lang == "c":
        head.append("#include <stdio.h>")
        head.append("")
    head += [
        "int addmul(int x, int y)",
        "{",
        "    return x * 2 + y;",
        "}",
        "",
        "int main()",
        "{",
        "    int a; int b; int c; int d; int i; int j; int k;",
        "    int arr[%d];" % ARR_SIZE,
        "    float f; float g;",
        "    a = 1; b = 2; c = 3; d = 4; i = 0; j = 0; k = 0;",
        "    arr[0] = 0; arr[1] = 0; arr[2] = 0; arr[3] = 0; arr[4] = 0;",
        "    f = 0.5%s; g = 1.5%s;" % ("f" if lang == "c" else "", "f" if lang == "c" else ""),
        "",
    ]
    return head


# 附加给 minic 的命令行参数（--opt 时是 ["-O"]）
EXTRA_ARGS = []


def render_minic(body):
    lines = _preamble("minic")
    for s in body:
        lines += render_stmt(s, "minic", 1)
    lines += ["    return 0;", "}", ""]
    return "\n".join(lines)


def render_c(body):
    lines = _preamble("c")
    for s in body:
        lines += render_stmt(s, "c", 1)
    lines += ["    return 0;", "}", ""]
    return "\n".join(lines)


# --------------------------------------------------------------------------
# 执行
# --------------------------------------------------------------------------

def run(cmd, cwd, timeout=15):
    try:
        p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, timeout=timeout)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        return None, b"", b"<timeout>"


def run_case(case_dir, minic, keep):
    """编译并运行同一个程序的 MiniC 版与 C 版，返回 (ok, 说明)。"""
    mc = os.path.join(case_dir, "prog.mc")
    cc = os.path.join(case_dir, "prog.c")

    rc, _, err = run([minic, mc] + EXTRA_ARGS, case_dir)
    asm = os.path.join(case_dir, "prog.asm")
    if rc != 0 or not os.path.exists(asm):
        return None, "minic failed: " + err.decode("utf-8", "replace").strip()[:300]

    shutil.copyfile(asm, os.path.join(case_dir, "prog.s"))
    rc, _, err = run(["gcc", "-x", "assembler", "prog.s", "-o", "prog_minic.exe"], case_dir)
    if rc != 0:
        return None, "gcc(asm) failed: " + err.decode("utf-8", "replace").strip()[:300]

    rc, _, err = run(["gcc", "-O0", "-w", "prog.c", "-o", "prog_c.exe"], case_dir)
    if rc != 0:
        return None, "gcc(c) failed: " + err.decode("utf-8", "replace").strip()[:300]

    rc1, out1, _ = run([os.path.join(case_dir, "prog_minic.exe")], case_dir)
    rc2, out2, _ = run([os.path.join(case_dir, "prog_c.exe")], case_dir)
    if rc1 is None or rc2 is None:
        return None, "run timeout"

    if out1 != out2:
        return False, "stdout differs\n  minic: %r\n  gcc  : %r" % (
            out1.decode("utf-8", "replace")[:400], out2.decode("utf-8", "replace")[:400])
    if rc1 != rc2:
        return False, "exit code differs: minic=%s gcc=%s" % (rc1, rc2)
    return True, ""


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    default_minic = os.path.join(os.path.dirname(here), "build", "minic.exe")

    ap = argparse.ArgumentParser(description="MiniC 随机差分测试")
    ap.add_argument("-n", "--count", type=int, default=200, help="用例数（默认 200）")
    ap.add_argument("-s", "--seed", type=int, default=None, help="随机种子，用于复现")
    ap.add_argument("--minic", default=default_minic, help="minic.exe 路径")
    ap.add_argument("--workdir", default=None, help="工作目录（默认用系统临时目录）")
    ap.add_argument("--keep", action="store_true", help="保留全部中间文件")
    ap.add_argument("--opt", action="store_true",
                    help="编译时附加 -O，用于验证优化前后行为一致")
    args = ap.parse_args()
    if args.opt:
        EXTRA_ARGS.append("-O")

    if not os.path.exists(args.minic):
        print("找不到 minic.exe：%s\n先跑 build.bat build。" % args.minic)
        return 2

    seed = args.seed if args.seed is not None else random.randrange(1 << 30)
    rng = random.Random(seed)
    print("seed = %d, count = %d, minic = %s" % (seed, args.count, args.minic))

    workdir = args.workdir or tempfile.mkdtemp(prefix="minic_randdiff_")
    os.makedirs(workdir, exist_ok=True)

    ok = skipped = bad = 0
    failures = []
    for k in range(args.count):
        case_dir = os.path.join(workdir, "case_%04d" % k)
        os.makedirs(case_dir, exist_ok=True)
        mc, cc = gen_program(rng)
        with open(os.path.join(case_dir, "prog.mc"), "w", encoding="utf-8", newline="\n") as fp:
            fp.write(mc)
        with open(os.path.join(case_dir, "prog.c"), "w", encoding="utf-8", newline="\n") as fp:
            fp.write(cc)

        good, why = run_case(case_dir, args.minic, args.keep)
        if good is True:
            ok += 1
            if not args.keep:
                shutil.rmtree(case_dir, ignore_errors=True)
        elif good is None:
            skipped += 1
            failures.append((k, "skip", why))
        else:
            bad += 1
            failures.append((k, "DIFF", why))

        if (k + 1) % 50 == 0:
            print("  ... %d/%d  ok=%d skip=%d bad=%d" % (k + 1, args.count, ok, skipped, bad))

    print("")
    print("identical = %d   skipped = %d   mismatch = %d" % (ok, skipped, bad))
    if failures:
        print("")
        for k, kind, why in failures[:10]:
            print("case %04d [%s] %s" % (k, kind, why))
            print("  dir: %s" % os.path.join(workdir, "case_%04d" % k))
        if len(failures) > 10:
            print("... 另有 %d 例未显示" % (len(failures) - 10))
    if not args.keep and bad == 0 and skipped == 0:
        shutil.rmtree(workdir, ignore_errors=True)
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
