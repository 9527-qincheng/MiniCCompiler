// ============================================================================
// MiniC 编译器 —— 语法分析模块 CParser 实现
// 扩展文法（在 PPT 基础版文法上增加一元运算与控制流）：
//   Program   → DeclList StmtList
//   DeclList  → Decl DeclList | ε          Decl → Type ID ;   Type → int | float
//   StmtList  → Stmt StmtList | ε
//   Stmt      → ID = Expr ;
//             | if ( Expr ) Stmt [ else Stmt ]
//             | while ( Expr ) Stmt
//             | return [ Expr ] ;
//             | { StmtList }
//   Expr      → Term { (+|-) Term }
//   Term      → Factor { (*|/) Factor }
//   Factor    → (+|-) Factor | INT | FLOAT | ID | ( Expr )
// 实现要点（v3.1 与代码对齐）：
//   - Token 字段用 m_line(LineInfo) / m_bIsFloat / m_fVal(double) / m_iVal(long long)；
//   - 常量类别为 TOKEN_NUM_INT / TOKEN_NUM_FLOAT；
//   - NextToken() 沿用骨架语义（先 ++ 再取，越界保护，末尾停在 TOKEN_EOF），
//     Parse() 预置 m_CurToken = tokens[0]，消费完当前单词后自行调用 NextToken()；
//   - Expr/Term 用循环处理 { ... } 重复，消除左递归；
//   - 语法错误经 ReportError 记入 m_errors 并汇总至 g_ErrorList（Q8），
//     Synchronize() 跳到语句边界继续解析，尽量一次报出多个错误。
// 依赖说明（跨模块，未在本模块实现）：
//   - 条件里的比较运算（< > <= >= == !=）需要词法模块补充 TokenType，
//     现有一元/算术条件已可解析；断言 print/数组/函数按 Q5 顺序后续开展
//     （print 需词法提供 print 记号约定，数组需 [ ] 界符）。
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

    // ---- StmtList → Stmt StmtList | ε ----（解析到程序末尾）
    ASTNodePtr stmtList = ParseStmtList(TokenType::TOKEN_EOF);

    auto program = std::make_unique<ListNode>(startLoc);
    program->m_left  = std::move(declList);   // 左子 = DeclList
    program->m_right = std::move(stmtList);   // 右子 = StmtList
    return program;
}

// StmtList → Stmt StmtList | ε
// 解析一条条语句直到 terminator（不消费 terminator，EOF 亦自动收尾）。
// 链表式构建：ListNode 左子=当前语句，右子=剩余序列。程序与复合语句共用。
ASTNodePtr CParser::ParseStmtList(TokenType terminator)
{
    auto atEnd = [this, terminator]() {
        if (m_CurToken.m_type == TokenType::TOKEN_EOF)
        {
            return true;
        }
        return terminator != TokenType::TOKEN_EOF &&
               m_CurToken.m_type == terminator;
    };

    ASTNodePtr head = nullptr;
    ListNode*  tail = nullptr;
    while (!atEnd())
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
        if (tail)
        {
            tail->m_right = std::move(node);
        }
        else
        {
            head = std::move(node);
        }
        tail = raw;
    }
    return head;
}

// Stmt → ID = Expr ;
//      | if ( Expr ) Stmt [ else Stmt ]
//      | while ( Expr ) Stmt
//      | return [ Expr ] ;
//      | { StmtList }
ASTNodePtr CParser::ParseStatement()
{
    switch (m_CurToken.m_type)
    {
    case TokenType::TOKEN_LBRACE: return ParseBlock();
    case TokenType::TOKEN_IF:     return ParseIfStatement();
    case TokenType::TOKEN_WHILE:  return ParseWhileStatement();
    case TokenType::TOKEN_RETURN: return ParseReturnStatement();
    default:                      break;
    }

    if (m_CurToken.m_type != TokenType::TOKEN_IDENT)
    {
        ReportError("unexpected '" + m_CurToken.m_szContent + "' (" +
                    TokenTypeToString(m_CurToken.m_type) +
                    "); expected a statement (assignment, if, while, return or block)");
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

// Stmt → { StmtList }
// 复合语句：左子挂语句序列；空块 { } 的左子为 nullptr。
ASTNodePtr CParser::ParseBlock()
{
    const LineInfo blockLoc = m_CurToken.m_line;   // '{'
    NextToken();                                   // 消费 '{'

    ASTNodePtr body = ParseStmtList(TokenType::TOKEN_RBRACE);

    if (m_CurToken.m_type == TokenType::TOKEN_RBRACE)
    {
        NextToken();                               // 消费 '}'
    }
    else
    {
        ReportError("expected '}' to close block, but found '" +
                    m_CurToken.m_szContent + "' (" +
                    TokenTypeToString(m_CurToken.m_type) + ")");
        Synchronize();
    }

    auto block = std::make_unique<ListNode>(blockLoc);
    block->m_left = std::move(body);
    return block;
}

// Stmt → if ( Expr ) Stmt [ else Stmt ]
// 递归下降天然实现“else 就近匹配”，无需额外规则。
ASTNodePtr CParser::ParseIfStatement()
{
    const LineInfo ifLoc = m_CurToken.m_line;
    NextToken();                                   // 消费 'if'

    if (!Expect(TokenType::TOKEN_LPAREN, "'(' after 'if'"))
    {
        Synchronize();
        return nullptr;
    }
    ASTNodePtr cond = ParseExpression();
    if (!cond)
    {
        Synchronize();
        return nullptr;
    }
    if (!Expect(TokenType::TOKEN_RPAREN, "')' after 'if' condition"))
    {
        Synchronize();
        return nullptr;
    }

    ASTNodePtr thenStmt = ParseStatement();
    if (!thenStmt)
    {
        return nullptr;                            // 子语句已报错
    }

    ASTNodePtr elseStmt = nullptr;
    if (m_CurToken.m_type == TokenType::TOKEN_ELSE)
    {
        NextToken();                               // 消费 'else'
        elseStmt = ParseStatement();
        if (!elseStmt)
        {
            return nullptr;
        }
    }

    return std::make_unique<IfNode>(std::move(cond), std::move(thenStmt),
                                    std::move(elseStmt), ifLoc);
}

// Stmt → while ( Expr ) Stmt
ASTNodePtr CParser::ParseWhileStatement()
{
    const LineInfo whileLoc = m_CurToken.m_line;
    NextToken();                                   // 消费 'while'

    if (!Expect(TokenType::TOKEN_LPAREN, "'(' after 'while'"))
    {
        Synchronize();
        return nullptr;
    }
    ASTNodePtr cond = ParseExpression();
    if (!cond)
    {
        Synchronize();
        return nullptr;
    }
    if (!Expect(TokenType::TOKEN_RPAREN, "')' after 'while' condition"))
    {
        Synchronize();
        return nullptr;
    }

    ASTNodePtr body = ParseStatement();
    if (!body)
    {
        return nullptr;                            // 子语句已报错
    }

    return std::make_unique<WhileNode>(std::move(cond), std::move(body), whileLoc);
}

// Stmt → return [ Expr ] ;
ASTNodePtr CParser::ParseReturnStatement()
{
    const LineInfo returnLoc = m_CurToken.m_line;
    NextToken();                                   // 消费 'return'

    ASTNodePtr expr = nullptr;
    if (m_CurToken.m_type != TokenType::TOKEN_SEMI)
    {
        expr = ParseExpression();
        if (!expr)
        {
            Synchronize();
            return nullptr;
        }
    }

    if (!Expect(TokenType::TOKEN_SEMI, "';' after return statement"))
    {
        Synchronize();
    }
    return std::make_unique<ReturnNode>(std::move(expr), returnLoc);
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

    // 基础版文法 Decl → Type ID ;：不支持一条声明里出现多个声明符（int a, b;）
    if (m_CurToken.m_type == TokenType::TOKEN_COMMA)
    {
        ReportError("multiple declarators in one declaration are not supported; "
                    "split into separate declarations like 'int a; int b;'");
        // 跳到本条声明的分号，避免把后续声明符再当成一条语句重复报错
        while (m_CurToken.m_type != TokenType::TOKEN_SEMI &&
               m_CurToken.m_type != TokenType::TOKEN_EOF)
        {
            NextToken();
        }
        if (m_CurToken.m_type == TokenType::TOKEN_SEMI)
        {
            NextToken();
        }
        return std::make_unique<DeclNode>(declType, name, declLoc);
    }

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

// Factor → (+|-) Factor | INT | FLOAT | ID | ( Expr )
ASTNodePtr CParser::ParseFactor()
{
    const LineInfo loc = m_CurToken.m_line;

    switch (m_CurToken.m_type)
    {
    case TokenType::TOKEN_PLUS:
    case TokenType::TOKEN_MINUS:
    {
        // 一元运算符：优先级高于 * /，右结合，可连续出现（--a）
        const TokenType op = m_CurToken.m_type;
        NextToken();
        ASTNodePtr operand = ParseFactor();
        if (!operand)
        {
            return nullptr;
        }
        return std::make_unique<UnaryOpNode>(op, std::move(operand), loc);
    }
    case TokenType::TOKEN_NUM_INT:
    case TokenType::TOKEN_NUM_FLOAT:
    {
        const bool isFloat = (m_CurToken.m_type == TokenType::TOKEN_NUM_FLOAT) ||
                             m_CurToken.m_bIsFloat;
        const double value = isFloat ? m_CurToken.m_fVal
                                     : static_cast<double>(m_CurToken.m_iVal);
        const int constIdx = m_CurToken.m_iConstIdx;   // 消费前捕获常量表位序（Q12）
        NextToken();
        return std::make_unique<NumNode>(value, isFloat, loc, constIdx);
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
        ReportError("expected number, identifier, '(' or unary sign, but found '" +
                    m_CurToken.m_szContent + "' (" +
                    TokenTypeToString(m_CurToken.m_type) + ")");
        return nullptr;                    // 不消费：由上层循环保证前进
    }
}
