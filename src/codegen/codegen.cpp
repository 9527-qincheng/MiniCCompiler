// ============================================================================
// MiniC 编译器 —— 目标代码生成模块 CCodeGen 实现
// 状态：骨架版——接口/输出结构完整，四元式→x86 汇编核心逻辑留 TODO。
// TODO（下一阶段，挑战版 Q3）：
//   1. IRtoASM：指令模板匹配（m_TemplateLib）+ 寄存器分配（§4.6 骨架）
//   2. AsmOutput 三段的填充（文件头/数据段/代码段）
// ============================================================================
#include "codegen/codegen.h"

#include <iostream>

CCodeGen::CCodeGen(const CodeList& codes)
    : m_codes(codes)
{
}

bool CCodeGen::Generate(AsmOutput& out)
{
    // 挑战版入口：绑定 IR 输入已由构造完成，生成汇编文本
    return IRtoASM(m_codes, out);
}

bool CCodeGen::IRtoASM(const CodeList& codes, AsmOutput& out)
{
    // TODO: 四元式 → x86 汇编
    //   1. 遍历 codes，按 m_TemplateLib 指令模板匹配
    //   2. 变量/常量分配 .data 数据段（m_DataSection）
    //   3. 生成 .text 代码段（m_CodeSection）
    out.Clear();

    // 骨架占位输出（挑战版实现前的可演示结构）
    out.m_FileHeader.push_back("; MiniC target code (challenge) - TODO");
    out.m_FileHeader.push_back("section .data");
    out.m_FileHeader.push_back("section .text");
    out.m_FileHeader.push_back("global _start");
    out.m_FileHeader.push_back("_start:");
    out.m_FileHeader.push_back("    ; TODO: IR-to-ASM translation not implemented yet");
    out.m_FileHeader.push_back("    ; input IR quads: " + std::to_string(codes.size()));

    return true;
}
