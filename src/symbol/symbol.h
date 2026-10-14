#ifndef SYMBOL_H
#define SYMBOL_H

#include <string>
#include <vector>
#include <unordered_map>
#include <ostream>
#include "lexer/token.h"

enum class ScopeType {
    SCOPE_GLOBAL,
    SCOPE_FUNCTION,
    SCOPE_BLOCK      // 新增：块作用域
};

struct SymbolEntry
{
    std::string m_szName;
};

struct VarEntry : public SymbolEntry
{
    TokenType m_type;
    int       m_iOffset = 0;
    bool      m_bIsArray = false;     // 新增：是否数组
    int       m_iArraySize = 0;       // 新增：数组元素个数
    int       m_line;
    int       m_col;
    int       m_scopeLevel;
};

struct ConstEntry : public SymbolEntry
{
    TokenType m_type;
    double    m_value;
    int       m_line;
    int       m_col;
};

struct TypeEntry : public SymbolEntry
{
    std::string m_szName;
    TokenType m_baseType;
    int m_size;
};

struct ProcEntry : public SymbolEntry
{
    // 过程/函数表项，预留
};

struct ParamEntry : public SymbolEntry
{
    // 形参表项，预留
};

struct LabelEntry : public SymbolEntry
{
    std::string m_name;
    int m_addr;
};

class SymbolTable
{
public:
    SymbolTable();
    bool EnterScope(ScopeType t);
    void ExitScope();

    VarEntry* RecVar(const std::string& name, TokenType type, int line, int col);
    VarEntry* RecArray(const std::string& name, TokenType type, int size, int line, int col);

    ConstEntry* RecConst(const std::string& name, TokenType type, double value, int line, int col);

    const VarEntry*   SearchVar(const std::string& name) const;
    const ConstEntry* SearchConst(const std::string& name) const;

    void BackfillConst(int iConstIdx, const std::string& name, TokenType type);
    bool CheckAssign(TokenType lhs, TokenType rhs, int line, int col);
    void DumpSymTab(std::ostream& os) const;

private:
    // --------【内部改造：作用域栈，替换旧扁平数组】--------
    std::vector<std::unordered_map<std::string, VarEntry>> m_varScopeStack;
    int m_currentScopeLevel;

    // 常量、类型表保持全局存储
    std::vector<ConstEntry>  m_ConstInfoTbl;
    std::vector<TypeEntry>   m_TypeInfoTbl;
};

#endif
