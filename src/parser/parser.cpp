// ============================================================================
// MiniC 编译器 —— 语法分析模块 CParser 实现
// 状态：骨架版——接口/状态完整，递归下降核心逻辑留 TODO（词法模块先行演示）
// TODO（下一阶段）：
//   1. 按文法实现 ParseProgram/ParseStatement/ParseDeclaration/
//      ParseExpression/ParseTerm/ParseFactor（优先级：声明→赋值→算术）
//   2. 错误处理与恢复（Q8：报告行列号并尽量恢复，参考书 EmitError 风格）
//   3. 挑战版：if/else/while/print/数组/函数（Q5 顺序）
// ============================================================================
#include "parser/parser.h"

CParser::CParser(const TokenList& tokens)
    : m_pTokenList(&tokens), m_iListPos(0)
{
}

void CParser::NextToken()
{
    if (m_pTokenList && m_iListPos + 1 < m_pTokenList->size())
    {
        ++m_iListPos;
    }
    m_CurToken = m_pTokenList ? (*m_pTokenList)[m_iListPos] : Token();
}

bool CParser::HasError() const
{
    return !m_errors.empty();
}

const std::vector<CompileError>& CParser::GetErrors() const
{
    return m_errors;
}

// ============================================================================
// 递归下降核心：TODO —— 骨架仅完成状态初始化，不做实际语法分析
// ============================================================================
ASTNodePtr CParser::Parse()
{
    m_iListPos = 0;
    m_errors.clear();
    if (!m_pTokenList || m_pTokenList->empty())
    {
        EmitError("empty token list", LineInfo(1, 1));
        return nullptr;
    }
    m_CurToken = (*m_pTokenList)[0];

    // TODO: 实现递归下降解析
    //   root = ParseProgram();
    //   （ParseProgram 内：循环 ParseStatement/ParseDeclaration，构建 ListNode）
    // 骨架阶段：不构建 AST，返回 nullptr；语法错误清空（避免阻断词法演示）
    return nullptr;
}

ASTNodePtr CParser::ParseProgram()
{
    // TODO: 程序 = 语句/声明序列
    return nullptr;
}

ASTNodePtr CParser::ParseStatement()
{
    // TODO: 赋值语句 a = expr; / （挑战版）控制流语句
    return nullptr;
}

ASTNodePtr CParser::ParseDeclaration()
{
    // TODO: int/float 声明
    return nullptr;
}

ASTNodePtr CParser::ParseExpression()
{
    // TODO: expr -> term (('+'|'-') term)*
    return nullptr;
}

ASTNodePtr CParser::ParseTerm()
{
    // TODO: term -> factor (('*'|'/') factor)*
    return nullptr;
}

ASTNodePtr CParser::ParseFactor()
{
    // TODO: factor -> NUM | IDENT | '(' expr ')'
    return nullptr;
}
