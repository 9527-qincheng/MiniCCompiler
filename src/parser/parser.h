// ============================================================================
// MiniC 编译器 —— 语法分析模块 CParser（第 4.3 节 + 第 6.3 节接口契约）
// Q4 定稿：递归下降（保留 LL(1) 思路，不另做预测表驱动）
// ============================================================================
#ifndef MINIC_PARSER_PARSER_H
#define MINIC_PARSER_PARSER_H

#include "lexer/token.h"
#include "parser/ast.h"

#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// 4.3.4 语法分析器 CParser（第 6 章接口契约：构造绑定 TokenList，Parse 返回 AST）
// ----------------------------------------------------------------------------
class CParser
{
public:
    explicit CParser(const TokenList& tokens);

    // --- 第 6 章接口契约 ---
    ASTNodePtr Parse();                       // 递归下降解析，构建 AST
    bool HasError() const;                    // 是否出现语法错误
    const std::vector<CompileError>& GetErrors() const; // 语法错误列表

private:
    // 语法错误列表（同时汇总至全局 g_ErrorList）
    std::vector<CompileError> m_errors;

    // 内部词法流状态
    const TokenList* m_pTokenList;   // 绑定 Token 序列
    size_t           m_iListPos;     // 当前读取位置
    Token            m_CurToken;     // 当前单词（前瞻）

    // 辅助：向前读取一个 Token
    void NextToken();

    // --- 私有辅助（v3.1：parser.h 无 Check/Expect 终结符匹配辅助，按需自加；
    //     第 6.3 节 4 个公开接口签名保持不变） ---
    bool Check(TokenType t) const;      // 当前单词类型是否为 t
    bool IsDeclStart() const;           // 当前单词是否可开始一条声明（int/float）
    bool Expect(TokenType t, const std::string& what);  // 匹配则消费并返回 true，否则报错
    void ReportError(const std::string& msg);                       // 用当前单词位置报错
    void ReportError(const std::string& msg, const LineInfo& loc);  // 用指定位置报错
    void Synchronize();                 // 语法错误恢复：跳到下一个语句/声明边界

    // 递归下降分析函数（基础版 + 控制流扩展；挑战版其余项按 Q5 顺序扩展）
    ASTNodePtr ParseProgram();      // 程序 = 声明序列 + 语句序列
    ASTNodePtr ParseDeclaration();  // 声明 int/float
    ASTNodePtr ParseStmtList(TokenType terminator); // 语句序列（解析到 terminator 之前，不消费）
    ASTNodePtr ParseStatement();    // 语句（赋值 / if / while / return / 复合语句）
    ASTNodePtr ParseBlock();        // 复合语句 { StmtList }
    ASTNodePtr ParseIfStatement();  // if ( Expr ) Stmt [ else Stmt ]
    ASTNodePtr ParseWhileStatement();  // while ( Expr ) Stmt
    ASTNodePtr ParseReturnStatement(); // return [ Expr ] ;
    ASTNodePtr ParseExpression();   // 算术表达式（+ -）
    ASTNodePtr ParseTerm();         // 项（* /）
    ASTNodePtr ParseFactor();       // 因子（一元 +/-、数字、标识符、括号）
};

#endif // MINIC_PARSER_PARSER_H
