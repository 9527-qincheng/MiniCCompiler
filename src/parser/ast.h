// ============================================================================
// MiniC 编译器 —— 抽象语法树 ASTNode（设计文档 v4.0 §5.3.2【变更·破坏性】）
//   阶段⑤：多子节点重构
//     · 基类由 m_left / m_right 两指针改为 m_children 任意子节点序列；
//     · 节点类别由 11 类扩展为 18 类（含挑战版的函数/形参/print/下标/调用/数组/空语句）；
//     · Print 由基类统一实现（各节点只提供单行标签 Label()），输出格式与 v3.1 保持一致的
//       “名称(附加信息)  @行:列 + 每层 2 空格缩进”风格，便于回归基线比对。
//   下游（语义 04 / 中间代码 05）统一按 m_kind 分派、按 m_children 取操作数：
//     PROGRAM/BLOCK 的 children 即序列；BINOP children[0]/[1] 为左右；IF children[0]/[1]/[2]
//     为条件/then/else（无 else 时只有 2 个）；FUNCDEF children 末元素为函数体 BLOCK，其余为 PARAM。
// ============================================================================
#ifndef MINIC_PARSER_AST_H
#define MINIC_PARSER_AST_H

#include "lexer/token.h"

#include <memory>
#include <string>
#include <vector>
#include <ostream>

// ----------------------------------------------------------------------------
// 5.3.2.1 AST 节点类别（18 类；AST_NODE_NONE 为未使用哨兵）
//   设计文档枚举 17 类 + 02 任务书 §5.2 要求的 ARRAYDECL（数组声明），共 18 类。
//   LIST 已删除：序列由 PROGRAM / BLOCK 的 m_children 表达（D3）。
// ----------------------------------------------------------------------------
enum class ASTNodeKind
{
    AST_NODE_NONE,       // 哨兵（不使用；单元测试断言树中不得出现）
    AST_NODE_PROGRAM,    // 程序根
    AST_NODE_DECL,       // 变量声明
    AST_NODE_FUNCDEF,    // 函数定义
    AST_NODE_PARAM,      // 形参
    AST_NODE_BLOCK,      // 复合语句块
    AST_NODE_ASSIGN,     // 赋值语句
    AST_NODE_IF,         // if / if-else
    AST_NODE_WHILE,      // while
    AST_NODE_PRINT,      // print
    AST_NODE_RETURN,     // return
    AST_NODE_EMPTY,      // 空语句 ;
    AST_NODE_BINOP,      // 二元运算
    AST_NODE_UNOP,       // 一元运算（! / 取负）
    AST_NODE_NUM,        // 数值常量（整型/实型/字符/布尔统一）
    AST_NODE_IDENT,      // 标识符引用
    AST_NODE_INDEX,      // 数组下标
    AST_NODE_CALL,       // 函数调用
    AST_NODE_ARRAYDECL   // 数组声明（02 任务书 §5.2 要求，设计文档枚举中无）
};

class ASTNode;
using ASTNodePtr = std::unique_ptr<ASTNode>;

// ----------------------------------------------------------------------------
// 5.3.2.2 ASTNode 基类（Q10：unique_ptr 管理）
// ----------------------------------------------------------------------------
class ASTNode
{
public:
    ASTNode(ASTNodeKind kind, const LineInfo& loc)
        : m_kind(kind), m_loc(loc) {}
    virtual ~ASTNode() = default;

    ASTNodeKind m_kind;                    // 节点类别
    LineInfo    m_loc;                     // 对应源码位置（行/列）
    std::vector<ASTNodePtr> m_children;    // 子节点序列（顺序由各节点语义约定）

    // 前序遍历 + 缩进（--dump-ast）——基类统一实现，保证格式一致（D7）
    // 调用点签名与 v3.1 相同：node->Print(os, indent)
    void Print(std::ostream& os, int indent = 0) const;

    // 各节点的单行标签（不含缩进与 @行:列），由基类 Print 拼接（D7）
    virtual std::string Label() const = 0;
};

// ----------------------------------------------------------------------------
// 5.3.2.3 派生节点
// ----------------------------------------------------------------------------

// 程序根：children = 顶层声明… + 顶层语句…
class ProgramNode : public ASTNode
{
public:
    explicit ProgramNode(const LineInfo& loc);
    std::string Label() const override;
};

// 变量声明：int a; / float b;（数组由 ArrayDeclNode 承载）
class DeclNode : public ASTNode
{
public:
    DeclNode(TokenType type, const std::string& name, const LineInfo& loc, int arraySize = 0);
    std::string Label() const override;

    TokenType   m_type;         // TOKEN_INT / FLOAT / BOOL / CHAR
    std::string m_szName;       // 变量名
    int         m_iArraySize = 0;   // 0 = 非数组；数组时与 ARRAYDECL 同步
    int         m_iSymIdx = -1;     // 语义阶段回填：符号表项索引
};

// 数组声明：children[0] = DeclNode（元素声明），children[1] = 维度 NumNode（可空）
class ArrayDeclNode : public ASTNode
{
public:
    ArrayDeclNode(TokenType type, const std::string& name, int arraySize,
                  ASTNodePtr decl, ASTNodePtr dim, const LineInfo& loc);
    std::string Label() const override;

    TokenType   m_type;
    std::string m_szName;
    int         m_iArraySize = 0;
    int         m_iSymIdx = -1;     // 语义阶段回填：符号表项索引
};

// 函数定义：children = 形参… + 函数体（末元素为 BLOCK）
class FuncDefNode : public ASTNode
{
public:
    FuncDefNode(TokenType retType, const std::string& name, const LineInfo& loc);
    std::string Label() const override;

    TokenType   m_retType;      // TOKEN_INT / FLOAT / BOOL / CHAR / VOID
    std::string m_szName;
    int         m_iSymIdx = -1; // 语义阶段回填：符号表项索引
};

// 形参：无子节点（类型/名/维度为字段）
class ParamNode : public ASTNode
{
public:
    ParamNode(TokenType type, const std::string& name, const LineInfo& loc, int arraySize = 0);
    std::string Label() const override;

    TokenType   m_type;
    std::string m_szName;
    int         m_iArraySize = 0;   // 0 = 非数组
    int         m_iSymIdx = -1;     // 语义阶段回填：符号表项索引
};

// 复合语句块：children = 块内声明与语句（序列）
class BlockNode : public ASTNode
{
public:
    explicit BlockNode(const LineInfo& loc);
    std::string Label() const override;
};

// 赋值语句：children[0] = 目标（IDENT / INDEX），children[1] = 表达式
class AssignNode : public ASTNode
{
public:
    AssignNode(ASTNodePtr target, ASTNodePtr value, const LineInfo& loc);
    std::string Label() const override;
};

// if 语句：children = [条件, then] 或 [条件, then, else]（D5）
class IfNode : public ASTNode
{
public:
    IfNode(ASTNodePtr cond, ASTNodePtr thenStmt, ASTNodePtr elseStmt, const LineInfo& loc);
    std::string Label() const override;
};

// while 语句：children = [条件, 循环体]
class WhileNode : public ASTNode
{
public:
    WhileNode(ASTNodePtr cond, ASTNodePtr body, const LineInfo& loc);
    std::string Label() const override;
};

// print 语句：children = [表达式]
class PrintNode : public ASTNode
{
public:
    PrintNode(ASTNodePtr expr, const LineInfo& loc);
    std::string Label() const override;
};

// return 语句：children = [表达式]；空 return 无子节点
class ReturnNode : public ASTNode
{
public:
    ReturnNode(ASTNodePtr expr, const LineInfo& loc);
    std::string Label() const override;
};

// 空语句 ;：无子节点
class EmptyNode : public ASTNode
{
public:
    explicit EmptyNode(const LineInfo& loc);
    std::string Label() const override;
};

// 二元运算：children[0]/[1] = 左/右操作数
class BinOpNode : public ASTNode
{
public:
    BinOpNode(TokenType op, ASTNodePtr left, ASTNodePtr right, const LineInfo& loc);
    std::string Label() const override;

    TokenType m_op;   // PLUS/MINUS/MUL/DIV/AND/OR/EQ/NE/LT/LE/GT/GE
};

// 一元运算：children[0] = 操作数
class UnOpNode : public ASTNode
{
public:
    UnOpNode(TokenType op, ASTNodePtr operand, const LineInfo& loc, bool isPostfix = false);
    std::string Label() const override;

    TokenType m_op;        // TOKEN_NOT / TOKEN_MINUS / TOKEN_INC / TOKEN_DEC
    bool      m_bPostfix;  // ++/-- 是否为后缀写法（x++ 取值旧值；前缀 ++x 取新值）
};

// 数值常量（整型/实型/字符/布尔统一）：无子节点
class NumNode : public ASTNode
{
public:
    // constIdx：常量表位序（Q12），由词法 Token::m_iConstIdx 透传，语义阶段回填使用
    NumNode(double value, bool isFloat, const LineInfo& loc,
            int constIdx = -1, bool isChar = false, bool isBool = false);
    std::string Label() const override;

    double m_fValue;        // 数值（统一 double 存储，打印时按 m_bIsFloat 区分）
    bool   m_bIsFloat;      // 是否为浮点常量
    bool   m_isChar;        // 是否来自字符常量
    bool   m_isBool;        // 是否来自 true / false
    int    m_iConstIdx;     // 常量表位序（与词法/语义常量表一致）
};

// 标识符引用：无子节点
class IdentNode : public ASTNode
{
public:
    IdentNode(const std::string& name, const LineInfo& loc);
    std::string Label() const override;

    std::string m_szName;
    int         m_iSymIdx = -1;   // 语义阶段回填：符号表项索引
};

// 数组下标：children = [基址, 下标表达式]
class IndexNode : public ASTNode
{
public:
    IndexNode(ASTNodePtr base, ASTNodePtr index, const LineInfo& loc);
    std::string Label() const override;
};

// 函数调用：children = [被调标识符, 实参…]
class CallNode : public ASTNode
{
public:
    CallNode(ASTNodePtr callee, std::vector<ASTNodePtr> args, const LineInfo& loc);
    std::string Label() const override;
};

#endif // MINIC_PARSER_AST_H
