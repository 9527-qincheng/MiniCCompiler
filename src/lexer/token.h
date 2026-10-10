// ============================================================================
// MiniC 编译器 —— 公共基础结构（Token / TokenType / LineInfo / 错误记录）
// 依据：《MiniC编译器数据结构设计.md》第 4.1 / 4.2 节 + 第 6 章接口契约
// 说明：公共基础结构由词法模块提供（参考书 CToken/LineInfo 位于 Lex 头文件）
// ============================================================================
#ifndef MINIC_LEXER_TOKEN_H
#define MINIC_LEXER_TOKEN_H

#include <string>
#include <vector>
#include <ostream>

// ----------------------------------------------------------------------------
// 4.1.1 行列信息 LineInfo（参考书声明 2-5）
// ----------------------------------------------------------------------------
struct LineInfo
{
    int  m_iRow = 1;          // 行号（1-based）
    int  m_iCol = 1;          // 列号（PPT 扩展，1-based）

    LineInfo() = default;
    LineInfo(int row, int col) : m_iRow(row), m_iCol(col) {}
};

// ----------------------------------------------------------------------------
// 4.1.2 单词类型 TokenType（参考书 TokenType 枚举，MiniC 收窄 + 扩展）
// 注：为支持挑战版控制流（if/else/while），在文档 TokenType 基础上补充
//     TOKEN_LBRACE / TOKEN_RBRACE（文档第 4 章未单列花括号界符，详见交付说明）。
// ----------------------------------------------------------------------------
enum class TokenType
{
    TOKEN_NONE = 0,
    // 关键字
    TOKEN_INT,          // int
    TOKEN_FLOAT,        // float
    TOKEN_IF,           // if（挑战版）
    TOKEN_ELSE,         // else（挑战版）
    TOKEN_WHILE,        // while（挑战版）
    TOKEN_RETURN,       // return（挑战版）
    TOKEN_VOID,         // void（挑战版）
    // 标识符 / 常量
    TOKEN_IDENT,        // 标识符
    TOKEN_NUM_INT,      // 整型常量
    TOKEN_NUM_FLOAT,    // 浮点常量
    TOKEN_STRING,       // 字符串常量（"..."）
    // 运算符
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_MUL,          // *
    TOKEN_DIV,          // /
    TOKEN_ASSIGN,       // =
    // 界符
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_LBRACE,       // {（扩展：挑战版控制流）
    TOKEN_RBRACE,       // }（扩展：挑战版控制流）
    TOKEN_SEMI,         // ;
    TOKEN_COMMA,        // ,
    // 结束
    TOKEN_EOF
};

// TokenType 文本名（用于 --dump-tokens / 调试）
const char* TokenTypeToString(TokenType t);

// ----------------------------------------------------------------------------
// 4.1.3 单词 Token（参考书声明 2-4 CToken，扩展 PPT 列号与常量表位序）
// ----------------------------------------------------------------------------
struct Token
{
    TokenType   m_type = TokenType::TOKEN_NONE; // 单词类型（参考书 m_iKind）
    std::string m_szContent;                    // 单词原文（参考书 m_szContent）
    LineInfo    m_line;                         // 行列信息（参考书 m_LineInfo + PPT 列号）
    int         m_iConstIdx = -1;               // 常量表位序（Q12：词法登记 m_ConstTexts 索引）

    // 常量的语义值（词法阶段登记，供后续语义回填使用）
    bool        m_bIsFloat = false;             // 是否为浮点常量
    double      m_fVal = 0.0;                   // 浮点值
    long long   m_iVal = 0;                     // 整型值

    Token() = default;
    Token(TokenType type, const std::string& content, int row, int col)
        : m_type(type), m_szContent(content), m_line(row, col) {}
};

// 单词序列（参考书声明 2-6 TokenList）
using TokenList = std::vector<Token>;

// ----------------------------------------------------------------------------
// 4.1.4 编译错误记录 + 全局错误表 g_ErrorList（Q8：含行列号，尽量错误恢复）
// ----------------------------------------------------------------------------
struct CompileError
{
    std::string m_szMsg;    // 错误信息
    LineInfo    m_loc;      // 出错位置（行列号）
};

// 全局错误汇总表（main 汇总，非零退出码依据）
extern std::vector<CompileError> g_ErrorList;

// 错误记录辅助函数（参考书 EmitError 风格）：追加到 g_ErrorList 并打印
void EmitError(const std::string& msg, const LineInfo& loc);

#endif // MINIC_LEXER_TOKEN_H
