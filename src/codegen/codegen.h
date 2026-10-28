// ============================================================================
// MiniC 编译器 —— 目标代码生成模块 CCodeGen（第 4.6 节 + 第 6.6 节接口契约）
// Q3 定稿：挑战版做 x86 汇编（启用 §4.6 骨架）；课程最低要求止于四元式
// 阶段 08：实现 IRtoASM（四元式 → x86-64 汇编，Windows/MinGW 口径）
// ============================================================================
#ifndef MINIC_CODEGEN_CODEGEN_H
#define MINIC_CODEGEN_CODEGEN_H

#include "sema/ir.h"

#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// 4.6.1 指令模板结构（参考书 Pattern/Code，扩展预留，暂不承诺实现）
// ----------------------------------------------------------------------------
struct InstPattern
{
    QuadOp     m_eOp;          // 匹配的四元式操作码
    std::string m_szPattern;   // 指令模板文本
    std::vector<std::string> m_Code; // 展开代码行
};

// 单行汇编输出（参考书 Code）
struct CodeLine
{
    std::string m_szText;      // 汇编文本
    std::string m_szComment;   // 注释
    int         m_iIndent = 0; // 缩进层级
};

// 寄存器标志（参考书声明 9-1~9-5，扩展预留）
enum class RegFlag
{
    REG_NONE,
    REG_INT,
    REG_FLOAT,
    REG_ADDR
};

// ----------------------------------------------------------------------------
// 4.6.2 汇编输出 AsmOutput（文件头/数据段/代码段）
// ----------------------------------------------------------------------------
struct AsmOutput
{
    std::vector<std::string> m_FileHeader;   // 文件头（段声明等）
    std::vector<std::string> m_DataSection;  // 数据段（.data/.bss）
    std::vector<std::string> m_CodeSection;  // 代码段（.text）

    void Clear()
    {
        m_FileHeader.clear();
        m_DataSection.clear();
        m_CodeSection.clear();
    }
};

// ----------------------------------------------------------------------------
// 4.6.3 目标代码生成器 CCodeGen（第 6 章接口契约）
// ----------------------------------------------------------------------------
class CCodeGen
{
public:
    explicit CCodeGen(const CodeList& codes);   // 构造：绑定 IR 输入

    // --- 阶段 08 新增：绑定符号表（变量表/过程表/参数表，用于栈帧与全局定位）---
    // 说明：仅新增，不改变第 6.6 节既有签名；未绑定时 Generate 退化为「仅顶层」模式。
    void BindSymTab(const SymbolTable& symtab);

    // --- 第 6 章接口契约 ---
    bool Generate(AsmOutput& out);              // 生成汇编文本（挑战版）

    // --- 第 4 章扩展接口 ---
    bool IRtoASM(const CodeList& codes, AsmOutput& out); // 四元式 → x86 汇编

private:
    const CodeList& m_codes;                    // 绑定 IR 输入
    const SymbolTable* m_symtab = nullptr;      // 绑定符号表（阶段 08 新增，可为空）
    std::vector<InstPattern> m_TemplateLib;     // 指令模板库（参考书 Pattern 的预留结构）
                                                // 说明：挑战版按 QuadOp 的 switch 直接翻译
                                                // （见 codegen.cpp 的 AsmEmitter），本表未填充，
                                                // 保留仅为对齐设计文档 §4.6.2 的数据结构定义。
};

#endif // MINIC_CODEGEN_CODEGEN_H
