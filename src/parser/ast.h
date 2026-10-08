// ============================================================================
// MiniC 编译器 —— 抽象语法树 ASTNode（第 4.3 节 + Q10 定稿：基类+派生类+unique_ptr）
// ============================================================================
#ifndef MINIC_PARSER_AST_H
#define MINIC_PARSER_AST_H

#include "lexer/token.h"

#include <memory>
#include <string>
#include <ostream>

// ----------------------------------------------------------------------------
// 4.3.1 AST 节点类别（PPT 约定）
// ----------------------------------------------------------------------------
enum class ASTNodeKind
{
    AST_NODE_NONE,
    AST_NODE_NUM,       // 数字常量
    AST_NODE_IDENT,     // 标识符引用
    AST_NODE_DECL,      // 变量声明
    AST_NODE_ASSIGN,    // 赋值语句
    AST_NODE_BINOP,     // 二元算术运算
    AST_NODE_LIST       // 语句/声明序列
};

// ----------------------------------------------------------------------------
// 4.3.2 ASTNode 基类（Q10：unique_ptr 管理，ASTNodePtr 类型别名）
// ----------------------------------------------------------------------------
class ASTNode
{
public:
    ASTNode(ASTNodeKind kind, const LineInfo& loc)
        : m_kind(kind), m_loc(loc) {}
    virtual ~ASTNode() = default;

    ASTNodeKind m_kind;       // 节点类别
    LineInfo    m_loc;        // 对应源码位置（行/列）

    std::unique_ptr<ASTNode> m_left;   // 左子节点（Q10：unique_ptr 管理）
    std::unique_ptr<ASTNode> m_right;  // 右子节点

    virtual void Print(std::ostream& os, int indent = 0) const = 0; // 前序遍历+缩进（--dump-ast）
};

using ASTNodePtr = std::unique_ptr<ASTNode>;

// ----------------------------------------------------------------------------
// 4.3.3 派生类：NumNode / IdentNode / DeclNode / AssignNode / BinOpNode / ListNode
// ----------------------------------------------------------------------------
// 数字常量节点
class NumNode : public ASTNode
{
public:
    NumNode(double value, bool isFloat, const LineInfo& loc);
    void Print(std::ostream& os, int indent) const override;

    double  m_fValue;   // 数值（统一 double 存储，打印时按 m_bIsFloat 区分）
    bool    m_bIsFloat; // 是否为浮点常量
};

// 标识符引用节点
class IdentNode : public ASTNode
{
public:
    IdentNode(const std::string& name, const LineInfo& loc);
    void Print(std::ostream& os, int indent) const override;

    std::string m_szName; // 标识符名
};

// 变量声明节点（int a; / float b;）
class DeclNode : public ASTNode
{
public:
    DeclNode(TokenType type, const std::string& name, const LineInfo& loc);
    void Print(std::ostream& os, int indent) const override;

    TokenType   m_type;    // TOKEN_INT / TOKEN_FLOAT
    std::string m_szName;  // 变量名
};

// 赋值语句节点（a = b + 1;）
class AssignNode : public ASTNode
{
public:
    AssignNode(std::unique_ptr<ASTNode> lhs, std::unique_ptr<ASTNode> rhs, const LineInfo& loc);
    void Print(std::ostream& os, int indent) const override;
};

// 二元运算节点（+ - * /）
class BinOpNode : public ASTNode
{
public:
    BinOpNode(TokenType op,
              std::unique_ptr<ASTNode> left,
              std::unique_ptr<ASTNode> right,
              const LineInfo& loc);
    void Print(std::ostream& os, int indent) const override;

    TokenType m_op; // TOKEN_PLUS / MINUS / MUL / DIV
};

// 语句/声明序列节点（链表式：左=首元素，右=剩余序列）
class ListNode : public ASTNode
{
public:
    ListNode(const LineInfo& loc);
    void Print(std::ostream& os, int indent) const override;
};

#endif // MINIC_PARSER_AST_H
