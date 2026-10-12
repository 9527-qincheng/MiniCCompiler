// ============================================================================
// MiniC 编译器 —— 公共基础结构（Token / TokenType / LineInfo / 错误记录）
// 依据：《MiniC编译器设计文档 v4.0 挑战版 LL1》§4.1 / §5.1 / §5.2
// ============================================================================
#ifndef MINIC_LEXER_TOKEN_H
#define MINIC_LEXER_TOKEN_H

#include <string>
#include <vector>
#include <ostream>

// ----------------------------------------------------------------------------
// 4.1.1 行列信息 LineInfo
// ----------------------------------------------------------------------------
struct LineInfo
{
    int  m_iRow = 1;
    int  m_iCol = 1;

    LineInfo() = default;
    LineInfo(int row, int col) : m_iRow(row), m_iCol(col) {}
};

// ----------------------------------------------------------------------------
// 4.1.2 TokenType 单词类型（v4.0 挑战版：39 个文法终结符 + NONE + STRING）
// 枚举顺序与编码无关；编码由 TokenTypeToCode() 映射（预测分析表以编码为下标）
// ----------------------------------------------------------------------------
enum class TokenType
{
    TOKEN_NONE = 0,
    /* 关键字（编码 001~012） */
    TOKEN_INT,          // 001 int
    TOKEN_FLOAT,        // 002 float
    TOKEN_BOOL,         // 003 bool
    TOKEN_CHAR,         // 004 char
    TOKEN_VOID,         // 005 void
    TOKEN_IF,           // 006 if
    TOKEN_ELSE,         // 007 else
    TOKEN_WHILE,        // 008 while
    TOKEN_PRINT,        // 009 print
    TOKEN_RETURN,       // 010 return
    TOKEN_TRUE,         // 011 true
    TOKEN_FALSE,        // 012 false
    /* 界符（编码 021~028） */
    TOKEN_LPAREN,       // 021 (
    TOKEN_RPAREN,       // 022 )
    TOKEN_LBRACE,       // 023 {
    TOKEN_RBRACE,       // 024 }
    TOKEN_LBRACKET,     // 025 [
    TOKEN_RBRACKET,     // 026 ]
    TOKEN_SEMI,         // 027 ;
    TOKEN_COMMA,        // 028 ,
    /* 运算符（编码 031~044） */
    TOKEN_PLUS,         // 031 +
    TOKEN_MINUS,        // 032 -
    TOKEN_MUL,          // 033 *
    TOKEN_DIV,          // 034 /
    TOKEN_ASSIGN,       // 035 =
    TOKEN_EQ,           // 036 ==
    TOKEN_NE,           // 037 !=
    TOKEN_LT,           // 038 <
    TOKEN_LE,           // 039 <=
    TOKEN_GT,           // 040 >
    TOKEN_GE,           // 041 >=
    TOKEN_AND,          // 042 &&
    TOKEN_OR,           // 043 ||
    TOKEN_NOT,          // 044 !
    /* 标识符 / 常量（编码 051~054） */
    TOKEN_IDENT,        // 051 标识符
    TOKEN_NUM_INT,      // 052 整型常量
    TOKEN_NUM_FLOAT,    // 053 实型常量
    TOKEN_CHAR_CONST,   // 054 字符常量
    TOKEN_STRING,       // 字符串常量（词法扩展，不入文法，无编码）
    /* 结束（编码 099） */
    TOKEN_EOF
};

// TokenType 文本名（--dump-tokens / 调试）
const char* TokenTypeToString(TokenType t);

// ----------------------------------------------------------------------------
// T3：编码双向映射（预测分析表以数字编码为下标）
//   TokenTypeToCode：枚举 -> 编码；TOKEN_NONE/TOKEN_STRING 返回 0
//   CodeToTokenType：编码 -> 枚举；未知编码返回 TOKEN_NONE
// ----------------------------------------------------------------------------
int       TokenTypeToCode(TokenType t);
TokenType CodeToTokenType(int code);

// ----------------------------------------------------------------------------
// 4.1.3 单词 Token
// ----------------------------------------------------------------------------
struct Token
{
    TokenType   m_type = TokenType::TOKEN_NONE;
    std::string m_szContent;
    LineInfo    m_line;
    int         m_iConstIdx = -1;

    bool        m_bIsFloat = false;
    double      m_fVal = 0.0;
    long long   m_iVal = 0;
    char        m_cVal = 0;              // 【v4.0 新增】字符常量值（'a' -> 'a'）

    Token() = default;
    Token(TokenType type, const std::string& content, int row, int col)
        : m_type(type), m_szContent(content), m_line(row, col) {}
};

using TokenList = std::vector<Token>;

// ----------------------------------------------------------------------------
// 4.1.4 编译错误记录 + 全局错误表
// ----------------------------------------------------------------------------
struct CompileError
{
    std::string m_szMsg;
    LineInfo    m_loc;
};

extern std::vector<CompileError> g_ErrorList;
void EmitError(const std::string& msg, const LineInfo& loc);

#endif // MINIC_LEXER_TOKEN_H
