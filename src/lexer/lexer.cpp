// ============================================================================
// MiniC 编译器 —— 词法分析模块 CLex 实现
// 已实现：识别关键字(int/float/if/else/while/return/void)、标识符、整数、
//         浮点常量、运算符(+ - * / =)、界符((){};,)；字符串常量 "..."；
//         注释 // 单行、/* */ 多行；常量登记 m_ConstTexts；
//         错误报告含行列号（Q8），错误恢复（未闭合串/多小数点/非法字符）。
// 转换表 m_szLexTbl 已初始化字符分类映射，供后续 DFA 状态机驱动迁移。
// ============================================================================
#include "lexer/lexer.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

// 转换表（Q11 手写简化版）：
// 行=DFA状态，列=字符ASCII。字符分类映射（列=ASCII，值=类别）：
//   0=空白  1=字母  2=数字  3=运算符  4=界符  5=引号  其余=99非法
// 在构造函数 InitLexTable() 中填充，兼容 C++11。
int CLex::m_szLexTbl[50][130] = {{0}};

static void InitLexTable(int tbl[50][130])
{
    // 空白类
    tbl[0][' '] = 0; tbl[0]['\t'] = 0; tbl[0]['\r'] = 0; tbl[0]['\n'] = 0;
    // 字母类
    for (char c = 'A'; c <= 'Z'; ++c) tbl[0][c] = 1;
    for (char c = 'a'; c <= 'z'; ++c) tbl[0][c] = 1;
    tbl[0]['_'] = 1;
    // 数字类
    for (char c = '0'; c <= '9'; ++c) tbl[0][c] = 2;
    // 运算符类
    tbl[0]['+'] = 3; tbl[0]['-'] = 3; tbl[0]['*'] = 3;
    tbl[0]['/'] = 3; tbl[0]['='] = 3;
    // 界符类
    tbl[0]['('] = 4; tbl[0][')'] = 4; tbl[0]['{'] = 4; tbl[0]['}'] = 4;
    tbl[0][';'] = 4; tbl[0][','] = 4;
    // 引号类
    tbl[0]['"'] = 5;
}

CLex::CLex()
{
    InitKeywordTable();
    InitLexTable(m_szLexTbl);
}

void CLex::InitKeywordTable()
{
    // MiniC 关键字集合（与文档 4.1.2 TokenType 对应）
    m_KeywordTbl["int"]    = TokenType::TOKEN_INT;
    m_KeywordTbl["float"]  = TokenType::TOKEN_FLOAT;
    m_KeywordTbl["if"]     = TokenType::TOKEN_IF;
    m_KeywordTbl["else"]   = TokenType::TOKEN_ELSE;
    m_KeywordTbl["while"]  = TokenType::TOKEN_WHILE;
    m_KeywordTbl["return"] = TokenType::TOKEN_RETURN;
    m_KeywordTbl["void"]   = TokenType::TOKEN_VOID;
}

int CLex::LookupKeyword(const std::string& word) const
{
    auto it = m_KeywordTbl.find(word);
    return (it != m_KeywordTbl.end()) ? static_cast<int>(it->second) : -1;
}

bool CLex::OpenSource(const std::string& szFileName)
{
    m_szFileName = szFileName;
    std::ifstream ifs(szFileName, std::ios::binary);
    if (!ifs)
    {
        EmitError("cannot open source file: " + szFileName, LineInfo(0, 0));
        return false;
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    m_szSource = oss.str();
    m_iPos = 0;
    m_iRow = 1;
    m_iCol = 1;
    m_TokenList.clear();
    m_ConstTexts.clear();
    return true;
}

void CLex::SkipSpace()
{
    while (m_iPos < m_szSource.size())
    {
        char ch = m_szSource[m_iPos];
        if (ch == ' ' || ch == '\t' || ch == '\r')
        {
            ++m_iPos;
            ++m_iCol;
        }
        else if (ch == '\n')
        {
            ++m_iPos;
            ++m_iRow;
            m_iCol = 1;
        }
        else
        {
            break;
        }
    }
}

void CLex::SkipComment()
{
    // 调用前提：m_szSource[m_iPos] == '/'
    // 形式：// 单行注释   或   /* 多行注释 */
    if (m_iPos + 1 >= m_szSource.size())
        return;

    char next = m_szSource[m_iPos + 1];
    if (next == '/')
    {
        // 单行注释：跳到行尾
        while (m_iPos < m_szSource.size() && m_szSource[m_iPos] != '\n')
        {
            ++m_iPos;
            ++m_iCol;
        }
    }
    else if (next == '*')
    {
        // 多行注释：跳到 */
        int startRow = m_iRow, startCol = m_iCol;
        m_iPos += 2;
        m_iCol += 2;
        bool closed = false;
        while (m_iPos < m_szSource.size())
        {
            if (m_szSource[m_iPos] == '*' &&
                m_iPos + 1 < m_szSource.size() &&
                m_szSource[m_iPos + 1] == '/')
            {
                m_iPos += 2;
                m_iCol += 2;
                closed = true;
                break;
            }
            if (m_szSource[m_iPos] == '\n')
            {
                ++m_iPos;
                ++m_iRow;
                m_iCol = 1;
            }
            else
            {
                ++m_iPos;
                ++m_iCol;
            }
        }
        if (!closed)
        {
            EmitError("unterminated /* comment (started here)", LineInfo(startRow, startCol));
        }
    }
    else
    {
        // 不是注释，是除法运算符，留给 ScanOperator 处理
        return;
    }
}

void CLex::ScanIdent()
{
    size_t start = m_iPos;
    int col = m_iCol;
    while (m_iPos < m_szSource.size() &&
           (std::isalnum(static_cast<unsigned char>(m_szSource[m_iPos])) ||
            m_szSource[m_iPos] == '_'))
    {
        ++m_iPos;
        ++m_iCol;
    }
    std::string word = m_szSource.substr(start, m_iPos - start);
    int kw = LookupKeyword(word);
    TokenType type = (kw >= 0) ? static_cast<TokenType>(kw) : TokenType::TOKEN_IDENT;
    m_TokenList.emplace_back(type, word, m_iRow, col);
}

void CLex::ScanNumber()
{
    size_t start = m_iPos;
    int col = m_iCol;
    bool isFloat = false;

    // 整数部分
    while (m_iPos < m_szSource.size() &&
           std::isdigit(static_cast<unsigned char>(m_szSource[m_iPos])))
    {
        ++m_iPos;
        ++m_iCol;
    }
    // 小数部分
    if (m_iPos < m_szSource.size() && m_szSource[m_iPos] == '.')
    {
        isFloat = true;
        ++m_iPos;
        ++m_iCol;
        while (m_iPos < m_szSource.size() &&
               std::isdigit(static_cast<unsigned char>(m_szSource[m_iPos])))
        {
            ++m_iPos;
            ++m_iCol;
        }
    }

    std::string text = m_szSource.substr(start, m_iPos - start);

    // 错误恢复：数字中出现第二个小数点（如 1.2.3）
    if (isFloat && m_iPos < m_szSource.size() && m_szSource[m_iPos] == '.')
    {
        EmitError("malformed number: extra decimal point", LineInfo(m_iRow, m_iCol));
        // 跳过多余的小数点及其后的数字，避免级联报错
        ++m_iPos; ++m_iCol;
        while (m_iPos < m_szSource.size() &&
               std::isdigit(static_cast<unsigned char>(m_szSource[m_iPos])))
        {
            ++m_iPos; ++m_iCol;
        }
    }

    TokenType type = isFloat ? TokenType::TOKEN_NUM_FLOAT : TokenType::TOKEN_NUM_INT;
    Token tok(type, text, m_iRow, col);
    tok.m_bIsFloat = isFloat;
    if (isFloat)
    {
        tok.m_fVal = std::strtod(text.c_str(), nullptr);
        tok.m_iVal = static_cast<long long>(tok.m_fVal);
    }
    else
    {
        tok.m_iVal = std::strtoll(text.c_str(), nullptr, 10);
        tok.m_fVal = static_cast<double>(tok.m_iVal);
    }

    // Q12：常量词法登记——文本登记入 m_ConstTexts，Token 记录位序
    tok.m_iConstIdx = static_cast<int>(m_ConstTexts.size());
    m_ConstTexts.push_back(text);

    m_TokenList.push_back(tok);
}

void CLex::ScanString()
{
    // 调用前提：m_szSource[m_iPos] == '"'
    int row = m_iRow, col = m_iCol;
    ++m_iPos;  // 跳过开头的 "
    ++m_iCol;

    size_t start = m_iPos;
    bool closed = false;
    while (m_iPos < m_szSource.size())
    {
        char ch = m_szSource[m_iPos];
        if (ch == '"')
        {
            closed = true;
            break;
        }
        if (ch == '\n')
        {
            // 字符串跨行：报错但继续扫描
            EmitError("newline in string literal", LineInfo(m_iRow, m_iCol));
            ++m_iRow;
            m_iCol = 1;
            ++m_iPos;
        }
        else
        {
            ++m_iPos;
            ++m_iCol;
        }
    }
    std::string text = m_szSource.substr(start, m_iPos - start);
    if (closed)
    {
        ++m_iPos;  // 跳过收尾的 "
        ++m_iCol;
    }
    else
    {
        EmitError("unterminated string literal", LineInfo(row, col));
    }
    // 字符串原文登记入常量表
    Token tok(TokenType::TOKEN_STRING, "\"" + text + "\"", row, col);
    tok.m_iConstIdx = static_cast<int>(m_ConstTexts.size());
    m_ConstTexts.push_back(text);
    m_TokenList.push_back(tok);
}

void CLex::ScanOperator()
{
    char ch = m_szSource[m_iPos];
    int col = m_iCol;
    TokenType type = TokenType::TOKEN_NONE;
    switch (ch)
    {
    case '+': type = TokenType::TOKEN_PLUS;   break;
    case '-': type = TokenType::TOKEN_MINUS;  break;
    case '*': type = TokenType::TOKEN_MUL;    break;
    case '/': type = TokenType::TOKEN_DIV;    break;
    case '=': type = TokenType::TOKEN_ASSIGN; break;
    case '(': type = TokenType::TOKEN_LPAREN; break;
    case ')': type = TokenType::TOKEN_RPAREN; break;
    case '{': type = TokenType::TOKEN_LBRACE; break;
    case '}': type = TokenType::TOKEN_RBRACE; break;
    case ';': type = TokenType::TOKEN_SEMI;   break;
    case ',': type = TokenType::TOKEN_COMMA;  break;
    default:
        // 未识别字符：报错（含行列号），跳过继续扫描（Q8 尽量恢复）
        EmitError(std::string("unexpected character '") + ch + "'", LineInfo(m_iRow, m_iCol));
        ++m_iPos;
        ++m_iCol;
        return;
    }
    ++m_iPos;
    ++m_iCol;
    m_TokenList.emplace_back(type, std::string(1, ch), m_iRow, col);
}

bool CLex::Run()
{
    m_TokenList.clear();
    m_ConstTexts.clear();

    while (m_iPos < m_szSource.size())
    {
        SkipSpace();
        if (m_iPos >= m_szSource.size())
        {
            break;
        }
        char ch = m_szSource[m_iPos];

        // 注释：// 或 /* ... */
        if (ch == '/' && m_iPos + 1 < m_szSource.size() &&
            (m_szSource[m_iPos + 1] == '/' || m_szSource[m_iPos + 1] == '*'))
        {
            SkipComment();
            continue;
        }

        // 字符串常量
        if (ch == '"')
        {
            ScanString();
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_')
        {
            ScanIdent();
        }
        else if (std::isdigit(static_cast<unsigned char>(ch)))
        {
            ScanNumber();
        }
        else
        {
            ScanOperator();
        }
    }

    // 追加 EOF 标记
    m_TokenList.emplace_back(TokenType::TOKEN_EOF, "<eof>", m_iRow, m_iCol);
    return true;
}

const TokenList& CLex::GetTokenList() const
{
    return m_TokenList;
}
