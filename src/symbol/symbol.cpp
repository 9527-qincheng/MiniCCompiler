#include "symbol/symbol.h"

SymbolTable::SymbolTable()
{
    m_currentScopeLevel = 0;
    m_varScopeStack.clear();
    m_varScopeStack.emplace_back();   // 初始化全局作用域
    m_ConstInfoTbl.clear();
    m_TypeInfoTbl.clear();
}

bool SymbolTable::EnterScope(ScopeType t)
{
    (void)t;
    m_currentScopeLevel++;
    m_varScopeStack.emplace_back();
    return true;
}

void SymbolTable::ExitScope()
{
    if (m_currentScopeLevel > 0)
    {
        m_varScopeStack.pop_back();
        m_currentScopeLevel--;
    }
}

// 普通变量登记：仅在当前内层作用域查重
VarEntry* SymbolTable::RecVar(const std::string& name, TokenType type, int line, int col)
{
    auto& curMap = m_varScopeStack.back();
    if (curMap.find(name) != curMap.end())
    {
        // 当前作用域重定义，返回nullptr表示失败
        return nullptr;
    }
    VarEntry e{};
    e.m_szName = name;
    e.m_type = type;
    e.m_line = line;
    e.m_col = col;
    e.m_scopeLevel = m_currentScopeLevel;
    e.m_bIsArray = false;
    e.m_iArraySize = 0;
    curMap[name] = e;
    return &curMap[name];
}

// 数组变量登记
VarEntry* SymbolTable::RecArray(const std::string& name, TokenType type, int size, int line, int col)
{
    auto& curMap = m_varScopeStack.back();
    if (curMap.find(name) != curMap.end())
    {
        return nullptr;
    }
    VarEntry e{};
    e.m_szName = name;
    e.m_type = type;
    e.m_line = line;
    e.m_col = col;
    e.m_scopeLevel = m_currentScopeLevel;
    e.m_bIsArray = true;
    e.m_iArraySize = size;
    curMap[name] = e;
    return &curMap[name];
}

ConstEntry* SymbolTable::RecConst(const std::string& name, TokenType type, double value, int line, int col)
{
    for(auto &c : m_ConstInfoTbl)
    {
        if(c.m_szName == name)
            return nullptr;
    }
    ConstEntry e{};
    e.m_szName = name;
    e.m_type = type;
    e.m_value = value;
    e.m_line = line;
    e.m_col = col;
    m_ConstInfoTbl.push_back(e);
    return &m_ConstInfoTbl.back();
}

// 【核心改造：自内向外查找，实现内层遮蔽外层】
const VarEntry* SymbolTable::SearchVar(const std::string& name) const
{
    for(auto it = m_varScopeStack.rbegin(); it != m_varScopeStack.rend(); ++it)
    {
        auto iter = it->find(name);
        if(iter != it->end())
        {
            return &iter->second;
        }
    }
    return nullptr;
}

const ConstEntry* SymbolTable::SearchConst(const std::string& name) const
{
    for(const auto &c : m_ConstInfoTbl)
    {
        if(c.m_szName == name)
            return &c;
    }
    return nullptr;
}

void SymbolTable::BackfillConst(int iConstIdx, const std::string& name, TokenType type)
{
    // 预留实现
    (void)iConstIdx;
    (void)name;
    (void)type;
}

// 扩展：支持 int / float / bool / char 四种基础类型赋值检查
bool SymbolTable::CheckAssign(TokenType lhs, TokenType rhs, int line, int col)
{
    (void)line;
    (void)col;
    if(lhs == rhs)
        return true;
    // 允许 int <-> float 隐式转换，其余类型不兼容
    if( (lhs == TokenType::TOKEN_INT && rhs == TokenType::TOKEN_FLOAT) ||
        (lhs == TokenType::TOKEN_FLOAT && rhs == TokenType::TOKEN_INT) )
    {
        return true;
    }
    return false;
}

// 符号表导出打印，供 --dump‑symtab 使用
void SymbolTable::DumpSymTab(std::ostream& os) const
{
    os << "==== Symbol Table Dump ====\n";
    os << "Current scope level: " << m_currentScopeLevel << "\n";
    os << "-- Variables --\n";
    for(size_t scopeIdx = 0; scopeIdx < m_varScopeStack.size(); scopeIdx++)
    {
        os << "Scope[" << scopeIdx << "]:\n";
        for(auto &pair : m_varScopeStack[scopeIdx])
        {
            const auto& v = pair.second;
            os << "  " << v.m_szName
               << " type:" << (int)v.m_type
               << " scope:" << v.m_scopeLevel;
            if(v.m_bIsArray) os << " array[" << v.m_iArraySize << "]";
            os << "\n";
        }
    }
    os << "-- Constants --\n";
    for(auto &c : m_ConstInfoTbl)
    {
        os << "  " << c.m_szName << " = " << c.m_value << "\n";
    }
}
