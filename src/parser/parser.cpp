// ============================================================================
// MiniC 编译器 —— 语法分析模块 CParser 实现
// 基础版文法（PPT 第 15 页）：
//   Program   → DeclList StmtList
//   DeclList  → Decl DeclList | ε          Decl → Type ID ;   Type → int | float
//   StmtList  → Stmt StmtList | ε          Stmt → ID = Expr ;
//   Expr      → Term { (+|-) Term }
//   Term      → Factor { (*|/) Factor }
//   Factor    → INT | FLOAT | ID | ( Expr )
// 实现要点（v3.1 与代码对齐）：
//   - Token 字段用 m_line(LineInfo) / m_bIsFloat / m_fVal(double) / m_iVal(long long)；
//   - 常量类别为 TOKEN_NUM_INT / TOKEN_NUM_FLOAT；
//   - NextToken() 沿用骨架语义（先 ++ 再取，越界保护，末尾停在 TOKEN_EOF），
//     Parse() 预置 m_CurToken = tokens[0]，消费完当前单词后自行调用 NextToken()；
//   - Expr/Term 用循环处理 { ... } 重复，消除左递归；
//   - 语法错误经 ReportError 记入 m_errors 并汇总至 g_ErrorList（Q8），
//     Synchronize() 跳到语句边界继续解析，尽量一次报出多个错误。
// TODO（挑战版，Q5 顺序）：if/else、while、print、数组、函数。
// ============================================================================
#include "parser/parser.h"

CParser::CParser(const TokenList& tokens)
    : m_pTokenList(&tokens), m_iListPos(0)
{
}

// ---------------------------------------------------------------------------
// 私有辅助
// ---------------------------------------------------------------------------
void CParser::NextToken()
{
    if (m_pTokenList && m_iListPos + 1 < m_pTokenList->size())
    {
        ++m_iListPos;
    }
    m_CurToken = m_pTokenList ? (*m_pTokenList)[m_iListPos] : Token();
}

bool CParser::Check(TokenType t) const
{
    return m_CurToken.m_type == t;
}

bool CParser::IsDeclStart() const
{
    return m_CurToken.m_type == TokenType::TOKEN_INT ||
           m_CurToken.m_type == TokenType::TOKEN_FLOAT;
}

// 匹配则消费当前单词并返回 true；否则记错误（不消费）并返回 false
bool CParser::Expect(TokenType t, const std::string& what)
{
    if (m_CurToken.m_type != t)
    {
        ReportError("expected " + what + ", but found '" +
                    m_CurToken.m_szContent + "' (" +
                    TokenTypeToString(m_CurToken.m_type) + ")");
        return false;
    }
    NextToken();
    return true;
}

void CParser::ReportError(const std::string& msg)
{
    ReportError(msg, m_CurToken.m_line);
}

void CParser::ReportError(const std::string& msg, const LineInfo& loc)
{
    m_errors.push_back(CompileError{msg, loc});
    EmitError(msg, loc);   // 同时汇总至全局 g_ErrorList（Q8：含行列号）
}

// 恐慌式恢复：跳过单词直到「语句/声明边界」，边界符号本身不消费
void CParser::Synchronize()
{
    while (m_CurToken.m_type != TokenType::TOKEN_EOF)
    {
        if (m_CurToken.m_type == TokenType::TOKEN_SEMI)
        {
            NextToken();   // 消费掉分号，从下一条语句继续
            return;
        }
        if (m_CurToken.m_type == TokenType::TOKEN_INT ||
            m_CurToken.m_type == TokenType::TOKEN_FLOAT ||
            m_CurToken.m_type == TokenType::TOKEN_IDENT ||
            m_CurToken.m_type == TokenType::TOKEN_RBRACE)
        {
            return;        // 已是新语句/声明的起点
        }
        NextToken();
    }
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
// 递归下降核心
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

    ASTNodePtr root = ParseProgram();

    // 程序结束后不应再有残留单词（词法输出以 TOKEN_EOF 收尾）
    if (m_CurToken.m_type != TokenType::TOKEN_EOF)
    {
        ReportError("unexpected token after end of program");
    }
    return root;
}

// Program → DeclList StmtList
// 两个列表都按「链表式」构建：ListNode 左子=当前元素，右子=剩余列表
ASTNodePtr CParser::ParseProgram()
{
    const LineInfo startLoc = m_CurToken.m_line;

    // ---- DeclList → Decl DeclList | ε ----
    ASTNodePtr declList = nullptr;
    ListNode*  declTail = nullptr;
    while (IsDeclStart())
    {
        const size_t before = m_iListPos;
        ASTNodePtr decl = ParseDeclaration();
        if (!decl)
        {
            if (m_iListPos == before && m_CurToken.m_type != TokenType::TOKEN_EOF)
            {
                NextToken();   // 无进展时强制前进，避免死循环
            }
            continue;          // 已报错，继续收集后续错误
        }
        auto node = std::make_unique<ListNode>(decl->m_loc);
        ListNode* raw = node.get();
        node->m_left = std::move(decl);
        if (declTail)
        {
            declTail->m_right = std::move(node);
        }
        else
        {
            declList = std::move(node);
        }
        declTail = raw;
    }

    // ---- StmtList → Stmt StmtList | ε ----
    ASTNodePtr stmtList = nullptr;
    ListNode*  stmtTail = nullptr;
    while (m_CurToken.m_type != TokenType::TOKEN_EOF)
    {
        const size_t before = m_iListPos;
        ASTNodePtr stmt = ParseStatement();
        if (!stmt)
        {
            if (m_iListPos == before && m_CurToken.m_type != TokenType::TOKEN_EOF)
            {
                NextToken();   // 无进展时强制前进，避免死循环
            }
            continue;
        }
        auto node = std::make_unique<ListNode>(stmt->m_loc);
        ListNode* raw = node.get();
        node->m_left = std::move(stmt);
        if (stmtTail)
        {
            stmtTail->m_right = std::move(node);
        }
        else
        {
            stmtList = std::move(node);
        }
        stmtTail = raw;
    }

    auto program = std::make_unique<ListNode>(startLoc);
    program->m_left  = std::move(declList);   // 左子 = DeclList
    program->m_right = std::move(stmtList);   // 右子 = StmtList
    return program;
}

// Stmt → ID = Expr ;
ASTNodePtr CParser::ParseStatement()
{
    // 挑战版语句暂未实现：给出明确提示，而非笼统的“期望赋值语句”
    switch (m_CurToken.m_type)
    {
    case TokenType::TOKEN_IF:
    case TokenType::TOKEN_WHILE:
    case TokenType::TOKEN_RETURN:
    case TokenType::TOKEN_LBRACE:
        ReportError("unsupported statement '" + m_CurToken.m_szContent +
                    "' (challenge edition, not implemented in base version)");
        Synchronize();
        return nullptr;
    default:
        break;
    }

    if (m_CurToken.m_type != TokenType::TOKEN_IDENT)
    {
        ReportError("expected assignment statement (identifier expected)");
        Synchronize();
        return nullptr;
    }

    const LineInfo stmtLoc = m_CurToken.m_line;
    auto lhs = std::make_unique<IdentNode>(m_CurToken.m_szContent, m_CurToken.m_line);
    NextToken();

    if (!Expect(TokenType::TOKEN_ASSIGN, "'=' in assignment"))
    {
        Synchronize();
        return nullptr;
    }

    ASTNodePtr rhs = ParseExpression();
    if (!rhs)
    {
        Synchronize();
        return nullptr;
    }

    if (!Expect(TokenType::TOKEN_SEMI, "';' after statement"))
    {
        Synchronize();
    }

    return std::make_unique<AssignNode>(std::move(lhs), std::move(rhs), stmtLoc);
}

// Decl → Type ID ;
ASTNodePtr CParser::ParseDeclaration()
{
    const LineInfo declLoc = m_CurToken.m_line;
    const TokenType declType = m_CurToken.m_type;   // TOKEN_INT / TOKEN_FLOAT
    NextToken();                                    // 消费类型关键字

    // 先取名字再消费：Expect 成功时会前进到下一个单词
    if (m_CurToken.m_type != TokenType::TOKEN_IDENT)
    {
        ReportError("expected variable name after type, but found '" +
                    m_CurToken.m_szContent + "' (" +
                    TokenTypeToString(m_CurToken.m_type) + ")");
        Synchronize();
        return nullptr;
    }
    const std::string name = m_CurToken.m_szContent;
    NextToken();                                    // 消费标识符

    if (!Expect(TokenType::TOKEN_SEMI, "';' after declaration"))
    {
        Synchronize();
    }
    return std::make_unique<DeclNode>(declType, name, declLoc);
}

// Expr → Term { (+|-) Term }   左结合，循环消除左递归
ASTNodePtr CParser::ParseExpression()
{
    ASTNodePtr left = ParseTerm();
    if (!left)
    {
        return nullptr;
    }

    while (Check(TokenType::TOKEN_PLUS) || Check(TokenType::TOKEN_MINUS))
    {
        const TokenType op = m_CurToken.m_type;
        const LineInfo  opLoc = m_CurToken.m_line;
        NextToken();

        ASTNodePtr right = ParseTerm();
        if (!right)
        {
            return nullptr;
        }
        left = std::make_unique<BinOpNode>(op, std::move(left), std::move(right), opLoc);
    }
    return left;
}

// Term → Factor { (*|/) Factor }   左结合，循环消除左递归
ASTNodePtr CParser::ParseTerm()
{
    ASTNodePtr left = ParseFactor();
    if (!left)
    {
        return nullptr;
    }

    while (Check(TokenType::TOKEN_MUL) || Check(TokenType::TOKEN_DIV))
    {
        const TokenType op = m_CurToken.m_type;
        const LineInfo  opLoc = m_CurToken.m_line;
        NextToken();

        ASTNodePtr right = ParseFactor();
        if (!right)
        {
            return nullptr;
        }
        left = std::make_unique<BinOpNode>(op, std::move(left), std::move(right), opLoc);
    }
    return left;
}

// Factor → INT | FLOAT | ID | ( Expr )
ASTNodePtr CParser::ParseFactor()
{
    const LineInfo loc = m_CurToken.m_line;

    switch (m_CurToken.m_type)
    {
    case TokenType::TOKEN_NUM_INT:
    case TokenType::TOKEN_NUM_FLOAT:
    {
        const bool isFloat = (m_CurToken.m_type == TokenType::TOKEN_NUM_FLOAT) ||
                             m_CurToken.m_bIsFloat;
        const double value = isFloat ? m_CurToken.m_fVal
                                     : static_cast<double>(m_CurToken.m_iVal);
        NextToken();
        return std::make_unique<NumNode>(value, isFloat, loc);
    }
    case TokenType::TOKEN_IDENT:
    {
        const std::string name = m_CurToken.m_szContent;
        NextToken();
        return std::make_unique<IdentNode>(name, loc);
    }
    case TokenType::TOKEN_LPAREN:
    {
        NextToken();                       // 消费 '('
        ASTNodePtr inner = ParseExpression();
        if (!inner)
        {
            return nullptr;
        }
        if (!Expect(TokenType::TOKEN_RPAREN, "')'"))
        {
            return nullptr;
        }
        return inner;                      // Factor → ( Expr )：不建冗余括号节点
    }
    default:
        ReportError("expected number, identifier or '(' , but found '" +
                    m_CurToken.m_szContent + "' (" +
                    TokenTypeToString(m_CurToken.m_type) + ")");
        return nullptr;                    // 不消费：由上层循环保证前进
    }
}
