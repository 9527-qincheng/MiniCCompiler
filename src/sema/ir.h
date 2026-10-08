// ============================================================================
// MiniC 编译器 —— 中间代码模块（Quad / Operand / QuadOp / IRGenerator）
// 第 4.5 节数据结构 + 第 6.5 节接口契约
// Q6 定稿：操作数引用式（表索引），打印时按索引转原文（OperandToString）
// ============================================================================
#ifndef MINIC_SEMA_IR_H
#define MINIC_SEMA_IR_H

#include "lexer/token.h"
#include "parser/ast.h"
#include "symbol/symbol.h"

#include <stack>
#include <string>
#include <vector>
#include <ostream>

// ----------------------------------------------------------------------------
// 4.5.1 四元式操作码 QuadOp（参考书 NPIR 操作符，按 MiniC 收窄）
// ----------------------------------------------------------------------------
enum class QuadOp
{
    Q_NONE,     // 空操作
    Q_ASSIGN,   // 赋值 =
    Q_ADD,      // 加 +
    Q_SUB,      // 减 -
    Q_MUL,      // 乘 *
    Q_DIV,      // 除 /
    Q_JMP,      // 无条件跳转（挑战版）
    Q_JZ,       // 条件跳转（为零/假，挑战版）
    Q_JNZ,      // 条件跳转（非零/真，挑战版）
    Q_LABEL,    // 标号定义（挑战版）
    Q_RETURN,   // 返回（挑战版）
    Q_CALL,     // 过程调用（挑战版）
    Q_PRINT     // 打印（挑战版 print 语句）
};

// ----------------------------------------------------------------------------
// 4.5.2 操作数 Operand（参考书 OpInfo：引用式，UD/DU 链基础版暂不启用）
// ----------------------------------------------------------------------------
enum class OperandKind
{
    OP_NONE,    // 空操作数
    OP_CONST,   // 常量（m_iLink = 常量表位序）
    OP_VAR,     // 变量（m_iLink = 变量表位序）
    OP_TEMP,    // 临时变量（m_iLink = 临时编号 _Txxx）
    OP_LABEL    // 标号（m_iLink = 标号编号 Lxxx）
};

class Operand
{
public:
    OperandKind m_kind;      // 操作数种类（参考书 m_iType）
    int         m_iLink;     // 链接索引（常量表/变量表/临时编号/标号编号）
    TokenType   m_type;      // 数据类型：TOKEN_INT / TOKEN_FLOAT（目标代码定字节宽度）
    bool        m_bRef;      // 是否为间接引用（挑战版指针/数组用）
    int         m_iDetailType; // 详细类型信息（栈式类型链用）

    Operand() : m_kind(OperandKind::OP_NONE), m_iLink(-1),
                m_type(TokenType::TOKEN_EOF), m_bRef(false), m_iDetailType(0) {}

    // 便捷构造
    static Operand Const(int idx, TokenType t);   // 常量
    static Operand Var(int idx, TokenType t);     // 变量
    static Operand Temp(int idx, TokenType t);    // 临时变量
    static Operand Label(int idx);                // 标号
};

// ----------------------------------------------------------------------------
// 4.5.3 四元式 Quad（参考书 IRCode：m_eOpType/m_Op1/m_Op2/m_Rslt）
// ----------------------------------------------------------------------------
class Quad
{
public:
    QuadOp      m_eOp;       // 操作码（参考书 m_eOpType）
    Operand     m_Op1;       // arg1（参考书 m_Op1）
    Operand     m_Op2;       // arg2（参考书 m_Op2）
    Operand     m_Rslt;      // res （参考书 m_Rslt）
    LineInfo    m_loc;       // 对应源码位置

    Quad(QuadOp op, const Operand& op1, const Operand& op2, const Operand& rslt)
        : m_eOp(op), m_Op1(op1), m_Op2(op2), m_Rslt(rslt) {}
    void Print(std::ostream& os) const;   // 打印四元式（交付物）
};

using CodeList = std::vector<Quad>;

// ----------------------------------------------------------------------------
// 4.5.4 中间代码生成器 IRGenerator（第 6 章接口契约）
// ----------------------------------------------------------------------------
class IRGenerator
{
private:
    int         m_iTmpVarCounter;   // 临时变量计数器（_T1,_T2,...）
    int         m_iLabelCounter;    // 标号计数器（L1,L2,...）
    CodeList    m_Codes;            // 当前过程四元式序列
    SymbolTable& m_symtab;          // 绑定符号表（确定操作数类型，第 6 章契约）

public:
    explicit IRGenerator(SymbolTable& symtab);

    // --- 第 6 章接口契约 ---
    void    Generate(const ASTNodePtr& root);            // 以程序根节点生成代码
    int     GetTmpVar(TokenType t);                     // 申请临时变量
    int     GetTmpLabel();                              // 申请标号（挑战版）
    void    EmitIR(QuadOp op, const Operand& op1, const Operand& op2, const Operand& rslt);
    const CodeList& GetCodes() const;                    // 输出四元式序列
    std::string OperandToString(const Operand& op) const; // Q6：操作数索引转原文

    // --- 第 4 章参考书成员/扩展 ---
    std::stack<Operand>  m_OperandStack;   // 操作数栈
    std::stack<int>      m_OperationStack; // 运算符栈
    void FlushToProc(ProcEntry& proc);     // 挂到过程表项（挑战版）
};

#endif // MINIC_SEMA_IR_H
