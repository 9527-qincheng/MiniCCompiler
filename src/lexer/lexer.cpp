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
#include "common/pathutil.h"
#include <sstream>

// DFA 状态定义（s0~s6）
//   s0 = S_START      初始状态，读入一个字符后决定走向
//   s1 = S_IDENT      正在识别标识符/关键字
//   s2 = S_NUMBER     正在识别整数
//   s3 = S_NUMBER_DOT 正在识别浮点数（已遇小数点）
//   s4 = S_STRING     正在识别字符串 "..."
//   s5 = S_CHAR       正在识别字符常量 'c'
//   s6 = S_OP         正在识别运算符/界符
// 字符类别（列=ASCII，值=类别）：
//   0=空白  1=字母  2=数字  3=运算符  4=界符  5=双引号  6=单引号  其余=非法
// m_szLexTbl[状态][字符类别] = 下一状态
int CLex::m_szLexTbl[50][256] = {{0}};
bool CLex::m_bAccept[50] = {false};

static void InitLexTable(int tbl[50][256])
{
    // === DFA 状态编号 ===
    // 0 = S_START      初始状态
    // 1 = S_IDENT      标识符/关键字
    // 2 = S_NUMBER     数字
    // 3 = S_NUMBER_DOT 浮点数小数部分
    // 4 = S_STRING     字符串
    // 5 = S_CHAR       字符常量
    // 6 = S_OP         运算符/界符
    // 99 = S_ERROR     非法

    // --- 先全部填成错误态 ---
    for (int s = 0; s < 50; ++s)
        for (int c = 0; c < 256; ++c)
            tbl[s][c] = 99;

    // --- S_START (状态0) 的转移 ---
    // 空白留在初始态（SkipSpace会吃掉）
    tbl[0][' '] = 0; tbl[0]['\t'] = 0; tbl[0]['\r'] = 0; tbl[0]['\n'] = 0;
    // 字母/下划线 -> S_IDENT
    for (int c = 'A'; c <= 'Z'; ++c) tbl[0][c] = 1;
    for (int c = 'a'; c <= 'z'; ++c) tbl[0][c] = 1;
    tbl[0]['_'] = 1;
    // 数字 -> S_NUMBER
    for (int c = '0'; c <= '9'; ++c) tbl[0][c] = 2;
    // 双引号 -> S_STRING
    tbl[0]['"'] = 4;
    // 单引号 -> S_CHAR
    tbl[0]['\''] = 5;
    // 运算符 -> S_OP
    tbl[0]['+'] = 6; tbl[0]['-'] = 6; tbl[0]['*'] = 6;
    tbl[0]['/'] = 6; tbl[0]['='] = 6;
    tbl[0]['<'] = 6; tbl[0]['>'] = 6; tbl[0]['!'] = 6;
    tbl[0]['&'] = 6; tbl[0]['|'] = 6;
    // 界符 -> S_OP
    tbl[0]['('] = 6; tbl[0][')'] = 6; tbl[0]['{'] = 6; tbl[0]['}'] = 6;
    tbl[0][';'] = 6; tbl[0][','] = 6;
    tbl[0]['['] = 6; tbl[0][']'] = 6;

    // --- S_IDENT (状态1) 的转移：后续字母数字下划线留在标识符状态 ---
    for (int c = 'A'; c <= 'Z'; ++c) tbl[1][c] = 1;
    for (int c = 'a'; c <= 'z'; ++c) tbl[1][c] = 1;
    for (int c = '0'; c <= '9'; ++c) tbl[1][c] = 1;
    tbl[1]['_'] = 1;

    // --- S_NUMBER (状态2) 的转移 ---
    for (int c = '0'; c <= '9'; ++c) tbl[2][c] = 2;
    tbl[2]['.'] = 3;  // 遇小数点转 S_NUMBER_DOT

    // --- S_NUMBER_DOT (状态3) 的转移 ---
    for (int c = '0'; c <= '9'; ++c) tbl[3][c] = 3;
}

CLex::CLex()
{
    InitKeywordTable();
    InitLexTable(m_szLexTbl);
    // 接受态标记
    m_bAccept[1] = true;   // S_IDENT
    m_bAccept[2] = true;   // S_NUMBER
    m_bAccept[3] = true;   // S_NUMBER_DOT
}

void CLex::InitKeywordTable()
{
    // v4.0 挑战版关键字集合（12 个）
    m_KeywordTbl["int"]    = TokenType::TOKEN_INT;
    m_KeywordTbl["float"]  = TokenType::TOKEN_FLOAT;
    m_KeywordTbl["bool"]   = TokenType::TOKEN_BOOL;
    m_KeywordTbl["char"]   = TokenType::TOKEN_CHAR;
    m_KeywordTbl["void"]   = TokenType::TOKEN_VOID;
    m_KeywordTbl["if"]     = TokenType::TOKEN_IF;
    m_KeywordTbl["else"]   = TokenType::TOKEN_ELSE;
    m_KeywordTbl["while"] = TokenType::TOKEN_WHILE;
    m_KeywordTbl["print"]  = TokenType::TOKEN_PRINT;
    m_KeywordTbl["return"] = TokenType::TOKEN_RETURN;
    m_KeywordTbl["true"]   = TokenType::TOKEN_TRUE;
    m_KeywordTbl["false"]  = TokenType::TOKEN_FALSE;
}

int CLex::LookupKeyword(const std::string& word) const
{
    auto it = m_KeywordTbl.find(word);
    return (it != m_KeywordTbl.end()) ? static_cast<int>(it->second) : -1;
}

bool CLex::OpenSource(const std::string& szFileName)
{
    m_szFileName = szFileName;
    // 走宽字符路径：Windows 下 std::ifstream 会把 std::string 当 ANSI 代码页解释，
    // 路径含该代码页表示不了的字（生僻字 / emoji）时打不开。见 common/pathutil.h。
    if (!minic_path::ReadFileUtf8(szFileName, m_szSource))
    {
        EmitError("cannot open source file: " + szFileName, LineInfo(0, 0));
        return false;
    }

    // 跳过 UTF-8 BOM（EF BB BF）：Windows 记事本/PS 保存的文件常带 BOM
    if (m_szSource.size() >= 3 &&
        static_cast<unsigned char>(m_szSource[0]) == 0xEF &&
        static_cast<unsigned char>(m_szSource[1]) == 0xBB &&
        static_cast<unsigned char>(m_szSource[2]) == 0xBF)
    {
        m_szSource.erase(0, 3);
    }

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

void CLex::ScanCharConst()
{
    // 调用前提：m_szSource[m_iPos] == '\''
    int row = m_iRow, col = m_iCol;
    size_t start = m_iPos;       // 含开头引号，用于回填原文
    ++m_iPos;                    // 跳过开头 '
    ++m_iCol;

    char cVal = 0;
    bool ok = true;   // 是否成功产出 token（出错则跳过，不产出）

    if (m_iPos >= m_szSource.size())
    {
        EmitError("unterminated char literal", LineInfo(row, col));
        return;
    }

    char ch = m_szSource[m_iPos];
    if (ch == '\\')
    {
        // 转义序列
        ++m_iPos;
        ++m_iCol;
        if (m_iPos >= m_szSource.size())
        {
            EmitError("unterminated char literal after escape", LineInfo(row, col));
            return;
        }
        char esc = m_szSource[m_iPos];
        switch (esc)
        {
        case 'n':  cVal = '\n'; break;
        case 't':  cVal = '\t'; break;
        case 'r':  cVal = '\r'; break;
        case '0':  cVal = '\0'; break;
        case '\\': cVal = '\\'; break;
        case '\'': cVal = '\''; break;
        case '"':  cVal = '"';  break;
        default:
            EmitError(std::string("unknown escape sequence '\\") + esc + "'",
                      LineInfo(m_iRow, m_iCol));
            cVal = esc;
            ok = false;
            break;
        }
        ++m_iPos;
        ++m_iCol;
    }
    else if (ch == '\'')
    {
        // 空字符常量 ''
        EmitError("empty char literal", LineInfo(row, col));
        ok = false;
    }
    else if (ch == '\n')
    {
        EmitError("newline in char literal", LineInfo(row, col));
        ok = false;
        ++m_iRow;
        m_iCol = 1;
        ++m_iPos;
    }
    else
    {
        cVal = ch;
        ++m_iPos;
        ++m_iCol;
        // 多字符常量检测：消费完一个字符后，下一个不是 ' 就是错误
        if (m_iPos < m_szSource.size() && m_szSource[m_iPos] != '\'')
        {
            EmitError("multi-character char literal", LineInfo(row, col));
            ok = false;
            // 跳到行尾或收尾引号，尽量恢复
            while (m_iPos < m_szSource.size() &&
                   m_szSource[m_iPos] != '\'' &&
                   m_szSource[m_iPos] != '\n')
            {
                ++m_iPos;
                ++m_iCol;
            }
        }
    }

    // 期望收尾 '
    if (m_iPos < m_szSource.size() && m_szSource[m_iPos] == '\'')
    {
        ++m_iPos;
        ++m_iCol;
    }
    else if (ok)
    {
        EmitError("unterminated char literal", LineInfo(row, col));
        ok = false;
    }

    // 出错则不产出 token（方案 B），让语法阶段自然发现缺失
    if (!ok) return;

    std::string text = m_szSource.substr(start, m_iPos - start);
    Token tok(TokenType::TOKEN_CHAR_CONST, text, row, col);
    tok.m_cVal = cVal;
    tok.m_iVal = static_cast<long long>(static_cast<unsigned char>(cVal));
    m_TokenList.push_back(tok);
}

void CLex::ScanOperator()
{
    // v4.0：双字符运算符优先，再回落单字符
    int col = m_iCol;
    char ch = m_szSource[m_iPos];

    // 步骤 1~2：先看两位，查双字符表
    if (m_iPos + 1 < m_szSource.size())
    {
        char two[3] = { ch, m_szSource[m_iPos + 1], '\0' };
        TokenType dbl = TokenType::TOKEN_NONE;
        if      (two[0]=='=' && two[1]=='=') dbl = TokenType::TOKEN_EQ;
        else if (two[0]=='!' && two[1]=='=') dbl = TokenType::TOKEN_NE;
        else if (two[0]=='<' && two[1]=='=') dbl = TokenType::TOKEN_LE;
        else if (two[0]=='>' && two[1]=='=') dbl = TokenType::TOKEN_GE;
        else if (two[0]=='&' && two[1]=='&') dbl = TokenType::TOKEN_AND;
        else if (two[0]=='|' && two[1]=='|') dbl = TokenType::TOKEN_OR;
        // 挑战版扩展：自增/自减（最长匹配优先，+ + 不会被拆成两个 PLUS）
        else if (two[0]=='+' && two[1]=='+') dbl = TokenType::TOKEN_INC;
        else if (two[0]=='-' && two[1]=='-') dbl = TokenType::TOKEN_DEC;
        if (dbl != TokenType::TOKEN_NONE)
        {
            m_TokenList.emplace_back(dbl, std::string(two, 2), m_iRow, col);
            m_iPos += 2;
            m_iCol += 2;
            return;
        }
    }

    // 步骤 3：单字符表
    TokenType type = TokenType::TOKEN_NONE;
    switch (ch)
    {
    case '+': type = TokenType::TOKEN_PLUS;     break;
    case '-': type = TokenType::TOKEN_MINUS;    break;
    case '*': type = TokenType::TOKEN_MUL;      break;
    case '/': type = TokenType::TOKEN_DIV;      break;
    case '=': type = TokenType::TOKEN_ASSIGN;   break;
    case '<': type = TokenType::TOKEN_LT;       break;
    case '>': type = TokenType::TOKEN_GT;       break;
    case '!': type = TokenType::TOKEN_NOT;      break;
    case '(': type = TokenType::TOKEN_LPAREN;   break;
    case ')': type = TokenType::TOKEN_RPAREN;   break;
    case '{': type = TokenType::TOKEN_LBRACE;   break;
    case '}': type = TokenType::TOKEN_RBRACE;   break;
    case '[': type = TokenType::TOKEN_LBRACKET; break;
    case ']': type = TokenType::TOKEN_RBRACKET; break;
    case ';': type = TokenType::TOKEN_SEMI;     break;
    case ',': type = TokenType::TOKEN_COMMA;    break;
    default:
        // 步骤 4：未识别字符（含单独的 & |）
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

    // === 教科书式 DFA：逐字符查表 + 接受态回退 ===
    // 状态：0=初始 1=标识符 2=整数 3=浮点数 99=错误
    // 接受态：1(标识符) 2(整数) 3(浮点数)
    while (true)
    {
        SkipSpace();
        if (m_iPos >= m_szSource.size()) break;

        unsigned char ch = static_cast<unsigned char>(m_szSource[m_iPos]);

        // 非标识符/数字开头的，直接交给原Scan函数（字符串/字符/运算符/注释）
        if (ch == '"')  { ScanString();   continue; }
        if (ch == '\'') { ScanCharConst(); continue; }
        if (ch == '/' && m_iPos + 1 < m_szSource.size() &&
            (m_szSource[m_iPos+1]=='/' || m_szSource[m_iPos+1]=='*'))
        { SkipComment(); continue; }

        // 查s0表：决定是标识符还是数字
        int first = m_szLexTbl[0][ch];
        if (first != 1 && first != 2) {  // 运算符/界符/非法
            if (first == 6) { ScanOperator(); continue; }
            // 非法字符
            EmitError(std::string("unexpected character '") + (char)ch + "'",
                      LineInfo(m_iRow, m_iCol));
            ++m_iPos; ++m_iCol;
            continue;
        }

        // 真DFA：逐字符查表转移
        size_t startPos = m_iPos;
        int startRow = m_iRow, startCol = m_iCol;
        int state = first;
        size_t lastAccept = m_iPos;
        int lastRow = m_iRow, lastCol = m_iCol;

        while (m_iPos < m_szSource.size())
        {
            unsigned char c = static_cast<unsigned char>(m_szSource[m_iPos]);
            int next = m_szLexTbl[state][c];

            if (next == 99) break;  // 非法转移，停止

            state = next;
            // 前进
            if (c == '\n') { ++m_iRow; m_iCol = 1; } else { ++m_iCol; }
            ++m_iPos;

            // 接受态：记录位置
            if (m_bAccept[state]) {
                lastAccept = m_iPos;
                lastRow = m_iRow; lastCol = m_iCol;
            }
        }

        // 回退到最后一个接受位置
        m_iPos = lastAccept; m_iRow = lastRow; m_iCol = lastCol;

        // 归约输出token
        std::string text = m_szSource.substr(startPos, m_iPos - startPos);
        if (state == 1) {
            int kw = LookupKeyword(text);
            TokenType type = (kw >= 0) ? static_cast<TokenType>(kw) : TokenType::TOKEN_IDENT;
            m_TokenList.emplace_back(type, text, startRow, startCol);
        } else {
            bool isFloat = (state == 3);
            TokenType type = isFloat ? TokenType::TOKEN_NUM_FLOAT : TokenType::TOKEN_NUM_INT;
            Token tok(type, text, startRow, startCol);
            tok.m_bIsFloat = isFloat;
            if (isFloat) {
                tok.m_fVal = std::strtod(text.c_str(), nullptr);
                tok.m_iVal = static_cast<long long>(tok.m_fVal);
            } else {
                tok.m_iVal = std::strtoll(text.c_str(), nullptr, 10);
                tok.m_fVal = static_cast<double>(tok.m_iVal);
            }
            tok.m_iConstIdx = static_cast<int>(m_ConstTexts.size());
            m_ConstTexts.push_back(text);
            m_TokenList.push_back(tok);
        }
    }

    m_TokenList.emplace_back(TokenType::TOKEN_EOF, "<eof>", m_iRow, m_iCol);
    return true;
}

const TokenList& CLex::GetTokenList() const
{
    return m_TokenList;
}
