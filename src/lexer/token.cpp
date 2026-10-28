// ============================================================================
// MiniC 编译器 —— TokenTypeToString / 编码映射 / 错误表
// ============================================================================
#include "lexer/token.h"

#include <iostream>
#include <unordered_map>

std::vector<CompileError> g_ErrorList;

// 见 token.h 的说明：语义阶段在语法已出错之后产生的诊断，打印时加来源提示。
bool g_bErrorsMayBeSecondary = false;

// 错误条数上限（0 = 不限制）。100 条远超仓库里所有基线的最大错误数（4 条），
// 只用于拦住「同源错误刷屏」的场景。
int g_iMaxErrors = 100;
int g_iSuppressedErrors = 0;

// EmitError 的累计调用次数（含被抑制的），与 g_ErrorList.size() 相同，
// 单独留一份是为了在改上限时不依赖全局表是否被清空
static int g_iTotalErrors = 0;

const char* TokenTypeToString(TokenType t)
{
    switch (t)
    {
    case TokenType::TOKEN_NONE:        return "NONE";
    /* 关键字 */
    case TokenType::TOKEN_INT:        return "INT";
    case TokenType::TOKEN_FLOAT:       return "FLOAT";
    case TokenType::TOKEN_BOOL:       return "BOOL";
    case TokenType::TOKEN_CHAR:       return "CHAR";
    case TokenType::TOKEN_VOID:       return "VOID";
    case TokenType::TOKEN_IF:         return "IF";
    case TokenType::TOKEN_ELSE:       return "ELSE";
    case TokenType::TOKEN_WHILE:      return "WHILE";
    case TokenType::TOKEN_PRINT:      return "PRINT";
    case TokenType::TOKEN_RETURN:     return "RETURN";
    case TokenType::TOKEN_TRUE:        return "TRUE";
    case TokenType::TOKEN_FALSE:      return "FALSE";
    /* 界符 */
    case TokenType::TOKEN_LPAREN:     return "LPAREN";
    case TokenType::TOKEN_RPAREN:     return "RPAREN";
    case TokenType::TOKEN_LBRACE:     return "LBRACE";
    case TokenType::TOKEN_RBRACE:     return "RBRACE";
    case TokenType::TOKEN_LBRACKET:   return "LBRACKET";
    case TokenType::TOKEN_RBRACKET:   return "RBRACKET";
    case TokenType::TOKEN_SEMI:       return "SEMI";
    case TokenType::TOKEN_COMMA:      return "COMMA";
    /* 运算符 */
    case TokenType::TOKEN_PLUS:        return "PLUS";
    case TokenType::TOKEN_MINUS:       return "MINUS";
    case TokenType::TOKEN_MUL:        return "MUL";
    case TokenType::TOKEN_DIV:        return "DIV";
    case TokenType::TOKEN_ASSIGN:     return "ASSIGN";
    case TokenType::TOKEN_EQ:         return "EQ";
    case TokenType::TOKEN_NE:         return "NE";
    case TokenType::TOKEN_LT:         return "LT";
    case TokenType::TOKEN_LE:         return "LE";
    case TokenType::TOKEN_GT:         return "GT";
    case TokenType::TOKEN_GE:         return "GE";
    case TokenType::TOKEN_AND:        return "AND";
    case TokenType::TOKEN_OR:         return "OR";
    case TokenType::TOKEN_NOT:        return "NOT";
    case TokenType::TOKEN_INC:        return "INC";
    case TokenType::TOKEN_DEC:        return "DEC";
    /* 常量 */
    case TokenType::TOKEN_IDENT:       return "IDENT";
    case TokenType::TOKEN_NUM_INT:     return "NUM_INT";
    case TokenType::TOKEN_NUM_FLOAT:   return "NUM_FLOAT";
    case TokenType::TOKEN_CHAR_CONST: return "CHAR_CONST";
    case TokenType::TOKEN_STRING:      return "STRING";
    /* 结束 */
    case TokenType::TOKEN_EOF:         return "EOF";
    }
    return "UNKNOWN";
}

// ----------------------------------------------------------------------------
// T3：编码双向映射
//   编码区间：关键字 001~012、界符 021~028、运算符 031~046、
//             标识符/常量 051~054、结束 099
// ----------------------------------------------------------------------------
int TokenTypeToCode(TokenType t)
{
    switch (t)
    {
    case TokenType::TOKEN_INT:      return 1;
    case TokenType::TOKEN_FLOAT:    return 2;
    case TokenType::TOKEN_BOOL:     return 3;
    case TokenType::TOKEN_CHAR:     return 4;
    case TokenType::TOKEN_VOID:     return 5;
    case TokenType::TOKEN_IF:       return 6;
    case TokenType::TOKEN_ELSE:     return 7;
    case TokenType::TOKEN_WHILE:    return 8;
    case TokenType::TOKEN_PRINT:    return 9;
    case TokenType::TOKEN_RETURN:   return 10;
    case TokenType::TOKEN_TRUE:     return 11;
    case TokenType::TOKEN_FALSE:    return 12;

    case TokenType::TOKEN_LPAREN:   return 21;
    case TokenType::TOKEN_RPAREN:   return 22;
    case TokenType::TOKEN_LBRACE:  return 23;
    case TokenType::TOKEN_RBRACE:  return 24;
    case TokenType::TOKEN_LBRACKET: return 25;
    case TokenType::TOKEN_RBRACKET:return 26;
    case TokenType::TOKEN_SEMI:     return 27;
    case TokenType::TOKEN_COMMA:    return 28;

    case TokenType::TOKEN_PLUS:     return 31;
    case TokenType::TOKEN_MINUS:    return 32;
    case TokenType::TOKEN_MUL:      return 33;
    case TokenType::TOKEN_DIV:      return 34;
    case TokenType::TOKEN_ASSIGN:   return 35;
    case TokenType::TOKEN_EQ:       return 36;
    case TokenType::TOKEN_NE:       return 37;
    case TokenType::TOKEN_LT:       return 38;
    case TokenType::TOKEN_LE:       return 39;
    case TokenType::TOKEN_GT:       return 40;
    case TokenType::TOKEN_GE:       return 41;
    case TokenType::TOKEN_AND:      return 42;
    case TokenType::TOKEN_OR:       return 43;
    case TokenType::TOKEN_NOT:      return 44;
    case TokenType::TOKEN_INC:      return 45;
    case TokenType::TOKEN_DEC:      return 46;

    case TokenType::TOKEN_IDENT:       return 51;
    case TokenType::TOKEN_NUM_INT:     return 52;
    case TokenType::TOKEN_NUM_FLOAT:   return 53;
    case TokenType::TOKEN_CHAR_CONST:  return 54;

    case TokenType::TOKEN_EOF:       return 99;

    // TOKEN_NONE / TOKEN_STRING 不参与编码
    case TokenType::TOKEN_NONE:
    case TokenType::TOKEN_STRING:
    default:
        return 0;
    }
}

TokenType CodeToTokenType(int code)
{
    switch (code)
    {
    case 1:  return TokenType::TOKEN_INT;
    case 2:  return TokenType::TOKEN_FLOAT;
    case 3:  return TokenType::TOKEN_BOOL;
    case 4:  return TokenType::TOKEN_CHAR;
    case 5:  return TokenType::TOKEN_VOID;
    case 6:  return TokenType::TOKEN_IF;
    case 7:  return TokenType::TOKEN_ELSE;
    case 8:  return TokenType::TOKEN_WHILE;
    case 9:  return TokenType::TOKEN_PRINT;
    case 10: return TokenType::TOKEN_RETURN;
    case 11: return TokenType::TOKEN_TRUE;
    case 12: return TokenType::TOKEN_FALSE;

    case 21: return TokenType::TOKEN_LPAREN;
    case 22: return TokenType::TOKEN_RPAREN;
    case 23: return TokenType::TOKEN_LBRACE;
    case 24: return TokenType::TOKEN_RBRACE;
    case 25: return TokenType::TOKEN_LBRACKET;
    case 26: return TokenType::TOKEN_RBRACKET;
    case 27: return TokenType::TOKEN_SEMI;
    case 28: return TokenType::TOKEN_COMMA;

    case 31: return TokenType::TOKEN_PLUS;
    case 32: return TokenType::TOKEN_MINUS;
    case 33: return TokenType::TOKEN_MUL;
    case 34: return TokenType::TOKEN_DIV;
    case 35: return TokenType::TOKEN_ASSIGN;
    case 36: return TokenType::TOKEN_EQ;
    case 37: return TokenType::TOKEN_NE;
    case 38: return TokenType::TOKEN_LT;
    case 39: return TokenType::TOKEN_LE;
    case 40: return TokenType::TOKEN_GT;
    case 41: return TokenType::TOKEN_GE;
    case 42: return TokenType::TOKEN_AND;
    case 43: return TokenType::TOKEN_OR;
    case 44: return TokenType::TOKEN_NOT;
    case 45: return TokenType::TOKEN_INC;
    case 46: return TokenType::TOKEN_DEC;

    case 51: return TokenType::TOKEN_IDENT;
    case 52: return TokenType::TOKEN_NUM_INT;
    case 53: return TokenType::TOKEN_NUM_FLOAT;
    case 54: return TokenType::TOKEN_CHAR_CONST;

    case 99: return TokenType::TOKEN_EOF;

    default: return TokenType::TOKEN_NONE;
    }
}

const std::string* g_pSourceText = nullptr;

namespace {

// 取源文件的第 row 行（1-based）；取不到返回 false。
bool SourceLineOf(int row, std::string& out)
{
    if (g_pSourceText == nullptr || row <= 0)
    {
        return false;
    }
    const std::string& s = *g_pSourceText;
    std::size_t pos = 0;
    for (int r = 1; r < row; ++r)
    {
        pos = s.find('\n', pos);
        if (pos == std::string::npos)
        {
            return false;
        }
        ++pos;
    }
    std::size_t end = s.find('\n', pos);
    if (end == std::string::npos)
    {
        end = s.size();
    }
    out = s.substr(pos, end - pos);
    if (!out.empty() && out.back() == '\r')
    {
        out.pop_back();
    }
    return true;
}

// 把制表符展开成 4 个空格，保证「源码行」与「^ 定位行」的列能对齐。
std::string ExpandTabs(const std::string& s)
{
    std::string r;
    r.reserve(s.size() + 8);
    for (char c : s)
    {
        if (c == '\t')
        {
            r.append(4, ' ');
        }
        else
        {
            r.push_back(c);
        }
    }
    return r;
}

// 打印「行号 | 源码」+「   |    ^」。格式刻意和常见编译器（gcc/clang）一致，
// 便于肉眼看；测试脚本按 `minic: error:` 前缀过滤，这些行不会进基线。
void PrintDiagContext(const LineInfo& loc)
{
    std::string raw;
    if (!SourceLineOf(loc.m_iRow, raw))
    {
        return;
    }
    const std::string line = ExpandTabs(raw);
    if (line.find_first_not_of(" \t") == std::string::npos)
    {
        return;   // 空行不打印，否则 ^ 指在空白处毫无意义
    }
    const std::string num = std::to_string(loc.m_iRow);
    const std::string pad(num.size(), ' ');
    std::cerr << "  " << num << " | " << line << "\n";
    std::cerr << "  " << pad << " | ";
    int col = (loc.m_iCol > 0) ? loc.m_iCol : 1;
    for (int i = 1; i < col && static_cast<std::size_t>(i - 1) < line.size(); ++i)
    {
        std::cerr << ' ';
    }
    std::cerr << '^' << std::endl;
}

}  // namespace

void EmitError(const std::string& msg, const LineInfo& loc)
{
    CompileError err;
    err.m_szMsg = msg;
    err.m_loc = loc;

    // 超过上限后仍然记录（退出码与「有错」判断都依赖条数），
    // 但不再逐条打到 stderr —— 避免一份源文件刷出几百行同源报错。
    ++g_iTotalErrors;
    if (g_iMaxErrors > 0 && g_iTotalErrors > g_iMaxErrors)
    {
        ++g_iSuppressedErrors;
        g_ErrorList.push_back(err);
        return;
    }

    g_ErrorList.push_back(err);
    std::cerr << "minic: error: " << loc.m_iRow << ":" << loc.m_iCol
              << ": " << msg;
    if (g_bErrorsMayBeSecondary)
    {
        std::cerr << " (possibly caused by earlier lexical/syntax errors)";
    }
    std::cerr << std::endl;
    PrintDiagContext(loc);
}

void EmitWarning(const std::string& msg, const LineInfo& loc)
{
    // 只打到 stderr，不进 g_ErrorList —— 退出码只看 g_ErrorList，
    // 所以警告不会把一次成功编译变成失败（run_sema_tests.ps1 的
    // `warn:N` 用例正是靠这个前提要求 exit=0）。
    std::cerr << "minic: warning: " << loc.m_iRow << ":" << loc.m_iCol
              << ": " << msg << std::endl;
    PrintDiagContext(loc);
}
