// ============================================================================
// MiniC 编译器 —— 符号表模块 SymbolTable（第 4.4 节 + 第 6.4 节接口契约）
// Q1 定稿：多表组织（Var/Const/Type/Proc/Param/Label 分表）
// Q12 定稿：常量表词法登记、语义回填（BackfillConst）
// ============================================================================
#ifndef MINIC_SYMBOL_SYMBOL_H
#define MINIC_SYMBOL_SYMBOL_H

#include "lexer/token.h"

#include <memory>
#include <string>
#include <vector>
#include <ostream>
#include <unordered_map>

// 前向声明：Quad 在中间代码模块（sema/ir.h）完整定义，
// 此处仅用于 ProcEntry::m_Codes 的容器声明（C++17 允许 vector<不完整类型> 成员）。
struct Quad;
using CodeList = std::vector<Quad>;

// ----------------------------------------------------------------------------
// 4.4.1 符号表项基类 SymbolEntry（参考书 ObjectInfo：m_szName/m_iProcIndex，
//        扩展 PPT 的 scope/line/col）
// ----------------------------------------------------------------------------
class SymbolEntry
{
public:
    std::string m_szName;       // 名字（参考书 m_szName）
    int         m_iProcIndex;   // 所属过程索引（参考书 m_iProcIndex，-1 表示全局）
    int         m_iScope;       // 作用域编号（PPT 扩展）
    int         m_iLine;        // 声明行号
    int         m_iCol;         // 声明列号

    SymbolEntry() : m_iProcIndex(-1), m_iScope(0), m_iLine(0), m_iCol(0) {}
    virtual ~SymbolEntry() = default;
};

// 变量表项 VarEntry（参考书 VarInfo）
class VarEntry : public SymbolEntry
{
public:
    TokenType m_type = TokenType::TOKEN_NONE; // 变量类型（TOKEN_INT / TOKEN_FLOAT）
    int       m_iOffset = 0;                  // 目标代码偏移（挑战版）
    bool      m_bIsArray = false;             // 【新增 v4.0】是否数组
    int       m_iArraySize = 0;               // 【新增 v4.0】数组元素个数
};

// 常量表项 ConstEntry（参考书 ConstInfo）
class ConstEntry : public SymbolEntry
{
public:
    TokenType m_constType = TokenType::TOKEN_NONE; // 常量类型
    bool      m_bIsFloat = false;                  // 是否为浮点常量
    double    m_fValue = 0.0;                      // 浮点值
    long long m_iValue = 0;                        // 整型值
    std::string m_szVal;                           // 词法文本（登记原文，Q12）
};

// 类型表项 TypeEntry（参考书 TypeInfo）
class TypeEntry : public SymbolEntry
{
public:
    TokenType m_baseType = TokenType::TOKEN_NONE;  // 基础类型（int/float）
    bool      m_bIsArray = false;                  // 是否为数组（挑战版）
    int       m_iArraySize = 0;                    // 数组大小（挑战版）
};

// 过程表项 ProcEntry（参考书 ProcInfo，挑战版）
class ProcEntry : public SymbolEntry
{
public:
    TokenType m_retType = TokenType::TOKEN_VOID;   // 返回类型
    CodeList  m_Codes;                             // 过程四元式序列（参考书 m_Codes）
};

// 参数表项 ParamEntry（参考书 ParaInfo，挑战版）
class ParamEntry : public SymbolEntry
{
public:
    TokenType m_type = TokenType::TOKEN_NONE; // 参数类型
    int       m_iParamPos = 0;                // 参数序号
};

// 标号表项 LabelEntry（参考书 LabelInfo，挑战版）
class LabelEntry : public SymbolEntry
{
public:
    int m_iLabelIdx = 0;   // 标号编号
};

// 枚举表项 EnumEntry（参考书 EnumInfo，挑战版预留）
class EnumEntry : public SymbolEntry
{
public:
    TokenType m_baseType = TokenType::TOKEN_INT;   // 基础类型
    std::vector<int> m_EnumConst;                  // 枚举常量值表
};

// 作用域类型（第 6 章 EnterScope 参数；基础版仅全局，挑战版函数作用域）
enum class ScopeType
{
    SCOPE_GLOBAL,   // 全局作用域
    SCOPE_FUNCTION, // 函数作用域（挑战版）
    SCOPE_BLOCK     // 【新增 v4.0】块作用域
};

// ----------------------------------------------------------------------------
// 4.4.2 多表符号表 SymbolTable（第 6 章接口契约 + 第 4 章参考书成员）
// ----------------------------------------------------------------------------
class SymbolTable
{
public:
    SymbolTable();

    // ================= 第 6 章接口契约（定稿） =================
    bool        EnterScope(ScopeType t);                     // 进入作用域
    void        ExitScope();                                 // 退出作用域
    VarEntry*   RecVar(const std::string& name, TokenType type, int line, int col);
    VarEntry*   RecArray(const std::string& name, TokenType type, int size, int line, int col); // 【新增 v4.0】
    ConstEntry* RecConst(const std::string& name, TokenType type, double value, int line, int col);
    const VarEntry*   SearchVar(const std::string& name) const;
    const ConstEntry* SearchConst(const std::string& name) const;
    void        BackfillConst(int iConstIdx, const std::string& name, TokenType type);
    bool        CheckAssign(TokenType lhs, TokenType rhs, int line, int col);
    void        DumpSymTab(std::ostream& os) const;

    // ================= 第 4 章参考书扩展接口 =================
    int         RecVarTbl(const std::string& name, TokenType type, int line);       // 返回索引（参考书 RecVar 风格）
    int         RecConstTbl(const std::string& szVal, TokenType constType);         // 词法常量登记（Q12）
    VarEntry*   SearchVarInfoTbl(const std::string& name);                          // 查变量表
    ConstEntry* SearchConstInfoTbl(const std::string& name);                        // 查常量表
    TypeEntry*  RecType(const std::string& name, TokenType baseType);               // 登记类型
    TypeEntry*  SearchTypeInfoTbl(const std::string& name);                         // 查类型表
    LabelEntry* RecLabel(const std::string& name, int labelIdx, int line, int col); // 登记标号（挑战版）
    LabelEntry* SearchLabelInfoTbl(const std::string& name);                        // 查标号表
    int         GetSerialId() const { return m_iSerialId; }                         // 对象序号

    // ================= 多表成员（参考书 CSymbolTbl） =================
    std::vector<VarEntry>    m_VarInfoTbl;      // 变量表
    std::vector<ConstEntry>  m_ConstInfoTbl;    // 常量表
    std::vector<TypeEntry>   m_TypeInfoTbl;     // 类型表
    std::vector<ProcEntry>   m_ProcInfoTbl;     // 过程表（挑战版）
    std::vector<ParamEntry>  m_ParamInfoTbl;    // 参数表（挑战版）
    std::vector<LabelEntry>  m_LabelInfoTbl;    // 标号表（挑战版）
    std::vector<EnumEntry>   m_EnumInfoTbl;     // 枚举表（挑战版预留）

    // 辅助栈（参考书声明 4-13，挑战版为主）
    int m_iTypePos = 0;      // 类型位置辅助栈
    int m_iTypeFlag = 0;     // 类型标志
    int m_iIdListFlag = 0;   // 标识符列表标志

private:
    int         m_iSerialId;      // 对象序号
    int         m_iScopeCounter;  // 作用域计数器
    ScopeType   m_curScope;       // 当前作用域
    int         m_curProcIndex;   // 当前过程索引（-1 全局）

    // 【v4.0 新增】作用域链：每层是「名字 → m_VarInfoTbl 下标」。
    // 存下标而非指针：m_VarInfoTbl 会 reallocate，而 ir.cpp 依赖扁平下标稳定。
    std::vector<std::unordered_map<std::string, std::size_t>> m_varScopeStack;
    std::vector<ScopeType> m_scopeTypeStack;   // 与上面同层，用于退出时恢复 m_curScope
};

#endif // MINIC_SYMBOL_SYMBOL_H
