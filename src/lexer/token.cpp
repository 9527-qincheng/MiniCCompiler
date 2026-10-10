// ============================================================================
// MiniC 编译器 —— 公共基础结构实现（TokenTypeToString / g_ErrorList / EmitError）
// ============================================================================
#include "lexer/token.h"

#include <iostream>

std::vector<CompileError> g_ErrorList;

const char* TokenTypeToString(TokenType t)
{
    switch (t)
    {
    case TokenType::TOKEN_NONE:      return "NONE";
    case TokenType::TOKEN_INT:       return "INT";
    case TokenType::TOKEN_FLOAT:     return "FLOAT";
    case TokenType::TOKEN_IF:        return "IF";
    case TokenType::TOKEN_ELSE:      return "ELSE";
    case TokenType::TOKEN_WHILE:     return "WHILE";
    case TokenType::TOKEN_RETURN:    return "RETURN";
    case TokenType::TOKEN_VOID:      return "VOID";
    case TokenType::TOKEN_IDENT:     return "IDENT";
    case TokenType::TOKEN_NUM_INT:   return "NUM_INT";
    case TokenType::TOKEN_NUM_FLOAT: return "NUM_FLOAT";
    case TokenType::TOKEN_STRING:    return "STRING";
    case TokenType::TOKEN_PLUS:      return "PLUS";
    case TokenType::TOKEN_MINUS:     return "MINUS";
    case TokenType::TOKEN_MUL:       return "MUL";
    case TokenType::TOKEN_DIV:       return "DIV";
    case TokenType::TOKEN_ASSIGN:    return "ASSIGN";
    case TokenType::TOKEN_LPAREN:    return "LPAREN";
    case TokenType::TOKEN_RPAREN:    return "RPAREN";
    case TokenType::TOKEN_LBRACE:    return "LBRACE";
    case TokenType::TOKEN_RBRACE:    return "RBRACE";
    case TokenType::TOKEN_SEMI:      return "SEMI";
    case TokenType::TOKEN_COMMA:     return "COMMA";
    case TokenType::TOKEN_EOF:       return "EOF";
    }
    return "UNKNOWN";
}

void EmitError(const std::string& msg, const LineInfo& loc)
{
    CompileError err;
    err.m_szMsg = msg;
    err.m_loc = loc;
    g_ErrorList.push_back(err);
    std::cerr << "minic: error: " << loc.m_iRow << ":" << loc.m_iCol
              << ": " << msg << std::endl;
}
