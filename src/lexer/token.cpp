// ============================================================================
// MiniC 编译器 —— TokenTypeToString / 编码映射 / 错误表
// ============================================================================
#include "lexer/token.h"

#include <iostream>
#include <unordered_map>

std::vector<CompileError> g_ErrorList;

// 见 token.h 的说明：语义阶段在语法已出错之后产生的诊断，打印时加来源提示。
bool g_bErrorsMayBeSecondary = false;

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

void EmitError(const std::string& msg, const LineInfo& loc)
{
    CompileError err;
    err.m_szMsg = msg;
    err.m_loc = loc;
    g_ErrorList.push_back(err);
    std::cerr << "minic: error: " << loc.m_iRow << ":" << loc.m_iCol
              << ": " << msg;
    if (g_bErrorsMayBeSecondary)
    {
        std::cerr << " (possibly caused by earlier lexical/syntax errors)";
    }
    std::cerr << std::endl;
}
