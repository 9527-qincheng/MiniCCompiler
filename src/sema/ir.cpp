// ============================================================================
// MiniC 编译器 —— 中间代码模块 IRGenerator 实现
// 状态：阶段 07 已实现（张灏宇）——
//   1. Generate() 递归遍历 AST：表达式翻译（EvalExpr）→ EmitIR
//   2. 类型转换按设计文档 §8.1（int/float 少量规则，ITOF/FTOI）
//   3. 挑战版：控制流（GetTmpLabel + JZ/JMP/LABEL 标号）、短路求值、
//      print、数组（QuadOp::Q_INDEX）、函数（QuadOp::Q_PARA + QuadOp::Q_CALL）
// 接口约定（Q6）：操作数引用式（表索引），打印时按索引转原文（OperandToString）
// ============================================================================
#include "sema/ir.h"

#include <cstdio>
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
// 注：本方法按「裸索引」打印（Q6 约定操作数打印原文由 main.cpp 经
//     IRGenerator::OperandToString 完成，此处保留参考书形态）。
// ----------------------------------------------------------------------------
void Quad::Print(std::ostream& os) const
{
    static const char* opName[] = {
        "NONE", "=", "+", "-", "*", "/",
        "JMP", "JZ", "JNZ", "LABEL", "RETURN", "CALL", "PRINT",
        "PARA", "INDEX", "ITOF", "FTOI",
        "==", "!=", "<", "<=", ">", ">="
    };
    const char* name = (static_cast<int>(m_eOp) < 23) ? opName[static_cast<int>(m_eOp)] : "??";
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
    // 临时量类型登记（编号与 m_TmpTypes 一一对应：_Tk 的类型在 m_TmpTypes[k-1]）
    m_TmpTypes.push_back(t);
    return m_iTmpVarCounter++;
}

int IRGenerator::GetTmpLabel()
{
    return m_iLabelCounter++;
}

void IRGenerator::EmitIR(QuadOp op, const Operand& op1, const Operand& op2, const Operand& rslt)
{
    EmitIR(op, op1, op2, rslt, LineInfo(0, 0));
}

void IRGenerator::EmitIR(QuadOp op, const Operand& op1, const Operand& op2,
                         const Operand& rslt, const LineInfo& loc)
{
    Quad q(op, op1, op2, rslt);
    q.m_loc = loc;
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
        // 查常量表原文（m_szVal 词法登记文本；语义登记未存文本则按类型回退）
        if (op.m_iLink >= 0 && static_cast<size_t>(op.m_iLink) < m_symtab.m_ConstInfoTbl.size())
        {
            const ConstEntry& ce = m_symtab.m_ConstInfoTbl[op.m_iLink];
            if (!ce.m_szVal.empty())
            {
                return ce.m_szVal;
            }
            // 语义 RecConst 登记（Q12 回填）未保存词法文本：按常量类型格式化原文
            if (ce.m_constType == TokenType::TOKEN_FLOAT ||
                ce.m_constType == TokenType::TOKEN_NUM_FLOAT || ce.m_bIsFloat)
            {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%g", ce.m_fValue);
                return buf;
            }
            return std::to_string(ce.m_iValue);
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

// ----------------------------------------------------------------------------
// 类型工具（与语义模块 §8.1 对齐）
// ----------------------------------------------------------------------------
namespace
{
bool IsFloatType(TokenType t)
{
    return t == TokenType::TOKEN_FLOAT || t == TokenType::TOKEN_NUM_FLOAT;
}

bool IsIntType(TokenType t)
{
    // char 在算术中按整数处理（C 语义）；bool 不参与算术
    return t == TokenType::TOKEN_INT || t == TokenType::TOKEN_NUM_INT ||
           t == TokenType::TOKEN_CHAR;
}
} // namespace

TokenType IRGenerator::OperandType(const Operand& op) const
{
    switch (op.m_kind)
    {
    case OperandKind::OP_CONST:
        if (op.m_iLink >= 0 && static_cast<size_t>(op.m_iLink) < m_symtab.m_ConstInfoTbl.size())
        {
            return m_symtab.m_ConstInfoTbl[op.m_iLink].m_constType;
        }
        return TokenType::TOKEN_NONE;
    case OperandKind::OP_VAR:
        if (op.m_iLink >= 0 && static_cast<size_t>(op.m_iLink) < m_symtab.m_VarInfoTbl.size())
        {
            return m_symtab.m_VarInfoTbl[op.m_iLink].m_type;
        }
        return TokenType::TOKEN_NONE;
    case OperandKind::OP_TEMP:
        if (op.m_iLink >= 1 && static_cast<size_t>(op.m_iLink - 1) < m_TmpTypes.size())
        {
            return m_TmpTypes[op.m_iLink - 1];
        }
        return TokenType::TOKEN_NONE;
    default:
        return TokenType::TOKEN_NONE;
    }
}

// 取整型常量（0/1 等）：先在常量表中查找，未登记则登记（IR 阶段在 --dump-symtab 之后，不影响符号表产物）
Operand IRGenerator::ConstInt(int value, const LineInfo& loc)
{
    for (std::size_t i = 0; i < m_symtab.m_ConstInfoTbl.size(); ++i)
    {
        const ConstEntry& ce = m_symtab.m_ConstInfoTbl[i];
        if (!ce.m_bIsFloat && !IsFloatType(ce.m_constType) &&
            ce.m_iValue == static_cast<long long>(value))
        {
            return Operand::Const(static_cast<int>(i), TokenType::TOKEN_INT);
        }
    }
    ConstEntry* ce = m_symtab.RecConst(std::to_string(value), TokenType::TOKEN_INT,
                                       static_cast<double>(value), loc.m_iRow, loc.m_iCol);
    if (!ce)
    {
        return Operand();
    }
    return Operand::Const(static_cast<int>(ce - m_symtab.m_ConstInfoTbl.data()),
                          TokenType::TOKEN_INT);
}

Operand IRGenerator::IntToFloatConv(const Operand& op, const LineInfo& loc)
{
    int t = GetTmpVar(TokenType::TOKEN_FLOAT);
    EmitIR(QuadOp::Q_ITOF, op, Operand(), Operand::Temp(t, TokenType::TOKEN_FLOAT), loc);
    return Operand::Temp(t, TokenType::TOKEN_FLOAT);
}

Operand IRGenerator::FloatToIntConv(const Operand& op, const LineInfo& loc)
{
    int t = GetTmpVar(TokenType::TOKEN_INT);
    EmitIR(QuadOp::Q_FTOI, op, Operand(), Operand::Temp(t, TokenType::TOKEN_INT), loc);
    return Operand::Temp(t, TokenType::TOKEN_INT);
}

// ----------------------------------------------------------------------------
// 形参类型表（P2 缺陷修复用）
// 符号表的 ParamEntry 没有过程归属，m_ParamInfoTbl 是所有函数形参混在一起的一张
// 扁平表，无法按被调函数定位形参类型。故在 IR 阶段自己扫一遍 AST 的
// FuncDefNode/ParamNode 建「函数名 → 形参类型列表」，供 Q_PARA 插类型转换。
// ----------------------------------------------------------------------------
void IRGenerator::BuildParamTypeTable(const ASTNodePtr& root)
{
    m_ParamTypes.clear();
    if (!root)
    {
        return;
    }
    // 顶层 Program 的子节点里找 FUNCDEF（MiniC 不支持嵌套函数定义）
    const ASTNode* prog = root.get();
    if (prog->m_kind != ASTNodeKind::AST_NODE_PROGRAM)
    {
        return;
    }
    for (const auto& c : prog->m_children)
    {
        if (!c || c->m_kind != ASTNodeKind::AST_NODE_FUNCDEF)
        {
            continue;
        }
        auto* fn = static_cast<const FuncDefNode*>(c.get());
        std::vector<TokenType> types;
        for (const auto& p : fn->m_children)
        {
            if (p && p->m_kind == ASTNodeKind::AST_NODE_PARAM)
            {
                types.push_back(static_cast<const ParamNode*>(p.get())->m_type);
            }
        }
        m_ParamTypes[fn->m_szName] = types;
    }
}

TokenType IRGenerator::ParamTypeOf(const std::string& procName, int pos) const
{
    const auto it = m_ParamTypes.find(procName);
    if (it == m_ParamTypes.end() || pos < 0 ||
        static_cast<std::size_t>(pos) >= it->second.size())
    {
        return TokenType::TOKEN_NONE;   // 查不到 → 不插转换（保持原行为）
    }
    return it->second[static_cast<std::size_t>(pos)];
}

// ----------------------------------------------------------------------------
// 表达式 → Operand（递归求值，含类型转换插入）
// ----------------------------------------------------------------------------
Operand IRGenerator::EvalExpr(const ASTNodePtr& node)
{
    if (!node)
    {
        return Operand();
    }
    switch (node->m_kind)
    {
    case ASTNodeKind::AST_NODE_NUM:
    {
        auto* num = static_cast<NumNode*>(node.get());
        TokenType t = TokenType::TOKEN_INT;
        if (num->m_bIsFloat)      { t = TokenType::TOKEN_FLOAT; }
        else if (num->m_isChar)   { t = TokenType::TOKEN_CHAR; }
        else if (num->m_isBool)   { t = TokenType::TOKEN_BOOL; }
        return Operand::Const(num->m_iConstIdx, t);
    }
    case ASTNodeKind::AST_NODE_IDENT:
    {
        auto* id = static_cast<IdentNode*>(node.get());
        if (id->m_iSymIdx < 0 ||
            static_cast<size_t>(id->m_iSymIdx) >= m_symtab.m_VarInfoTbl.size())
        {
            return Operand();
        }
        return Operand::Var(id->m_iSymIdx, m_symtab.m_VarInfoTbl[id->m_iSymIdx].m_type);
    }
    case ASTNodeKind::AST_NODE_BINOP:
    {
        auto* bin = static_cast<BinOpNode*>(node.get());
        if (bin->m_children.size() < 2)
        {
            return Operand();
        }
        const TokenType op = bin->m_op;
        const LineInfo& loc = bin->m_loc;

        // 逻辑运算 && / ||：短路求值（JZ/JNZ + 标号 + 常量 0/1）
        // 关键：右操作数必须在跳转四元式之后才展开求值，否则会「先算 b 再判 a」，
        // 右操作数里的函数调用/副作用就失去短路语义（P1 缺陷修复）。
        if (op == TokenType::TOKEN_AND || op == TokenType::TOKEN_OR)
        {
            Operand lo = EvalExpr(bin->m_children[0]);
            if (lo.m_kind == OperandKind::OP_NONE)
            {
                return Operand();
            }
            const int tIdx = GetTmpVar(TokenType::TOKEN_BOOL);
            const int lMid = GetTmpLabel();   // && 的假出口 / || 的真出口
            const int lEnd = GetTmpLabel();
            if (op == TokenType::TOKEN_AND)
            {
                // t = a && b :  a 假 → 置 0 结束；a 真 → t = b
                EmitIR(QuadOp::Q_JZ, lo, Operand(), Operand::Label(lMid), loc);
            }
            else
            {
                // t = a || b :  a 真 → 置 1 结束；a 假 → t = b
                EmitIR(QuadOp::Q_JNZ, lo, Operand(), Operand::Label(lMid), loc);
            }
            Operand ro = EvalExpr(bin->m_children[1]);
            if (ro.m_kind != OperandKind::OP_NONE)
            {
                EmitIR(QuadOp::Q_ASSIGN, ro, Operand(), Operand::Temp(tIdx, TokenType::TOKEN_BOOL), loc);
            }
            EmitIR(QuadOp::Q_JMP, Operand(), Operand(), Operand::Label(lEnd), loc);
            EmitIR(QuadOp::Q_LABEL, Operand::Label(lMid), Operand(), Operand(), loc);
            EmitIR(QuadOp::Q_ASSIGN, ConstInt(op == TokenType::TOKEN_AND ? 0 : 1, loc), Operand(),
                   Operand::Temp(tIdx, TokenType::TOKEN_BOOL), loc);
            EmitIR(QuadOp::Q_LABEL, Operand::Label(lEnd), Operand(), Operand(), loc);
            return Operand::Temp(tIdx, TokenType::TOKEN_BOOL);
        }

        Operand lo = EvalExpr(bin->m_children[0]);
        Operand ro = EvalExpr(bin->m_children[1]);
        if (lo.m_kind == OperandKind::OP_NONE || ro.m_kind == OperandKind::OP_NONE)
        {
            return Operand();
        }
        const TokenType lt = OperandType(lo);
        const TokenType rt = OperandType(ro);

        // 关系运算：结果 T_BOOL；int/float 混合不插转换（§8.1 表：无）
        if (op == TokenType::TOKEN_EQ || op == TokenType::TOKEN_NE ||
            op == TokenType::TOKEN_LT || op == TokenType::TOKEN_LE ||
            op == TokenType::TOKEN_GT || op == TokenType::TOKEN_GE)
        {
            QuadOp qop = QuadOp::Q_NONE;
            switch (op)
            {
            case TokenType::TOKEN_EQ: qop = QuadOp::Q_EQ; break;
            case TokenType::TOKEN_NE: qop = QuadOp::Q_NE; break;
            case TokenType::TOKEN_LT: qop = QuadOp::Q_LT; break;
            case TokenType::TOKEN_LE: qop = QuadOp::Q_LE; break;
            case TokenType::TOKEN_GT: qop = QuadOp::Q_GT; break;
            case TokenType::TOKEN_GE: qop = QuadOp::Q_GE; break;
            default: break;
            }
            const int t = GetTmpVar(TokenType::TOKEN_BOOL);
            EmitIR(qop, lo, ro, Operand::Temp(t, TokenType::TOKEN_BOOL), loc);
            return Operand::Temp(t, TokenType::TOKEN_BOOL);
        }

        // 算术 + - * /：结果类型按 Promote（任一 float → float）；int/float 混合插 ITOF
        if (op == TokenType::TOKEN_PLUS || op == TokenType::TOKEN_MINUS ||
            op == TokenType::TOKEN_MUL  || op == TokenType::TOKEN_DIV)
        {
            const TokenType resType = (IsFloatType(lt) || IsFloatType(rt))
                                          ? TokenType::TOKEN_FLOAT
                                          : TokenType::TOKEN_INT;
            if (IsFloatType(rt) && IsIntType(lt))
            {
                lo = IntToFloatConv(lo, loc);
            }
            else if (IsFloatType(lt) && IsIntType(rt))
            {
                ro = IntToFloatConv(ro, loc);
            }
            QuadOp qop = QuadOp::Q_NONE;
            switch (op)
            {
            case TokenType::TOKEN_PLUS:  qop = QuadOp::Q_ADD; break;
            case TokenType::TOKEN_MINUS: qop = QuadOp::Q_SUB; break;
            case TokenType::TOKEN_MUL:   qop = QuadOp::Q_MUL; break;
            case TokenType::TOKEN_DIV:   qop = QuadOp::Q_DIV; break;
            default: break;
            }
            const int t = GetTmpVar(resType);
            EmitIR(qop, lo, ro, Operand::Temp(t, resType), loc);
            return Operand::Temp(t, resType);
        }
        return Operand();
    }
    case ASTNodeKind::AST_NODE_UNOP:
    {
        auto* u = static_cast<UnOpNode*>(node.get());
        if (u->m_children.empty())
        {
            return Operand();
        }
        Operand op = EvalExpr(u->m_children[0]);
        if (op.m_kind == OperandKind::OP_NONE)
        {
            return Operand();
        }
        const LineInfo& loc = u->m_loc;
        if (u->m_op == TokenType::TOKEN_MINUS)
        {
            // 取负：0 - x（常量 0 按需取/登记）
            const TokenType t = OperandType(op);
            const TokenType resType = IsFloatType(t) ? TokenType::TOKEN_FLOAT
                                                     : TokenType::TOKEN_INT;
            const int tmp = GetTmpVar(resType);
            EmitIR(QuadOp::Q_SUB, ConstInt(0, loc), op, Operand::Temp(tmp, resType), loc);
            return Operand::Temp(tmp, resType);
        }
        if (u->m_op == TokenType::TOKEN_NOT)
        {
            // 逻辑非：t = (x == 0)（bool 域）
            const int tmp = GetTmpVar(TokenType::TOKEN_BOOL);
            EmitIR(QuadOp::Q_EQ, op, ConstInt(0, loc), Operand::Temp(tmp, TokenType::TOKEN_BOOL), loc);
            return Operand::Temp(tmp, TokenType::TOKEN_BOOL);
        }
        return op;
    }
    case ASTNodeKind::AST_NODE_INDEX:
    {
        // 数组读：t = 数组[下标] → (INDEX, 数组, 下标, t)
        auto* ix = static_cast<IndexNode*>(node.get());
        if (ix->m_children.size() < 2)
        {
            return Operand();
        }
        ASTNode* base = ix->m_children[0].get();
        if (!base || base->m_kind != ASTNodeKind::AST_NODE_IDENT)
        {
            return Operand();
        }
        auto* id = static_cast<IdentNode*>(base);
        if (id->m_iSymIdx < 0 ||
            static_cast<size_t>(id->m_iSymIdx) >= m_symtab.m_VarInfoTbl.size())
        {
            return Operand();
        }
        const TokenType elemT = m_symtab.m_VarInfoTbl[id->m_iSymIdx].m_type;
        Operand idxOp = EvalExpr(ix->m_children[1]);
        if (idxOp.m_kind == OperandKind::OP_NONE)
        {
            return Operand();
        }
        const int t = GetTmpVar(elemT);
        EmitIR(QuadOp::Q_INDEX, Operand::Var(id->m_iSymIdx, elemT), idxOp,
               Operand::Temp(t, elemT), ix->m_loc);
        return Operand::Temp(t, elemT);
    }
    case ASTNodeKind::AST_NODE_CALL:
    {
        // 函数调用：逐实参 QuadOp::Q_PARA → QuadOp::Q_CALL → 返回值临时量
        auto* call = static_cast<CallNode*>(node.get());
        if (call->m_children.empty())
        {
            return Operand();
        }
        ASTNode* callee = call->m_children[0].get();
        if (!callee || callee->m_kind != ASTNodeKind::AST_NODE_IDENT)
        {
            return Operand();
        }
        auto* id = static_cast<IdentNode*>(callee);
        TokenType retT = TokenType::TOKEN_VOID;
        if (id->m_iSymIdx >= 0 &&
            static_cast<size_t>(id->m_iSymIdx) < m_symtab.m_VarInfoTbl.size())
        {
            retT = m_symtab.m_VarInfoTbl[id->m_iSymIdx].m_type;
        }
        for (std::size_t i = 1; i < call->m_children.size(); ++i)
        {
            Operand arg = EvalExpr(call->m_children[i]);
            if (arg.m_kind != OperandKind::OP_NONE)
            {
                // 实参 → 形参类型转换（P2 缺陷修复）：符号表 ParamEntry 无过程归属，
                // 故用 IR 阶段自建的 m_ParamTypes 按「被调函数名 + 实参序号」查形参类型。
                const TokenType pt = ParamTypeOf(id->m_szName, static_cast<int>(i) - 1);
                if (IsFloatType(pt) && IsIntType(OperandType(arg)))
                {
                    arg = IntToFloatConv(arg, call->m_loc);
                }
                else if (IsIntType(pt) && IsFloatType(OperandType(arg)))
                {
                    arg = FloatToIntConv(arg, call->m_loc);
                }
                EmitIR(QuadOp::Q_PARA, arg, Operand(), Operand(), call->m_loc);
            }
        }
        if (retT == TokenType::TOKEN_VOID)
        {
            EmitIR(QuadOp::Q_CALL, Operand::Var(id->m_iSymIdx, retT), Operand(), Operand(), call->m_loc);
            return Operand();
        }
        const int t = GetTmpVar(retT);
        EmitIR(QuadOp::Q_CALL, Operand::Var(id->m_iSymIdx, retT), Operand(),
               Operand::Temp(t, retT), call->m_loc);
        return Operand::Temp(t, retT);
    }
    default:
        return Operand();
    }
}

// ----------------------------------------------------------------------------
// 语句 → 四元式
// ----------------------------------------------------------------------------
void IRGenerator::GenStmt(const ASTNodePtr& node)
{
    if (!node)
    {
        return;
    }
    switch (node->m_kind)
    {
    case ASTNodeKind::AST_NODE_PROGRAM:
        // 顶层：声明无代码；FUNCDEF → FlushToProc；其余语句 → 全局代码
        for (const auto& c : node->m_children)
        {
            if (c && c->m_kind == ASTNodeKind::AST_NODE_FUNCDEF)
            {
                GenFuncDef(static_cast<FuncDefNode*>(c.get()));
            }
            else
            {
                GenStmt(c);
            }
        }
        return;
    case ASTNodeKind::AST_NODE_BLOCK:
        for (const auto& c : node->m_children)
        {
            GenStmt(c);
        }
        return;
    case ASTNodeKind::AST_NODE_FUNCDEF:
        GenFuncDef(static_cast<FuncDefNode*>(node.get()));
        return;
    case ASTNodeKind::AST_NODE_ASSIGN:
    {
        auto* a = static_cast<AssignNode*>(node.get());
        if (a->m_children.size() < 2)
        {
            return;
        }
        ASTNode* target = a->m_children[0].get();
        Operand val = EvalExpr(a->m_children[1]);
        if (val.m_kind == OperandKind::OP_NONE)
        {
            return;
        }
        if (target->m_kind == ASTNodeKind::AST_NODE_IDENT)
        {
            auto* id = static_cast<IdentNode*>(target);
            if (id->m_iSymIdx < 0 ||
                static_cast<size_t>(id->m_iSymIdx) >= m_symtab.m_VarInfoTbl.size())
            {
                return;
            }
            const TokenType lhsT = m_symtab.m_VarInfoTbl[id->m_iSymIdx].m_type;
            // FLOAT → INT：插入 FTOI（Q9 告警由语义阶段发出）
            if (IsIntType(lhsT) && IsFloatType(OperandType(val)))
            {
                val = FloatToIntConv(val, a->m_loc);
            }
            // INT → FLOAT：插入 ITOF（§8.1；原先缺失导致赋值后按位解释成垃圾浮点）
            else if (IsFloatType(lhsT) && IsIntType(OperandType(val)))
            {
                val = IntToFloatConv(val, a->m_loc);
            }
            EmitIR(QuadOp::Q_ASSIGN, val, Operand(), Operand::Var(id->m_iSymIdx, lhsT), a->m_loc);
        }
        else if (target->m_kind == ASTNodeKind::AST_NODE_INDEX)
        {
            // 数组写：数组[下标] = 值 → (INDEX, 值, 下标, 数组)
            auto* ix = static_cast<IndexNode*>(target);
            if (ix->m_children.size() < 2)
            {
                return;
            }
            ASTNode* base = ix->m_children[0].get();
            if (!base || base->m_kind != ASTNodeKind::AST_NODE_IDENT)
            {
                return;
            }
            auto* id = static_cast<IdentNode*>(base);
            if (id->m_iSymIdx < 0 ||
                static_cast<size_t>(id->m_iSymIdx) >= m_symtab.m_VarInfoTbl.size())
            {
                return;
            }
            const TokenType elemT = m_symtab.m_VarInfoTbl[id->m_iSymIdx].m_type;
            Operand idxOp = EvalExpr(ix->m_children[1]);
            if (idxOp.m_kind == OperandKind::OP_NONE)
            {
                return;
            }
            if (IsIntType(elemT) && IsFloatType(OperandType(val)))
            {
                val = FloatToIntConv(val, a->m_loc);
            }
            else if (IsFloatType(elemT) && IsIntType(OperandType(val)))
            {
                val = IntToFloatConv(val, a->m_loc);
            }
            EmitIR(QuadOp::Q_INDEX, val, idxOp, Operand::Var(id->m_iSymIdx, elemT), a->m_loc);
        }
        return;
    }
    case ASTNodeKind::AST_NODE_IF:
    {
        auto* in = static_cast<IfNode*>(node.get());
        if (in->m_children.empty())
        {
            return;
        }
        Operand cond = EvalExpr(in->m_children[0]);
        if (cond.m_kind == OperandKind::OP_NONE)
        {
            return;
        }
        const LineInfo& loc = in->m_loc;
        if (in->m_children.size() >= 3)
        {
            // if (e) S1 else S2
            const int lElse = GetTmpLabel();
            const int lEnd = GetTmpLabel();
            EmitIR(QuadOp::Q_JZ, cond, Operand(), Operand::Label(lElse), loc);
            GenStmt(in->m_children[1]);
            EmitIR(QuadOp::Q_JMP, Operand(), Operand(), Operand::Label(lEnd), loc);
            EmitIR(QuadOp::Q_LABEL, Operand::Label(lElse), Operand(), Operand(), loc);
            GenStmt(in->m_children[2]);
            EmitIR(QuadOp::Q_LABEL, Operand::Label(lEnd), Operand(), Operand(), loc);
        }
        else
        {
            // if (e) S
            const int lEnd = GetTmpLabel();
            EmitIR(QuadOp::Q_JZ, cond, Operand(), Operand::Label(lEnd), loc);
            GenStmt(in->m_children[1]);
            EmitIR(QuadOp::Q_LABEL, Operand::Label(lEnd), Operand(), Operand(), loc);
        }
        return;
    }
    case ASTNodeKind::AST_NODE_WHILE:
    {
        auto* wn = static_cast<WhileNode*>(node.get());
        if (wn->m_children.size() < 2)
        {
            return;
        }
        const int lBegin = GetTmpLabel();
        const int lEnd = GetTmpLabel();
        const LineInfo& loc = wn->m_loc;
        EmitIR(QuadOp::Q_LABEL, Operand::Label(lBegin), Operand(), Operand(), loc);
        Operand cond = EvalExpr(wn->m_children[0]);
        if (cond.m_kind == OperandKind::OP_NONE)
        {
            return;
        }
        EmitIR(QuadOp::Q_JZ, cond, Operand(), Operand::Label(lEnd), loc);
        GenStmt(wn->m_children[1]);
        EmitIR(QuadOp::Q_JMP, Operand(), Operand(), Operand::Label(lBegin), loc);
        EmitIR(QuadOp::Q_LABEL, Operand::Label(lEnd), Operand(), Operand(), loc);
        return;
    }
    case ASTNodeKind::AST_NODE_PRINT:
    {
        auto* pn = static_cast<PrintNode*>(node.get());
        if (pn->m_children.empty())
        {
            return;
        }
        Operand e = EvalExpr(pn->m_children[0]);
        if (e.m_kind == OperandKind::OP_NONE)
        {
            return;
        }
        EmitIR(QuadOp::Q_PRINT, e, Operand(), Operand(), pn->m_loc);
        return;
    }
    case ASTNodeKind::AST_NODE_RETURN:
    {
        auto* rn = static_cast<ReturnNode*>(node.get());
        if (rn->m_children.empty())
        {
            EmitIR(QuadOp::Q_RETURN, Operand(), Operand(), Operand(), rn->m_loc);
            return;
        }
        Operand e = EvalExpr(rn->m_children[0]);
        if (e.m_kind == OperandKind::OP_NONE)
        {
            return;
        }
        // 返回值 → 函数返回类型转换（P3 缺陷修复）：
        // 形如 `float f(){ return 7; }` 原先直接把整数 7 放进返回值寄存器，
        // 调用方按 float 读就成了垃圾浮点。此处按 m_curRetType 补 ITOF/FTOI。
        if (IsFloatType(m_curRetType) && IsIntType(OperandType(e)))
        {
            e = IntToFloatConv(e, rn->m_loc);
        }
        else if (IsIntType(m_curRetType) && IsFloatType(OperandType(e)))
        {
            e = FloatToIntConv(e, rn->m_loc);
        }
        EmitIR(QuadOp::Q_RETURN, e, Operand(), Operand(), rn->m_loc);
        return;
    }
    case ASTNodeKind::AST_NODE_EMPTY:
    case ASTNodeKind::AST_NODE_DECL:
    case ASTNodeKind::AST_NODE_ARRAYDECL:
    case ASTNodeKind::AST_NODE_PARAM:
        // 声明/空语句无运行时四元式
        return;
    default:
        // 防御：表达式单独成句时求值并丢弃结果
        EvalExpr(node);
        return;
    }
}

void IRGenerator::GenFuncDef(FuncDefNode* fn)
{
    if (!fn)
    {
        return;
    }
    // 语义阶段已把函数登记进过程表（m_ProcInfoTbl），按名字匹配
    ProcEntry* proc = nullptr;
    for (auto& p : m_symtab.m_ProcInfoTbl)
    {
        if (p.m_szName == fn->m_szName)
        {
            proc = &p;
            break;
        }
    }
    if (!proc)
    {
        return;
    }
    m_Codes.clear();
    // 当前函数返回类型（return 语句据此插类型转换）
    const TokenType savedRet = m_curRetType;
    m_curRetType = proc->m_retType;
    // 函数体 = children 末元素（BLOCK）
    if (!fn->m_children.empty())
    {
        GenStmt(fn->m_children.back());
    }
    m_curRetType = savedRet;
    FlushToProc(*proc);
}

// ----------------------------------------------------------------------------
// Generate —— 以程序根节点生成代码
// ----------------------------------------------------------------------------
void IRGenerator::Generate(const ASTNodePtr& root)
{
    if (!root)
    {
        return;
    }
    m_Codes.clear();
    m_curRetType = TokenType::TOKEN_NONE;
    // 形参类型表必须在翻译任何语句之前建好（Q_PARA 要用）
    BuildParamTypeTable(root);
    GenStmt(root);
    // 顶层全局语句的四元式保留在 m_Codes（--dump-quads 输出）；
    // 函数定义的四元式已由 GenFuncDef → FlushToProc 挂入 m_ProcInfoTbl。
}

void IRGenerator::FlushToProc(ProcEntry& proc)
{
    // 挑战版：将 m_Codes 挂到过程表项，并清空当前序列
    proc.m_Codes = m_Codes;
    m_Codes.clear();
}
