// ============================================================================
// MiniC 编译器 —— 词法分析模块 CLex 实现（完整 DFA 状态转换表驱动）
// ============================================================================
#include "lexer/lexer.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include "common/pathutil.h"
#include <sstream>

// DFA 状态常量
namespace LexState {
    constexpr int START       = 0;
    constexpr int IDENT       = 1;
    constexpr int NUMBER      = 2;
    constexpr int NUMBER_DOT  = 3;
    constexpr int STRING      = 4;
    constexpr int STRING_ESC  = 5;
    constexpr int STRING_DONE = 6;
    constexpr int CHAR        = 7;
    constexpr int CHAR_ESC    = 8;
    constexpr int CHAR_DONE   = 9;
    constexpr int OP          = 10;
    constexpr int OP2         = 11;
    constexpr int ACCEPT      = 90;
    constexpr int ERROR       = 99;
}

static bool IsDoubleOp(const std::string& t)
{
    return t == "==" || t == "!=" || t == "<=" || t == ">=" ||
           t == "&&" || t == "||" || t == "++" || t == "--";
}

int CLex::m_szLexTbl[13][256] = {{0}};
bool CLex::m_bAccept[13] = {false};

static void SetAll(int tbl[][256], int state, int val)
{
    for (int c = 0; c < 256; ++c) tbl[state][c] = val;
}

static void InitLexTable(int tbl[13][256])
{
    using namespace LexState;
    // 先全部填成错误态
    for (int s = 0; s < 13; ++s)
        for (int c = 0; c < 256; ++c)
            tbl[s][c] = ERROR;

    // --- S_START (0) ---
    tbl[START][' '] = START; tbl[START]['\t'] = START;
    tbl[START]['\r'] = START; tbl[START]['\n'] = START;
    for (int c = 'A'; c <= 'Z'; ++c) tbl[START][c] = IDENT;
    for (int c = 'a'; c <= 'z'; ++c) tbl[START][c] = IDENT;
    tbl[START]['_'] = IDENT;
    for (int c = '0'; c <= '9'; ++c) tbl[START][c] = NUMBER;
    tbl[START]['"'] = STRING;
    tbl[START]['\''] = CHAR;
    const char* ops = "+-*/=<>!&|";
    const char* delims = "(){};,[]";
    for (const char* p = ops; *p; ++p) tbl[START][(unsigned char)*p] = OP;
    for (const char* p = delims; *p; ++p) tbl[START][(unsigned char)*p] = OP;

    // --- S_IDENT (1) ---
    SetAll(tbl, IDENT, ACCEPT);
    for (int c = 'A'; c <= 'Z'; ++c) tbl[IDENT][c] = IDENT;
    for (int c = 'a'; c <= 'z'; ++c) tbl[IDENT][c] = IDENT;
    for (int c = '0'; c <= '9'; ++c) tbl[IDENT][c] = IDENT;
    tbl[IDENT]['_'] = IDENT;

    // --- S_NUMBER (2) / S_NUMBER_DOT (3) ---
    SetAll(tbl, NUMBER, ACCEPT);
    SetAll(tbl, NUMBER_DOT, ACCEPT);
    for (int c = '0'; c <= '9'; ++c) tbl[NUMBER][c] = NUMBER;
    tbl[NUMBER]['.'] = NUMBER_DOT;
    for (int c = '0'; c <= '9'; ++c) tbl[NUMBER_DOT][c] = NUMBER_DOT;

    // --- S_STRING (4) / S_STRING_ESC (5) / S_STRING_DONE (6) ---
    SetAll(tbl, STRING, STRING);
    tbl[STRING]['"'] = STRING_DONE;
    tbl[STRING]['\\'] = STRING_ESC;
    SetAll(tbl, STRING_ESC, STRING);

    // --- S_CHAR (7) / S_CHAR_ESC (8) / S_CHAR_DONE (9) ---
    SetAll(tbl, CHAR, CHAR);
    tbl[CHAR]['\''] = CHAR_DONE;
    tbl[CHAR]['\\'] = CHAR_ESC;
    SetAll(tbl, CHAR_ESC, CHAR);

    // --- S_OP (10) / S_OP2 (11) ---
    SetAll(tbl, OP, ACCEPT);
    tbl[OP]['+'] = OP2; tbl[OP]['-'] = OP2;
    tbl[OP]['='] = OP2; tbl[OP]['<'] = OP2;
    tbl[OP]['>'] = OP2; tbl[OP]['!'] = OP2;
    tbl[OP]['&'] = OP2; tbl[OP]['|'] = OP2;
    SetAll(tbl, OP2, ACCEPT);
}

CLex::CLex()
{
    InitKeywordTable();
    InitLexTable(m_szLexTbl);
    using namespace LexState;
    m_bAccept[IDENT] = true;
    m_bAccept[NUMBER] = true;
    m_bAccept[NUMBER_DOT] = true;
    m_bAccept[STRING_DONE] = true;
    m_bAccept[CHAR_DONE] = true;
    m_bAccept[OP] = true;
    m_bAccept[OP2] = true;
}

void CLex::InitKeywordTable()
{
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
    if (!minic_path::ReadFileUtf8(szFileName, m_szSource))
    {
        EmitError("cannot open source file: " + szFileName, LineInfo(0, 0));
        return false;
    }
    if (m_szSource.size() >= 3 &&
        static_cast<unsigned char>(m_szSource[0]) == 0xEF &&
        static_cast<unsigned char>(m_szSource[1]) == 0xBB &&
        static_cast<unsigned char>(m_szSource[2]) == 0xBF)
    {
        m_szSource.erase(0, 3);
    }
    m_iPos = 0; m_iRow = 1; m_iCol = 1;
    m_TokenList.clear(); m_ConstTexts.clear();
    return true;
}

void CLex::SkipSpace()
{
    while (m_iPos < m_szSource.size())
    {
        char ch = m_szSource[m_iPos];
        if (ch == ' ' || ch == '\t' || ch == '\r') { ++m_iPos; ++m_iCol; }
        else if (ch == '\n') { ++m_iPos; ++m_iRow; m_iCol = 1; }
        else break;
    }
}

void CLex::SkipComment()
{
    if (m_iPos + 1 >= m_szSource.size()) return;
    char next = m_szSource[m_iPos + 1];
    if (next == '/') {
        while (m_iPos < m_szSource.size() && m_szSource[m_iPos] != '\n') { ++m_iPos; ++m_iCol; }
    } else if (next == '*') {
        int startRow = m_iRow, startCol = m_iCol;
        m_iPos += 2; m_iCol += 2;
        bool closed = false;
        while (m_iPos < m_szSource.size()) {
            if (m_szSource[m_iPos] == '*' && m_iPos + 1 < m_szSource.size() &&
                m_szSource[m_iPos + 1] == '/') {
                m_iPos += 2; m_iCol += 2; closed = true; break;
            }
            if (m_szSource[m_iPos] == '\n') { ++m_iPos; ++m_iRow; m_iCol = 1; }
            else { ++m_iPos; ++m_iCol; }
        }
        if (!closed) EmitError("unterminated /* comment (started here)", LineInfo(startRow, startCol));
    }
}

void CLex::ReduceToken(int state, size_t start, size_t end, int row, int col)
{
    using namespace LexState;
    std::string text = m_szSource.substr(start, end - start);

    switch (state) {
    case IDENT: {
        int kw = LookupKeyword(text);
        TokenType type = (kw >= 0) ? static_cast<TokenType>(kw) : TokenType::TOKEN_IDENT;
        m_TokenList.emplace_back(type, text, row, col);
        break;
    }
    case NUMBER:
    case NUMBER_DOT: {
        bool isFloat = (state == NUMBER_DOT);
        TokenType type = isFloat ? TokenType::TOKEN_NUM_FLOAT : TokenType::TOKEN_NUM_INT;
        Token tok(type, text, row, col);
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
        break;
    }
    case STRING_DONE: {
        std::string inner = text.substr(1, text.size() - 2);
        Token tok(TokenType::TOKEN_STRING, text, row, col);
        tok.m_iConstIdx = static_cast<int>(m_ConstTexts.size());
        m_ConstTexts.push_back(inner);
        m_TokenList.push_back(tok);
        break;
    }
    case CHAR_DONE: {
        std::string inner = text.substr(1, text.size() - 2);
        bool valid = false;
        char c = 0;
        if (inner.size() == 1) {
            c = inner[0];
            valid = true;
        } else if (inner.size() == 2 && inner[0] == '\\') {
            char e = inner[1];
            switch (e) {
            case 'n': case 't': case 'r': case '0':
            case '\\': case '\'': case '"':
                c = (e == 'n') ? '\n' : (e == 't') ? '\t' :
                    (e == 'r') ? '\r' : (e == '0') ? '\0' : e;
                valid = true;
                break;
            }
        }
        if (!valid) {
            EmitError("invalid char constant: " + text, LineInfo(row, col));
            break;
        }
        Token tok(TokenType::TOKEN_CHAR_CONST, text, row, col);
        tok.m_cVal = c;
        tok.m_iVal = static_cast<long long>(static_cast<unsigned char>(c));
        m_TokenList.push_back(tok);
        break;
    }
    case OP:
    case OP2: {
        TokenType type = TokenType::TOKEN_NONE;
        if (state == OP2 && text.size() == 2) {
            char a = text[0], b = text[1];
            if      (a=='=' && b=='=') type = TokenType::TOKEN_EQ;
            else if (a=='!' && b=='=') type = TokenType::TOKEN_NE;
            else if (a=='<' && b=='=') type = TokenType::TOKEN_LE;
            else if (a=='>' && b=='=') type = TokenType::TOKEN_GE;
            else if (a=='&' && b=='&') type = TokenType::TOKEN_AND;
            else if (a=='|' && b=='|') type = TokenType::TOKEN_OR;
            else if (a=='+' && b=='+') type = TokenType::TOKEN_INC;
            else if (a=='-' && b=='-') type = TokenType::TOKEN_DEC;
        }
        if (type == TokenType::TOKEN_NONE && text.size() == 1) {
            char ch = text[0];
            switch (ch) {
            case '+': type = TokenType::TOKEN_PLUS; break;
            case '-': type = TokenType::TOKEN_MINUS; break;
            case '*': type = TokenType::TOKEN_MUL; break;
            case '/': type = TokenType::TOKEN_DIV; break;
            case '=': type = TokenType::TOKEN_ASSIGN; break;
            case '<': type = TokenType::TOKEN_LT; break;
            case '>': type = TokenType::TOKEN_GT; break;
            case '!': type = TokenType::TOKEN_NOT; break;
            case '(': type = TokenType::TOKEN_LPAREN; break;
            case ')': type = TokenType::TOKEN_RPAREN; break;
            case '{': type = TokenType::TOKEN_LBRACE; break;
            case '}': type = TokenType::TOKEN_RBRACE; break;
            case '[': type = TokenType::TOKEN_LBRACKET; break;
            case ']': type = TokenType::TOKEN_RBRACKET; break;
            case ';': type = TokenType::TOKEN_SEMI; break;
            case ',': type = TokenType::TOKEN_COMMA; break;
            }
        }
        if (type != TokenType::TOKEN_NONE)
            m_TokenList.emplace_back(type, text, row, col);
        break;
    }
    }
}

bool CLex::Run()
{
    using namespace LexState;
    m_TokenList.clear();
    m_ConstTexts.clear();

    while (true)
    {
        SkipSpace();
        if (m_iPos >= m_szSource.size()) break;

        // 注释单独处理
        if (m_szSource[m_iPos] == '/' && m_iPos + 1 < m_szSource.size() &&
            (m_szSource[m_iPos+1] == '/' || m_szSource[m_iPos+1] == '*'))
        { SkipComment(); continue; }

        size_t startPos = m_iPos;
        int startRow = m_iRow, startCol = m_iCol;
        int state = START;
        size_t lastAccept = std::string::npos;
        int acceptState = ERROR;
        int lastRow = 0, lastCol = 0;

        while (m_iPos < m_szSource.size())
        {
            unsigned char c = static_cast<unsigned char>(m_szSource[m_iPos]);

            // 字符串里遇到换行：报错但结束当前token，下一行继续扫描
            if (c == '\n' && (state == STRING || state == STRING_ESC)) {
                EmitError("newline in string literal", LineInfo(m_iRow, m_iCol));
                break;
            }
            // 字符常量里遇到换行：交给后面的 unterminated 统一报错，不重复报
            if (c == '\n' && (state == CHAR || state == CHAR_ESC)) {
                break;
            }

            int next = m_szLexTbl[state][c];
            if (next == ERROR) break;
            if (next == ACCEPT) break;

            state = next;
            if (c == '\n') { ++m_iRow; m_iCol = 1; } else { ++m_iCol; }
            ++m_iPos;

            if (m_bAccept[state]) {
                lastAccept = m_iPos; acceptState = state;
                lastRow = m_iRow; lastCol = m_iCol;
            }
            if (state == STRING_DONE || state == CHAR_DONE) break;
        }

        // 数字里出现第二个小数点（1.2.3）：报错并跳过多余的 . 和数字
        size_t skipTo = 0;
        if (state == NUMBER_DOT && m_iPos < m_szSource.size() && m_szSource[m_iPos] == '.') {
            EmitError("malformed number: extra decimal point", LineInfo(m_iRow, m_iCol));
            size_t p = m_iPos + 1;
            while (p < m_szSource.size() &&
                   std::isdigit(static_cast<unsigned char>(m_szSource[p]))) {
                ++p;
            }
            skipTo = p;
        }

        if (lastAccept == std::string::npos) {
            // 未闭合字符串：报专属错误，仍产出 STRING token 给 parser
            if (state == STRING || state == STRING_ESC) {
                EmitError("unterminated string literal", LineInfo(startRow, startCol));
                std::string text = m_szSource.substr(startPos, m_iPos - startPos);
                Token tok(TokenType::TOKEN_STRING, text, startRow, startCol);
                tok.m_iConstIdx = static_cast<int>(m_ConstTexts.size());
                m_ConstTexts.push_back(text.size() >= 1 ? text.substr(1) : text);
                m_TokenList.push_back(tok);
                // 停在换行处，外层 SkipSpace 吃掉换行，下一行继续
                continue;
            }
            if (state == CHAR || state == CHAR_ESC) {
                EmitError("unterminated char literal", LineInfo(startRow, startCol));
                continue;
            }

            EmitError(std::string("unexpected character '") + m_szSource[startPos] + "'",
                      LineInfo(startRow, startCol));
            char badch = m_szSource[m_iPos];
            if (badch == '\n') { ++m_iRow; m_iCol = 1; } else { ++m_iCol; }
            ++m_iPos;
            continue;
        }

        // OP2 不是真双字符运算符就回退一个字符，按单字符 OP 归约
        if (acceptState == LexState::OP2 && !IsDoubleOp(m_szSource.substr(startPos, lastAccept - startPos))) {
            lastAccept--;
            acceptState = LexState::OP;
            lastCol--;
        }

        m_iPos = lastAccept; m_iRow = lastRow; m_iCol = lastCol;
        if (skipTo > m_iPos) {
            m_iCol += static_cast<int>(skipTo - m_iPos);
            m_iPos = skipTo;
        }
        ReduceToken(acceptState, startPos, lastAccept, startRow, startCol);
    }

    m_TokenList.emplace_back(TokenType::TOKEN_EOF, "<eof>", m_iRow, m_iCol);
    return true;
}

const TokenList& CLex::GetTokenList() const
{
    return m_TokenList;
}
