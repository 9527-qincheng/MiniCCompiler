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
#include <vector>

namespace {

// 实参类型兼容判定（2026-10-28 新增）。
// 只把「int / float / char 三者互转」当作可接受的隐式转换（C 的常规算术转换），
// bool 与数值之间、void 与任何类型之间都视为不兼容 —— 那两类基本只会是写错，
// 而不是有意转换。
bool ArgCompatible(TokenType want, TokenType got)
{
    if (want == got) return true;
    const auto numeric = [](TokenType t) {
        return t == TokenType::TOKEN_INT || t == TokenType::TOKEN_FLOAT ||
               t == TokenType::TOKEN_CHAR;
    };
    return numeric(want) && numeric(got);
}

// char 只能表示 -128..127（codegen 用 movb 存单字节，超范围静默截断）。
int TruncateToChar(long long v)
{
    return static_cast<int>(static_cast<signed char>(static_cast<int>(v)));
}

}  // namespace

SemanticAnalyzer::SemanticAnalyzer(SymbolTable& symtab) : m_symtab(symtab) {}

// ---------------------------------------------------------------- 入口

void SemanticAnalyzer::Analyze(const ASTNodePtr& root)
{
    if (!root) return;
    // 全局作用域已由 SymbolTable 构造函数建立，这里不能再 EnterScope，
    // 否则全局变量会被记成 scope 2，与表头显示的 scope 1 不符。
    //
    // 先把内建类型登记进类型表：TypeEntry 里存的是「这个类型占几个字节」，
    // 与 codegen 的访问指令（char 用 movb、int/float/bool 用 movl/movss）对齐。
    // 这件事原本挂在 semantic001 上，但语义动作统一由本类实现后一直没落地，
    // 导致 RecType 成了零调用的死接口、--dump-symtab 的 Types 节恒为空。
    m_symtab.RecBuiltinTypes();
    // 再把全部函数签名扫一遍再遍历：MiniC 允许「函数体里调用后面才定义的函数」，
    // 若边遍历边登记签名，前面的调用就查不到形参个数，实参个数检查会漏。
    CollectSignatures(root);
    Visit(root);

    // 【新增】既没有 main 函数、也没有任何顶层语句 → 生成的 main 是空函数，
    // 编译链接一切正常，跑起来什么都不做。这类"沉默的空程序"几乎总是漏写了
    // 入口，给一条警告而不是错误：
    //   - 只有函数定义、没有 main 的写法是合法的（codegen 会用顶层语句合成
    //     入口，tests/codegen 里 9 个用例就是这么写的），不能报错；
    //   - 只有顶层语句、没有 main 也合法（同一个合成入口）。
    // 只有"两者都没有"才值得提醒。
    //
    // 三个前提缺一不可：
    //   [a] 前面各阶段一条错都没有 —— 语法错时 AST 本就残缺，"没有 main"
    //       只是残缺的表象，再追一条警告纯属噪音（tests/parser 的
    //       err_decl_no_name / err_func_bad_param 就是这么炸的）。
    //   [b] 根节点至少有一个子节点 —— 空文件走不到语义阶段都该闭嘴
    //       （tests/parser/empty.mc 期望 stderr 为空）。
    //   [c] 排除纯注释/纯空白的文件：Parser 会产出 PROGRAM + 若干 EMPTY 的骨架，
    //       那不是"漏写入口"，只是没什么可编译。
    // 判据只看「有没有顶层语句」的字面形态，不做控制流分析。
    if (!m_bHasMain && g_ErrorList.empty() && !root->m_children.empty())
    {
        bool bHasTopLevelStmt = false;
        bool bHasFuncDef      = false;
        for (const auto& c : root->m_children)
        {
            if (!c) continue;
            if (c->m_kind == ASTNodeKind::AST_NODE_FUNCDEF) { bHasFuncDef = true; continue; }
            if (c->m_kind == ASTNodeKind::AST_NODE_EMPTY)   continue;
            bHasTopLevelStmt = true;
            break;
        }
        // 只有「有函数但没有 main、且没有顶层语句」才提醒；
        // 连函数都没有的空文件不提醒。
        if (bHasFuncDef && !bHasTopLevelStmt)
        {
            EmitWarning("no 'main' function and no top-level statements; "
                        "the generated entry point will do nothing",
                        root->m_loc);
        }
    }
}

void SemanticAnalyzer::CollectSignatures(const ASTNodePtr& root)
{
    m_FuncSigs.clear();
    if (!root || root->m_kind != ASTNodeKind::AST_NODE_PROGRAM) return;

    for (const auto& c : root->m_children)
    {
        if (!c || c->m_kind != ASTNodeKind::AST_NODE_FUNCDEF) continue;
        auto* fn = static_cast<const FuncDefNode*>(c.get());

        std::vector<TokenType> types;
        for (const auto& p : fn->m_children)
        {
            if (p && p->m_kind == ASTNodeKind::AST_NODE_PARAM)
                types.push_back(static_cast<const ParamNode*>(p.get())->m_type);
        }
        // 重名函数取第一次出现的签名（同名函数重复定义由 RecVar 报错，这里不重复报）
        if (fn->m_szName == "main") m_bHasMain = true;
        m_FuncSigs.emplace(fn->m_szName, std::move(types));
    }
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
        for (const auto& c : node->m_children) Visit(c);
        return TokenType::TOKEN_NONE;

    case ASTNodeKind::AST_NODE_RETURN:
        return VisitReturn(static_cast<ReturnNode*>(node.get()));

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

    // 【2026-10-27 补全】数组类型也登记进类型表。
    // 之前 TypeEntry::m_bIsArray / m_iArraySize 两个字段全仓零写入，
    // `int arr[5]` 在 --dump-symtab 的 Types 节里只能看到一行 `int : INT  size 4`，
    // 看不出它是个 20 字节的数组。现在按「基类型[元素个数]」拼出类型名登记，
    // 同一形状只占一行（如 int[5]）。
    {
        const std::string szTypeName = std::string(TokenTypeToString(ad->m_type)) + "[" +
                                       std::to_string(ad->m_iArraySize) + "]";
        m_symtab.RecArrayType(szTypeName, ad->m_type, ad->m_iArraySize);
    }

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

    // 记录当前函数的返回类型，供 return 语句检查（退出时恢复，支持将来的嵌套场景）
    const TokenType savedRetType = m_curRetType;
    const std::string savedFuncName = m_szCurFunc;
    m_curRetType = fn->m_retType;
    m_szCurFunc  = fn->m_szName;

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
    const ASTNodePtr* pBody = nullptr;
    for (const auto& c : fn->m_children)
    {
        if (!c || c->m_kind != ASTNodeKind::AST_NODE_BLOCK) continue;
        if (!pBody) pBody = &c;
        for (const auto& stmt : c->m_children) Visit(stmt);
    }

    // 【新增】非 void 函数一条 return 都没有 → 必然走落空出口。
    // codegen 对落空出口已按返回类型补默认值（ret_semantics.mc 固定的语义），
    // 但「忘了写 return」几乎总是笔误，这里给警告而不是错误：
    // 判据只看「有没有 return」，不看「所有路径是否都 return」（后者需要
    // 控制流分析，且会让 `if (n>0) return 1;` 这类常见写法被误报）。
    if (m_curRetType != TokenType::TOKEN_VOID &&
        m_curRetType != TokenType::TOKEN_NONE &&
        m_iFuncDepth == 1 &&
        pBody != nullptr &&
        !HasReturnStmt(*pBody))
    {
        EmitWarning("non-void function '" + m_szCurFunc +
                    "' has no return statement; the caller will receive an "
                    "unspecified value", fn->m_loc);
    }

    m_symtab.ExitScope();
    m_curRetType = savedRetType;
    m_szCurFunc  = savedFuncName;
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

    // char 越界赋值检查（2026-10-28 新增）。
    // `char c; c = 300;` 原先静默通过，codegen 用 movb 存单字节，实际存进去是 44。
    // 只查「右边是整型字面量」这一种形态 —— 变量里的值静态看不出来，
    // 运行期检查不在本编译器范围内（见项目总结 6.2 限制）。
    if (lt == TokenType::TOKEN_CHAR && rt == TokenType::TOKEN_INT &&
        assign->m_children[1] &&
        assign->m_children[1]->m_kind == ASTNodeKind::AST_NODE_NUM)
    {
        const auto* lit = static_cast<const NumNode*>(assign->m_children[1].get());
        if (!lit->m_bIsFloat && !lit->m_isChar && !lit->m_isBool &&
            (lit->m_fValue > 127.0 || lit->m_fValue < -128.0))
        {
            const long long v = static_cast<long long>(lit->m_fValue);
            EmitWarning("value " + std::to_string(v) +
                        " cannot be represented in char (-128..127); it will be "
                        "truncated to " + std::to_string(TruncateToChar(v)),
                        assign->m_loc);
        }
    }
}

// ---------------------------------------------------------------- 表达式

TokenType SemanticAnalyzer::VisitReturn(ReturnNode* ret)
{
    // 表达式语义：先把返回表达式求值，再做返回类型兼容检查。
    // 检查规则（挑战版第 ⑥ 项）：
    //   · 非 void 函数必须带返回值：int f() { return; } → 报错
    //   · void 函数不能带返回值：void f() { return 1; } → 报错
    //   · 有返回值时按 §8.1 判可赋值性，复用 CheckAssign（含 Q9 的 float→int 告警）
    // 顶层（函数外）的 return 不参与检查：MiniC 允许脚本式顶层语句，
    // tests/parser/return_stmt.mc 与若干 codegen 用例都在顶层写了裸 return;
    if (!ret) return TokenType::TOKEN_NONE;

    const bool bHasValue = !ret->m_children.empty() && ret->m_children[0];
    TokenType vt = TokenType::TOKEN_NONE;
    if (bHasValue) vt = Visit(ret->m_children[0]);

    if (m_iFuncDepth <= 0) return TokenType::TOKEN_NONE;   // 顶层 return，不检查

    if (!bHasValue)
    {
        if (m_curRetType != TokenType::TOKEN_VOID && m_curRetType != TokenType::TOKEN_NONE)
        {
            EmitError("non-void function '" + m_szCurFunc +
                      "' must return a value", ret->m_loc);
        }
        return TokenType::TOKEN_NONE;
    }

    if (m_curRetType == TokenType::TOKEN_VOID)
    {
        EmitError("void function '" + m_szCurFunc +
                  "' cannot return a value", ret->m_loc);
        return TokenType::TOKEN_NONE;
    }

    if (vt != TokenType::TOKEN_NONE)
        m_symtab.CheckAssign(m_curRetType, vt, ret->m_loc.m_iRow, ret->m_loc.m_iCol);

    return TokenType::TOKEN_NONE;
}

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

    // 【新增】数组名必须带下标才能取值。
    // 修复前：`int a[3]; int b; b = a;` 会静默取 a[0] 的值（codegen 的
    // LoadAddr 把数组名当地址常量，读出来的就是首元素），`print(a)` 同理。
    // 这在 C 里是「数组退化为指针」，MiniC 没有指针，退化的唯一结果就是
    // 一个不声不响的错值，所以这里直接报错。
    if (entry->m_bIsArray)
    {
        EmitError("'" + ident->m_szName + "' is an array; a subscript is required "
                  "to read an element", ident->m_loc);
        return TokenType::TOKEN_NONE;
    }

    return entry->m_type;
}

TokenType SemanticAnalyzer::VisitIndex(IndexNode* ix)
{
    if (!ix || ix->m_children.size() < 2) return TokenType::TOKEN_NONE;

    ASTNode* base = ix->m_children[0].get();
    TokenType elemType = TokenType::TOKEN_NONE;

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
        elemType = ve->m_type;

        // 【新增】下标是整型常量时做编译期越界检查
        CheckConstIndex(*ve, id->m_szName, ix->m_children[1], ix->m_loc);
    }

    TokenType it = Visit(ix->m_children[1]);
    // char 下标按整型提升处理（`a['a']` 等于 `a[97]`，C 语义）：
    // 只认 TOKEN_INT 会把合法的字符下标误判成「must be an integer」，
    // 而且会和上面的越界检查叠成两条互相矛盾的诊断。
    if (it != TokenType::TOKEN_NONE && it != TokenType::TOKEN_INT &&
        it != TokenType::TOKEN_CHAR)
        EmitError("array subscript must be an integer", ix->m_children[1]->m_loc);

    // 基址是标识符时上面已解析完（且不能重复 Visit：
    // VisitIdent 现在会对数组名直接报错，再走一遍就成了自己踩自己）。
    if (base && base->m_kind == ASTNodeKind::AST_NODE_IDENT)
        return elemType;

    (void)base;
    return Visit(ix->m_children[0]);
}

bool SemanticAnalyzer::FoldConstIndex(const ASTNodePtr& expr, long long& out)
{
    if (!expr) return false;

    // 形态一：数字字面量（整型 / 字符常量都算；实型下标已在别处报「must be an integer」）
    if (expr->m_kind == ASTNodeKind::AST_NODE_NUM)
    {
        const auto* num = static_cast<const NumNode*>(expr.get());
        if (num->m_bIsFloat) return false;
        out = static_cast<long long>(num->m_fValue);
        return true;
    }

    // 形态二：一元负号套数字字面量（`a[-1]` 里的 -1 就是这个形态）
    if (expr->m_kind == ASTNodeKind::AST_NODE_UNOP && expr->m_children.size() == 1)
    {
        const auto* un = static_cast<const UnOpNode*>(expr.get());
        if (un->m_op == TokenType::TOKEN_MINUS && !un->m_bPostfix)
        {
            long long v = 0;
            if (FoldConstIndex(un->m_children[0], v)) { out = -v; return true; }
        }
    }
    return false;
}

void SemanticAnalyzer::CheckConstIndex(const VarEntry& ve, const std::string& szName,
                                       const ASTNodePtr& indexExpr, const LineInfo& loc)
{
    // 实型下标由调用点先报「array subscript must be an integer」，这里不再重复
    long long idx = 0;
    if (!FoldConstIndex(indexExpr, idx)) return;
    if (ve.m_iArraySize <= 0) return;

    if (idx < 0)
    {
        EmitError("array subscript " + std::to_string(idx) +
                  " is negative (array '" + szName + "' has " +
                  std::to_string(ve.m_iArraySize) + " element(s))", loc);
        return;
    }
    if (idx >= ve.m_iArraySize)
    {
        EmitError("array subscript " + std::to_string(idx) +
                  " is out of bounds (array '" + szName + "' has " +
                  std::to_string(ve.m_iArraySize) + " element(s))", loc);
    }
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
    if (u->m_op == TokenType::TOKEN_INC || u->m_op == TokenType::TOKEN_DEC)
    {
        // 自增自减：操作数必须是可修改左值，且为非 bool 的数值类型
        const ASTNode* operand = u->m_children[0].get();
        const ASTNodeKind k = operand ? operand->m_kind : ASTNodeKind::AST_NODE_NONE;
        if (k != ASTNodeKind::AST_NODE_IDENT && k != ASTNodeKind::AST_NODE_INDEX)
        {
            EmitError(std::string(u->m_op == TokenType::TOKEN_INC ? "'++'" : "'--'") +
                      " requires a modifiable lvalue", u->m_loc);
            return TokenType::TOKEN_NONE;
        }
        if (!IsNumeric(t))
        {
            EmitError(std::string(u->m_op == TokenType::TOKEN_INC ? "'++'" : "'--'") +
                      " requires a numeric operand", u->m_loc);
            return TokenType::TOKEN_NONE;
        }
        return t;   // 前缀与后缀的表达式类型相同（值不同，由中间代码区分）
    }
    return t;
}

TokenType SemanticAnalyzer::VisitCall(CallNode* call)
{
    if (!call || call->m_children.empty()) return TokenType::TOKEN_NONE;

    // children[0] = 被调标识符（返回类型即函数返回类型）
    TokenType ft = Visit(call->m_children[0]);

    // children[1..] = 实参，逐个求值（顺带检查未声明/类型）
    std::vector<TokenType> argTypes;
    for (std::size_t i = 1; i < call->m_children.size(); ++i)
        argTypes.push_back(Visit(call->m_children[i]));

    // ---- 实参个数检查（挑战版第 ⑥ 项）----
    // 只对被调者是「已声明的函数」生效：标识符可能解析到普通变量，
    // 那种情况 VisitIdent 已经报 undefined identifier 或直接拿到变量类型，
    // 拿不到签名就不做个数检查（宁可不报，也不报错人）。
    if (call->m_children[0] && call->m_children[0]->m_kind == ASTNodeKind::AST_NODE_IDENT)
    {
        auto* id = static_cast<IdentNode*>(call->m_children[0].get());
        const auto it = m_FuncSigs.find(id->m_szName);
        if (it != m_FuncSigs.end())
        {
            const std::size_t iWant = it->second.size();
            const std::size_t iGot  = call->m_children.size() - 1;
            if (iWant != iGot)
            {
                EmitError("function '" + id->m_szName + "' expects " +
                          std::to_string(iWant) + " argument(s), but " +
                          std::to_string(iGot) + " given", call->m_loc);
            }
            else
            {
                // 实参类型检查（2026-10-28 新增）。
                // 修复前 `int f(int a)` 传 float 会静默截断，编译器一声不吭。
                // 判为警告而非错误：int/float/char 之间的隐式转换是 C 的常规行为，
                // 只有 bool↔数值、void 实参这类才提示。
                for (std::size_t i = 0; i < iGot; ++i)
                {
                    const TokenType want = it->second[i];
                    const TokenType got  = argTypes[i];
                    if (got == TokenType::TOKEN_NONE || ArgCompatible(want, got))
                    {
                        continue;
                    }
                    EmitWarning("argument " + std::to_string(i + 1) + " of function '" +
                                id->m_szName + "' expects " + TokenTypeToString(want) +
                                ", but " + TokenTypeToString(got) + " given", call->m_loc);
                }
            }
        }
    }

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
        // 整型字面量越界检查（2026-10-28 新增）。
        // 阈值取 2147483648 而不是 2147483647：`-2147483648` 在 AST 里是
        // UnOp(-, Num(2147483648))，只看 Num 节点无法区分「INT_MIN 的绝对值」
        // 与「真的溢出」。宁可放过 2147483648 这一个值，也不误报 INT_MIN。
        if (num->m_fValue > 2147483648.0 || num->m_fValue < -2147483648.0)
        {
            char ob[64] = {0};
            std::snprintf(ob, sizeof(ob), "%.0f", num->m_fValue);
            EmitError(std::string("integer literal ") + ob +
                      " is out of range for int (-2147483648..2147483647)", num->m_loc);
        }
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
    // char 在算术与关系运算里按整数处理（C 语义的整型提升）。
    // 这一条必须和 IR 侧的 IsIntType(src/sema/ir.cpp) 保持一致：那边早就把
    // TOKEN_CHAR 算作整数，若这里只认 int/float，char 会出现「IR 能算、语义
    // 先报错」的两层口径分裂，实际表现就是 char 只能 print、不能参与运算。
    // bool 仍然排除：布尔不参与算术是 §8.1 类型表定下的规则。
    return t == TokenType::TOKEN_INT || t == TokenType::TOKEN_FLOAT ||
           t == TokenType::TOKEN_CHAR;
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
        // bool 在比较里按整型提升（同 C/C++）：使 a < b < c 这类链式比较可用。
        // 这里只放开比较，不放开算术 —— bool + 1 仍然报错（§8.1 类型表的约定）。
        const auto comparable = [this](TokenType t) {
            return IsNumeric(t) || t == TokenType::TOKEN_BOOL;
        };
        if (!comparable(lt) || !comparable(rt))
        {
            EmitError("relational operands must be numeric or bool", loc);
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

bool SemanticAnalyzer::HasReturnStmt(const ASTNodePtr& node)
{
    if (!node) return false;
    if (node->m_kind == ASTNodeKind::AST_NODE_RETURN) return true;
    // 不进嵌套函数定义：MiniC 语法本来就不允许函数嵌套，
    // 但这里显式挡住更稳妥（万一将来放开嵌套，也不会把内层的 return 算到外层头上）。
    if (node->m_kind == ASTNodeKind::AST_NODE_FUNCDEF) return false;
    for (const auto& c : node->m_children)
    {
        if (HasReturnStmt(c)) return true;
    }
    return false;
}