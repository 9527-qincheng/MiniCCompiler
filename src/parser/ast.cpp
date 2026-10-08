// ============================================================================
// MiniC 编译器 —— AST 节点实现（构造 + 前序遍历打印）
// ============================================================================
#include "parser/ast.h"

#include <iomanip>

// --- 缩进辅助 ---
static void PrintIndent(std::ostream& os, int indent)
{
    for (int i = 0; i < indent; ++i)
    {
        os << "  ";
    }
}

NumNode::NumNode(double value, bool isFloat, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_NUM, loc), m_fValue(value), m_bIsFloat(isFloat)
{
}

void NumNode::Print(std::ostream& os, int indent) const
{
    PrintIndent(os, indent);
    os << "Num";
    if (m_bIsFloat)
    {
        os << "(" << std::fixed << std::setprecision(6) << m_fValue << ")";
    }
    else
    {
        os << "(" << static_cast<long long>(m_fValue) << ")";
    }
    os << "  @" << m_loc.m_iRow << ":" << m_loc.m_iCol << "\n";
}

IdentNode::IdentNode(const std::string& name, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_IDENT, loc), m_szName(name)
{
}

void IdentNode::Print(std::ostream& os, int indent) const
{
    PrintIndent(os, indent);
    os << "Ident(" << m_szName << ")"
       << "  @" << m_loc.m_iRow << ":" << m_loc.m_iCol << "\n";
}

DeclNode::DeclNode(TokenType type, const std::string& name, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_DECL, loc), m_type(type), m_szName(name)
{
}

void DeclNode::Print(std::ostream& os, int indent) const
{
    PrintIndent(os, indent);
    os << "Decl(" << TokenTypeToString(m_type) << " " << m_szName << ")"
       << "  @" << m_loc.m_iRow << ":" << m_loc.m_iCol << "\n";
}

AssignNode::AssignNode(std::unique_ptr<ASTNode> lhs, std::unique_ptr<ASTNode> rhs, const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_ASSIGN, loc)
{
    m_left = std::move(lhs);
    m_right = std::move(rhs);
}

void AssignNode::Print(std::ostream& os, int indent) const
{
    PrintIndent(os, indent);
    os << "Assign"
       << "  @" << m_loc.m_iRow << ":" << m_loc.m_iCol << "\n";
    if (m_left)
    {
        m_left->Print(os, indent + 1);
    }
    if (m_right)
    {
        m_right->Print(os, indent + 1);
    }
}

BinOpNode::BinOpNode(TokenType op,
                     std::unique_ptr<ASTNode> left,
                     std::unique_ptr<ASTNode> right,
                     const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_BINOP, loc), m_op(op)
{
    m_left = std::move(left);
    m_right = std::move(right);
}

void BinOpNode::Print(std::ostream& os, int indent) const
{
    PrintIndent(os, indent);
    os << "BinOp(" << TokenTypeToString(m_op) << ")"
       << "  @" << m_loc.m_iRow << ":" << m_loc.m_iCol << "\n";
    if (m_left)
    {
        m_left->Print(os, indent + 1);
    }
    if (m_right)
    {
        m_right->Print(os, indent + 1);
    }
}

ListNode::ListNode(const LineInfo& loc)
    : ASTNode(ASTNodeKind::AST_NODE_LIST, loc)
{
}

void ListNode::Print(std::ostream& os, int indent) const
{
    PrintIndent(os, indent);
    os << "List"
       << "  @" << m_loc.m_iRow << ":" << m_loc.m_iCol << "\n";
    if (m_left)
    {
        m_left->Print(os, indent + 1);
    }
    if (m_right)
    {
        m_right->Print(os, indent + 1);
    }
}
