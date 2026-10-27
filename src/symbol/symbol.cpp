// ============================================================================
// MiniC 编译器 —— 符号表模块 SymbolTable 实现
// 状态：已完工（阶段⑥）——六张子表登记、作用域链查找、常量回填、
//       类型检查（CheckAssign）、DumpSymTab 产物输出均已实现并接入主流程。
// 三项历史遗留的落实口径：
//   1. 作用域链查找：SearchVarInfoTbl 沿 m_varScopeStack 自内向外查找，
//      全局层在构造函数建立且永不弹出（不再依赖 m_curProcIndex 链）。
//   2. 常量语义回填：由 SemanticAnalyzer 在 AST_NODE_NUM 分派中统一 RecConst
//      登记，IdentNode 侧回填 m_iConstIdx，见 src/sema/semantic.cpp。
//   3. float→int 赋值告警：CheckAssign 打印 Q9 告警到 stderr，不阻断编译。
// 已知限制：VarEntry::m_iProcIndex 恒为 -1（m_curProcIndex 无赋值点），
//           ParamEntry 无过程归属；目标代码侧以「形参名 + 声明行列」回连，
//           IR 侧以 BuildParamTypeTable 扫描 AST 兜底。
// ============================================================================
#include "symbol/symbol.h"

#include "sema/ir.h"   // 完整 Quad 定义（ProcEntry::m_Codes 析构需要）

#include <cstdlib>
#include <iostream>

SymbolTable::SymbolTable()
    : m_iSerialId(0), m_iScopeCounter(1),
      m_curScope(ScopeType::SCOPE_GLOBAL), m_curProcIndex(-1)
{
    // 【v4.0】构造函数即建立全局作用域（层级 1），且永不弹出
    m_varScopeStack.emplace_back();
    m_scopeTypeStack.push_back(ScopeType::SCOPE_GLOBAL);
}

bool SymbolTable::EnterScope(ScopeType t)
{
    ++m_iScopeCounter;
    m_varScopeStack.emplace_back();      // 压入一层新作用域
    m_scopeTypeStack.push_back(t);
    m_curScope = t;
    return true;
}

void SymbolTable::ExitScope()
{
    // 全局作用域永不弹出（size()==1 时不动）
    if (m_varScopeStack.size() > 1)
    {
        m_varScopeStack.pop_back();
        m_scopeTypeStack.pop_back();
        --m_iScopeCounter;
    }
    m_curScope = m_scopeTypeStack.empty() ? ScopeType::SCOPE_GLOBAL
                                          : m_scopeTypeStack.back();
}

VarEntry* SymbolTable::RecVar(const std::string& name, TokenType type, int line, int col)
{
    // ⚠️ 查重只看【当前】作用域：内层允许遮蔽外层同名变量（C 语义）。
    // 若用 SearchVarInfoTbl（全链查找）查重，内层就永远无法遮蔽外层了。
    auto& curMap = m_varScopeStack.back();
    auto it = curMap.find(name);
    if (it != curMap.end())
    {
        EmitError("duplicate variable '" + name + "'", LineInfo(line, col));
        return &m_VarInfoTbl[it->second];
    }

    VarEntry ve;
    ve.m_szName = name;
    ve.m_type = type;
    ve.m_iProcIndex = m_curProcIndex;
    ve.m_iScope = m_iScopeCounter;
    ve.m_iLine = line;
    ve.m_iCol = col;
    ve.m_bIsArray = false;
    ve.m_iArraySize = 0;
    m_VarInfoTbl.push_back(ve);

    const std::size_t idx = m_VarInfoTbl.size() - 1;
    curMap[name] = idx;                  // 登记「名字 → 扁平下标」
    return &m_VarInfoTbl[idx];
}

VarEntry* SymbolTable::RecArray(const std::string& name, TokenType type, int size, int line, int col)
{
    auto& curMap = m_varScopeStack.back();
    auto it = curMap.find(name);
    if (it != curMap.end())
    {
        EmitError("duplicate variable '" + name + "'", LineInfo(line, col));
        return &m_VarInfoTbl[it->second];
    }

    VarEntry ve;
    ve.m_szName = name;
    ve.m_type = type;
    ve.m_iProcIndex = m_curProcIndex;
    ve.m_iScope = m_iScopeCounter;
    ve.m_iLine = line;
    ve.m_iCol = col;
    ve.m_bIsArray = true;
    ve.m_iArraySize = size;
    m_VarInfoTbl.push_back(ve);

    const std::size_t idx = m_VarInfoTbl.size() - 1;
    curMap[name] = idx;
    return &m_VarInfoTbl[idx];
}

ConstEntry* SymbolTable::RecConst(const std::string& name, TokenType type, double value, int line, int col)
{
    ConstEntry ce;
    ce.m_szName = name;
    ce.m_constType = type;
    // 语义阶段传入的是 TOKEN_FLOAT（NumType 的结果），词法阶段是 TOKEN_NUM_FLOAT，两者都算浮点常量
    ce.m_bIsFloat = (type == TokenType::TOKEN_NUM_FLOAT || type == TokenType::TOKEN_FLOAT);
    ce.m_fValue = value;
    ce.m_iValue = static_cast<long long>(value);
    ce.m_iProcIndex = m_curProcIndex;
    ce.m_iScope = m_iScopeCounter;
    ce.m_iLine = line;
    ce.m_iCol = col;
    m_ConstInfoTbl.push_back(ce);
    return &m_ConstInfoTbl.back();
}

const VarEntry* SymbolTable::SearchVar(const std::string& name) const
{
    return const_cast<SymbolTable*>(this)->SearchVarInfoTbl(name);
}

const ConstEntry* SymbolTable::SearchConst(const std::string& name) const
{
    return const_cast<SymbolTable*>(this)->SearchConstInfoTbl(name);
}

void SymbolTable::BackfillConst(int iConstIdx, const std::string& name, TokenType type)
{
    // Q12：常量语义回填——词法阶段登记文本，语义阶段按位序回填名字/类型
    if (iConstIdx < 0 || static_cast<size_t>(iConstIdx) >= m_ConstInfoTbl.size())
    {
        return;
    }
    ConstEntry& ce = m_ConstInfoTbl[iConstIdx];
    ce.m_szName = name;
    ce.m_constType = type;
    ce.m_bIsFloat = (type == TokenType::TOKEN_NUM_FLOAT);
}

bool SymbolTable::CheckAssign(TokenType lhs, TokenType rhs, int line, int col)
{
    // Q9：float→int 允许隐式转换（C 语义截断）并打印警告，不阻断编译
    if (lhs == TokenType::TOKEN_INT && rhs == TokenType::TOKEN_FLOAT)
    {
        std::cerr << "minic: warning: " << line << ":" << col
                  << ": implicit conversion from float to int (truncated, Q9)"
                  << std::endl;
        return true;
    }
    // char↔int 兼容（C 语义）；bool 只与 bool 赋值为同类
    if (lhs == rhs) { return true; }
    if ((lhs == TokenType::TOKEN_CHAR && rhs == TokenType::TOKEN_INT) ||
        (lhs == TokenType::TOKEN_INT  && rhs == TokenType::TOKEN_CHAR) ||
        (lhs == TokenType::TOKEN_FLOAT && rhs == TokenType::TOKEN_INT))
    {
        return true;
    }
    // 其余组合：先只警告，不返回 false
    // 理由：语法制导语义动作（parser.cpp 的 semantic025/026）还没接线，
    //       现在改成硬失败，将来接线时行为不可预期。严格类型检查应先组内对齐再改。
    std::cerr << "minic: warning: " << line << ":" << col
              << ": suspicious assignment between "
              << TokenTypeToString(lhs) << " and " << TokenTypeToString(rhs)
              << std::endl;
    return true;
}

void SymbolTable::DumpSymTab(std::ostream& os) const
{
    os << "=== Symbol Table (scope " << m_iScopeCounter << ") ===\n";

    os << "-- Variables --\n";
    for (const auto& v : m_VarInfoTbl)
    {
        os << "  " << v.m_szName << " : " << TokenTypeToString(v.m_type)
           << "  @line " << v.m_iLine << " scope " << v.m_iScope << "\n";
    }
    os << "-- Constants --\n";
    for (const auto& c : m_ConstInfoTbl)
    {
        os << "  " << (c.m_szName.empty() ? "<anon>" : c.m_szName) << " : "
           << TokenTypeToString(c.m_constType) << " = "
           << (c.m_bIsFloat ? std::to_string(c.m_fValue) : std::to_string(c.m_iValue))
           << "  @line " << c.m_iLine << "\n";
    }
    os << "-- Types --\n";
    for (const auto& t : m_TypeInfoTbl)
    {
        os << "  " << t.m_szName << " : " << TokenTypeToString(t.m_baseType)
           << "  size " << t.m_iSize << "\n";
    }
    os << "-- Procedures (challenge) --\n";
    for (const auto& p : m_ProcInfoTbl)
    {
        os << "  " << p.m_szName << " : " << TokenTypeToString(p.m_retType)
           << "  @line " << p.m_iLine << "\n";
    }
    os << "-- Labels (challenge) --\n";
    for (const auto& l : m_LabelInfoTbl)
    {
        os << "  " << l.m_szName << " -> L" << l.m_iLabelIdx << "\n";
    }
}

// ================= 第 4 章参考书扩展接口 =================

int SymbolTable::RecVarTbl(const std::string& name, TokenType type, int line)
{
    VarEntry* ve = RecVar(name, type, line, 0);
    // 用指针差算下标：重名路径 RecVar 不会 push_back，而是返回已存在项，
    // 此时 size()-1 是"最后一项"而非该项，会返回错下标（ir.cpp 按它回查会用错对象）。
    return ve ? static_cast<int>(ve - m_VarInfoTbl.data()) : -1;
}

int SymbolTable::RecConstTbl(const std::string& szVal, TokenType constType)
{
    // Q12：词法常量登记（文本先入表，名字语义阶段回填）
    ConstEntry ce;
    ce.m_szVal = szVal;
    ce.m_constType = constType;
    ce.m_bIsFloat = (constType == TokenType::TOKEN_NUM_FLOAT);
    ce.m_fValue = std::strtod(szVal.c_str(), nullptr);
    ce.m_iValue = static_cast<long long>(ce.m_fValue);
    ce.m_iProcIndex = m_curProcIndex;
    ce.m_iScope = m_iScopeCounter;
    m_ConstInfoTbl.push_back(ce);
    return static_cast<int>(m_ConstInfoTbl.size()) - 1;
}

VarEntry* SymbolTable::SearchVarInfoTbl(const std::string& name)
{
    // 【v4.0】自内向外按作用域链查找，先命中先返回（内层遮蔽外层）
    for (auto it = m_varScopeStack.rbegin(); it != m_varScopeStack.rend(); ++it)
    {
        auto found = it->find(name);
        if (found != it->end())
        {
            return &m_VarInfoTbl[found->second];
        }
    }
    return nullptr;
}

ConstEntry* SymbolTable::SearchConstInfoTbl(const std::string& name)
{
    for (auto& c : m_ConstInfoTbl)
    {
        if (c.m_szName == name)
        {
            return &c;
        }
    }
    return nullptr;
}

TypeEntry* SymbolTable::RecType(const std::string& name, TokenType baseType)
{
    TypeEntry te;
    te.m_szName = name;
    te.m_baseType = baseType;
    te.m_iSize = SizeOfType(baseType);
    te.m_iProcIndex = m_curProcIndex;
    te.m_iScope = m_iScopeCounter;
    m_TypeInfoTbl.push_back(te);
    return &m_TypeInfoTbl.back();
}

int SymbolTable::SizeOfType(TokenType baseType)
{
    // 口径与 codegen 的访问指令一致（src/codegen/codegen.cpp）：
    //   char  —— 数组元素用 movb、取值用 movsbl，1 字节
    //   int / bool / float —— 一律 movl / movss，4 字节
    //   void —— 无对象，0
    switch (baseType)
    {
        case TokenType::TOKEN_CHAR: return 1;
        case TokenType::TOKEN_INT:
        case TokenType::TOKEN_BOOL:
        case TokenType::TOKEN_FLOAT: return 4;
        default: return 0;
    }
}

void SymbolTable::RecBuiltinTypes()
{
    // 五个内建类型只登记一次。重复调用（例如同一编译单元里多次 Analyze）
    // 不会产生重复行。
    static const char* const kNames[] = { "int", "float", "bool", "char", "void" };
    static const TokenType kTypes[] = {
        TokenType::TOKEN_INT, TokenType::TOKEN_FLOAT, TokenType::TOKEN_BOOL,
        TokenType::TOKEN_CHAR, TokenType::TOKEN_VOID
    };
    for (std::size_t i = 0; i < sizeof(kTypes) / sizeof(kTypes[0]); ++i)
    {
        if (SearchTypeInfoTbl(kNames[i]) != nullptr)
        {
            continue;
        }
        RecType(kNames[i], kTypes[i]);
    }
}

TypeEntry* SymbolTable::SearchTypeInfoTbl(const std::string& name)
{
    for (auto& t : m_TypeInfoTbl)
    {
        if (t.m_szName == name)
        {
            return &t;
        }
    }
    return nullptr;
}

LabelEntry* SymbolTable::RecLabel(const std::string& name, int labelIdx, int line, int col)
{
    LabelEntry le;
    le.m_szName = name;
    le.m_iLabelIdx = labelIdx;
    le.m_iProcIndex = m_curProcIndex;
    le.m_iScope = m_iScopeCounter;
    le.m_iLine = line;
    le.m_iCol = col;
    m_LabelInfoTbl.push_back(le);
    return &m_LabelInfoTbl.back();
}

LabelEntry* SymbolTable::SearchLabelInfoTbl(const std::string& name)
{
    for (auto& l : m_LabelInfoTbl)
    {
        if (l.m_szName == name)
        {
            return &l;
        }
    }
    return nullptr;
}
