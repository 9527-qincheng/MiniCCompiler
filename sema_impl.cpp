#include "sema/semantic.h"
#include "lexer/token.h"
#include <string>

SemanticAnalyzer::SemanticAnalyzer(SymbolTable& symtab) : m_symtab(symtab) {}

void SemanticAnalyzer::Analyze(const ASTNodePtr& root) {
    if (!root) return;
    m_symtab.EnterScope(ScopeType::SCOPE_GLOBAL);
    Visit(root);
    m_symtab.ExitScope();
}

TokenType SemanticAnalyzer::Visit(const ASTNodePtr& node) {
    if (!node) return TokenType::TOKEN_EOF;
    switch (node->m_kind) {
        // 顶层程序/代码块节点：直接遍历所有子节点（声明+语句序列）
        case ASTNodeKind::AST_NODE_PROGRAM:
        case ASTNodeKind::AST_NODE_BLOCK: {
            for (const auto& child : node->m_children) {
                Visit(child);
            }
            return TokenType::TOKEN_EOF;
        }
        // 变量声明
        case ASTNodeKind::AST_NODE_DECL:
            VisitDecl(static_cast<DeclNode*>(node.get()));
            return TokenType::TOKEN_EOF;
        // 赋值语句
        case ASTNodeKind::AST_NODE_ASSIGN:
            VisitAssign(static_cast<AssignNode*>(node.get()));
            return TokenType::TOKEN_EOF;
        // 标识符引用
        case ASTNodeKind::AST_NODE_IDENT:
            return VisitIdent(static_cast<IdentNode*>(node.get()));
        // 二元运算
        case ASTNodeKind::AST_NODE_BINOP:
            return VisitBinOp(static_cast<BinOpNode*>(node.get()));
        // 数字/字符/布尔常量
        case ASTNodeKind::AST_NODE_NUM: {
            NumNode* num = static_cast<NumNode*>(node.get());
            VisitNum(num);
            return NumType(num);
        }
        // 控制流、函数调用、数组声明类节点：安全遍历全部子节点
        case ASTNodeKind::AST_NODE_IF:
        case ASTNodeKind::AST_NODE_WHILE:
        case ASTNodeKind::AST_NODE_CALL:
        case ASTNodeKind::AST_NODE_ARRAYDECL:
        case ASTNodeKind::AST_NODE_RETURN:
        case ASTNodeKind::AST_NODE_PRINT: {
            for (const auto& child : node->m_children) {
                Visit(child);
            }
            return TokenType::TOKEN_EOF;
        }
        default:
            return TokenType::TOKEN_EOF;
    }
}

void SemanticAnalyzer::VisitDecl(DeclNode* decl) {
    // RecVar 内部已在当前作用域完成重复声明查重并自动报错，无需额外查重
    VarEntry* entry = m_symtab.RecVar(decl->m_szName, decl->m_type,
                                      decl->m_loc.m_iRow, decl->m_loc.m_iCol);
    if (entry) {
        decl->m_iSymIdx = static_cast<int>(entry - m_symtab.m_VarInfoTbl.data());
    }
}

void SemanticAnalyzer::VisitAssign(AssignNode* assign) {
    if (assign->m_children.empty()) return;
    ASTNode* lhs = assign->m_children[0].get();
    if (!lhs || lhs->m_kind != ASTNodeKind::AST_NODE_IDENT) {
        EmitError("invalid lvalue", assign->m_loc);
        return;
    }
    TokenType lt = VisitIdent(static_cast<IdentNode*>(lhs));
    TokenType rt = TokenType::TOKEN_EOF;
    if (assign->m_children.size() >= 2) {
        rt = Visit(assign->m_children[1]);
    }
    if (lt == TokenType::TOKEN_EOF || rt == TokenType::TOKEN_EOF) return;
    m_symtab.CheckAssign(lt, rt, assign->m_loc.m_iRow, assign->m_loc.m_iCol);
}

TokenType SemanticAnalyzer::VisitIdent(IdentNode* ident) {
    const VarEntry* entry = m_symtab.SearchVar(ident->m_szName);
    if (!entry) {
        EmitError("undefined identifier " + ident->m_szName, ident->m_loc);
        return TokenType::TOKEN_EOF;
    }
    ident->m_iSymIdx = static_cast<int>(entry - m_symtab.m_VarInfoTbl.data());
    return entry->m_type;
}

TokenType SemanticAnalyzer::VisitBinOp(BinOpNode* op) {
    if (op->m_children.size() < 2) return TokenType::TOKEN_EOF;
    TokenType lt = Visit(op->m_children[0]);
    TokenType rt = Visit(op->m_children[1]);
    if (lt == TokenType::TOKEN_EOF || rt == TokenType::TOKEN_EOF) return TokenType::TOKEN_EOF;
    return DeduceBinOp(op->m_op, lt, rt, op->m_loc);
}

void SemanticAnalyzer::VisitNum(NumNode* num) {
    TokenType t = NumType(num);
    if (num->m_iConstIdx >= 0) {
        // 词法阶段已登记的常量，直接回填类型
        m_symtab.BackfillConst(num->m_iConstIdx, std::string(), t);
    } else {
        // 字符/布尔常量词法未登记，语义阶段自行登记
        m_symtab.RecConstTbl(std::string(), t);
    }
}

bool SemanticAnalyzer::IsNumeric(TokenType t) const {
    return t == TokenType::TOKEN_INT || t == TokenType::TOKEN_FLOAT;
}

TokenType SemanticAnalyzer::Promote(TokenType a, TokenType b) const {
    if (a == TokenType::TOKEN_FLOAT || b == TokenType::TOKEN_FLOAT) return TokenType::TOKEN_FLOAT;
    return TokenType::TOKEN_INT;
}

TokenType SemanticAnalyzer::NumType(const NumNode* n) const {
    if (n->m_bIsFloat) return TokenType::TOKEN_FLOAT;
    if (n->m_isChar) return TokenType::TOKEN_CHAR;
    if (n->m_isBool) return TokenType::TOKEN_BOOL;
    return TokenType::TOKEN_INT;
}

TokenType SemanticAnalyzer::DeduceBinOp(TokenType, TokenType lt, TokenType rt,
                                        const LineInfo& loc) {
    if (!IsNumeric(lt) || !IsNumeric(rt)) {
        EmitError("operands must be numeric", loc);
        return TokenType::TOKEN_EOF;
    }
    return Promote(lt, rt);
}
