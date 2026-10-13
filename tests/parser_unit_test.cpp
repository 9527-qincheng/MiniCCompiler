// ============================================================================
// MiniC 编译器 —— Parser 单元测试（直接检查 AST 字段，CLI 产物无法覆盖）
// 覆盖：NumNode::m_iConstIdx 是否按 Q12 从 Token::m_iConstIdx 透传，
//       以及节点数值是否与源常量一致。
// v4.0 挑战版补充：
//   - 常量取"四类常量 token"（整型/实型/字符/布尔，§4.1 + §8 semantic003），
//     字符常量取 Token::m_cVal、true/false 取 1/0，不再只统计 NUM_INT/NUM_FLOAT；
//   - 数组声明的维度常量也会成为 Num 节点（§8 semantic004），参与上述比对；
//   - 跨模块事实：现有词法只为整型/实型字面量回填 m_iConstIdx（token.cpp Q12 常量表），
//     字符/布尔常量的 m_iConstIdx 为 -1，Parser 只能原样透传，
//     故"常量表下标非负"只对整型/实型断言（不属本模块可修复范围）。
// 用法：parser_unit_test <source.mc>
//   退出码：0 全部通过 / 1 存在失败断言 / 2 用法或打开文件失败
// ============================================================================
#include "lexer/lexer.h"
#include "parser/parser.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace
{

int g_fail = 0;

void Check(bool cond, const std::string& what)
{
    if (cond)
    {
        std::cout << "PASS  " << what << "\n";
    }
    else
    {
        std::cout << "FAIL  " << what << "\n";
        ++g_fail;
    }
}

// 前序遍历子节点：IfNode 的 else 分支不在 m_left/m_right 上，必须单独访问，
// 否则收集时会漏掉 else 分支里的节点。
template <typename Fn>
void ForEachChild(const ASTNode* node, Fn fn)
{
    fn(node->m_left.get());
    fn(node->m_right.get());
    if (node->m_kind == ASTNodeKind::AST_NODE_IF)
    {
        fn(static_cast<const IfNode*>(node)->m_else.get());
    }
}

// 前序遍历收集 Num 节点：数字都是叶子，前序顺序即源码从左到右的顺序
void CollectNumNodes(const ASTNode* node, std::vector<const NumNode*>& out)
{
    if (!node)
    {
        return;
    }
    if (node->m_kind == ASTNodeKind::AST_NODE_NUM)
    {
        out.push_back(static_cast<const NumNode*>(node));
    }
    ForEachChild(node, [&out](const ASTNode* child) { CollectNumNodes(child, out); });
}

void CollectIdentNodes(const ASTNode* node, std::vector<const IdentNode*>& out)
{
    if (!node)
    {
        return;
    }
    if (node->m_kind == ASTNodeKind::AST_NODE_IDENT)
    {
        out.push_back(static_cast<const IdentNode*>(node));
    }
    ForEachChild(node, [&out](const ASTNode* child) { CollectIdentNodes(child, out); });
}

void CollectDeclNodes(const ASTNode* node, std::vector<const DeclNode*>& out)
{
    if (!node)
    {
        return;
    }
    if (node->m_kind == ASTNodeKind::AST_NODE_DECL)
    {
        out.push_back(static_cast<const DeclNode*>(node));
    }
    ForEachChild(node, [&out](const ASTNode* child) { CollectDeclNodes(child, out); });
}

// 常量 token：v4.0 四类常量（§4.1 编码 52/53/54/11/12），对应产生式 83-87
bool IsConstToken(TokenType t)
{
    return t == TokenType::TOKEN_NUM_INT || t == TokenType::TOKEN_NUM_FLOAT ||
           t == TokenType::TOKEN_CHAR_CONST || t == TokenType::TOKEN_TRUE ||
           t == TokenType::TOKEN_FALSE;
}

// 字面量在 AST NumNode 中应有的数值（字符取 m_cVal、true/false 取 1/0）
double ExpectedValue(const Token& t)
{
    switch (t.m_type)
    {
    case TokenType::TOKEN_CHAR_CONST: return static_cast<double>(t.m_cVal);
    case TokenType::TOKEN_TRUE:       return 1.0;
    case TokenType::TOKEN_FALSE:      return 0.0;
    case TokenType::TOKEN_NUM_FLOAT:  return t.m_fVal;
    default:                          return static_cast<double>(t.m_iVal);
    }
}

// 是否为"词法已登记常量表下标"的字面量（整型/实型）
bool IsLexRegisteredConst(TokenType t)
{
    return t == TokenType::TOKEN_NUM_INT || t == TokenType::TOKEN_NUM_FLOAT;
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "usage: parser_unit_test <source.mc>\n";
        return 2;
    }

    CLex lex;
    if (!lex.OpenSource(argv[1]))
    {
        return 2;
    }
    lex.Run();

    // 源文件中出现的常量字面量（整型/实型/字符/布尔），按出现顺序
    std::vector<const Token*> numTokens;
    for (const auto& t : lex.GetTokenList())
    {
        if (IsConstToken(t.m_type))
        {
            numTokens.push_back(&t);
        }
    }

    CParser parser(lex.GetTokenList());
    ASTNodePtr root = parser.Parse();
    Check(root != nullptr, "Parse() returns a non-null AST");
    Check(!parser.HasError(), "Parse() reports no syntax error for this input");

    std::vector<const NumNode*> numNodes;
    CollectNumNodes(root.get(), numNodes);

    Check(numTokens.empty() || !numNodes.empty(),
          "AST contains a Num node for every constant literal in the source");
    Check(numNodes.size() == numTokens.size(),
          "Num node count equals constant token count (int/float/char/bool/dimension)");

    const size_t n = std::min(numNodes.size(), numTokens.size());
    bool idxOk = true, nonNegOk = true, valOk = true;
    for (size_t i = 0; i < n; ++i)
    {
        if (numNodes[i]->m_iConstIdx != numTokens[i]->m_iConstIdx)
        {
            idxOk = false;
        }
        // 仅整型/实型字面量由词法登记常量表下标（跨模块事实，见文件头注释）
        if (IsLexRegisteredConst(numTokens[i]->m_type) && numNodes[i]->m_iConstIdx < 0)
        {
            nonNegOk = false;
        }
        const double expected = ExpectedValue(*numTokens[i]);
        if (numNodes[i]->m_fValue != expected ||
            numNodes[i]->m_bIsFloat != numTokens[i]->m_bIsFloat)
        {
            valOk = false;
        }
    }
    Check(nonNegOk, "every lexer-registered constant Num node has m_iConstIdx >= 0");
    Check(idxOk, "NumNode::m_iConstIdx equals originating Token::m_iConstIdx");
    Check(valOk, "NumNode value/float-flag matches originating token");

    // m_iSymIdx 由语义阶段回填；Parser 必须保持默认 -1
    std::vector<const IdentNode*> idents;
    std::vector<const DeclNode*>  decls;
    CollectIdentNodes(root.get(), idents);
    CollectDeclNodes(root.get(), decls);
    bool identUnset = true, declUnset = true;
    for (const IdentNode* n : idents) { if (n->m_iSymIdx != -1) { identUnset = false; } }
    for (const DeclNode* n : decls)   { if (n->m_iSymIdx != -1) { declUnset = false; } }
    Check(!idents.empty(), "AST contains at least one Ident node");
    Check(!decls.empty(), "AST contains at least one Decl node");
    Check(identUnset, "parser leaves IdentNode::m_iSymIdx = -1 (semantic backfill)");
    Check(declUnset, "parser leaves DeclNode::m_iSymIdx = -1 (semantic backfill)");

    if (g_fail == 0)
    {
        std::cout << "parser_unit_test: ALL PASSED\n";
        return 0;
    }
    std::cout << "parser_unit_test: " << g_fail << " check(s) FAILED\n";
    return 1;
}
