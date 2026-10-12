// ============================================================================
// MiniC 编译器 —— Parser 单元测试（直接检查 AST 字段，CLI 产物无法覆盖）
// 覆盖：NumNode::m_iConstIdx 是否按 Q12 从 Token::m_iConstIdx 透传，
//       以及节点数值是否与源常量一致。
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

bool IsNumToken(TokenType t)
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

    // 源文件中出现的数字常量，按出现顺序
    std::vector<const Token*> numTokens;
    for (const auto& t : lex.GetTokenList())
    {
        if (IsNumToken(t.m_type))
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

    Check(!numNodes.empty(), "AST contains at least one Num node");
    Check(numNodes.size() == numTokens.size(),
          "Num node count equals NUM token count");

    const size_t n = std::min(numNodes.size(), numTokens.size());
    bool idxOk = true, nonNegOk = true, valOk = true;
    for (size_t i = 0; i < n; ++i)
    {
        if (numNodes[i]->m_iConstIdx != numTokens[i]->m_iConstIdx)
        {
            idxOk = false;
        }
        if (numNodes[i]->m_iConstIdx < 0)
        {
            nonNegOk = false;
        }
        const double expected = numTokens[i]->m_bIsFloat
                                    ? numTokens[i]->m_fVal
                                    : static_cast<double>(numTokens[i]->m_iVal);
        if (numNodes[i]->m_fValue != expected ||
            numNodes[i]->m_bIsFloat != numTokens[i]->m_bIsFloat)
        {
            valOk = false;
        }
    }
    Check(nonNegOk, "every Num node has a non-negative m_iConstIdx");
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
