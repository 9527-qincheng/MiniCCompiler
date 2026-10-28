# ================================================================
# MiniC 编译器 —— 目标代码（x86-64 / GNU as / Windows MinGW）
# 由 CCodeGen::IRtoASM 生成：朴素栈式翻译，逐四元式 load-运算-store
# 入口：global main；调用约定：栈传参（自定义），printf 走 Win64 ABI
# ================================================================
    .globl main

# ---------------- 数据段 ----------------
    .data

    .bss
    .align 8
a:
    .zero 8
    .align 8
b:
    .zero 8

# ---------------- 代码段 ----------------
    .text

# ----------------------------------------------------------------
# main（顶层语句） 栈帧 32 字节
# ----------------------------------------------------------------
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $32, %rsp
    movl $1, %eax
    cvtsi2ss %eax, %xmm0
    movss %xmm0, -8(%rbp)
    movss b(%rip), %xmm0
    movss -8(%rbp), %xmm1
    addss %xmm1, %xmm0
    movss %xmm0, -16(%rbp)
    movss -16(%rbp), %xmm0
    cvttss2si %xmm0, %eax
    movl %eax, a(%rip)
    xorl %eax, %eax
.Lret0:
    movq %rbp, %rsp
    popq %rbp
    ret

# ---- end of generated code ----
