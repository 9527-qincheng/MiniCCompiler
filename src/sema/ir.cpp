// ============================================================================
// MiniC 编译器 —— 中间代码模块 IRGenerator 实现
// 状态：骨架版——临时变量/标号计数、EmitIR、GetCodes、OperandToString 可用；
//       遍历 AST 生成四元式的核心逻辑留 TODO（词法模块先行演示）。
// TODO（下一阶段）：
//   1. Generate() 递归遍历 AST：表达式翻译（操作数栈+运算符栈）→ EmitIR
//   2. 类型转换按 g_TypeSysTable（int/float 少量规则）
//   3. 挑战版：控制流（GetTmpLabel + 跳转四元式）、print、数组、函数
// ============================================================================
#include "sema/ir.h"

#include <iostream>

// ----------------------------------------------------------------------------
// Operand 便捷构造
// ----------------------------------------------------------------------------
Operand Operand::Const(int idx, TokenType t)
{
    Operand op;
    op.m_kind = OperandKind::OP_CONST;
    op.m_iLink = idx;
    op.m_type = t;
    return op;
}

Operand Operand::Var(int idx, TokenType t)
{
    Operand op;
    op.m_kind = OperandKind::OP_VAR;
    op.m_iLink = idx;
    op.m_type = t;
    return op;
}

Operand Operand::Temp(int idx, TokenType t)
{
    Operand op;
    op.m_kind = OperandKind::OP_TEMP;
    op.m_iLink = idx;
    op.m_type = t;
    return op;
}

Operand Operand::Label(int idx)
{
    Operand op;
    op.m_kind = OperandKind::OP_LABEL;
    op.m_iLink = idx;
    return op;
}

// ----------------------------------------------------------------------------
// Quad::Print —— 四元式打印（交付物 --dump-quads）
// ----------------------------------------------------------------------------
void Quad::Print(std::ostream& os) const
{
    static const char* opName[] = {
        "NONE", "=", "+", "-", "*", "/",
        "JMP", "JZ", "JNZ", "LABEL", "RETURN", "CALL", "PRINT"
    };
    const char* name = (static_cast<int>(m_eOp) < 13) ? opName[static_cast<int>(m_eOp)] : "??";
    os << "(" << name << ", " << m_Op1.m_iLink << ", " << m_Op2.m_iLink
       << ", " << m_Rslt.m_iLink << ")  @" << m_loc.m_iRow << ":" << m_loc.m_iCol << "\n";
}

// ----------------------------------------------------------------------------
// IRGenerator
// ----------------------------------------------------------------------------
IRGenerator::IRGenerator(SymbolTable& symtab)
    : m_iTmpVarCounter(1), m_iLabelCounter(1), m_symtab(symtab)
{
}

int IRGenerator::GetTmpVar(TokenType t)
{
    (void)t; // TODO：按类型登记临时变量类型（当前仅计数）
    return m_iTmpVarCounter++;
}

int IRGenerator::GetTmpLabel()
{
    return m_iLabelCounter++;
}

void IRGenerator::EmitIR(QuadOp op, const Operand& op1, const Operand& op2, const Operand& rslt)
{
    Quad q(op, op1, op2, rslt);
    q.m_loc = LineInfo(0, 0);
    m_Codes.push_back(q);
}

const CodeList& IRGenerator::GetCodes() const
{
    return m_Codes;
}

std::string IRGenerator::OperandToString(const Operand& op) const
{
    // Q6：操作数索引转原文（打印用）
    switch (op.m_kind)
    {
    case OperandKind::OP_NONE:
        return "-";
    case OperandKind::OP_CONST:
    {
        // 查常量表原文（m_szVal 词法登记文本；未登记则回退编号）
        if (op.m_iLink >= 0 && static_cast<size_t>(op.m_iLink) < m_symtab.m_ConstInfoTbl.size())
        {
            const ConstEntry& ce = m_symtab.m_ConstInfoTbl[op.m_iLink];
            return ce.m_szVal.empty() ? std::to_string(ce.m_iValue) : ce.m_szVal;
        }
        return "C" + std::to_string(op.m_iLink);
    }
    case OperandKind::OP_VAR:
    {
        if (op.m_iLink >= 0 && static_cast<size_t>(op.m_iLink) < m_symtab.m_VarInfoTbl.size())
        {
            return m_symtab.m_VarInfoTbl[op.m_iLink].m_szName;
        }
        return "V" + std::to_string(op.m_iLink);
    }
    case OperandKind::OP_TEMP:
        return "_T" + std::to_string(op.m_iLink);
    case OperandKind::OP_LABEL:
        return "L" + std::to_string(op.m_iLink);
    }
    return "?";
}

void IRGenerator::Generate(const ASTNodePtr& root)
{
    // TODO: 遍历 AST 生成四元式序列
    //   示例：AssignNode(a, BinOp(b, +, 1))
    //      t1 = GetTmpVar(TOKEN_FLOAT); EmitIR(Q_ADD, op(b), op(1), Temp(t1))
    //      EmitIR(Q_ASSIGN, Temp(t1), OP_NONE, op(a))
    if (!root)
    {
        return;
    }
    // 骨架阶段：不生成四元式（留空 CodeList），仅供 --dump-quads 演示空输出
}

void IRGenerator::FlushToProc(ProcEntry& proc)
{
    // TODO: 将 m_Codes 挂到过程表项（挑战版）
    proc.m_Codes = m_Codes;
    m_Codes.clear();
}
