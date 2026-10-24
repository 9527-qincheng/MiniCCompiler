#pragma once
// ============================================================
// SemanticAnalyzer —— 语义分析模块（第 5 组，陆禹辰/陕海波/于霄）
// 职责：遍历语法模块产出的 AST，驱动 SymbolTable 完成
//        1) 变量/数组/函数/形参声明登记 + 作用域内重复声明查重
//        2) 标识符引用检查（先声明后使用）
//        3) 赋值两侧类型相容检查（float->int 告警）
//        4) 表达式类型推导（设计文档 §8.1 类型系统表）
//        5) 常量登记（数字/字符/布尔统一在语义阶段登记）
// 接口依据：《MiniC 编译器设计文档》v4.0 §5.4 / §8.1 / §6 契约
// 放置位置：src/sema/semantic.h（与 ir.h 同目录）
// ============================================================

#include "parser/ast.h"
#include "symbol/symbol.h"

class SemanticAnalyzer
{
public:
    explicit SemanticAnalyzer(SymbolTable& symtab);

    // 语义分析入口。调用后符号表已填好，可直接 symtab.DumpSymTab()
    void Analyze(const ASTNodePtr& root);

private:
    SymbolTable& m_symtab;

    // 函数定义嵌套深度。文法上允许块内出现函数定义（设计文档 §15.2），
    // 但 C 语言不允许，且嵌套函数会与外层同名函数一起生成重复的汇编标号，
    // 因此语义阶段必须拦截「函数定义只能出现在最外层」。
    int m_iFuncDepth = 0;

    // 递归遍历单个节点；表达式节点返回推导类型，语句/声明节点返回 TOKEN_NONE
    TokenType Visit(const ASTNodePtr& node);

    // —— 声明 ——
    void VisitDecl(DeclNode* decl);
    void VisitArrayDecl(ArrayDeclNode* ad);
    void VisitFuncDef(FuncDefNode* fn);

    // —— 语句 ——
    void VisitAssign(AssignNode* assign);

    // —— 表达式 ——
    TokenType VisitIdent(IdentNode* ident);
    TokenType VisitIndex(IndexNode* ix);
    TokenType VisitBinOp(BinOpNode* op);
    TokenType VisitUnOp(UnOpNode* u);
    TokenType VisitCall(CallNode* call);
    void      VisitNum(NumNode* num);

    // —— 类型工具 ——
    bool      IsNumeric(TokenType t) const;
    TokenType Promote(TokenType a, TokenType b) const;
    TokenType NumType(const NumNode* n) const;
    TokenType DeduceBinOp(TokenType op, TokenType lt, TokenType rt,
                          const LineInfo& loc);
};