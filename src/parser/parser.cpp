// ============================================================================
// MiniC 编译器 —— LL(1) 预测分析驱动器实现
// 依据：《MiniC编译器设计文档 v4.0 挑战版 LL1》
//   · §4.1/§4.2  终结符 / 非终结符编码表
//   · §4.9       预测分析表 g_PredictTable（grammar.cpp 提供）
//   · §4.10      预测分析驱动器（分析栈 StackItem + 非递归算法 + 恐慌式恢复）
//   · §4.11      语法分析过程（--dump-parse 的 82 步 golden trace 格式）
//   · §7.2       CParser 类形状（公开签名冻结，私有实现由递归下降改为表驱动）
//   · §8         语义动作 semantic000..semantic027
//
// 驱动与语义的职责边界（与 §8 的“语法制导”一致）：
//   · 驱动器只在 §8 规定的位置“调度”语义动作（ExecuteAction），并记录轨迹；
//   · 动作本身（符号表登记/查重、四元式生成、作用域进出等）属于语义分析与中间
//     代码阶段，不在 Parser 模块内实现，避免越界修改其他模块；
//   · AST 的组装就是本阶段的语义动作落点：每个产生式右部扫描完毕（REDUCE 哨兵
//     浮到栈顶）时，用右部各符号的语义值组装出该非终结符的语义值。
// ============================================================================
#include "parser/parser.h"
#include "parser/grammar.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

// ----------------------------------------------------------------------------
// 编码 → 显示名（§4.1 / §4.2；与 §4.11 轨迹逐字一致）
// ----------------------------------------------------------------------------
struct CodeName
{
    int         m_iCode;
    const char* m_szName;
};

const CodeName kTermNames[] =
{
    {  1, "int"     }, {  2, "float"   }, {  3, "bool"    }, {  4, "char"    },
    {  5, "void"    }, {  6, "if"      }, {  7, "else"    }, {  8, "while"   },
    {  9, "print"   }, { 10, "return"  }, { 11, "true"    }, { 12, "false"   },
    { 21, "("       }, { 22, ")"       }, { 23, "{"       }, { 24, "}"       },
    { 25, "["       }, { 26, "]"       }, { 27, ";"       }, { 28, ","       },
    { 31, "+"       }, { 32, "-"       }, { 33, "*"       }, { 34, "/"       },
    { 35, "="       }, { 36, "=="      }, { 37, "!="      }, { 38, "<"       },
    { 39, "<="      }, { 40, ">"       }, { 41, ">="      }, { 42, "&&"      },
    { 43, "||"      }, { 44, "!"       },
    { 51, "标识符"  }, { 52, "整型常量" }, { 53, "实型常量" }, { 54, "字符常量" },
    { 99, "#"       },
};

const CodeName kNTermNames[] =
{
    { 100, "程序"       }, { 101, "声明部分"   }, { 102, "声明"       },
    { 103, "声明后续"   }, { 104, "变量声明尾" }, { 105, "变量声明尾1" },
    { 106, "数组后缀"   }, { 107, "类型"       }, { 108, "基本类型"   },
    { 109, "形参列表"   }, { 110, "形参列表1"  }, { 111, "形参"       },
    { 112, "语句部分"   }, { 113, "复合语句"   }, { 114, "块项序列"   },
    { 115, "块项"       }, { 116, "语句"       }, { 117, "标识符后续" },
    { 118, "赋值"       }, { 119, "条件语句"   }, { 120, "否则部分"   },
    { 121, "循环语句"   }, { 122, "输出语句"   }, { 123, "返回语句"   },
    { 124, "返回尾"     }, { 125, "表达式"     }, { 126, "或表达式"   },
    { 127, "或表达式1"  }, { 128, "与表达式"   }, { 129, "与表达式1"  },
    { 130, "关系表达式" }, { 131, "关系表达式1" }, { 132, "关系运算符" },
    { 133, "加法表达式" }, { 134, "加法表达式1" }, { 135, "加减运算符" },
    { 136, "乘法表达式" }, { 137, "乘法表达式1" }, { 138, "乘除运算符" },
    { 139, "一元表达式" }, { 140, "后缀表达式" }, { 141, "后缀表达式1" },
    { 142, "基本表达式" }, { 143, "实参列表"   }, { 144, "实参列表1"  },
};

const char* TermName(int iCode)
{
    for (const CodeName& e : kTermNames)
    {
        if (e.m_iCode == iCode) { return e.m_szName; }
    }
    return "?";
}

const char* NTermName(int iCode)
{
    for (const CodeName& e : kNTermNames)
    {
        if (e.m_iCode == iCode) { return e.m_szName; }
    }
    return "?";
}

// 符号名：非终结符编码 >= 100，终结符编码 < 100（§4.1/§4.2）
std::string SymbolName(int iCode)
{
    return (iCode >= NTERM_CODE_BASE) ? NTermName(iCode) : TermName(iCode);
}

// 语义动作名：semantic000 .. semantic027（§8）
std::string SemanticName(int iSemantic)
{
    std::string s = "semantic";
    if (iSemantic < 100) { s += "0"; }
    if (iSemantic < 10)  { s += "0"; }
    s += std::to_string(iSemantic);
    return s;
}

// 输入符号的报错显示：'内容' (词法类型)
std::string TokenText(const Token& t)
{
    return "'" + t.m_szContent + "' (" + TokenTypeToString(t.m_type) + ")";
}

// 期望终结符集合文本（取自 FIRST(非终结符)）
std::string FirstSetText(int iNTermCode)
{
    std::string out;
    const std::set<int>& first = g_FirstSet[iNTermCode - NTERM_CODE_BASE];
    for (int code : first)
    {
        if (code == 0) { continue; }
        if (!out.empty()) { out += " "; }
        out += TermName(code);
    }
    return out.empty() ? "(空)" : out;
}

// 产生式文本（§4.11 动作列）：LHS→RHS，ε 产生式写作 LHS→ε
std::string ProductionText(int iProdNum)
{
    const Production& P = g_Grammar[iProdNum];
    std::string out = NTermName(P.m_iLhs);
    out += "→";
    if (P.m_Rhs.empty())
    {
        out += "ε";
        return out;
    }
    for (std::size_t i = 0; i < P.m_Rhs.size(); ++i)
    {
        if (i > 0) { out += " "; }
        out += SymbolName(P.m_Rhs[i]);
    }
    return out;
}

// 同步符号集合（§4.10 恐慌式恢复）：; } ) + 语句/声明起始符号
bool IsSyncToken(int iCode)
{
    switch (iCode)
    {
    case 27:  // ;
    case 24:  // }
    case 22:  // )
    case  1:  // int
    case  2:  // float
    case  3:  // bool
    case  4:  // char
    case  5:  // void
    case  6:  // if
    case  8:  // while
    case  9:  // print
    case 10:  // return
    case 23:  // {
    case 51:  // 标识符
        return true;
    default:
        return false;
    }
}

// ----------------------------------------------------------------------------
// BridgeNode：阶段④的占位 / 扩展节点（v3.1 的 AST 节点集合只有 2 个子指针，
// 不足以表达数组、函数、调用、print、空语句等挑战版结构）
//   标签形如 Index / Call / FuncDef(INT f) / Param(INT a[10]) / Print / Empty，
//   阶段⑤（AST 多子节点重构）会替换为专用节点，本阶段只保证信息不丢失。
// ----------------------------------------------------------------------------
class BridgeNode : public ASTNode
{
public:
    BridgeNode(const std::string& label, const LineInfo& loc)
        : ASTNode(ASTNodeKind::AST_NODE_NONE, loc), m_szLabel(label)
    {
    }

    // m_left 为“待绑定基址”的空槽（下标链 / 调用名），由 语句→标识符 标识符后续 回填
    bool m_bOpenLeft = false;

    std::string m_szLabel;

    void Print(std::ostream& os, int indent) const override
    {
        for (int i = 0; i < indent; ++i) { os << "  "; }
        os << m_szLabel << "  @" << m_loc.m_iRow << ":" << m_loc.m_iCol << "\n";
        if (m_left)  { m_left->Print(os, indent + 1); }
        if (m_right) { m_right->Print(os, indent + 1); }
    }
};

// 把语义值列表串成 v3.1 的链表式 List（左=首元素，右=剩余序列）；空列表返回空指针
ASTNodePtr ChainNodes(std::vector<ASTNodePtr>& items)
{
    ASTNodePtr tail;
    for (std::size_t i = items.size(); i-- > 0;)
    {
        if (!items[i]) { continue; }
        ASTNodePtr cell = std::make_unique<ListNode>(items[i]->m_loc);
        cell->m_left  = std::move(items[i]);
        cell->m_right = std::move(tail);
        tail = std::move(cell);
    }
    return tail;
}

// 在“下标链 / 调用桥”上找到最内层的空基址槽并绑定标识符节点
void AttachBase(ASTNodePtr& root, ASTNodePtr base)
{
    ASTNodePtr* slot = &root;
    while (*slot)
    {
        BridgeNode* br = dynamic_cast<BridgeNode*>(slot->get());
        if (!br || !br->m_bOpenLeft) { break; }
        if (!br->m_left)
        {
            br->m_left = std::move(base);
            return;
        }
        slot = &br->m_left;
    }
    if (!*slot) { *slot = std::move(base); }
}

} // namespace

// ============================================================================
// 构造 / 公开接口
// ============================================================================
CParser::CParser(const TokenList& tokens)
    : m_pTokenList(&tokens),
      m_iListPos(0),
      m_bAborted(false),
      m_ProgramLoc(1, 1),
      m_iLastErrorPos(static_cast<std::size_t>(-1)),
      m_iLastErrorDepth(static_cast<std::size_t>(-1))
{
}

bool CParser::HasError() const
{
    return !m_errors.empty();
}

const std::vector<CompileError>& CParser::GetErrors() const
{
    return m_errors;
}

const std::vector<ParseStep>& CParser::GetTrace() const
{
    return m_trace;
}

// ============================================================================
// 输入 / 栈访问辅助
// ============================================================================
const Token& CParser::Cur() const
{
    static const Token kEofToken(TokenType::TOKEN_EOF, "<eof>", 0, 0);
    if (!m_pTokenList || m_iListPos >= m_pTokenList->size()) { return kEofToken; }
    return (*m_pTokenList)[m_iListPos];
}

int CParser::CurCode() const
{
    return TokenTypeToCode(Cur().m_type);
}

bool CParser::AtInputEnd() const
{
    return !m_pTokenList || m_iListPos + 1 >= m_pTokenList->size();
}

bool CParser::NextToken()
{
    if (!m_pTokenList || m_iListPos + 1 >= m_pTokenList->size())
    {
        return false;   // 已到末尾（正常为 TOKEN_EOF），停在原地
    }
    ++m_iListPos;
    return true;
}

// ============================================================================
// 错误与轨迹
// ============================================================================
void CParser::ReportError(const std::string& msg, const LineInfo& loc)
{
    EmitError(msg, loc);
    CompileError e;
    e.m_szMsg = msg;
    e.m_loc   = loc;
    m_errors.push_back(e);
}

void CParser::ReportError(const std::string& msg)
{
    ReportError(msg, Cur().m_line);
}

void CParser::RecordStep(const std::string& action)
{
    ParseStep step;
    step.m_iStep    = static_cast<int>(m_trace.size()) + 1;
    step.m_szStack  = StackText();
    step.m_szInput  = InputText();
    step.m_szAction = action;
    m_trace.push_back(std::move(step));
}

std::string CParser::StackText() const
{
    std::string out;
    for (std::size_t i = m_SymbolStack.size(); i-- > 0;)
    {
        const StackItem& it = m_SymbolStack[i];
        if (it.m_kind == StackItem::REDUCE) { continue; }   // 内部哨兵，不入轨迹
        if (!out.empty()) { out += " "; }
        out += (it.m_kind == StackItem::ACTION) ? SemanticName(it.m_iSemantic)
                                                : SymbolName(it.m_iCode);
    }
    return out;
}

std::string CParser::InputText() const
{
    if (!m_pTokenList || m_pTokenList->empty())
    {
        return "#";
    }
    std::string out;
    for (std::size_t i = m_iListPos; i < m_pTokenList->size(); ++i)
    {
        if (!out.empty()) { out += " "; }
        out += TermName(TokenTypeToCode((*m_pTokenList)[i].m_type));
    }
    return out.empty() ? "#" : out;
}

void CParser::HandleError(const std::string& msg)
{
    const bool bNoProgress = (m_iListPos == m_iLastErrorPos) &&
                             (m_SymbolStack.size() == m_iLastErrorDepth);
    if (!bNoProgress)
    {
        ReportError(msg);
    }
    else
    {
        // 同一位置 + 同一栈深再次报错：抑制重复消息，强制前进以保证终止
        if (!NextToken()) { m_bAborted = true; }
    }
    m_iLastErrorPos   = m_iListPos;
    m_iLastErrorDepth = m_SymbolStack.size();
    Synchronize();
}

// ============================================================================
// §4.10 恐慌式错误恢复
//   1) 丢弃输入符号直到同步符号集合（; } ) 或语句/声明起始符号）；
//   2) 弹分析栈直到栈顶符号能在当前输入符号下继续（表项非空 / 终结符匹配 /
//      非终结符的后继集包含当前符号则丢弃它），并同步截断语义值栈。
// ============================================================================
void CParser::Synchronize()
{
    while (CurCode() != TERM_CODE_EOF && !AtInputEnd() && !IsSyncToken(CurCode()))
    {
        if (!NextToken()) { break; }
    }

    while (!m_SymbolStack.empty())
    {
        const StackItem& X = m_SymbolStack.back();

        if (X.m_kind == StackItem::REDUCE || X.m_kind == StackItem::ACTION)
        {
            TruncateValues(X.m_iValueBase);
            m_SymbolStack.pop_back();
            continue;
        }

        if (X.m_kind == StackItem::TERM)
        {
            const int aCode = CurCode();
            if (X.m_iCode == aCode) { return; }
            if (X.m_iCode == TERM_CODE_EOF)
            {
                // 栈底 '#'：只能靠丢弃输入符号恢复
                while (CurCode() != TERM_CODE_EOF && NextToken()) { }
                return;
            }
            TruncateValues(X.m_iValueBase);
            m_SymbolStack.pop_back();
            continue;
        }

        // NTERM：表项非空即可继续；否则若当前符号属于它，则认为该非终结符被跳过
        const int prod = g_PredictTable[X.m_iCode - NTERM_CODE_BASE][CurCode()];
        if (prod > 0) { return; }
        const std::set<int>& follow = g_FollowSet[X.m_iCode - NTERM_CODE_BASE];
        TruncateValues(X.m_iValueBase);
        m_SymbolStack.pop_back();
        if (follow.count(CurCode()) > 0) { return; }
    }
}

void CParser::TruncateValues(std::size_t iBase)
{
    if (m_ValueStack.size() > iBase)
    {
        m_ValueStack.resize(iBase);
    }
}

// ============================================================================
// §4.10 按产生式压栈：右部符号逆序压栈，语义动作按其位置（§8 的 sem@pos）入栈，
//   并在最深处压入 REDUCE 哨兵标记本例右部结束。
//   例（产生式 1：程序 → s000 声明部分 语句部分）：
//     线性序列 L = [ACTION0, 声明部分, 语句部分, REDUCE]
//     逆序压栈后栈顶为 ACTION0，故紧接着执行 semantic000（与 §4.11 第 2 步一致）。
// ============================================================================
void CParser::PushProduction(int iProdNum)
{
    const Production& P = g_Grammar[iProdNum];
    const std::size_t iBase = m_ValueStack.size();

    std::vector<StackItem> items;
    items.reserve(P.m_Rhs.size() + P.m_SemAct.size() + 1);

    // 线性序列 L（元素按执行顺序排列：动作/右部符号交替，REDUCE 收尾）
    std::size_t iSem = 0;
    for (std::size_t pos = 0; pos <= P.m_Rhs.size(); ++pos)
    {
        while (iSem < P.m_SemPos.size() &&
               static_cast<std::size_t>(P.m_SemPos[iSem]) == pos)
        {
            StackItem act;
            act.m_kind       = StackItem::ACTION;
            act.m_iCode      = 0;
            act.m_iSemantic  = P.m_SemAct[iSem];
            act.m_iValueBase = iBase;
            items.push_back(act);
            ++iSem;
        }
        if (pos < P.m_Rhs.size())
        {
            StackItem sym;
            sym.m_kind       = (P.m_Rhs[pos] < NTERM_CODE_BASE) ? StackItem::TERM
                                                                : StackItem::NTERM;
            sym.m_iCode      = P.m_Rhs[pos];
            sym.m_iSemantic  = -1;
            sym.m_iValueBase = iBase;
            items.push_back(sym);
        }
    }

    // REDUCE 哨兵是线性序列的末元素 → 逆序压栈后位于栈底最深处（右部全部扫描完才归约）
    StackItem reduce;
    reduce.m_kind       = StackItem::REDUCE;
    reduce.m_iCode      = iProdNum;
    reduce.m_iSemantic  = -1;
    reduce.m_iValueBase = iBase;
    items.push_back(reduce);

    // 逆序压栈：栈顶 = L[0]（先执行），栈底 = REDUCE
    for (std::size_t i = items.size(); i-- > 0;)
    {
        m_SymbolStack.push_back(items[i]);
    }
}

// ============================================================================
// §8 语义动作调度
//   本模块只做“动作登记 + Parser 侧可完成的部分”（semantic000 记录程序起始位置）；
//   符号表登记/查重、四元式生成、作用域进出等属语义分析与中间代码阶段（阶段⑥⑦），
//   在此登记位置与职责，不越界实现。
// ============================================================================
bool CParser::ExecuteAction(int iSemantic)
{
    switch (iSemantic)
    {
    case 0:   // semantic000 初始化：清空操作数/操作符/类型位置栈，SerialId=0，建立空 IR 指令集
        m_ProgramLoc = Cur().m_line;   // Parser 侧：记录程序起始位置（AST 根节点用）
        break;
    case 1:   // semantic001 登记基本类型到类型信息表（int/float/bool/char）
        break;
    case 2:   // semantic002 登记变量：类型表/常量表/变量表查重，无重名则 AddVar
        break;
    case 3:   // semantic003 登记常量到常量信息表（整型/实型/字符/布尔）
        break;
    case 4:   // semantic004 数组声明：登记维度上界，计算元素个数与字节大小
        break;
    case 5:   // semantic005 类型转换：按类型系统表插入 IntToFloat / ToBool
        break;
    case 6:   // semantic006 生成赋值四元式 (=, 值, -, 目标变量)
        break;
    case 7:   // semantic007 生成算术四元式 (+ - * /)，结果存新临时变量
        break;
    case 8:   // semantic008 生成关系四元式 (> < == != >= <=)，结果类型 T_BOOL
        break;
    case 9:   // semantic009 生成逻辑四元式 (&& ||)，含短路求值与标号回填
        break;
    case 10:  // semantic010 一元取负：生成取负四元式
        break;
    case 11:  // semantic011 逻辑非：生成取反四元式，结果类型 T_BOOL
        break;
    case 12:  // semantic012 if 假出口：生成 JF 四元式并记录表达式结果变量
        break;
    case 13:  // semantic013 else 真出口：生成 JMP 四元式，回填假出口标号
        break;
    case 14:  // semantic014 if 语句结束：回填真/假出口标号
        break;
    case 15:  // semantic015 while 循环入口：记录入口标号，生成 JF 假出口四元式
        break;
    case 16:  // semantic016 while 回跳：生成 JMP 到入口标号，回填假出口标号
        break;
    case 17:  // semantic017 数组下标：生成地址偏移四元式（基址 + 下标 × 元素宽度）
        break;
    case 18:  // semantic018 函数定义入口：建立过程表项，进入函数作用域
        break;
    case 19:  // semantic019 形参登记：登记到参数表与变量表，记录参数序号
        break;
    case 20:  // semantic020 函数定义结束：退出作用域，IR 指令集挂到过程表项
        break;
    case 21:  // semantic021 函数调用：生成 CALL 四元式，绑定返回值临时变量
        break;
    case 22:  // semantic022 实参求值：生成传参四元式 PARA
        break;
    case 23:  // semantic023 return 语句：生成 RET 四元式（含返回值）
        break;
    case 24:  // semantic024 print 语句：生成 PRINT 四元式
        break;
    case 25:  // semantic025 复合语句开始：EnterScope，创建块作用域
        break;
    case 26:  // semantic026 复合语句结束：ExitScope，销毁块作用域局部符号
        break;
    case 27:  // semantic027 空语句：无动作
        break;
    default:
        ReportError("内部错误：未知语义动作编号 " + std::to_string(iSemantic));
        return false;
    }
    return true;
}

// ============================================================================
// 归约：右部扫描完毕后组装该非终结符的语义值
// ============================================================================
void CParser::ReduceProduction(int iProdNum, std::size_t iValueBase)
{
    const Production& P = g_Grammar[iProdNum];

    std::vector<SemValue> v;
    for (std::size_t i = iValueBase; i < m_ValueStack.size(); ++i)
    {
        v.push_back(std::move(m_ValueStack[i]));
    }
    m_ValueStack.resize(iValueBase);
    if (v.size() != P.m_Rhs.size())
    {
        v.resize(P.m_Rhs.size());   // 错误恢复后可能缺值：补 NONE，保持两栈同步
    }

    auto Node = [](ASTNodePtr n, const LineInfo& loc)
    {
        SemValue s;
        s.m_kind = SemKind::NODE;
        s.m_node = std::move(n);
        s.m_loc  = loc;
        return s;
    };
    auto Items = [](std::vector<ASTNodePtr> it, const LineInfo& loc)
    {
        SemValue s;
        s.m_kind  = SemKind::LIST;
        s.m_items = std::move(it);
        s.m_loc   = loc;
        return s;
    };
    auto AddItem = [](std::vector<ASTNodePtr>& vec, ASTNodePtr n)
    {
        if (n) { vec.push_back(std::move(n)); }
    };
    // ε 产生式没有右部语义值，位置回落到当前输入符号（§4.11 中 ε 展开不改变输入）
    auto Loc0 = [&]() { return v.empty() ? Cur().m_line : v[0].m_loc; };

    SemValue out;

    switch (iProdNum)
    {
    // ---------- 程序 / 声明列表 ----------
    case 1:    // 程序 → 声明部分 语句部分          | sem000@0
    {
        ASTNodePtr root = std::make_unique<ListNode>(m_ProgramLoc);
        root->m_left  = ChainNodes(v[0].m_items);
        root->m_right = ChainNodes(v[1].m_items);
        out = Node(std::move(root), m_ProgramLoc);
        break;
    }
    case 2:    // 声明部分 → 声明 声明部分
    {
        std::vector<ASTNodePtr> items;
        for (auto& d : v[0].m_items) { AddItem(items, std::move(d)); }
        for (auto& d : v[1].m_items) { AddItem(items, std::move(d)); }
        out = Items(std::move(items), Loc0());
        break;
    }
    case 3:    // 声明部分 → ε
        out = Items({}, Loc0());
        break;

    case 4:    // 声明 → 类型 标识符 声明后续
    {
        const TokenType type = v[0].m_type;
        const Token&    name = v[1].m_token;
        std::vector<ASTNodePtr> decls;

        if (v[2].m_kind == SemKind::DECLS)
        {
            // 变量声明：int a; / int a, b; / int a[10];
            // 数组声明（§8 semantic004：登记维度上界）在 v3.1 二叉树 AST 中无对应节点，
            // 故用 BridgeNode 承载：子节点 = Decl(类型 名) + Num(维度)，阶段⑤ 合并进 AST。
            auto addVar = [&](const std::string& szName, int iSize, ASTNodePtr dim,
                              const LineInfo& loc)
            {
                if (iSize > 0)
                {
                    std::string label = "ArrayDecl(" + std::string(TokenTypeToString(type)) +
                                        " " + szName + "[" + std::to_string(iSize) + "])";
                    auto arr = std::make_unique<BridgeNode>(label, loc);
                    arr->m_left  = std::make_unique<DeclNode>(type, szName, loc);
                    arr->m_right = std::move(dim);
                    decls.push_back(std::move(arr));
                }
                else
                {
                    decls.push_back(std::make_unique<DeclNode>(type, szName, loc));
                }
            };
            addVar(name.m_szContent, v[2].m_iInt, std::move(v[2].m_dim), Loc0());  // 首声明符沿用类型所在位置（与 v3.1 一致）
            for (DeclItem& d : v[2].m_decls)
            {
                addVar(d.m_szName, d.m_iArraySize, std::move(d.m_dim), d.m_loc);
            }
        }
        else if (v[2].m_node)
        {
            // 函数定义：声明后续 → ( 形参列表 ) 块
            BridgeNode* fn = dynamic_cast<BridgeNode*>(v[2].m_node.get());
            if (fn)
            {
                fn->m_szLabel = "FuncDef(" + std::string(TokenTypeToString(type)) +
                                " " + name.m_szContent + ")";
                fn->m_loc = Loc0();
            }
            decls.push_back(std::move(v[2].m_node));
        }
        out = Items(std::move(decls), Loc0());
        break;
    }
    case 5:    // 声明后续 → 变量声明尾 ;          | sem002@2
        out = std::move(v[0]);   // DECLS 透传
        break;
    case 6:    // 声明后续 → ( 形参列表 ) 块        | sem018@3, sem020@4
    {
        ASTNodePtr fn = std::make_unique<BridgeNode>("FuncDef", Loc0());
        fn->m_left  = ChainNodes(v[1].m_items);   // 形参链
        fn->m_right = std::move(v[3].m_node);     // 函数体
        out = Node(std::move(fn), Loc0());
        break;
    }
    case 7:    // 变量声明尾 → 数组后缀 变量声明尾1
    {
        out.m_kind  = SemKind::DECLS;
        out.m_iInt  = v[0].m_iInt;
        out.m_dim   = std::move(v[0].m_dim);
        out.m_decls = std::move(v[1].m_decls);
        out.m_loc   = Loc0();
        break;
    }
    case 8:    // 变量声明尾1 → , 标识符 数组后缀 变量声明尾1   | sem002@3
    {
        DeclItem item;
        item.m_szName    = v[1].m_token.m_szContent;
        item.m_iArraySize = v[2].m_iInt;
        item.m_dim       = std::move(v[2].m_dim);
        item.m_loc       = v[1].m_loc;
        out.m_kind = SemKind::DECLS;
        out.m_decls.push_back(std::move(item));
        for (DeclItem& d : v[3].m_decls) { out.m_decls.push_back(std::move(d)); }
        out.m_loc = v[1].m_loc;
        break;
    }
    case 9:    // 变量声明尾1 → ε
        out.m_kind = SemKind::DECLS;
        out.m_loc  = Loc0();
        break;
    case 10:   // 数组后缀 → [ 整型常量 ]          | sem004@3
        out.m_kind = SemKind::INT;
        out.m_iInt = static_cast<int>(v[1].m_token.m_iVal);
        // 维度常量同时保留为 Num 节点（透传词法常量表下标），供语义阶段 semantic004 取上界
        out.m_dim  = std::make_unique<NumNode>(static_cast<double>(v[1].m_token.m_iVal), false,
                                               Loc0(), v[1].m_token.m_iConstIdx);
        out.m_loc  = Loc0();
        break;
    case 11:   // 数组后缀 → ε
        out.m_kind = SemKind::INT;
        out.m_loc  = Loc0();
        break;

    // ---------- 类型 ----------
    case 12:   // 类型 → 基本类型
        out = std::move(v[0]);
        break;
    case 13:   // 类型 → void
        out.m_kind = SemKind::TYPE;
        out.m_type = TokenType::TOKEN_VOID;
        out.m_loc  = Loc0();
        break;
    case 14: case 15: case 16: case 17:   // 基本类型 → int/float/bool/char | sem001@1
        out.m_kind = SemKind::TYPE;
        out.m_type = v[0].m_token.m_type;
        out.m_loc  = Loc0();
        break;

    // ---------- 形参 ----------
    case 18:   // 形参列表 → 形参 形参列表1
    {
        std::vector<ASTNodePtr> items;
        AddItem(items, std::move(v[0].m_node));
        for (auto& p : v[1].m_items) { AddItem(items, std::move(p)); }
        out = Items(std::move(items), Loc0());
        break;
    }
    case 19:   // 形参列表 → ε
        out = Items({}, Loc0());
        break;
    case 20:   // 形参列表1 → , 形参 形参列表1
    {
        std::vector<ASTNodePtr> items;
        AddItem(items, std::move(v[1].m_node));
        for (auto& p : v[2].m_items) { AddItem(items, std::move(p)); }
        out = Items(std::move(items), v[1].m_loc);
        break;
    }
    case 21:   // 形参列表1 → ε
        out = Items({}, Loc0());
        break;
    case 22:   // 形参 → 类型 标识符 数组后缀      | sem019@3
    {
        std::string label = "Param(" + std::string(TokenTypeToString(v[0].m_type)) +
                            " " + v[1].m_token.m_szContent;
        if (v[2].m_iInt > 0) { label += "[" + std::to_string(v[2].m_iInt) + "]"; }
        label += ")";
        out = Node(std::make_unique<BridgeNode>(label, v[1].m_loc), v[1].m_loc);
        break;
    }

    // ---------- 语句序列 / 复合语句 ----------
    case 23:   // 语句部分 → 语句 语句部分
    {
        std::vector<ASTNodePtr> items;
        AddItem(items, std::move(v[0].m_node));
        for (auto& s : v[1].m_items) { AddItem(items, std::move(s)); }
        out = Items(std::move(items), Loc0());
        break;
    }
    case 24:   // 语句部分 → ε
        out = Items({}, Loc0());
        break;
    case 25:   // 复合语句 → { 块项序列 }           | sem025@1, sem026@3
    {
        ASTNodePtr block = std::make_unique<ListNode>(Loc0());
        block->m_left = ChainNodes(v[1].m_items);
        out = Node(std::move(block), Loc0());
        break;
    }
    case 26:   // 块项序列 → 块项 块项序列
    {
        std::vector<ASTNodePtr> items;
        for (auto& b : v[0].m_items) { AddItem(items, std::move(b)); }
        for (auto& b : v[1].m_items) { AddItem(items, std::move(b)); }
        out = Items(std::move(items), Loc0());
        break;
    }
    case 27:   // 块项序列 → ε
        out = Items({}, Loc0());
        break;
    case 28:   // 块项 → 声明
        out = Items(std::move(v[0].m_items), Loc0());
        break;
    case 29:   // 块项 → 语句
    {
        std::vector<ASTNodePtr> items;
        AddItem(items, std::move(v[0].m_node));
        out = Items(std::move(items), Loc0());
        break;
    }

    // ---------- 语句 ----------
    case 30:   // 语句 → 标识符 标识符后续
    {
        const std::string  szName = v[0].m_token.m_szContent;
        const LineInfo     loc    = Loc0();
        ASTNodePtr suffix = std::move(v[1].m_node);
        if (suffix)
        {
            if (suffix->m_kind == ASTNodeKind::AST_NODE_ASSIGN)
            {
                AttachBase(suffix->m_left, std::make_unique<IdentNode>(szName, loc));
            }
            else
            {
                AttachBase(suffix, std::make_unique<IdentNode>(szName, loc));
            }
            suffix->m_loc = loc;   // 语句位置以左值标识符为准（与 v3.1 AST 一致）
        }
        out = Node(std::move(suffix), loc);
        break;
    }
    case 31: case 32: case 33: case 34: case 35:   // 语句 → 条件/循环/输出/返回/复合语句
        out = std::move(v[0]);
        break;
    case 36:   // 语句 → ;                        | sem027@1
        out = Node(std::make_unique<BridgeNode>("Empty", Loc0()), Loc0());
        break;
    case 37:   // 标识符后续 → = 表达式 ;           | sem006@3
    case 40:   // 赋值 → = 表达式 ;                | sem006@3
        out = Node(std::make_unique<AssignNode>(nullptr, std::move(v[1].m_node), Loc0()),
                   Loc0());
        break;
    case 38:   // 标识符后续 → [ 表达式 ] 赋值      | sem017@3
    {
        ASTNodePtr assign = std::move(v[3].m_node);
        if (assign)
        {
            BridgeNode* idx = new BridgeNode("Index", Loc0());
            idx->m_bOpenLeft = true;                 // m_left 待 语句 绑定标识符
            idx->m_right     = std::move(v[1].m_node);
            assign->m_left.reset(idx);
        }
        out = Node(std::move(assign), v[1].m_loc);
        break;
    }
    case 39:   // 标识符后续 → ( 实参列表 ) ;       | sem021@4
    {
        BridgeNode* call = new BridgeNode("Call", Loc0());
        call->m_bOpenLeft = true;                    // m_left 待 语句 绑定被调名
        call->m_right     = ChainNodes(v[1].m_items);
        out = Node(ASTNodePtr(call), Loc0());
        break;
    }
    case 41:   // 条件语句 → if ( 表达式 ) 语句 否则部分 | sem012@4, sem014@6
    {
        ASTNodePtr node = std::make_unique<IfNode>(std::move(v[2].m_node),
                                                   std::move(v[4].m_node),
                                                   std::move(v[5].m_node),
                                                   Loc0());
        out = Node(std::move(node), Loc0());
        break;
    }
    case 42:   // 否则部分 → else 语句             | sem013@1
        out = std::move(v[1]);   // 有 else：语义值为 else 分支语句
        break;
    case 43:   // 否则部分 → ε
        out = Node(nullptr, Loc0());
        break;
    case 44:   // 循环语句 → while ( 表达式 ) 语句   | sem015@4, sem016@5
    {
        ASTNodePtr node = std::make_unique<WhileNode>(std::move(v[2].m_node),
                                                      std::move(v[4].m_node),
                                                      Loc0());
        out = Node(std::move(node), Loc0());
        break;
    }
    case 45:   // 输出语句 → print ( 表达式 ) ;     | sem024@5
    {
        BridgeNode* node = new BridgeNode("Print", Loc0());
        node->m_left = std::move(v[2].m_node);
        out = Node(ASTNodePtr(node), Loc0());
        break;
    }
    case 46:   // 返回语句 → return 返回尾
    {
        ASTNodePtr node = std::make_unique<ReturnNode>(std::move(v[1].m_node), Loc0());
        out = Node(std::move(node), Loc0());
        break;
    }
    case 47:   // 返回尾 → 表达式 ;                | sem023@2
        out = Node(std::move(v[0].m_node), Loc0());
        break;
    case 48:   // 返回尾 → ;                      | sem023@1
        out = Node(nullptr, Loc0());
        break;

    // ---------- 表达式（优先级 8 层 + 右递归尾） ----------
    case 49:   // 表达式 → 或表达式
        out = std::move(v[0]);
        break;
    case 50:   // 或表达式 → 与表达式 或表达式1
    case 53:   // 与表达式 → 关系表达式 与表达式1
    case 56:   // 关系表达式 → 加法表达式 关系表达式1
    case 65:   // 加法表达式 → 乘法表达式 加法表达式1
    case 70:   // 乘法表达式 → 一元表达式 乘法表达式1
    {
        // 右递归尾改写为“悬挂二元运算列表”，此处自左向右折叠 → 恢复左结合语义
        ASTNodePtr result = std::move(v[0].m_node);
        for (PendingOp& p : v[1].m_pairs)
        {
            result = std::make_unique<BinOpNode>(p.m_op, std::move(result),
                                                 std::move(p.m_operand), p.m_loc);
        }
        out = Node(std::move(result), Loc0());
        break;
    }
    case 51:   // 或表达式1 → || 与表达式 或表达式1  | sem009@2
    case 54:   // 与表达式1 → && 关系表达式 与表达式1 | sem009@2
    case 57:   // 关系表达式1 → 关系运算符 加法表达式 关系表达式1 | sem008@2
    case 66:   // 加法表达式1 → 加减运算符 乘法表达式 加法表达式1 | sem007@2
    case 71:   // 乘法表达式1 → 乘除运算符 一元表达式 乘法表达式1 | sem007@2
    {
        PendingOp p;
        p.m_op      = v[0].m_token.m_type;
        p.m_operand = std::move(v[1].m_node);
        p.m_loc     = Loc0();
        out.m_kind = SemKind::PAIRS;
        out.m_pairs.push_back(std::move(p));
        for (PendingOp& q : v[2].m_pairs) { out.m_pairs.push_back(std::move(q)); }
        out.m_loc = Loc0();
        break;
    }
    case 52: case 55: case 58: case 67: case 72:   // 各右递归尾 → ε
        out.m_kind = SemKind::PAIRS;
        out.m_loc  = Loc0();
        break;
    case 59: case 60: case 61: case 62: case 63: case 64:   // 关系运算符 → == != < <= > >=
    case 68: case 69:                                       // 加减运算符 → + -
    case 73: case 74:                                       // 乘除运算符 → * /
        out = std::move(v[0]);
        break;
    case 75:   // 一元表达式 → ! 一元表达式        | sem011@2
    case 76:   // 一元表达式 → - 一元表达式        | sem010@2
    {
        ASTNodePtr node = std::make_unique<UnaryOpNode>(v[0].m_token.m_type,
                                                        std::move(v[1].m_node),
                                                        Loc0());
        out = Node(std::move(node), Loc0());
        break;
    }
    case 77:   // 一元表达式 → 后缀表达式
        out = std::move(v[0]);
        break;
    case 78:   // 后缀表达式 → 基本表达式 后缀表达式1
    {
        ASTNodePtr cur = std::move(v[0].m_node);
        for (SuffixItem& s : v[1].m_suffixes)
        {
            BridgeNode* br = new BridgeNode(s.m_bCall ? "Call" : "Index", s.m_loc);
            br->m_right = std::move(s.m_operand);
            br->m_left  = std::move(cur);
            cur = ASTNodePtr(br);
        }
        out = Node(std::move(cur), Loc0());
        break;
    }
    case 79:   // 后缀表达式1 → [ 表达式 ] 后缀表达式1 | sem017@3
    {
        SuffixItem s;
        s.m_bCall   = false;
        s.m_operand = std::move(v[1].m_node);
        s.m_loc     = Loc0();
        out.m_kind = SemKind::SUFFIX;
        out.m_suffixes.push_back(std::move(s));
        for (SuffixItem& q : v[3].m_suffixes) { out.m_suffixes.push_back(std::move(q)); }
        out.m_loc = Loc0();
        break;
    }
    case 80:   // 后缀表达式1 → ( 实参列表 ) 后缀表达式1 | sem021@3
    {
        SuffixItem s;
        s.m_bCall   = true;
        s.m_operand = ChainNodes(v[1].m_items);
        s.m_loc     = Loc0();
        out.m_kind = SemKind::SUFFIX;
        out.m_suffixes.push_back(std::move(s));
        for (SuffixItem& q : v[3].m_suffixes) { out.m_suffixes.push_back(std::move(q)); }
        out.m_loc = Loc0();
        break;
    }
    case 81:   // 后缀表达式1 → ε
        out.m_kind = SemKind::SUFFIX;
        out.m_loc  = Loc0();
        break;
    case 82:   // 基本表达式 → 标识符
        out = Node(std::make_unique<IdentNode>(v[0].m_token.m_szContent, Loc0()),
                   Loc0());
        break;
    case 83:   // 基本表达式 → 整型常量           | sem003@1
        out = Node(std::make_unique<NumNode>(static_cast<double>(v[0].m_token.m_iVal), false,
                                             Loc0(), v[0].m_token.m_iConstIdx),
                   Loc0());
        break;
    case 84:   // 基本表达式 → 实型常量           | sem003@1
        out = Node(std::make_unique<NumNode>(v[0].m_token.m_fVal, true,
                                             Loc0(), v[0].m_token.m_iConstIdx),
                   Loc0());
        break;
    case 85:   // 基本表达式 → 字符常量           | sem003@1
        out = Node(std::make_unique<NumNode>(static_cast<double>(v[0].m_token.m_cVal), false,
                                             Loc0(), v[0].m_token.m_iConstIdx),
                   Loc0());
        break;
    case 86:   // 基本表达式 → true               | sem003@1
        out = Node(std::make_unique<NumNode>(1.0, false, Loc0(),
                                             v[0].m_token.m_iConstIdx),
                   Loc0());
        break;
    case 87:   // 基本表达式 → false              | sem003@1
        out = Node(std::make_unique<NumNode>(0.0, false, Loc0(),
                                             v[0].m_token.m_iConstIdx),
                   Loc0());
        break;
    case 88:   // 基本表达式 → ( 表达式 )
        out = std::move(v[1]);
        break;
    case 89:   // 实参列表 → 表达式 实参列表1       | sem022@1
    {
        std::vector<ASTNodePtr> items;
        AddItem(items, std::move(v[0].m_node));
        for (auto& a : v[1].m_items) { AddItem(items, std::move(a)); }
        out = Items(std::move(items), Loc0());
        break;
    }
    case 90:   // 实参列表 → ε
        out = Items({}, Loc0());
        break;
    case 91:   // 实参列表1 → , 表达式 实参列表1    | sem022@2
    {
        std::vector<ASTNodePtr> items;
        AddItem(items, std::move(v[1].m_node));
        for (auto& a : v[2].m_items) { AddItem(items, std::move(a)); }
        out = Items(std::move(items), v[1].m_loc);
        break;
    }
    case 92:   // 实参列表1 → ε
        out = Items({}, Loc0());
        break;

    default:
        out.m_kind = SemKind::NONE;
        break;
    }

    m_ValueStack.push_back(std::move(out));
}

// ============================================================================
// §4.10 预测分析主循环（非递归，显式栈）
//   初始化：栈 ← [#, 程序];  ip ← 0
//   循环直到栈空：
//     X ← 栈顶;  a ← 当前输入符号
//     · 语义动作：执行并弹栈（轨迹显示弹栈后的栈，与 §4.11 第 2/8/17 步一致）
//     · 终结符  ：X == a 则弹栈并前进；否则报错并恐慌式恢复
//     · 非终结符：M[X, a] 为空则报错并恢复；否则弹栈并把产生式右部逆序压栈
//   接受：栈空（此时输入已到达 #）
// ============================================================================
ASTNodePtr CParser::Parse()
{
    m_SymbolStack.clear();
    m_ValueStack.clear();
    m_trace.clear();
    m_errors.clear();
    m_Root.reset();
    m_bAborted = false;
    m_iListPos = 0;
    m_ProgramLoc = LineInfo(1, 1);
    m_iLastErrorPos   = static_cast<std::size_t>(-1);
    m_iLastErrorDepth = static_cast<std::size_t>(-1);

    if (!m_pTokenList || m_pTokenList->empty())
    {
        ReportError("empty token stream (lexer produced nothing)", LineInfo(1, 1));
        return nullptr;
    }

    // 栈 ← [#, 程序]
    StackItem hash;
    hash.m_kind       = StackItem::TERM;
    hash.m_iCode      = TERM_CODE_EOF;
    hash.m_iSemantic  = -1;
    hash.m_iValueBase = 0;
    m_SymbolStack.push_back(hash);

    StackItem start;
    start.m_kind       = StackItem::NTERM;
    start.m_iCode      = 100;   // 程序
    start.m_iSemantic  = -1;
    start.m_iValueBase = 0;
    m_SymbolStack.push_back(start);

    const std::size_t kMaxSteps = 1000000;   // 防御性上限（正常输入远小于此）

    while (!m_SymbolStack.empty())
    {
        if (m_trace.size() > kMaxSteps)
        {
            ReportError("parser step limit exceeded (internal error)");
            break;
        }

        const StackItem X = m_SymbolStack.back();

        // ---------- 语义动作 ----------
        if (X.m_kind == StackItem::ACTION)
        {
            m_SymbolStack.pop_back();
            const bool ok = ExecuteAction(X.m_iSemantic);
            RecordStep("执行语义动作 " + SemanticName(X.m_iSemantic));
            if (!ok) { break; }
            continue;
        }

        // ---------- 归约（右部结束哨兵） ----------
        if (X.m_kind == StackItem::REDUCE)
        {
            m_SymbolStack.pop_back();
            ReduceProduction(X.m_iCode, X.m_iValueBase);
            continue;
        }

        const int aCode = CurCode();

        // ---------- 终结符 ----------
        if (X.m_kind == StackItem::TERM)
        {
            if (X.m_iCode == aCode)
            {
                const bool bEnd = (aCode == TERM_CODE_EOF);
                RecordStep(TermName(aCode) + std::string(" 匹配") +
                           (bEnd ? "，接受原符号串" : ""));
                m_SymbolStack.pop_back();
                if (!bEnd)
                {
                    SemValue val;
                    val.m_kind  = SemKind::TOKEN;
                    val.m_token = Cur();
                    val.m_loc   = Cur().m_line;
                    m_ValueStack.push_back(std::move(val));
                    NextToken();
                }
                continue;
            }

            const std::size_t depthBefore = m_SymbolStack.size();
            HandleError("expected '" + std::string(TermName(X.m_iCode)) + "', but found " +
                        TokenText(Cur()));
            if (m_bAborted) { break; }
            if (m_SymbolStack.size() == depthBefore && m_iListPos == m_iLastErrorPos)
            {
                // 恢复未能取得进展：强制丢弃一个输入符号
                if (!NextToken()) { break; }
            }
            continue;
        }

        // ---------- 非终结符 ----------
        const int prod = g_PredictTable[X.m_iCode - NTERM_CODE_BASE][aCode];
        if (prod <= 0)
        {
            const std::size_t depthBefore = m_SymbolStack.size();
            HandleError("unexpected " + TokenText(Cur()) + "; expected one of: " +
                        FirstSetText(X.m_iCode) + " (in " + SymbolName(X.m_iCode) + ")");
            if (m_bAborted) { break; }
            if (m_SymbolStack.size() == depthBefore && m_iListPos == m_iLastErrorPos)
            {
                if (!NextToken()) { break; }
            }
            continue;
        }

        // 展开：P ← M[X, a]，弹栈后把右部（含语义动作）逆序压栈
        RecordStep(ProductionText(prod));
        m_SymbolStack.pop_back();
        PushProduction(prod);
        continue;
    }

    if (!m_ValueStack.empty() && m_ValueStack.back().m_kind == SemKind::NODE)
    {
        m_Root = std::move(m_ValueStack.back().m_node);
    }

    if (m_errors.empty() && m_Root == nullptr)
    {
        ReportError("parser produced no syntax tree (internal error)");
    }

    return std::move(m_Root);
}
