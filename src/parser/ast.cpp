// ============================================================================
// MiniC 编译器 —— AST 节点实现（构造 + 基类统一打印）
//   阶段⑤（v4.0 §5.3.2）：多子节点 AST。各节点只提供 Label()，
//   Print 由 ASTNode 基类统一实现，保证 --dump-ast 文本格式一致。
// ============================================================================
#include "parser/ast.h"

#include <iomanip>
#include <sstream>
#include <utility>

// --- 缩进辅助 ---
static void PrintIndent(std::ostream& os, int indent)
{
    for (int i = 0; i < indent; ++i)
    {
        os << "  ";
    }
}

// ============================================================================
// 基类：前序遍历打印（label + @行:列，子节点缩进 +1 层）
// ============================================================================
void ASTNode::Print(std::ostream& os, int indent) const
{
    PrintIndent(os, indent);
    os << Label() << "  @" << m_loc.m_iRow << ":" << m_loc.m_iCol << "\n";
    for (const ASTNodePtr& child : m_children)
    {
        if (child)
        {
            child->Print(os, indent + 1);
        }
    }
}

// ============================================================================
// 程序根
// ============================================================================
ProgramNode::ProgramNode(const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_PROGRAM, loc)
{
}

std::string ProgramNode::Label() const
{
    return "Program";
}

// ============================================================================
// 变量声明 / 数组声明
// ============================================================================
DeclNode::DeclNode(TokenType type, const std::string& name, const LineInfo& loc, int arraySize)
    : ASTNode(ASTNodeKind::AST_NODE_DECL, loc),
      m_type(type), m_szName(name), m_iArraySize(arraySize)
{
}

std::string DeclNode::Label() const
{
    return "Decl(" + std::string(TokenTypeToString(m_type)) + " " + m_szName + ")";
}

ArrayDeclNode::ArrayDeclNode(TokenType type, const std::string& name, int arraySize,
                             ASTNodePtr decl, ASTNodePtr dim, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_ARRAYDECL, loc),
      m_type(type), m_szName(name), m_iArraySize(arraySize)
{
    m_children.push_back(std::move(decl));   // children[0] = 元素声明（DeclNode）
    m_children.push_back(std::move(dim));    // children[1] = 维度常量（NumNode，可空）
}

std::string ArrayDeclNode::Label() const
{
    return "ArrayDecl(" + std::string(TokenTypeToString(m_type)) + " " + m_szName +
           "[" + std::to_string(m_iArraySize) + "])";
}

// ============================================================================
// 函数定义 / 形参
// ============================================================================
FuncDefNode::FuncDefNode(TokenType retType, const std::string& name, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_FUNCDEF, loc), m_retType(retType), m_szName(name)
{
}

std::string FuncDefNode::Label() const
{
    return "FuncDef(" + std::string(TokenTypeToString(m_retType)) + " " + m_szName + ")";
}

ParamNode::ParamNode(TokenType type, const std::string& name, const LineInfo& loc, int arraySize)
    : ASTNode(ASTNodeKind::AST_NODE_PARAM, loc),
      m_type(type), m_szName(name), m_iArraySize(arraySize)
{
}

std::string ParamNode::Label() const
{
    std::string label = "Param(" + std::string(TokenTypeToString(m_type)) + " " + m_szName;
    if (m_iArraySize > 0)
    {
        label += "[" + std::to_string(m_iArraySize) + "]";
    }
    label += ")";
    return label;
}

// ============================================================================
// 复合语句块
// ============================================================================
BlockNode::BlockNode(const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_BLOCK, loc)
{
}

std::string BlockNode::Label() const
{
    return "Block";
}

// ============================================================================
// 赋值
// ============================================================================
AssignNode::AssignNode(ASTNodePtr target, ASTNodePtr value, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_ASSIGN, loc)
{
    m_children.push_back(std::move(target));   // children[0] = 目标（IDENT / INDEX）
    m_children.push_back(std::move(value));    // children[1] = 表达式
}

std::string AssignNode::Label() const
{
    return "Assign";
}

// ============================================================================
// if / while
// ============================================================================
IfNode::IfNode(ASTNodePtr cond, ASTNodePtr thenStmt, ASTNodePtr elseStmt, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_IF, loc)
{
    m_children.push_back(std::move(cond));       // children[0] = 条件
    m_children.push_back(std::move(thenStmt));   // children[1] = then 分支
    if (elseStmt)
    {
        m_children.push_back(std::move(elseStmt));   // children[2] = else 分支（可选）
    }
}

std::string IfNode::Label() const
{
    return "If";
}

WhileNode::WhileNode(ASTNodePtr cond, ASTNodePtr body, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_WHILE, loc)
{
    m_children.push_back(std::move(cond));   // children[0] = 条件
    m_children.push_back(std::move(body));   // children[1] = 循环体
}

std::string WhileNode::Label() const
{
    return "While";
}

// ============================================================================
// print / return / 空语句
// ============================================================================
PrintNode::PrintNode(ASTNodePtr expr, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_PRINT, loc)
{
    m_children.push_back(std::move(expr));   // children[0] = 表达式
}

std::string PrintNode::Label() const
{
    return "Print";
}

ReturnNode::ReturnNode(ASTNodePtr expr, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_RETURN, loc)
{
    if (expr)
    {
        m_children.push_back(std::move(expr));   // children[0] = 返回表达式（可空）
    }
}

std::string ReturnNode::Label() const
{
    return "Return";
}

EmptyNode::EmptyNode(const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_EMPTY, loc)
{
}

std::string EmptyNode::Label() const
{
    return "Empty";
}

// ============================================================================
// 运算
// ============================================================================
BinOpNode::BinOpNode(TokenType op, ASTNodePtr left, ASTNodePtr right, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_BINOP, loc), m_op(op)
{
    m_children.push_back(std::move(left));    // children[0] = 左操作数
    m_children.push_back(std::move(right));   // children[1] = 右操作数
}

std::string BinOpNode::Label() const
{
    return "BinOp(" + std::string(TokenTypeToString(m_op)) + ")";
}

UnOpNode::UnOpNode(TokenType op, ASTNodePtr operand, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_UNOP, loc), m_op(op)
{
    m_children.push_back(std::move(operand)); // children[0] = 操作数
}

std::string UnOpNode::Label() const
{
    return "UnOp(" + std::string(TokenTypeToString(m_op)) + ")";
}

// ============================================================================
// 常量 / 标识符
// ============================================================================
NumNode::NumNode(double value, bool isFloat, const LineInfo& loc,
                 int constIdx, bool isChar, bool isBool)
    : ASTNode(ASTNodeKind::AST_NODE_NUM, loc),
      m_fValue(value), m_bIsFloat(isFloat), m_isChar(isChar), m_isBool(isBool),
      m_iConstIdx(constIdx)
{
}

std::string NumNode::Label() const
{
    std::ostringstream oss;
    if (m_bIsFloat)
    {
        oss << "Num(" << std::fixed << std::setprecision(6) << m_fValue << ")";
    }
    else
    {
        oss << "Num(" << static_cast<long long>(m_fValue) << ")";
    }
    return oss.str();
}

IdentNode::IdentNode(const std::string& name, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_IDENT, loc), m_szName(name)
{
}

std::string IdentNode::Label() const
{
    return "Ident(" + m_szName + ")";
}

// ============================================================================
// 数组下标 / 函数调用
// ============================================================================
IndexNode::IndexNode(ASTNodePtr base, ASTNodePtr index, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_INDEX, loc)
{
    m_children.push_back(std::move(base));    // children[0] = 基址
    m_children.push_back(std::move(index));   // children[1] = 下标表达式
}

std::string IndexNode::Label() const
{
    return "Index";
}

CallNode::CallNode(ASTNodePtr callee, std::vector<ASTNodePtr> args, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_CALL, loc)
{
    m_children.push_back(std::move(callee));   // children[0] = 被调标识符
    for (ASTNodePtr& a : args)
    {
        m_children.push_back(std::move(a));    // children[1..] = 实参
    }
}

std::string CallNode::Label() const
{
    return "Call";
}
