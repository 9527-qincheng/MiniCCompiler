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

#include <map>
#include <string>
#include <vector>

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

    // 当前正在分析的函数的返回类型与名字，用于 return 语句的返回值检查。
    // 顶层（函数体之外）的 return 不参与检查：MiniC 允许脚本式顶层语句，
    // tests/parser/return_stmt.mc 与多个 codegen 用例都在顶层写了裸 return;
    TokenType   m_curRetType = TokenType::TOKEN_NONE;
    std::string m_szCurFunc;

    // 递归遍历单个节点；表达式节点返回推导类型，语句/声明节点返回 TOKEN_NONE
    TokenType Visit(const ASTNodePtr& node);

    // —— 声明 ——
    void VisitDecl(DeclNode* decl);
    void VisitArrayDecl(ArrayDeclNode* ad);
    void VisitFuncDef(FuncDefNode* fn);

    // —— 语句 ——
    void VisitAssign(AssignNode* assign);
    TokenType VisitReturn(ReturnNode* ret);

    // —— 表达式 ——
    TokenType VisitIdent(IdentNode* ident);
    TokenType VisitIndex(IndexNode* ix);
    TokenType VisitBinOp(BinOpNode* op);
    TokenType VisitUnOp(UnOpNode* u);
    TokenType VisitCall(CallNode* call);
    void      VisitNum(NumNode* num);

    // —— 类型表按需登记（2026-10-29）——
    // 类型表原来在 Analyze 入口一次性登记 5 个内建类型，导致「只用 int 的程序」
    // 也会显示 float/bool/char/void —— 那是「语言支持的类型全集」，不是
    // 「本程序用到的类型」。改成各处遇到具体类型时调这个函数登记。
    void      NoteType(TokenType t);

    // —— 类型工具 ——
    bool      IsNumeric(TokenType t) const;
    TokenType Promote(TokenType a, TokenType b) const;
    TokenType NumType(const NumNode* n) const;
    TokenType DeduceBinOp(TokenType op, TokenType lt, TokenType rt,
                          const LineInfo& loc);

    // —— 【新增】静态检查辅助 ——
    // 函数体里是否存在 return 语句（递归进 block/if/while，不进嵌套函数定义）。
    // 用于「非 void 函数一条 return 都没有」的检查：这种函数必然走落空出口，
    // 返回值是寄存器残值，属于典型的隐蔽 bug（tests/codegen/ret_semantics.mc
    // 当年就是为了固定这段语义才写的）。
    static bool  HasReturnStmt(const ASTNodePtr& node);

    // 下标越界检查：下标是整型常量时，与数组声明的元素个数比较。
    // 只在两边都确定时才检查，下标是变量就放过（那是运行期的事）。
    void CheckConstIndex(const VarEntry& ve, const std::string& szName,
                         const ASTNodePtr& indexExpr, const LineInfo& loc);

    // 把「常量下标表达式」折叠成一个整数。
    // 为什么要折叠而不是直接看 NumNode：`a[-1]` 的 -1 在 AST 里是
    // UnOpNode(TOKEN_MINUS, NumNode(1))，不是 NumNode —— 只判 NumNode 会漏掉
    // 负下标这条最容易写错的路径（实测过：`a[-1] = 1;` 曾静默通过）。
    // 只认「数字字面量」与「一元负号套数字字面量」两种形态，其余一律返回 false。
    static bool FoldConstIndex(const ASTNodePtr& expr, long long& out);

    // —— 函数签名表（实参个数检查用）——
    // 为什么不用 SymbolTable：ParamEntry 没有「所属过程」字段（symbol.cpp 顶部
    // 已注明这个已知限制），m_ParamInfoTbl 是一张扁平表，无法按函数名筛形参。
    // 这里在 Analyze 入口先扫一遍 AST 建签名，避免「函数体里调后面才定义的函数」
    // 时签名还没登记的问题。
    std::map<std::string, std::vector<TokenType>> m_FuncSigs;
    void CollectSignatures(const ASTNodePtr& root);

    // 【新增】本轮遍历是否遇到过叫 main 的函数定义（入口缺失检查用）
    bool m_bHasMain = false;
};