#pragma once
// ============================================================
// SemanticAnalyzer —— 语义分析模块（第 5 组，陆禹辰/陕海波/于霄）
// 职责：遍历语法模块产出的 AST，驱动 SymbolTable 完成
//        1) 变量声明登记 + 作用域内重复声明查重
//        2) 标识符引用检查（先声明后使用）
//        3) 赋值两侧类型相容检查（float->int 告警）
//        4) 二元算术表达式类型推导（int/float 自动提升）
//        5) 常量语义回填（常量表回填 + 字符/布尔常量自登记）
// 接口依据：《MiniC编译器数据结构设计》v4.0 / 阶段⑤ §6.4 节
// 放置位置：src/sema/semantic.h（与 ir.h 同目录）
// ============================================================

#include "parser/ast.h"
#include "symbol/symbol.h"

class SemanticAnalyzer
{
public:
    // 绑定符号表（引用语义，不持有所有权）
    explicit SemanticAnalyzer(SymbolTable& symtab);

    // 语义分析入口：传入 AST 根节点
    // 调用后符号表已填好，可直接 symtab.DumpSymTab()
    void Analyze(const ASTNodePtr& root);

private:
    SymbolTable& m_symtab;

    // 递归遍历单个节点，返回该表达式节点的推导类型；
    // 声明/语句/列表节点返回 TokenType::TOKEN_EOF（不作为表达式类型使用）
    TokenType Visit(const ASTNodePtr& node);

    // —— 各类节点处理 ——
    void       VisitDecl(DeclNode* decl);            // 变量声明：RecVar 查重 + 回填 m_iSymIdx
    void       VisitAssign(AssignNode* assign);      // 赋值：左值查声明，右值求值，CheckAssign
    TokenType  VisitIdent(IdentNode* ident);          // 标识符引用：SearchVar + 回填 m_iSymIdx
    TokenType  VisitBinOp(BinOpNode* op);             // 二元运算：递归左右，按类型系统表推导结果类型
    void       VisitNum(NumNode* num);                // 常量：回填 + 字符/布尔常量自登记

    // —— 类型工具 ——
    bool       IsNumeric(TokenType t) const;          // int / float
    TokenType  Promote(TokenType a, TokenType b) const; // 混合运算提升：含 float 则 float，否则 int
    TokenType  NumType(const NumNode* n) const;       // NumNode 字面量类型映射，统一输出4种基础类型
    TokenType  DeduceBinOp(TokenType op, TokenType lt, TokenType rt, const LineInfo& loc);
};
