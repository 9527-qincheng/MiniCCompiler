// ============================================================
// SemanticAnalyzer 实现
// 放置位置：src/sema/semantic.cpp（build.bat 只收集 src 下的 .cpp）
// ============================================================
#include "sema/semantic.h"

#include "lexer/token.h"
// ⚠️ 必须 include ir.h：symbol.h 的 ProcEntry 内含 CodeList（std::vector<Quad>），
//    只 include symbol.h 时 Quad 是不完整类型，push ProcEntry 会报
//    「invalid use of incomplete type 'struct Quad'」一大片模板错误。
#include "sema/ir.h"

#include <cstdio>
#include <string>

SemanticAnalyzer::SemanticAnalyzer(SymbolTable& symtab) : m_symtab(symtab) {}

// ---------------------------------------------------------------- 入口

void SemanticAnalyzer::Analyze(const ASTNodePtr& root)
{
    if (!root) return;
    // 全局作用域已由 SymbolTable 构造函数建立，这里不能再 EnterScope，
    // 否则全局变量会被记成 scope 2，与表头显示的 scope 1 不符。
    Visit(root);
}

// ---------------------------------------------------------------- 分派

TokenType SemanticAnalyzer::Visit(const ASTNodePtr& node)
{
    if (!node) return TokenType::TOKEN_NONE;

    switch (node->m_kind)
    {
    case ASTNodeKind::AST_NODE_PROGRAM:
        for (const auto& c : node->m_children) Visit(c);
        return TokenType::TOKEN_NONE;

    case ASTNodeKind::AST_NODE_FUNCDEF:
        VisitFuncDef(static_cast<FuncDefNode*>(node.get()));
        return TokenType::TOKEN_NONE;

    case ASTNodeKind::AST_NODE_BLOCK:
    {
        m_symtab.EnterScope(ScopeType::SCOPE_BLOCK);
        for (const auto& c : node->m_children) Visit(c);
        m_symtab.ExitScope();
        return TokenType::TOKEN_NONE;
    }

    case ASTNodeKind::AST_NODE_DECL:
        VisitDecl(static_cast<DeclNode*>(node.get()));
        return TokenType::TOKEN_NONE;

    case ASTNodeKind::AST_NODE_ARRAYDECL:
        VisitArrayDecl(static_cast<ArrayDeclNode*>(node.get()));
        return TokenType::TOKEN_NONE;

    case ASTNodeKind::AST_NODE_ASSIGN:
        VisitAssign(static_cast<AssignNode*>(node.get()));
        return TokenType::TOKEN_NONE;

    case ASTNodeKind::AST_NODE_IDENT:
        return VisitIdent(static_cast<IdentNode*>(node.get()));

    case ASTNodeKind::AST_NODE_INDEX:
        return VisitIndex(static_cast<IndexNode*>(node.get()));

    case ASTNodeKind::AST_NODE_BINOP:
        return VisitBinOp(static_cast<BinOpNode*>(node.get()));

    case ASTNodeKind::AST_NODE_UNOP:
        return VisitUnOp(static_cast<UnOpNode*>(node.get()));

    case ASTNodeKind::AST_NODE_NUM:
    {
        auto* num = static_cast<NumNode*>(node.get());
        VisitNum(num);
        return NumType(num);
    }

    case ASTNodeKind::AST_NODE_CALL:
        return VisitCall(static_cast<CallNode*>(node.get()));

    case ASTNodeKind::AST_NODE_IF:
    {
        if (node->m_children.empty()) return TokenType::TOKEN_NONE;
        TokenType ct = Visit(node->m_children[0]);
        if (ct != TokenType::TOKEN_NONE && ct != TokenType::TOKEN_BOOL && !IsNumeric(ct))
            EmitError("if condition must be bool or numeric", node->m_children[0]->m_loc);
        for (std::size_t i = 1; i < node->m_children.size(); ++i) Visit(node->m_children[i]);
        return TokenType::TOKEN_NONE;
    }

    case ASTNodeKind::AST_NODE_WHILE:
    {
        if (node->m_children.empty()) return TokenType::TOKEN_NONE;
        TokenType ct = Visit(node->m_children[0]);
        if (ct != TokenType::TOKEN_NONE && ct != TokenType::TOKEN_BOOL && !IsNumeric(ct))
            EmitError("while condition must be bool or numeric", node->m_children[0]->m_loc);
        for (std::size_t i = 1; i < node->m_children.size(); ++i) Visit(node->m_children[i]);
        return TokenType::TOKEN_NONE;
    }

    case ASTNodeKind::AST_NODE_PRINT:
    case ASTNodeKind::AST_NODE_RETURN:
        for (const auto& c : node->m_children) Visit(c);
        return TokenType::TOKEN_NONE;

    case ASTNodeKind::AST_NODE_EMPTY:
    case ASTNodeKind::AST_NODE_PARAM:   // 形参在 VisitFuncDef 里处理
    case ASTNodeKind::AST_NODE_NONE:
    default:
        return TokenType::TOKEN_NONE;
    }
}

// ---------------------------------------------------------------- 声明

void SemanticAnalyzer::VisitDecl(DeclNode* decl)
{
    if (!decl) return;
    // RecVar 内部已在当前作用域完成查重并报错，无需重复查
    VarEntry* entry = m_symtab.RecVar(decl->m_szName, decl->m_type,
                                      decl->m_loc.m_iRow, decl->m_loc.m_iCol);
    if (entry)
        decl->m_iSymIdx = static_cast<int>(entry - m_symtab.m_VarInfoTbl.data());
}

void SemanticAnalyzer::VisitArrayDecl(ArrayDeclNode* ad)
{
    if (!ad) return;
    if (ad->m_iArraySize <= 0)
    {
        EmitError("array size must be a positive integer", ad->m_loc);
        return;
    }
    VarEntry* ve = m_symtab.RecArray(ad->m_szName, ad->m_type, ad->m_iArraySize,
                                     ad->m_loc.m_iRow, ad->m_loc.m_iCol);
    if (ve) ad->m_iSymIdx = static_cast<int>(ve - m_symtab.m_VarInfoTbl.data());

    for (const auto& c : ad->m_children)
        if (c && c->m_kind == ASTNodeKind::AST_NODE_NUM) Visit(c);
}

void SemanticAnalyzer::VisitFuncDef(FuncDefNode* fn)
{
    if (!fn) return;

    // 设计文档 §15.2：文法上允许 `{ int f(){...} }` 这类嵌套函数定义，
    // 但 C 语言不允许，且两个同名函数会生成重复的汇编标号
    // （`as` 报 symbol `f' is already defined）。此处按职责边界拦截。
    if (m_iFuncDepth > 0)
    {
        EmitError("function definition is only allowed at the outermost level: "
                  + fn->m_szName, fn->m_loc);
    }
    ++m_iFuncDepth;

    // 函数名：进变量表（保证 Call 能解析）+ 进过程表
    // 接口缺口：symbol.h 无 RecProc，暂用 RecVar 承载名字
    VarEntry* fv = m_symtab.RecVar(fn->m_szName, fn->m_retType,
                                   fn->m_loc.m_iRow, fn->m_loc.m_iCol);
    if (fv) fn->m_iSymIdx = static_cast<int>(fv - m_symtab.m_VarInfoTbl.data());

    ProcEntry pe;
    pe.m_szName     = fn->m_szName;
    pe.m_retType    = fn->m_retType;
    pe.m_iLine      = fn->m_loc.m_iRow;
    pe.m_iCol       = fn->m_loc.m_iCol;
    pe.m_iScope     = 1;
    pe.m_iProcIndex = static_cast<int>(m_symtab.m_ProcInfoTbl.size());
    m_symtab.m_ProcInfoTbl.push_back(pe);

    m_symtab.EnterScope(ScopeType::SCOPE_FUNCTION);

    int pos = 1;
    for (const auto& c : fn->m_children)
    {
        if (!c || c->m_kind != ASTNodeKind::AST_NODE_PARAM) continue;
        auto* p = static_cast<ParamNode*>(c.get());

        VarEntry* pv = m_symtab.RecVar(p->m_szName, p->m_type,
                                       p->m_loc.m_iRow, p->m_loc.m_iCol);
        if (pv) p->m_iSymIdx = static_cast<int>(pv - m_symtab.m_VarInfoTbl.data());

        ParamEntry pa;
        pa.m_szName    = p->m_szName;
        pa.m_type      = p->m_type;
        pa.m_iParamPos = pos++;
        pa.m_iLine     = p->m_loc.m_iRow;
        pa.m_iCol      = p->m_loc.m_iCol;
        pa.m_iScope    = 2;
        m_symtab.m_ParamInfoTbl.push_back(pa);
    }

    // 函数体的 Block 直接在本层展开，避免多套一层 SCOPE_BLOCK
    for (const auto& c : fn->m_children)
    {
        if (!c || c->m_kind != ASTNodeKind::AST_NODE_BLOCK) continue;
        for (const auto& stmt : c->m_children) Visit(stmt);
    }

    m_symtab.ExitScope();
    --m_iFuncDepth;
}

// ---------------------------------------------------------------- 语句

void SemanticAnalyzer::VisitAssign(AssignNode* assign)
{
    if (!assign || assign->m_children.size() < 2) return;

    ASTNode* lhs = assign->m_children[0].get();
    TokenType lt = TokenType::TOKEN_NONE;

    if (lhs && lhs->m_kind == ASTNodeKind::AST_NODE_IDENT)
        lt = VisitIdent(static_cast<IdentNode*>(lhs));
    else if (lhs && lhs->m_kind == ASTNodeKind::AST_NODE_INDEX)
        lt = VisitIndex(static_cast<IndexNode*>(lhs));
    else
    {
        EmitError("invalid lvalue", assign->m_loc);
        return;
    }

    TokenType rt = Visit(assign->m_children[1]);
    if (lt == TokenType::TOKEN_NONE || rt == TokenType::TOKEN_NONE) return;
    m_symtab.CheckAssign(lt, rt, assign->m_loc.m_iRow, assign->m_loc.m_iCol);
}

// ---------------------------------------------------------------- 表达式

TokenType SemanticAnalyzer::VisitIdent(IdentNode* ident)
{
    if (!ident) return TokenType::TOKEN_NONE;
    const VarEntry* entry = m_symtab.SearchVar(ident->m_szName);
    if (!entry)
    {
        EmitError("undefined identifier " + ident->m_szName, ident->m_loc);
        return TokenType::TOKEN_NONE;
    }
    ident->m_iSymIdx = static_cast<int>(entry - m_symtab.m_VarInfoTbl.data());
    return entry->m_type;
}

TokenType SemanticAnalyzer::VisitIndex(IndexNode* ix)
{
    if (!ix || ix->m_children.size() < 2) return TokenType::TOKEN_NONE;

    ASTNode* base = ix->m_children[0].get();
    if (base && base->m_kind == ASTNodeKind::AST_NODE_IDENT)
    {
        auto* id = static_cast<IdentNode*>(base);
        const VarEntry* ve = m_symtab.SearchVar(id->m_szName);
        if (!ve)
        {
            EmitError("undefined identifier " + id->m_szName, id->m_loc);
            return TokenType::TOKEN_NONE;
        }
        if (!ve->m_bIsArray)
        {
            EmitError("'" + id->m_szName + "' is not an array", base->m_loc);
            return TokenType::TOKEN_NONE;
        }
        id->m_iSymIdx = static_cast<int>(ve - m_symtab.m_VarInfoTbl.data());
    }

    TokenType it = Visit(ix->m_children[1]);
    if (it != TokenType::TOKEN_NONE && it != TokenType::TOKEN_INT)
        EmitError("array subscript must be an integer", ix->m_children[1]->m_loc);

    // 传 m_children[0]（unique_ptr）；传裸指针 base 会编译报
    //   cannot convert 'ASTNode*' to 'const ASTNodePtr&'
    (void)base;
    return Visit(ix->m_children[0]);
}

TokenType SemanticAnalyzer::VisitBinOp(BinOpNode* op)
{
    if (!op || op->m_children.size() < 2) return TokenType::TOKEN_NONE;
    TokenType lt = Visit(op->m_children[0]);
    TokenType rt = Visit(op->m_children[1]);
    if (lt == TokenType::TOKEN_NONE || rt == TokenType::TOKEN_NONE)
        return TokenType::TOKEN_NONE;
    return DeduceBinOp(op->m_op, lt, rt, op->m_loc);
}

TokenType SemanticAnalyzer::VisitUnOp(UnOpNode* u)
{
    if (!u || u->m_children.empty()) return TokenType::TOKEN_NONE;
    TokenType t = Visit(u->m_children[0]);
    if (t == TokenType::TOKEN_NONE) return TokenType::TOKEN_NONE;

    if (u->m_op == TokenType::TOKEN_MINUS)
    {
        if (!IsNumeric(t))
        {
            EmitError("unary '-' requires a numeric operand", u->m_loc);
            return TokenType::TOKEN_NONE;
        }
        return t;
    }
    if (u->m_op == TokenType::TOKEN_NOT)
    {
        if (t != TokenType::TOKEN_BOOL)
        {
            EmitError("'!' requires a bool operand", u->m_loc);
            return TokenType::TOKEN_NONE;
        }
        return TokenType::TOKEN_BOOL;
    }
    return t;
}

TokenType SemanticAnalyzer::VisitCall(CallNode* call)
{
    if (!call || call->m_children.empty()) return TokenType::TOKEN_NONE;

    // children[0] = 被调标识符（返回类型即函数返回类型）
    TokenType ft = Visit(call->m_children[0]);

    // children[1..] = 实参，逐个求值（顺带检查未声明/类型）
    for (std::size_t i = 1; i < call->m_children.size(); ++i)
        Visit(call->m_children[i]);

    return ft;
}

void SemanticAnalyzer::VisitNum(NumNode* num)
{
    if (!num) return;
    TokenType t = NumType(num);

    char buf[64] = {0};
    std::string nm;
    if (num->m_isChar)
    {
        std::snprintf(buf, sizeof(buf), "'\\x%02X'",
                      static_cast<unsigned>(static_cast<int>(num->m_fValue)) & 0xFFu);
        nm = buf;
    }
    else if (num->m_isBool)
    {
        nm = (num->m_fValue != 0.0) ? "true" : "false";
    }
    else if (num->m_bIsFloat)
    {
        std::snprintf(buf, sizeof(buf), "%g", num->m_fValue);
        nm = buf;
    }
    else
    {
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(num->m_fValue));
        nm = buf;
    }

    ConstEntry* ce = m_symtab.RecConst(nm, t, num->m_fValue,
                                       num->m_loc.m_iRow, num->m_loc.m_iCol);
    if (ce) num->m_iConstIdx = static_cast<int>(ce - m_symtab.m_ConstInfoTbl.data());
}

// ---------------------------------------------------------------- 类型工具

bool SemanticAnalyzer::IsNumeric(TokenType t) const
{
    return t == TokenType::TOKEN_INT || t == TokenType::TOKEN_FLOAT;
}

TokenType SemanticAnalyzer::Promote(TokenType a, TokenType b) const
{
    if (a == TokenType::TOKEN_FLOAT || b == TokenType::TOKEN_FLOAT)
        return TokenType::TOKEN_FLOAT;
    return TokenType::TOKEN_INT;
}

TokenType SemanticAnalyzer::NumType(const NumNode* n) const
{
    if (!n) return TokenType::TOKEN_NONE;
    if (n->m_bIsFloat) return TokenType::TOKEN_FLOAT;
    if (n->m_isChar)   return TokenType::TOKEN_CHAR;
    if (n->m_isBool)   return TokenType::TOKEN_BOOL;
    return TokenType::TOKEN_INT;
}

TokenType SemanticAnalyzer::DeduceBinOp(TokenType op, TokenType lt, TokenType rt,
                                        const LineInfo& loc)
{
    if (op == TokenType::TOKEN_AND || op == TokenType::TOKEN_OR)
    {
        if (lt != TokenType::TOKEN_BOOL || rt != TokenType::TOKEN_BOOL)
        {
            EmitError("logical operators require bool operands", loc);
            return TokenType::TOKEN_NONE;
        }
        return TokenType::TOKEN_BOOL;
    }

    if (op == TokenType::TOKEN_EQ || op == TokenType::TOKEN_NE ||
        op == TokenType::TOKEN_LT || op == TokenType::TOKEN_LE ||
        op == TokenType::TOKEN_GT || op == TokenType::TOKEN_GE)
    {
        if (!IsNumeric(lt) || !IsNumeric(rt))
        {
            EmitError("relational operands must be numeric", loc);
            return TokenType::TOKEN_NONE;
        }
        return TokenType::TOKEN_BOOL;
    }

    if (op == TokenType::TOKEN_PLUS || op == TokenType::TOKEN_MINUS ||
        op == TokenType::TOKEN_MUL  || op == TokenType::TOKEN_DIV)
    {
        if (!IsNumeric(lt) || !IsNumeric(rt))
        {
            EmitError("arithmetic operands must be numeric", loc);
            return TokenType::TOKEN_NONE;
        }
        return Promote(lt, rt);
    }

    EmitError("unsupported binary operator", loc);
    return TokenType::TOKEN_NONE;
}