#include "symbol/semantic.h"
#include "common/error.h"
#include <string>

SemanticAnalyzer::SemanticAnalyzer(SymbolTable& symtab) : m_symtab(symtab) {}

void SemanticAnalyzer::Analyze(const ASTNodePtr& root) {
    if (!root) return;
    m_symtab.EnterScope(ScopeType::SCOPE_GLOBAL);
    Visit(root);
    m_symtab.ExitScope();
}

TokenType SemanticAnalyzer::Visit(const ASTNodePtr& node) {
    if (!node) return TOKEN_EOF;
    switch (node->m_kind) {
        case ASTNodeKind::Program:
        case ASTNodeKind::DeclList:
        case ASTNodeKind::StmtList:
            VisitList(node.get()); return TOKEN_EOF;
        case ASTNodeKind::Decl:
            VisitDecl(static_cast<DeclNode*>(node.get())); return TOKEN_EOF;
        case ASTNodeKind::Assign:
            VisitAssign(static_cast<AssignNode*>(node.get())); return TOKEN_EOF;
        case ASTNodeKind::Ident:
            return VisitIdent(static_cast<IdentNode*>(node.get()));
        case ASTNodeKind::Op:
            return VisitBinOp(static_cast<BinOpNode*>(node.get()));
        case ASTNodeKind::Num:
            VisitNum(static_cast<NumNode*>(node.get()));
            return static_cast<NumNode*>(node.get())->m_type;
        case ASTNodeKind::If:
        case ASTNodeKind::While:
        case ASTNodeKind::FuncDef:
        case ASTNodeKind::Call:
        case ASTNodeKind::Array:
            Visit(node->m_left); Visit(node->m_right); return TOKEN_EOF;
        case ASTNodeKind::Expr:
            return Visit(node->m_left);
        default: return TOKEN_EOF;
    }
}

void SemanticAnalyzer::VisitList(ASTNode* list) {
    Visit(list->m_left);
    Visit(list->m_right);
}

void SemanticAnalyzer::VisitDecl(DeclNode* decl) {
    if (m_symtab.SearchVar(decl->m_szName) != nullptr) {
        EmitError("redefinition of variable " + decl->m_szName, decl->m_loc);
        return;
    }
    VarEntry* entry = m_symtab.RecVar(decl->m_szName, decl->m_declType,
                                      decl->m_loc.m_iRow, decl->m_loc.m_iCol);
    if (entry) decl->m_iSymIdx = (int)(entry - m_symtab.m_VarInfoTbl.data());
}

void SemanticAnalyzer::VisitAssign(AssignNode* assign) {
    ASTNode* lhs = assign->m_left.get();
    if (!lhs || lhs->m_kind != ASTNodeKind::Ident) {
        EmitError("invalid lvalue", assign->m_loc); return;
    }
    TokenType lt = VisitIdent(static_cast<IdentNode*>(lhs));
    TokenType rt = Visit(assign->m_right);
    if (lt == TOKEN_EOF || rt == TOKEN_EOF) return;
    m_symtab.CheckAssign(lt, rt, assign->m_loc.m_iRow, assign->m_loc.m_iCol);
}

TokenType SemanticAnalyzer::VisitIdent(IdentNode* ident) {
    const VarEntry* entry = m_symtab.SearchVar(ident->m_szName);
    if (!entry) {
        EmitError("undefined identifier " + ident->m_szName, ident->m_loc);
        return TOKEN_EOF;
    }
    ident->m_iSymIdx = (int)(entry - m_symtab.m_VarInfoTbl.data());
    const_cast<VarEntry*>(entry)->m_bUsed = true;
    return entry->m_baseType;
}

TokenType SemanticAnalyzer::VisitBinOp(BinOpNode* op) {
    TokenType lt = Visit(op->m_left);
    TokenType rt = Visit(op->m_right);
    if (lt == TOKEN_EOF || rt == TOKEN_EOF) return TOKEN_EOF;
    return DeduceBinOp(op->m_op, lt, rt, op->m_loc);
}

void SemanticAnalyzer::VisitNum(NumNode* num) {
    if (num->m_iConstIdx >= 0)
        m_symtab.BackfillConst(num->m_iConstIdx, std::string(), num->m_type);
}

bool SemanticAnalyzer::IsNumeric(TokenType t) const {
    return t == TOKEN_INT || t == TOKEN_FLOAT
        || t == TOKEN_INT_CONST || t == TOKEN_FLOAT_CONST;
}

TokenType SemanticAnalyzer::Promote(TokenType a, TokenType b) const {
    auto norm = [](TokenType t) -> TokenType {
        if (t == TOKEN_INT_CONST) return TOKEN_INT;
        if (t == TOKEN_FLOAT_CONST) return TOKEN_FLOAT;
        return t;
    };
    TokenType na = norm(a), nb = norm(b);
    if (na == TOKEN_FLOAT || nb == TOKEN_FLOAT) return TOKEN_FLOAT;
    return TOKEN_INT;
}

TokenType SemanticAnalyzer::DeduceBinOp(TokenType, TokenType lt, TokenType rt,
                                        const LineInfo& loc) {
    if (!IsNumeric(lt) || !IsNumeric(rt)) {
        EmitError("operands must be numeric", loc);
        return TOKEN_EOF;
    }
    return Promote(lt, rt);
}
