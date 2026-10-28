// ============================================================================
// MiniC 编译器 —— 目标代码生成模块 CCodeGen 实现
// 阶段 08（挑战版）：四元式 → x86-64 汇编
//   目标平台：Windows x86-64 / GNU as（AT&T 语法）/ MinGW-w64 gcc 链接
//   入口约定：global main + main:（gcc 启动代码调用 main，返回值在 eax）
//   翻译策略：朴素栈式翻译——每个变量/临时量固定 rbp 栈槽，逐四元式
//             load → 运算 → store，不做寄存器分配（正确性优先）
//   调用约定：自定义「栈传参」——实参按位置存入调用者的 8 字节槽，
//             被调用方第 i 个形参位于 16+8*(i-1)(%rbp)；
//             printf 仍按 Windows x64 ABI（rcx=格式串，xmm1=浮点值，eax=向量寄存器数）
// ============================================================================
#include "codegen/codegen.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

// ----------------------------------------------------------------------------
// 类型判定与文本小工具
// ----------------------------------------------------------------------------
bool IsFloatType(TokenType t)
{
    return t == TokenType::TOKEN_FLOAT;
}

bool IsCharType(TokenType t)
{
    return t == TokenType::TOKEN_CHAR;
}

// 单个数组元素占几个字节 —— 与 ElemWidth 的地址缩放因子必须口径一致。
// char 元素 1 字节（访问用 movb / movsbl），int / float 元素 4 字节（movl / movss）。
// 【2026-10-27 修复】此前分配一律按 4 字节算，char carr[4] 会多占 12 字节。
int ElemBytesOfType(TokenType t)
{
    return IsCharType(t) ? 1 : 4;
}

std::string IntToStr(long long v)
{
    return std::to_string(v);
}

std::string FloatLit(double v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.9g", v);
    std::string s(buf);
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos &&
        s.find("inf") == std::string::npos && s.find("nan") == std::string::npos)
    {
        s += ".0";
    }
    return s;
}

int AlignUp(int v, int a)
{
    return (v + a - 1) / a * a;
}

// ----------------------------------------------------------------------------
// AsmEmitter —— 单次编译的汇编发射器
//   生命周期：构造（绑定顶层四元式 + 符号表）→ Run（填 AsmOutput）
// ----------------------------------------------------------------------------
class AsmEmitter
{
public:
    AsmEmitter(const CodeList& topCodes, const SymbolTable* symtab)
        : m_top(topCodes), m_sym(symtab)
    {
    }

    void Run(AsmOutput& out);

private:
    // --- 输出缓冲 ---
    const CodeList&          m_top;
    const SymbolTable*       m_sym;
    std::vector<std::string> m_data;   // .data 段
    std::vector<std::string> m_bss;    // .bss 段
    std::vector<std::string> m_text;   // .text 段

    // --- 当前函数的栈帧状态 ---
    std::unordered_map<int, int> m_varOff;      // 变量表下标 → rbp 偏移（负）
    std::unordered_map<int, int> m_tmpOff;      // 临时编号   → rbp 偏移（负）
    std::unordered_set<int>      m_localVars;   // 本函数引用的局部变量（变量表下标）
    std::unordered_set<int>      m_localTemps;  // 本函数引用的临时量
    int                          m_iFrame = 0;  // 栈帧大小（16 字节对齐）
    std::string                  m_szRetLabel;  // 本函数统一出口标号
    bool                         m_bCurIsMain = false;   // 当前正在发射的是入口 main
    bool                         m_bCurRetFloat = false; // 当前函数的返回类型是否 float

    // --- 调用现场 ---
    std::vector<Operand> m_pendingArgs;         // Q_PARA 暂存（Q_CALL 时统一落栈）

    // --- 累加器直通（2026-10-28 新增）---
    // 每个四元式原本严格「load → 运算 → store」，临时量也落栈，于是
    // `b = a * 4` 会生成「存 _T1 / 立刻读 _T1」这样一对纯搬运指令。
    // 这里做两件事：
    //   ① m_szAcc 记住 %eax 当前装的是哪个操作数的值；LoadInt 遇到同一个
    //      操作数、且目标寄存器正是 %eax 时，跳过读取；
    //   ② m_forwardedTemps 记录「定义之后立刻被下一条四元式当 Op1 消费、
    //      且全函数只消费一次」的临时量，这类临时量干脆不落栈。
    // 两者都不改四元式，也不改内存语义：被直通的临时量本来就没有第二个读者。
    std::string             m_szAcc;            // "V<idx>"/"T<idx>"，空 = %eax 内容未知
    std::unordered_set<int> m_forwardedTemps;   // 免落栈的临时量编号

    // --- 窥孔：比较塌缩 + 原地更新（2026-10-28 新增）---
    // ① 比较塌缩：`_T = (a < b); JNT _T` 原本要 setcc → movzbl → cmpl $0 → je，
    //    四条指令只是为了把标志位搬进 %eax 再搬回来。若 _T 只被这条分支消费，
    //    就让比较结果留在标志位里，由 GenBranch 直接发一条 jcc。
    //    只对整型比较生效 —— 浮点走 ucomiss，NaN 时 setcc 与 jcc 的方向并不一致。
    // ② 原地更新：`_T = i + 1; i = _T` 原本发「装 %eax → 算 → 存回」，两头都是搬运。
    //    若 _T 只被这条赋值消费、且赋值目标正是二元式的 Op1，就把运算直接做到
    //    目标变量的家里（%ebx/%esi 或栈槽/全局符号）。
    std::unordered_map<int, int>     m_cmpFuseTemp;      // 临时量 → 0=JT / 1=JNT
    std::unordered_map<int, Operand> m_computeIntoTemp;  // 临时量 → 可直接写入的目标变量
    int  m_iFlagsTemp = -1;   // 标志位来自哪个临时量的比较（-1 = 无效）
    int  m_iFlagsCat  = 0;    // 该比较的 QuadCat（CAT_EQU … CAT_ME）
    bool m_bFlagsValid = false;

    // 【地址复用 / 写入即读回】只对「紧邻的下一条 INDEX」有效，见 GenIndex。
    // 写操作之后 %rax 是数组基址、%rcx 是已左移过的下标，两者都还在；
    // 读操作会把元素值读进 %eax，基址就没了，所以读路径一律作废。
    bool        m_bAddrValid = false;   // %rax / %rcx 里是不是有效的 (数组, 下标) 地址
    std::string m_szAddrKey;            // 那个地址对应的键
    bool        m_bStoreValid = false;  // %edx / %xmm0 里是不是刚写进该元素的值
    std::string m_szStoreKey;           // 那个元素对应的键

    static const char* CmpJumpMnemonic(int cat, bool bJumpIfZero);
    void EmitIntoVariable(const Quad& q, const char* iOp, const Operand& dest);

    // 【寄存器分配】本函数固定分配在 %rbx / %rsi 的局部整型变量：变量表下标 → 寄存器名
    std::unordered_map<int, std::string> m_varInReg;
    // 已经把值装进寄存器的变量下标（首次访问前需要先从栈槽装一次）
    std::unordered_set<int> m_regLoaded;
    void ChooseRegVars(const CodeList& codes);
    const std::string* RegOfVar(const Operand& op) const;
    void EnsureVarLoaded(const Operand& op, const std::string& reg);

    static std::string AccKeyOf(const Operand& op);
    static std::string IndexKeyOf(const Operand& arr, const Operand& idx, int w);
    void ClobberAcc() { m_szAcc.clear(); }
    void AnalyzeTempReuse(const CodeList& codes);

    // --- 数据段字面量 ---
    std::unordered_map<double, std::string> m_floatLbl;
    int  m_iFloatSeq = 0;
    int  m_iRetSeq   = 0;
    bool m_bUseIntFmt = false;
    bool m_bUseFltFmt = false;
    bool m_bUseChrFmt = false;

    // --- 发射辅助 ---
    void Emit(const std::string& s)    { m_text.push_back("    " + s); }
    void EmitRaw(const std::string& s) { m_text.push_back(s); }
    static std::string Lbl(int idx)    { return ".Lc" + std::to_string(idx); }

    std::string FloatLabel(double v);
    std::string FmtLabel(bool bFloat, bool bChar);

    // --- 全局数据 ---
    void EmitGlobals();

    // --- 函数 ---
    void EmitFunction(const std::string& name, const CodeList& codes, bool bIsMain);
    void CollectLocals(const CodeList& codes);
    void Touch(const Operand& op);
    void AssignOffsets();
    void EmitParams();
    int  SlotOf(const Operand& op) const;   // 返回 rbp 偏移；非栈槽返回 0x7FFFFFFF

    bool IsLocalVar(int varIdx) const { return m_localVars.count(varIdx) != 0; }

    // --- 操作数读写 ---
    void LoadInt(const Operand& op, const std::string& reg);
    void LoadFloat(const Operand& op, const std::string& xmm);
    void LoadAsFloat(const Operand& op, const std::string& xmm);
    void StoreInt(const std::string& reg, const Operand& op);
    void StoreFloat(const std::string& xmm, const Operand& op);
    void LoadAddr(const Operand& op, const std::string& reg);

    // --- 立即数直用（2026-10-28 新增）---
    // 常量参与运算时，原实现先 `movl $k, %ecx` 再 `imull %ecx, %eax`，
    // 白多一条指令。这里直接把常量当立即数操作数用：
    //   imull $4, %eax   /   cmpl $3, %eax   /   movl $7, slot(%rbp)
    bool IntConstOf(const Operand& op, long long& v) const;
    void StoreImmInt(long long v, const Operand& op);

    // --- 四元式翻译 ---
    void GenQuad(const Quad& q);
    void GenBinary(const Quad& q, const char* iOp, const char* fOp);
    void GenCompare(const Quad& q, const char* iCc, const char* fCc);
    void GenNeg(const Quad& q);
    void GenNot(const Quad& q);
    void GenBranch(const Quad& q, bool bJumpIfZero);
    void GenPrint(const Quad& q);
    void GenCall(const Quad& q);
    void GenIndex(const Quad& q);
    int  ElemWidth(const Operand& arrayOp) const;
    void GenReturn(const Quad& q);
};

// ----------------------------------------------------------------------------
// 数据段字面量
// ----------------------------------------------------------------------------
std::string AsmEmitter::FloatLabel(double v)
{
    auto it = m_floatLbl.find(v);
    if (it != m_floatLbl.end())
    {
        return it->second;
    }
    const std::string lbl = ".LFC" + std::to_string(m_iFloatSeq++);
    m_floatLbl[v] = lbl;
    m_data.push_back("    .align 4");
    m_data.push_back(lbl + ":");
    m_data.push_back("    .float " + FloatLit(v));
    return lbl;
}

std::string AsmEmitter::FmtLabel(bool bFloat, bool bChar)
{
    if (bFloat)
    {
        m_bUseFltFmt = true;
        return ".LCfmt_f";
    }
    if (bChar)
    {
        m_bUseChrFmt = true;
        return ".LCfmt_c";
    }
    m_bUseIntFmt = true;
    return ".LCfmt_i";
}

// ----------------------------------------------------------------------------
// 全局数据：.bss 中为每个全局变量/全局数组留空间
//   判据：m_iScope == 1 且名字不是过程名（过程名在语义阶段被 RecVar 借用承载）
// ----------------------------------------------------------------------------
void AsmEmitter::EmitGlobals()
{
    if (!m_sym)
    {
        return;
    }

    std::unordered_set<std::string> procNames;
    for (const ProcEntry& pe : m_sym->m_ProcInfoTbl)
    {
        procNames.insert(pe.m_szName);
    }

    for (const VarEntry& v : m_sym->m_VarInfoTbl)
    {
        if (v.m_iScope != 1)
        {
            continue;                       // 局部变量在栈帧里，不进 .bss
        }
        if (procNames.count(v.m_szName) != 0)
        {
            continue;                       // 函数名不是数据对象
        }
        if (v.m_szName.empty())
        {
            continue;
        }
        // 数组按「元素宽度 × 元素个数」分配；标量沿用 8 字节槽（movl/movss 都放得下）。
        // char carr[4] 现在只占 4 字节，不再是 16 字节。
        const int bytes = v.m_bIsArray
                              ? ElemBytesOfType(v.m_type) * (v.m_iArraySize > 0 ? v.m_iArraySize : 1)
                              : 8;
        m_bss.push_back("    .align 8");
        m_bss.push_back(v.m_szName + ":");
        m_bss.push_back("    .zero " + std::to_string(bytes));
    }
}

// ----------------------------------------------------------------------------
// 栈帧布局：先按变量表下标顺序给局部变量（含数组）排，再给临时量排
// ----------------------------------------------------------------------------
void AsmEmitter::Touch(const Operand& op)
{
    if (op.m_kind == OperandKind::OP_VAR)
    {
        if (!m_sym || op.m_iLink < 0 ||
            static_cast<std::size_t>(op.m_iLink) >= m_sym->m_VarInfoTbl.size())
        {
            return;
        }
        if (m_sym->m_VarInfoTbl[op.m_iLink].m_iScope >= 2)
        {
            m_localVars.insert(op.m_iLink);
        }
    }
    else if (op.m_kind == OperandKind::OP_TEMP)
    {
        m_localTemps.insert(op.m_iLink);
    }
}

void AsmEmitter::CollectLocals(const CodeList& codes)
{
    m_varOff.clear();
    m_tmpOff.clear();
    m_localVars.clear();
    m_localTemps.clear();
    m_iFrame = 0;

    for (const Quad& q : codes)
    {
        Touch(q.m_Op1);
        Touch(q.m_Op2);
        Touch(q.m_Rslt);
    }
}

void AsmEmitter::AssignOffsets()
{
    std::vector<int> vars(m_localVars.begin(), m_localVars.end());
    std::vector<int> tmps(m_localTemps.begin(), m_localTemps.end());
    // 稳定排序，保证同一份源码每次生成的汇编一致（便于回归比对）
    for (std::size_t i = 0; i < vars.size(); ++i)
        for (std::size_t j = i + 1; j < vars.size(); ++j)
            if (vars[j] < vars[i]) { int t = vars[i]; vars[i] = vars[j]; vars[j] = t; }
    for (std::size_t i = 0; i < tmps.size(); ++i)
        for (std::size_t j = i + 1; j < tmps.size(); ++j)
            if (tmps[j] < tmps[i]) { int t = tmps[i]; tmps[i] = tmps[j]; tmps[j] = t; }

    int off = 0;
    for (int idx : vars)
    {
        const VarEntry& v = m_sym->m_VarInfoTbl[static_cast<std::size_t>(idx)];
        // 与 EmitGlobals 同口径：数组按元素宽度算，char 元素 1 字节
        int bytes = v.m_bIsArray
                        ? ElemBytesOfType(v.m_type) * (v.m_iArraySize > 0 ? v.m_iArraySize : 1)
                        : 8;
        bytes = AlignUp(bytes, 8);
        off += bytes;
        m_varOff[idx] = -off;
    }
    for (int t : tmps)
    {
        off += 8;
        m_tmpOff[t] = -off;
    }
    m_iFrame = AlignUp(off, 16);
}

// 形参搬运：符号表里 ParamEntry 没有过程归属，故按「名字 + 声明行列」回连变量表项，
// 再用 m_iParamPos 定位它在实参槽中的位置。
void AsmEmitter::EmitParams()
{
    if (!m_sym)
    {
        return;
    }
    for (const ParamEntry& pa : m_sym->m_ParamInfoTbl)
    {
        for (int idx : m_localVars)
        {
            const VarEntry& v = m_sym->m_VarInfoTbl[static_cast<std::size_t>(idx)];
            if (v.m_szName != pa.m_szName || v.m_iLine != pa.m_iLine || v.m_iCol != pa.m_iCol)
            {
                continue;
            }
            const int src = 16 + 8 * (pa.m_iParamPos - 1);
            const int dst = m_varOff[idx];
            if (IsFloatType(v.m_type))
            {
                Emit("movss " + std::to_string(src) + "(%rbp), %xmm0");
                Emit("movss %xmm0, " + std::to_string(dst) + "(%rbp)");
            }
            else
            {
                Emit("movl " + std::to_string(src) + "(%rbp), %eax");
                Emit("movl %eax, " + std::to_string(dst) + "(%rbp)");
            }
        }
    }
}

// ----------------------------------------------------------------------------
// 操作数 → 寄存器
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// 累加器直通辅助（2026-10-28 新增）
// ----------------------------------------------------------------------------
std::string AsmEmitter::AccKeyOf(const Operand& op)
{
    if (op.m_kind == OperandKind::OP_VAR)
    {
        return "V" + std::to_string(op.m_iLink);
    }
    if (op.m_kind == OperandKind::OP_TEMP)
    {
        return "T" + std::to_string(op.m_iLink);
    }
    return std::string();
}

// 比 AccKeyOf 多认常量与标号：数组下标常常是常量（a[0] / a[1]），
// 只按「变量/临时量」建键会让 a[0] 与 a[1] 撞成同一个地址。
static std::string OpKeyOf(const Operand& op)
{
    switch (op.m_kind)
    {
    case OperandKind::OP_VAR:   return "V" + std::to_string(op.m_iLink);
    case OperandKind::OP_TEMP:  return "T" + std::to_string(op.m_iLink);
    case OperandKind::OP_CONST: return "C" + std::to_string(op.m_iLink);
    case OperandKind::OP_LABEL: return "L" + std::to_string(op.m_iLink);
    default:                    return "-";
    }
}

std::string AsmEmitter::IndexKeyOf(const Operand& arr, const Operand& idx, int w)
{
    return OpKeyOf(arr) + "|" + OpKeyOf(idx) + "|" + std::to_string(w);
}

// 判断某条四元式是否「一进来就把 Op1 读进 %eax」。
// 只有这类指令才能作为免落栈临时量的消费者 —— 例如 GenPrint 把值读进 %edx、
// GenIndex 先把数组基址装进 %rax，它们都不看 %eax 里现成的东西，
// 若把临时量直通给它们，就会读到从未写过的栈槽。
static bool ConsumesEaxFirst(QuadOp op)
{
    switch (op)
    {
    case QuadOp::Q_ASSIGN_1: case QuadOp::Q_ASSIGN_4: case QuadOp::Q_ASSIGN_4F:
    case QuadOp::Q_ADD_4:  case QuadOp::Q_ADD_4F:
    case QuadOp::Q_SUB_4:  case QuadOp::Q_SUB_4F:
    case QuadOp::Q_MUL_4:  case QuadOp::Q_MUL_4F:
    case QuadOp::Q_DIV_4:  case QuadOp::Q_DIV_4F:
    case QuadOp::Q_NEG_4:  case QuadOp::Q_NEG_4F:
    case QuadOp::Q_NOT_4:
    case QuadOp::Q_EQU_1:  case QuadOp::Q_EQU_4:  case QuadOp::Q_EQU_4F:
    case QuadOp::Q_NEQU_1: case QuadOp::Q_NEQU_4: case QuadOp::Q_NEQU_4F:
    case QuadOp::Q_LS_1:   case QuadOp::Q_LS_4:   case QuadOp::Q_LS_4F:
    case QuadOp::Q_LE_1:   case QuadOp::Q_LE_4:   case QuadOp::Q_LE_4F:
    case QuadOp::Q_MR_1:   case QuadOp::Q_MR_4:   case QuadOp::Q_MR_4F:
    case QuadOp::Q_ME_1:   case QuadOp::Q_ME_4:   case QuadOp::Q_ME_4F:
    case QuadOp::Q_JT:     case QuadOp::Q_JNT:
    case QuadOp::Q_ITOF:
    case QuadOp::Q_RETV:
        return true;
    default:
        return false;
    }
}

// 整型比较（大类 1~6）。浮点比较不参与「比较塌缩」，原因见 m_cmpFuseTemp 的注释。
static bool IsIntCompareOp(QuadOp op)
{
    const int cat = QuadCatOf(op);
    return cat >= static_cast<int>(QuadCat::CAT_EQU) &&
           cat <= static_cast<int>(QuadCat::CAT_ME);
}

// 可以原地进行的整型算术：加、减、乘。
// 除法不在其中 —— idivl 的结果必须经 %eax/%edx，没法直接写目标操作数。
static bool IsIntArithOp(QuadOp op)
{
    const int cat = QuadCatOf(op);
    return cat == static_cast<int>(QuadCat::CAT_ADD) ||
           cat == static_cast<int>(QuadCat::CAT_SUB) ||
           cat == static_cast<int>(QuadCat::CAT_MUL);
}

// 找出「定义后立刻被下一条四元式当 Op1 消费、且全函数只消费一次」的临时量。
// 判据有三条，缺一不可：
//   1. 全函数里只被读过一次（读两次就不能省栈槽）；
//   2. 唯一的读者就是紧邻的下一条四元式（中间隔着别的指令，%eax 早被覆盖了）；
//   3. 读者是「一进来就读 %eax」的指令（见 ConsumesEaxFirst），
//      否则该指令根本不看 %eax，直通过去就会读到没写过的栈槽。
void AsmEmitter::AnalyzeTempReuse(const CodeList& codes)
{
    m_forwardedTemps.clear();
    m_cmpFuseTemp.clear();
    m_computeIntoTemp.clear();
    m_bFlagsValid = false;
    m_iFlagsTemp  = -1;
    std::unordered_map<int, int> useCount;
    for (const Quad& q : codes)
    {
        if (q.m_Op1.m_kind == OperandKind::OP_TEMP) { ++useCount[q.m_Op1.m_iLink]; }
        if (q.m_Op2.m_kind == OperandKind::OP_TEMP) { ++useCount[q.m_Op2.m_iLink]; }
    }
    for (std::size_t i = 0; i + 1 < codes.size(); ++i)
    {
        const Quad& q  = codes[i];
        const Quad& nq = codes[i + 1];
        if (q.m_Rslt.m_kind != OperandKind::OP_TEMP) { continue; }
        const int t = q.m_Rslt.m_iLink;
        if (useCount[t] != 1) { continue; }
        if (nq.m_Op1.m_kind != OperandKind::OP_TEMP || nq.m_Op1.m_iLink != t) { continue; }
        if (!ConsumesEaxFirst(nq.m_eOp)) { continue; }
        m_forwardedTemps.insert(t);

        // ① 比较塌缩：nq 就是这个比较结果的条件分支
        if (IsIntCompareOp(q.m_eOp) &&
            !IsFloatType(q.m_Op1.m_type) && !IsFloatType(q.m_Op2.m_type) &&
            (nq.m_eOp == QuadOp::Q_JT || nq.m_eOp == QuadOp::Q_JNT))
        {
            m_cmpFuseTemp[t] = (nq.m_eOp == QuadOp::Q_JNT) ? 1 : 0;
        }

        // ② 原地更新：nq 是把该临时量赋回二元式左操作数的赋值
        if (IsIntArithOp(q.m_eOp) &&
            q.m_Op1.m_kind == OperandKind::OP_VAR &&
            !IsFloatType(q.m_Op1.m_type) && !IsFloatType(q.m_Op2.m_type) &&
            !IsFloatType(q.m_Rslt.m_type) &&
            (nq.m_eOp == QuadOp::Q_ASSIGN_1 || nq.m_eOp == QuadOp::Q_ASSIGN_4) &&
            nq.m_Rslt.m_kind == OperandKind::OP_VAR &&
            nq.m_Rslt.m_iLink == q.m_Op1.m_iLink)
        {
            m_computeIntoTemp[t] = nq.m_Rslt;
        }
    }
}

// 取「整型/字符/布尔常量」的字面值；浮点常量或非常量返回 false。
// 浮点不参与立即数优化：addss 没有立即数形式，只能走内存操作数。
bool AsmEmitter::IntConstOf(const Operand& op, long long& v) const
{
    if (op.m_kind != OperandKind::OP_CONST || !m_sym || op.m_iLink < 0 ||
        static_cast<std::size_t>(op.m_iLink) >= m_sym->m_ConstInfoTbl.size())
    {
        return false;
    }
    const ConstEntry& ce = m_sym->m_ConstInfoTbl[static_cast<std::size_t>(op.m_iLink)];
    if (ce.m_bIsFloat)
    {
        return false;
    }
    v = ce.m_iValue;
    return true;
}

// 把常量直接写进目标栈槽/全局变量，省掉「先装 %eax 再存」的一次搬运。
void AsmEmitter::StoreImmInt(long long v, const Operand& op)
{
    const std::string src = "$" + IntToStr(v);
    // 【寄存器分配】目标变量固定在 %rbx/%rsi 时直接写寄存器。
    if (const std::string* pReg = RegOfVar(op))
    {
        Emit("movl " + src + ", " + *pReg);
        m_regLoaded.insert(op.m_iLink);
        return;
    }
    if (op.m_kind == OperandKind::OP_VAR)
    {
        if (m_sym && op.m_iLink >= 0 &&
            static_cast<std::size_t>(op.m_iLink) < m_sym->m_VarInfoTbl.size() &&
            !IsLocalVar(op.m_iLink))
        {
            Emit("movl " + src + ", " +
                 m_sym->m_VarInfoTbl[static_cast<std::size_t>(op.m_iLink)].m_szName + "(%rip)");
            return;
        }
        Emit("movl " + src + ", " + std::to_string(SlotOf(op)) + "(%rbp)");
        return;
    }
    if (op.m_kind == OperandKind::OP_TEMP)
    {
        Emit("movl " + src + ", " + std::to_string(SlotOf(op)) + "(%rbp)");
    }
}

int AsmEmitter::SlotOf(const Operand& op) const
{
    if (op.m_kind == OperandKind::OP_TEMP)
    {
        auto it = m_tmpOff.find(op.m_iLink);
        return it == m_tmpOff.end() ? 0x7FFFFFFF : it->second;
    }
    if (op.m_kind == OperandKind::OP_VAR)
    {
        auto it = m_varOff.find(op.m_iLink);
        return it == m_varOff.end() ? 0x7FFFFFFF : it->second;
    }
    return 0x7FFFFFFF;
}

// ----------------------------------------------------------------------------
// 寄存器分配（2026-10-28 新增）
//
// 做法：每个函数挑出访问最频繁的至多两个局部整型标量，固定放在 %rbx 与 %rsi
// 里，函数体内所有读写直接走寄存器，不再经过栈槽。
//
// 为什么只挑「局部 + 整型 + 标量」：
//   * 全局变量不能进 —— MiniC 没有指针，被调函数唯一能改的就是全局变量，
//     缓存在寄存器里会在 call 之后变旧；
//   * 数组不能进 —— 数组要被取地址（LoadAddr 走 leaq），必须留在内存里；
//   * 实型不进 —— %xmm 全是 caller-saved，跨一次 call 就被破坏，
//     要么每次调用前后落栈，要么加保存恢复，代价大于收益。
//
// 为什么选 %rbx / %rsi：两者都是 callee-saved，递归调用返回后值仍在，
// 函数体内不需要任何额外保存动作；同时也不会与既有的
// %rax/%rcx/%rdx（整型临时）与 %xmm0/%xmm1（浮点临时）用法冲突。
//
// 值的一致性：变量一旦分配进寄存器，它的栈槽就只写不读（EmitParams 仍会写入，
// 首次读取时从栈槽装一次即可）。函数出口也不需要回写 —— 没有任何路径会去读
// 那个栈槽，回写只是多余的搬运。
// ----------------------------------------------------------------------------
const std::string* AsmEmitter::RegOfVar(const Operand& op) const
{
    if (op.m_kind != OperandKind::OP_VAR || !IsLocalVar(op.m_iLink))
    {
        return nullptr;
    }
    const auto it = m_varInReg.find(op.m_iLink);
    return (it == m_varInReg.end()) ? nullptr : &it->second;
}

// 首次访问某个已分配变量时，把它的栈槽内容装进寄存器。
// 这一步不能省：形参是 EmitParams 用裸 movl 写进栈槽的，
// 局部变量也可能在被分配之前就被别处写入过。
void AsmEmitter::EnsureVarLoaded(const Operand& op, const std::string& reg)
{
    if (m_regLoaded.count(op.m_iLink) != 0)
    {
        return;
    }
    Emit("movl " + std::to_string(SlotOf(op)) + "(%rbp), " + reg);
    m_regLoaded.insert(op.m_iLink);
}

void AsmEmitter::ChooseRegVars(const CodeList& codes)
{
    m_varInReg.clear();
    m_regLoaded.clear();
    if (!m_sym)
    {
        return;
    }

    std::unordered_map<int, int> useCount;
    for (const Quad& q : codes)
    {
        const Operand* ops[3] = { &q.m_Op1, &q.m_Op2, &q.m_Rslt };
        for (const Operand* p : ops)
        {
            if (p->m_kind != OperandKind::OP_VAR || !IsLocalVar(p->m_iLink))
            {
                continue;
            }
            if (static_cast<std::size_t>(p->m_iLink) >= m_sym->m_VarInfoTbl.size())
            {
                continue;
            }
            const VarEntry& v = m_sym->m_VarInfoTbl[static_cast<std::size_t>(p->m_iLink)];
            if (v.m_bIsArray || IsFloatType(v.m_type))
            {
                continue;
            }
            ++useCount[p->m_iLink];
        }
    }

    std::vector<std::pair<int, int>> ranked(useCount.begin(), useCount.end());
    std::sort(ranked.begin(), ranked.end(),
              [](const std::pair<int, int>& a, const std::pair<int, int>& b)
              { return a.second > b.second; });

    // 注意：这里必须是 32 位寄存器名 —— movl 配 %rbx 会报
    // "incorrect register `%rbx' used with `l' suffix"，
    // 保存/恢复时才用 64 位的 %rbx / %rsi。
    static const char* const kRegs[2] = { "%ebx", "%esi" };
    int n = 0;
    for (const std::pair<int, int>& pr : ranked)
    {
        // 访问少于 4 次不值当：省下的两次 movl 抵不过一次 push/pop
        if (n >= 2 || pr.second < 4)
        {
            break;
        }
        m_varInReg[pr.first] = kRegs[n++];
    }
}

void AsmEmitter::LoadInt(const Operand& op, const std::string& reg)
{
    // 【寄存器分配】局部整型变量若已固定分配在 %rbx/%rsi，直接从寄存器取值。
    if (const std::string* pReg = RegOfVar(op))
    {
        EnsureVarLoaded(op, *pReg);
        if (reg == "%eax")
        {
            m_szAcc = AccKeyOf(op);
        }
        if (reg != *pReg)
        {
            Emit("movl " + *pReg + ", " + reg);
        }
        return;
    }
    // 【累加器直通】%eax 里已经是这个操作数了，不必再读一次。
    // 只对 %eax 生效：其它寄存器（%ecx/%edx）不参与缓存，
    // 否则 GenPrint 那种「值必须落到 %edx」的场景会读不到数。
    if (reg == "%eax")
    {
        const std::string key = AccKeyOf(op);
        if (!key.empty() && key == m_szAcc)
        {
            return;
        }
        m_szAcc = key;      // 常量/无操作数 → 空串，表示 %eax 内容未知
    }
    switch (op.m_kind)
    {
    case OperandKind::OP_CONST:
    {
        long long v = 0;
        if (m_sym && op.m_iLink >= 0 &&
            static_cast<std::size_t>(op.m_iLink) < m_sym->m_ConstInfoTbl.size())
        {
            const ConstEntry& ce = m_sym->m_ConstInfoTbl[static_cast<std::size_t>(op.m_iLink)];
            v = ce.m_bIsFloat ? static_cast<long long>(ce.m_fValue) : ce.m_iValue;
        }
        Emit("movl $" + IntToStr(v) + ", " + reg);
        return;
    }
    case OperandKind::OP_VAR:
    {
        if (m_sym && op.m_iLink >= 0 &&
            static_cast<std::size_t>(op.m_iLink) < m_sym->m_VarInfoTbl.size() &&
            !IsLocalVar(op.m_iLink))
        {
            Emit("movl " + m_sym->m_VarInfoTbl[static_cast<std::size_t>(op.m_iLink)].m_szName +
                 "(%rip), " + reg);
            return;
        }
        Emit("movl " + std::to_string(SlotOf(op)) + "(%rbp), " + reg);
        return;
    }
    case OperandKind::OP_TEMP:
        Emit("movl " + std::to_string(SlotOf(op)) + "(%rbp), " + reg);
        return;
    default:
        Emit("xorl " + reg + ", " + reg);
        return;
    }
}

void AsmEmitter::LoadFloat(const Operand& op, const std::string& xmm)
{
    switch (op.m_kind)
    {
    case OperandKind::OP_CONST:
    {
        double v = 0.0;
        if (m_sym && op.m_iLink >= 0 &&
            static_cast<std::size_t>(op.m_iLink) < m_sym->m_ConstInfoTbl.size())
        {
            const ConstEntry& ce = m_sym->m_ConstInfoTbl[static_cast<std::size_t>(op.m_iLink)];
            v = ce.m_bIsFloat ? ce.m_fValue : static_cast<double>(ce.m_iValue);
        }
        Emit("movss " + FloatLabel(v) + "(%rip), " + xmm);
        return;
    }
    case OperandKind::OP_VAR:
    {
        if (m_sym && op.m_iLink >= 0 &&
            static_cast<std::size_t>(op.m_iLink) < m_sym->m_VarInfoTbl.size() &&
            !IsLocalVar(op.m_iLink))
        {
            Emit("movss " + m_sym->m_VarInfoTbl[static_cast<std::size_t>(op.m_iLink)].m_szName +
                 "(%rip), " + xmm);
            return;
        }
        Emit("movss " + std::to_string(SlotOf(op)) + "(%rbp), " + xmm);
        return;
    }
    case OperandKind::OP_TEMP:
        Emit("movss " + std::to_string(SlotOf(op)) + "(%rbp), " + xmm);
        return;
    default:
        Emit("xorps " + xmm + ", " + xmm);
        return;
    }
}

// 关系运算允许 int 与 float 混用（IR 不插转换），此处按需就地转成单精度
void AsmEmitter::LoadAsFloat(const Operand& op, const std::string& xmm)
{
    if (IsFloatType(op.m_type))
    {
        LoadFloat(op, xmm);
        return;
    }
    LoadInt(op, "%eax");
    Emit("cvtsi2ss %eax, " + xmm);
}

void AsmEmitter::StoreInt(const std::string& reg, const Operand& op)
{
    // 【寄存器分配】目标变量固定在 %rbx/%rsi 时直接写寄存器，不再落栈。
    if (const std::string* pReg = RegOfVar(op))
    {
        if (reg == "%eax")
        {
            m_szAcc = AccKeyOf(op);
        }
        m_regLoaded.insert(op.m_iLink);
        if (reg != *pReg)
        {
            Emit("movl " + reg + ", " + *pReg);
        }
        return;
    }
    // movl 不改源寄存器，所以存完之后 reg 里仍然是这个值；
    // 先记下状态，下面即使因「免落栈」提前返回，%eax 的内容也依然有效。
    if (reg == "%eax")
    {
        m_szAcc = AccKeyOf(op);
        // 【免落栈】这个临时量在下一条四元式里就会被 %eax 直接消费，
        // 中间没有别的指令，落栈再读回来纯属搬运，直接省掉这次 store。
        if (op.m_kind == OperandKind::OP_TEMP &&
            m_forwardedTemps.count(op.m_iLink) != 0)
        {
            return;
        }
    }
    if (op.m_kind == OperandKind::OP_VAR)
    {
        if (m_sym && op.m_iLink >= 0 &&
            static_cast<std::size_t>(op.m_iLink) < m_sym->m_VarInfoTbl.size() &&
            !IsLocalVar(op.m_iLink))
        {
            Emit("movl " + reg + ", " +
                 m_sym->m_VarInfoTbl[static_cast<std::size_t>(op.m_iLink)].m_szName + "(%rip)");
            return;
        }
        Emit("movl " + reg + ", " + std::to_string(SlotOf(op)) + "(%rbp)");
        return;
    }
    if (op.m_kind == OperandKind::OP_TEMP)
    {
        Emit("movl " + reg + ", " + std::to_string(SlotOf(op)) + "(%rbp)");
    }
}

void AsmEmitter::StoreFloat(const std::string& xmm, const Operand& op)
{
    if (op.m_kind == OperandKind::OP_VAR)
    {
        if (m_sym && op.m_iLink >= 0 &&
            static_cast<std::size_t>(op.m_iLink) < m_sym->m_VarInfoTbl.size() &&
            !IsLocalVar(op.m_iLink))
        {
            Emit("movss " + xmm + ", " +
                 m_sym->m_VarInfoTbl[static_cast<std::size_t>(op.m_iLink)].m_szName + "(%rip)");
            return;
        }
        Emit("movss " + xmm + ", " + std::to_string(SlotOf(op)) + "(%rbp)");
        return;
    }
    if (op.m_kind == OperandKind::OP_TEMP)
    {
        Emit("movss " + xmm + ", " + std::to_string(SlotOf(op)) + "(%rbp)");
    }
}

// 取地址（数组基址）：全局 → 名字(%rip)，局部 → 栈槽地址
void AsmEmitter::LoadAddr(const Operand& op, const std::string& reg)
{
    if (op.m_kind == OperandKind::OP_VAR && m_sym && op.m_iLink >= 0 &&
        static_cast<std::size_t>(op.m_iLink) < m_sym->m_VarInfoTbl.size())
    {
        const VarEntry& v = m_sym->m_VarInfoTbl[static_cast<std::size_t>(op.m_iLink)];
        if (IsLocalVar(op.m_iLink))
        {
            Emit("leaq " + std::to_string(m_varOff[op.m_iLink]) + "(%rbp), " + reg);
        }
        else
        {
            Emit("leaq " + v.m_szName + "(%rip), " + reg);
        }
        return;
    }
    Emit("xorq " + reg + ", " + reg);
}

// ----------------------------------------------------------------------------
// 四元式翻译
// ----------------------------------------------------------------------------
void AsmEmitter::GenBinary(const Quad& q, const char* iOp, const char* fOp)
{
    const bool bFloat = IsFloatType(q.m_Op1.m_type) || IsFloatType(q.m_Op2.m_type) ||
                        IsFloatType(q.m_Rslt.m_type);
    if (bFloat)
    {
        LoadAsFloat(q.m_Op1, "%xmm0");
        LoadAsFloat(q.m_Op2, "%xmm1");
        Emit(std::string(fOp) + " %xmm1, %xmm0");
        StoreFloat("%xmm0", q.m_Rslt);
        return;
    }
    // 【原地更新】见 m_computeIntoTemp 的注释
    if (q.m_Rslt.m_kind == OperandKind::OP_TEMP)
    {
        const auto itIn = m_computeIntoTemp.find(q.m_Rslt.m_iLink);
        if (itIn != m_computeIntoTemp.end())
        {
            EmitIntoVariable(q, iOp, itIn->second);
            return;
        }
    }
    LoadInt(q.m_Op1, "%eax");
    // 常量右操作数直接当立即数用（idiv 没有立即数形式，只能走寄存器）
    long long k = 0;
    if (std::string(iOp) != "idiv" && IntConstOf(q.m_Op2, k))
    {
        Emit(std::string(iOp) + " $" + IntToStr(k) + ", %eax");
        StoreInt("%eax", q.m_Rslt);
        return;
    }
    LoadInt(q.m_Op2, "%ecx");
    if (std::string(iOp) == "idiv")
    {
        Emit("cltd");
        Emit("idivl %ecx");
    }
    else
    {
        Emit(std::string(iOp) + " %ecx, %eax");
    }
    StoreInt("%eax", q.m_Rslt);
}

void AsmEmitter::GenCompare(const Quad& q, const char* iCc, const char* fCc)
{
    const bool bFloat = IsFloatType(q.m_Op1.m_type) || IsFloatType(q.m_Op2.m_type);
    if (bFloat)
    {
        LoadAsFloat(q.m_Op1, "%xmm0");
        LoadAsFloat(q.m_Op2, "%xmm1");
        Emit("ucomiss %xmm1, %xmm0");
        Emit(std::string("set") + fCc + " %al");
    }
    else
    {
        LoadInt(q.m_Op1, "%eax");
        long long k = 0;
        if (IntConstOf(q.m_Op2, k))
        {
            Emit("cmpl $" + IntToStr(k) + ", %eax");
        }
        else
        {
            LoadInt(q.m_Op2, "%ecx");
            Emit("cmpl %ecx, %eax");
        }
        // 【比较塌缩】下一条四元式就是这个比较结果的分支消费者：
        // 把结果留在标志位里，交给 GenBranch 直接 jcc，省 setcc/movzbl/cmpl 三条。
        if (q.m_Rslt.m_kind == OperandKind::OP_TEMP &&
            m_cmpFuseTemp.count(q.m_Rslt.m_iLink) != 0)
        {
            m_iFlagsTemp  = q.m_Rslt.m_iLink;
            m_iFlagsCat   = QuadCatOf(q.m_eOp);
            m_bFlagsValid = true;
            ClobberAcc();       // %eax 里是 Op1 的残值，不能当成任何变量的值
            return;
        }
        Emit(std::string("set") + iCc + " %al");
    }
    Emit("movzbl %al, %eax");
    StoreInt("%eax", q.m_Rslt);
}

// ----------------------------------------------------------------------------
// 取负 NEG（表 5-2 第 18 类）：结果 = 0 - 操作数
//   NEG_4  整型/字符：xorl %eax,%eax; subl 操作数
//   NEG_4F 单精度：xorps %xmm1,%xmm1; subss 操作数
// 注意：与「二元减法」的指令方向相反 —— subss 是 xmm1 - xmm0，所以必须先清零到 xmm1。
// ----------------------------------------------------------------------------
void AsmEmitter::GenNeg(const Quad& q)
{
    if (IsFloatType(q.m_Op1.m_type) || IsFloatType(q.m_Rslt.m_type))
    {
        LoadAsFloat(q.m_Op1, "%xmm0");
        Emit("xorps %xmm1, %xmm1");
        Emit("subss %xmm0, %xmm1");
        StoreFloat("%xmm1", q.m_Rslt);
        return;
    }
    LoadInt(q.m_Op1, "%eax");
    Emit("negl %eax");                 // 对 32 位整型 negl 即可，字符按整型提升后同此
    StoreInt("%eax", q.m_Rslt);
}

// ----------------------------------------------------------------------------
// 取非 NOT（表 5-2 第 19 类）：结果 = (操作数 == 0) ? 1 : 0
//   NOT_4 整型/字符：cmpl $0,%eax; sete %al; movzbl %al,%eax
// 说明：MiniC 的 `!` 只作用于 bool，bool 在栈上是 4 字节整型，所以走 NOT_4。
// ----------------------------------------------------------------------------
void AsmEmitter::GenNot(const Quad& q)
{
    LoadInt(q.m_Op1, "%eax");
    Emit("cmpl $0, %eax");
    Emit("sete %al");
    Emit("movzbl %al, %eax");
    StoreInt("%eax", q.m_Rslt);
}

void AsmEmitter::GenBranch(const Quad& q, bool bJumpIfZero)
{
    // 【比较塌缩】标志位上正好是这条分支要判的比较结果，直接跳
    if (m_bFlagsValid && q.m_Op1.m_kind == OperandKind::OP_TEMP &&
        q.m_Op1.m_iLink == m_iFlagsTemp)
    {
        m_bFlagsValid = false;
        m_iFlagsTemp  = -1;
        Emit(std::string(CmpJumpMnemonic(m_iFlagsCat, bJumpIfZero)) + " " +
             Lbl(q.m_Rslt.m_iLink));
        return;
    }
    m_bFlagsValid = false;      // 兜底：标志位对不上就按常规路径重新比较
    if (IsFloatType(q.m_Op1.m_type))
    {
        LoadFloat(q.m_Op1, "%xmm0");
        Emit("xorps %xmm1, %xmm1");
        Emit("ucomiss %xmm1, %xmm0");
    }
    else
    {
        LoadInt(q.m_Op1, "%eax");
        Emit("cmpl $0, %eax");
    }
    Emit(std::string(bJumpIfZero ? "je " : "jne ") + Lbl(q.m_Rslt.m_iLink));
}

// 比较大类 + 「结果为 0 时跳 / 不为 0 时跳」→ 对应的 jcc 助记符。
// 方向要反过来选：bJumpIfZero 表示「比较结果为假时跳」，用的是比较的反面。
const char* AsmEmitter::CmpJumpMnemonic(int cat, bool bJumpIfZero)
{
    const bool z = bJumpIfZero;
    if (cat == static_cast<int>(QuadCat::CAT_EQU))  { return z ? "jne" : "je";  }
    if (cat == static_cast<int>(QuadCat::CAT_NEQU)) { return z ? "je"  : "jne"; }
    if (cat == static_cast<int>(QuadCat::CAT_LS))   { return z ? "jge" : "jl";  }
    if (cat == static_cast<int>(QuadCat::CAT_LE))   { return z ? "jg"  : "jle"; }
    if (cat == static_cast<int>(QuadCat::CAT_MR))   { return z ? "jle" : "jg";  }
    if (cat == static_cast<int>(QuadCat::CAT_ME))   { return z ? "jl"  : "jge"; }
    return z ? "je" : "jne";
}

// 把整型二元运算直接做到目标变量上（原地更新）。
// 目标可以是寄存器（%ebx/%esi）、局部栈槽或全局符号 —— x86 的 addl / subl / imull
// 三种形态都允许目的操作数是寄存器或内存，因此都只要一条指令。
void AsmEmitter::EmitIntoVariable(const Quad& q, const char* iOp, const Operand& dest)
{
    std::string dst;
    const std::string* pReg = RegOfVar(dest);
    if (pReg != nullptr)
    {
        EnsureVarLoaded(dest, *pReg);
        dst = *pReg;
    }
    else if (m_sym != nullptr && dest.m_iLink >= 0 &&
             static_cast<std::size_t>(dest.m_iLink) < m_sym->m_VarInfoTbl.size() &&
             !IsLocalVar(dest.m_iLink))
    {
        dst = m_sym->m_VarInfoTbl[static_cast<std::size_t>(dest.m_iLink)].m_szName + "(%rip)";
    }
    else
    {
        dst = std::to_string(SlotOf(dest)) + "(%rbp)";
    }

    long long k = 0;
    if (IntConstOf(q.m_Op2, k))
    {
        Emit(std::string(iOp) + " $" + IntToStr(k) + ", " + dst);
    }
    else
    {
        LoadInt(q.m_Op2, "%ecx");
        Emit(std::string(iOp) + " %ecx, " + dst);
    }
    ClobberAcc();       // 结果落在目标变量里，%eax 的内容不再代表任何操作数
}

// ----------------------------------------------------------------------------
// 输出 PRINT
// ----------------------------------------------------------------------------
void AsmEmitter::GenPrint(const Quad& q)
{
    const bool bFloat = IsFloatType(q.m_Op1.m_type);
    const bool bChar  = IsCharType(q.m_Op1.m_type);
    const std::string fmt = FmtLabel(bFloat, bChar);

    Emit("subq $32, %rsp");                       // Windows x64 影子空间
    Emit("leaq " + fmt + "(%rip), %rcx");
    if (bFloat)
    {
        LoadFloat(q.m_Op1, "%xmm1");
        Emit("cvtss2sd %xmm1, %xmm1");            // 可变参数浮点按 double 传
        // 注意：GNU 的 printf 在 Windows 上不读 xmm 传参，必须同时放到第 2 个整型槽 %rdx
        Emit("movq %xmm1, %rdx");
        Emit("movl $0, %eax");
    }
    else
    {
        LoadInt(q.m_Op1, "%edx");
        Emit("movl $0, %eax");
    }
    Emit("call printf");
    ClobberAcc();                       // call 会覆盖 %eax
    Emit("addq $32, %rsp");
}

void AsmEmitter::GenCall(const Quad& q)
{
    std::string fname;
    if (m_sym && q.m_Op1.m_kind == OperandKind::OP_VAR && q.m_Op1.m_iLink >= 0 &&
        static_cast<std::size_t>(q.m_Op1.m_iLink) < m_sym->m_VarInfoTbl.size())
    {
        fname = m_sym->m_VarInfoTbl[static_cast<std::size_t>(q.m_Op1.m_iLink)].m_szName;
    }
    if (fname.empty())
    {
        Emit("# WARNING: unresolved call target");
        m_pendingArgs.clear();
        return;
    }

    const int n = static_cast<int>(m_pendingArgs.size());
    const int slot = AlignUp(8 * n, 16);          // 保持 16 字节对齐
    if (slot > 0)
    {
        Emit("subq $" + std::to_string(slot) + ", %rsp");
    }
    for (int i = 0; i < n; ++i)
    {
        const Operand& a = m_pendingArgs[static_cast<std::size_t>(i)];
        const std::string dst = std::to_string(i * 8) + "(%rsp)";
        if (IsFloatType(a.m_type))
        {
            LoadFloat(a, "%xmm0");
            Emit("movss %xmm0, " + dst);
        }
        else
        {
            LoadInt(a, "%eax");
            Emit("movl %eax, " + dst);
        }
    }
    Emit("call " + fname);
    ClobberAcc();                       // call 会覆盖 %eax/%ecx/%edx
    if (slot > 0)
    {
        Emit("addq $" + std::to_string(slot) + ", %rsp");
    }
    m_pendingArgs.clear();

    if (q.m_Rslt.m_kind == OperandKind::OP_TEMP || q.m_Rslt.m_kind == OperandKind::OP_VAR)
    {
        if (IsFloatType(q.m_Rslt.m_type))
        {
            StoreFloat("%xmm0", q.m_Rslt);
        }
        else
        {
            StoreInt("%eax", q.m_Rslt);
        }
    }
}

// 数组元素宽度（决定地址缩放因子）
int AsmEmitter::ElemWidth(const Operand& arrayOp) const
{
    if (m_sym && arrayOp.m_kind == OperandKind::OP_VAR && arrayOp.m_iLink >= 0 &&
        static_cast<std::size_t>(arrayOp.m_iLink) < m_sym->m_VarInfoTbl.size())
    {
        const VarEntry& v = m_sym->m_VarInfoTbl[static_cast<std::size_t>(arrayOp.m_iLink)];
        if (IsCharType(v.m_type))
        {
            return 1;
        }
    }
    return 4;   // int / float 元素均占 4 字节
}

void AsmEmitter::GenIndex(const Quad& q)
{
    // 读：(INDEX, 数组, 下标, 临时量 t) —— t = 数组[下标]
    // 写：(INDEX, 值,   下标, 数组)   —— 数组[下标] = 值
    // 判据：写操作的 m_Rslt 一定是「数组变量」，读操作的 m_Rslt 一定是临时量。
    //
    // 注意：下面 LoadAddr 往 %rax 写地址，会把 %eax 一起覆盖掉，
    // 所以进函数第一件事就是作废累加器缓存（否则下一条四元式会误以为
    // %eax 里还是上一个值而跳过读取）。
    ClobberAcc();
    const bool bStore = (q.m_Rslt.m_kind == OperandKind::OP_VAR && m_sym &&
                         q.m_Rslt.m_iLink >= 0 &&
                         static_cast<std::size_t>(q.m_Rslt.m_iLink) < m_sym->m_VarInfoTbl.size() &&
                         m_sym->m_VarInfoTbl[static_cast<std::size_t>(q.m_Rslt.m_iLink)].m_bIsArray);

    const int w = bStore ? ElemWidth(q.m_Rslt) : ElemWidth(q.m_Op1);
    const std::string szKey = IndexKeyOf(bStore ? q.m_Rslt : q.m_Op1, q.m_Op2, w);

    if (bStore)
    {
        // 【地址复用】上一条四元式刚算过同一个 (数组, 下标) 并且是写操作，
        // %rax / %rcx 里的地址还没被动过（写只读 %edx / %xmm0），直接接着用。
        if (!(m_bAddrValid && m_szAddrKey == szKey))
        {
            LoadAddr(q.m_Rslt, "%rax");
            LoadInt(q.m_Op2, "%ecx");
            Emit("movslq %ecx, %rcx");
        }
        if (IsFloatType(q.m_Op1.m_type))
        {
            LoadFloat(q.m_Op1, "%xmm0");
            Emit("movss %xmm0, (%rax,%rcx," + std::to_string(w) + ")");
        }
        else
        {
            LoadInt(q.m_Op1, "%edx");
            Emit(std::string(w == 1 ? "movb %dl, " : "movl %edx, ") +
                 "(%rax,%rcx," + std::to_string(w) + ")");
        }
        // 写完之后 %rax / %rcx 仍是地址，%edx（或 %xmm0）里是刚写进去的值。
        m_szAddrKey  = szKey;
        m_bAddrValid = true;
        m_bStoreValid = false;
        if (w == 4)   // 1 字节元素的值在 %dl / %al，不能直接当 32 位源寄存器用，不参与
        {
            m_szStoreKey  = szKey;
            m_bStoreValid = true;
        }
        return;
    }

    // 读：元素宽度由数组声明类型决定
    bool bFloatElem = IsFloatType(q.m_Rslt.m_type);
    if (q.m_Op1.m_kind == OperandKind::OP_VAR && m_sym && q.m_Op1.m_iLink >= 0 &&
        static_cast<std::size_t>(q.m_Op1.m_iLink) < m_sym->m_VarInfoTbl.size())
    {
        bFloatElem = IsFloatType(
            m_sym->m_VarInfoTbl[static_cast<std::size_t>(q.m_Op1.m_iLink)].m_type);
    }
    // 【写入即读回】上一条刚把同一个值写进同一个元素，值还在 %edx / %xmm0 里，
    // 不必再访存读一遍（common subexpression 的一种特例，安全性由「紧邻」保证）。
    if (m_bStoreValid && m_szStoreKey == szKey)
    {
        if (bFloatElem)
        {
            StoreFloat("%xmm0", q.m_Rslt);
        }
        else
        {
            StoreInt("%edx", q.m_Rslt);
        }
        m_bAddrValid  = false;
        m_bStoreValid = false;
        return;
    }
    if (!(m_bAddrValid && m_szAddrKey == szKey))
    {
        LoadAddr(q.m_Op1, "%rax");
        LoadInt(q.m_Op2, "%ecx");
        Emit("movslq %ecx, %rcx");
    }
    if (bFloatElem)
    {
        Emit("movss (%rax,%rcx," + std::to_string(w) + "), %xmm0");
        StoreFloat("%xmm0", q.m_Rslt);
    }
    else if (w == 1)
    {
        Emit("movsbl (%rax,%rcx,1), %eax");
        StoreInt("%eax", q.m_Rslt);
    }
    else
    {
        Emit("movl (%rax,%rcx,4), %eax");
        StoreInt("%eax", q.m_Rslt);
    }
    // 读回把 %rax 覆盖成元素值了，地址不再有效
    m_bAddrValid  = false;
    m_bStoreValid = false;
}

void AsmEmitter::GenReturn(const Quad& q)
{
    // 注意：这里**不能**在函数开头就 ClobberAcc()。RETV 的操作数常常正是
    // 上一条四元式刚算出来、因「免落栈」而没有写回栈槽的临时量，此时它的值
    // 只存在于 %eax 里；提前清掉缓存会让下面的 LoadInt 去读一个从未写过的
    // 栈槽（实测把 `int add(x,y){return x+y;}` 的返回值变成了栈垃圾）。
    if (q.m_Op1.m_kind != OperandKind::OP_NONE)
    {
        if (IsFloatType(q.m_Op1.m_type))
        {
            LoadFloat(q.m_Op1, "%xmm0");
        }
        else
        {
            LoadInt(q.m_Op1, "%eax");
        }
    }
    else
    {
        // 【2026-10-24 修复】裸 `return;`（Q_RETV 的操作数为 OP_NONE）原实现
        // 什么都不发，于是把 %eax/%xmm0 里的残值当返回值带出去。
        // 顶层语句与 void 函数里的裸 return 很常见（既有用例 arg_conv / ret_conv /
        // short_circuit 的顶层结尾都是 `return;`），必须显式清零：
        //   - main 的裸 return 等价于 return 0；
        //   - 其余函数按声明的返回类型清零（float 清 %xmm0，int/char/bool/void 清 %eax）。
        const bool bFloat = m_bCurIsMain ? false : m_bCurRetFloat;
        Emit(bFloat ? "xorps %xmm0, %xmm0" : "xorl %eax, %eax");
        ClobberAcc();                   // 上面直接写了 %eax，缓存作废
    }
    Emit("jmp " + m_szRetLabel);
}

void AsmEmitter::GenQuad(const Quad& q)
{
    // 标志位只在「比较 → 紧邻分支」这一对上有效，不跨四元式存活
    if (m_bFlagsValid && q.m_eOp != QuadOp::Q_JT && q.m_eOp != QuadOp::Q_JNT)
    {
        m_bFlagsValid = false;
    }
    // 地址复用与写入即读回同理：只认「紧邻的下一条 INDEX」，中间隔了别的四元式就作废
    if (q.m_eOp != QuadOp::Q_INDEX_1 && q.m_eOp != QuadOp::Q_INDEX_4 &&
        q.m_eOp != QuadOp::Q_INDEX_4F)
    {
        m_bAddrValid = false;
        m_bStoreValid = false;
    }
    switch (q.m_eOp)
    {
    case QuadOp::Q_NONE:
        return;

    // 2026-10-27：按表 5-2 的「大类」分派，宽度后缀由 GenXXX 从操作数类型取
    // （操作码自带宽度，这里仍按操作数类型选指令，两者一致；见 docs/interface.md 第 25 条）
    case QuadOp::Q_ASSIGN_1:
    case QuadOp::Q_ASSIGN_4:
    case QuadOp::Q_ASSIGN_4F:
    {
        // 【原地更新】运算已经直接做进目标变量，这条赋值是空的
        if (q.m_Op1.m_kind == OperandKind::OP_TEMP &&
            m_computeIntoTemp.count(q.m_Op1.m_iLink) != 0)
        {
            return;
        }
        // 目标/源类型可能不一致：IR 在 int→float 处已插 ITOF，但保险起见仍就地补转换，
        // 避免把整数的位模式直接当浮点数存。
        const bool bFloatDst = IsFloatType(q.m_Rslt.m_type);
        const bool bFloatSrc = IsFloatType(q.m_Op1.m_type);
        if (bFloatDst && bFloatSrc)
        {
            LoadFloat(q.m_Op1, "%xmm0");
            StoreFloat("%xmm0", q.m_Rslt);
        }
        else if (bFloatDst && !bFloatSrc)
        {
            LoadInt(q.m_Op1, "%eax");
            Emit("cvtsi2ss %eax, %xmm0");
            StoreFloat("%xmm0", q.m_Rslt);
        }
        else if (!bFloatDst && bFloatSrc)
        {
            LoadFloat(q.m_Op1, "%xmm0");
            Emit("cvttss2si %xmm0, %eax");
            StoreInt("%eax", q.m_Rslt);
        }
        else
        {
            // 整数 ← 整数常量：直接写内存，省一次「装 %eax 再存」
            long long k = 0;
            if (IntConstOf(q.m_Op1, k))
            {
                StoreImmInt(k, q.m_Rslt);
                ClobberAcc();
                return;
            }
            LoadInt(q.m_Op1, "%eax");
            StoreInt("%eax", q.m_Rslt);
        }
        return;
    }

    // ---- 8~13 类：算术（每个大类都有整型/单精度两个成员，宽度决定选哪条指令）----
    case QuadOp::Q_ADD_4:  case QuadOp::Q_ADD_4F:  GenBinary(q, "addl", "addss");   return;
    case QuadOp::Q_SUB_4:  case QuadOp::Q_SUB_4F:  GenBinary(q, "subl", "subss");   return;
    case QuadOp::Q_MUL_4:  case QuadOp::Q_MUL_4F:  GenBinary(q, "imull", "mulss");  return;
    case QuadOp::Q_DIV_4:  case QuadOp::Q_DIV_4F:  GenBinary(q, "idiv", "divss");   return;

    // ---- 18/19 类：取负 NEG、取非 NOT ----
    case QuadOp::Q_NEG_4:  case QuadOp::Q_NEG_4F:  GenNeg(q);    return;
    case QuadOp::Q_NOT_4:                          GenNot(q);    return;

    case QuadOp::Q_JMP:
        Emit("jmp " + Lbl(q.m_Rslt.m_iLink));
        ClobberAcc();                   // 跳转后 %eax 的状态无法跨基本块推断
        return;
    case QuadOp::Q_JNT:                      // 条件假跳转（原 Q_JZ）
        GenBranch(q, true);
        return;
    case QuadOp::Q_JT:                       // 条件真跳转（原 Q_JNZ）
        GenBranch(q, false);
        return;
    case QuadOp::Q_LABEL:
        EmitRaw(Lbl(q.m_Op1.m_iLink) + ":");
        ClobberAcc();                   // 基本块入口：%eax 可能是任一路径带过来的
        return;

    case QuadOp::Q_RETV:                     // 传递返回值（原 Q_RETURN）
        GenReturn(q);
        return;
    case QuadOp::Q_CALL:
        GenCall(q);
        return;
    case QuadOp::Q_PARA:
        m_pendingArgs.push_back(q.m_Op1);
        return;
    case QuadOp::Q_PRINT_1:
    case QuadOp::Q_PRINT_4:
    case QuadOp::Q_PRINT_4F:
        GenPrint(q);
        return;
    case QuadOp::Q_INDEX_1:
    case QuadOp::Q_INDEX_4:
    case QuadOp::Q_INDEX_4F:
        GenIndex(q);
        return;

    case QuadOp::Q_ITOF:
        LoadInt(q.m_Op1, "%eax");
        Emit("cvtsi2ss %eax, %xmm0");
        StoreFloat("%xmm0", q.m_Rslt);
        return;
    case QuadOp::Q_FTOI:
        LoadFloat(q.m_Op1, "%xmm0");
        Emit("cvttss2si %xmm0, %eax");
        StoreInt("%eax", q.m_Rslt);
        return;

    // ---- 1~6 类：相等/不等/小于/大于/小于等于/大于等于 ----
    case QuadOp::Q_EQU_1:  case QuadOp::Q_EQU_4:  case QuadOp::Q_EQU_4F:
        GenCompare(q, "e", "e");   return;
    case QuadOp::Q_NEQU_1: case QuadOp::Q_NEQU_4: case QuadOp::Q_NEQU_4F:
        GenCompare(q, "ne", "ne"); return;
    case QuadOp::Q_LS_1:   case QuadOp::Q_LS_4:   case QuadOp::Q_LS_4F:
        GenCompare(q, "l", "b");   return;
    case QuadOp::Q_LE_1:   case QuadOp::Q_LE_4:   case QuadOp::Q_LE_4F:
        GenCompare(q, "le", "be"); return;
    case QuadOp::Q_MR_1:   case QuadOp::Q_MR_4:   case QuadOp::Q_MR_4F:
        GenCompare(q, "g", "a");   return;
    case QuadOp::Q_ME_1:   case QuadOp::Q_ME_4:   case QuadOp::Q_ME_4F:
        GenCompare(q, "ge", "ae"); return;

    default:
        return;   // MiniC 不产生的成员（位运算/位移/集合/字符串等）
    }
}

// ----------------------------------------------------------------------------
// 单函数生成
// ----------------------------------------------------------------------------
void AsmEmitter::EmitFunction(const std::string& name, const CodeList& codes, bool bIsMain)
{
    CollectLocals(codes);
    ChooseRegVars(codes);
    AssignOffsets();
    m_pendingArgs.clear();
    m_szRetLabel = ".Lret" + std::to_string(m_iRetSeq++);

    // 当前函数的返回约定（供裸 `return;` 与落空出口补默认值使用）
    m_bCurIsMain   = bIsMain;
    m_bCurRetFloat = false;
    if (!bIsMain && m_sym)
    {
        for (const ProcEntry& pe : m_sym->m_ProcInfoTbl)
        {
            if (pe.m_szName == name)
            {
                m_bCurRetFloat = IsFloatType(pe.m_retType);
                break;
            }
        }
    }

    EmitRaw("");
    EmitRaw("# ----------------------------------------------------------------");
    EmitRaw("# " + name + (bIsMain ? "（顶层语句）" : "（函数）") +
            " 栈帧 " + std::to_string(m_iFrame) + " 字节");
    EmitRaw("# ----------------------------------------------------------------");
    EmitRaw(name + ":");
    Emit("pushq %rbp");
    Emit("movq %rsp, %rbp");
    if (m_iFrame > 0)
    {
        Emit("subq $" + std::to_string(m_iFrame) + ", %rsp");
    }
    if (!m_varInReg.empty())
    {
        // 两个 push 共 16 字节，正好保持调用前的 16 字节栈对齐
        Emit("pushq %rbx");
        Emit("pushq %rsi");
    }

    EmitParams();
    AnalyzeTempReuse(codes);
    ClobberAcc();       // 形参搬运用的是裸 movl，%eax 内容不可信
    m_bFlagsValid = false;
    m_iFlagsTemp  = -1;

    for (const Quad& q : codes)
    {
        GenQuad(q);
    }

    // 统一出口
    //
    // 【2026-10-24 修复】原实现在出口标号「之后」无条件 `xorl %eax, %eax`，
    // 把 `return 5;` 已经装进 %eax 的返回值清零了 —— 实测
    //   `int main() { return 5; }` 编译运行后退出码是 0 而不是 5。
    // 现改为：只有控制流能「落到」出口时才补默认返回值，且必须在标号之前补，
    // 否则显式 return 的 jmp 又会被清零。
    //
    // 判据只看最后一条四元式是不是 Q_RETV：Q_RETV 自带返回值并跳转，
    // 其后没有落空路径；而 Q_JMP **不能**当作不可达判据 —— 循环体末尾回跳到
    // 循环头的 Q_JMP 之后，循环的出口标号仍然会落到函数末尾
    // （实测按 Q_JMP 判定会让 arg_conv / ret_conv / short_circuit 三个用例
    //  main 的返回码变成残值，全部报 run exit≠0）。
    const bool bFallThrough = codes.empty() || codes.back().m_eOp != QuadOp::Q_RETV;
    if (bFallThrough)
    {
        // 落空出口时的默认值：main 返回 0；非 main 取过程表里的返回类型，
        // 浮点用 %xmm0，其余（int/char/bool/void）用 %eax。
        // 这样 `int f() { }` 这类无 return 的函数也返回确定的 0，而不是 %eax 里的残值
        // （实测修复前 `int f() { } print(f());` 输出 2043900272）。
        Emit(m_bCurRetFloat ? "xorps %xmm0, %xmm0" : "xorl %eax, %eax");
    }
    EmitRaw(m_szRetLabel + ":");
    if (!m_varInReg.empty())
    {
        Emit("popq %rsi");
        Emit("popq %rbx");
    }
    Emit("movq %rbp, %rsp");
    Emit("popq %rbp");
    Emit("ret");
}

// ----------------------------------------------------------------------------
// 主流程
// ----------------------------------------------------------------------------
void AsmEmitter::Run(AsmOutput& out)
{
    out.Clear();

    // ---- 1. 数据段：全局变量与全局数组 ----
    EmitGlobals();

    // ---- 2. 代码段：入口 main + 各过程 ----
    // 【2026-10-24 修复】用户可能自己写 `int main() { ... }`
    // （《MiniC语法支持清单》7.1~7.3 的示例全是这种写法）。原实现无条件把顶层语句
    // 编成 main，随后又为 ProcInfoTbl 里每个过程各编一份，于是 `int main(){...}`
    // 会产出两个 `main:` 标号，as 直接报
    //     Error: symbol `main' is already defined
    // 现改为：若用户定义了 main，就把顶层语句接在它的函数体之前，合并成唯一的 main
    // （顶层语句先执行，语义上等价于「入口代码」）；未定义 main 时行为完全不变。
    const ProcEntry* userMain = nullptr;
    if (m_sym)
    {
        for (const ProcEntry& pe : m_sym->m_ProcInfoTbl)
        {
            if (pe.m_szName == "main")
            {
                userMain = &pe;
                break;
            }
        }
    }

    if (userMain != nullptr)
    {
        CodeList merged = m_top;
        merged.insert(merged.end(), userMain->m_Codes.begin(), userMain->m_Codes.end());
        EmitFunction("main", merged, true);
    }
    else
    {
        EmitFunction("main", m_top, true);
    }

    if (m_sym)
    {
        for (const ProcEntry& pe : m_sym->m_ProcInfoTbl)
        {
            if (userMain != nullptr && &pe == userMain)
            {
                continue;                   // 已合并进入口 main，不重复输出
            }
            // 【2026-10-24 修复】不能因为 m_Codes 为空就跳过整个函数体。
            // 空函数（void f() { }）没有任何四元式，若跳过则符号 f 在汇编里
            // 根本没有定义，而调用点仍然会生成 `call f`，链接期报
            //   ld: undefined reference to `f'
            // （实测 `void nothing() { } nothing(); print(1);` 汇编链接失败）。
            // ProcEntry 只为「已定义的函数」创建（MiniC 不支持函数原型），
            // 因此这里必须无条件为每个 ProcEntry 输出一份函数体。
            EmitFunction(pe.m_szName, pe.m_Codes, false);
        }
    }

    // ---- 3. 收尾：补齐用到的格式串 ----
    // 注意：格式串里**不带** '\n'。print 只输出值本身，连续两条 print 会接在
    // 同一行上（print(a); print(b); → "ab"）。要换行就自己写 print('\n')。
    // 这样 print 的语义与 C 的 printf 一致，而换行变成显式可控的行为。
    if (m_bUseIntFmt)
    {
        m_data.push_back("    .align 4");
        m_data.push_back(".LCfmt_i:");
        m_data.push_back("    .string \"%d\"");
    }
    if (m_bUseFltFmt)
    {
        m_data.push_back("    .align 4");
        m_data.push_back(".LCfmt_f:");
        m_data.push_back("    .string \"%g\"");
    }
    if (m_bUseChrFmt)
    {
        m_data.push_back("    .align 4");
        m_data.push_back(".LCfmt_c:");
        m_data.push_back("    .string \"%c\"");
    }

    // ---- 4. 装配 AsmOutput 三段 ----
    out.m_FileHeader.push_back("# ================================================================");
    out.m_FileHeader.push_back("# MiniC 编译器 —— 目标代码（x86-64 / GNU as / Windows MinGW）");
    out.m_FileHeader.push_back("# 由 CCodeGen::IRtoASM 生成：朴素栈式翻译，逐四元式 load-运算-store");
    out.m_FileHeader.push_back("# 入口：global main；调用约定：栈传参（自定义），printf 走 Win64 ABI");
    out.m_FileHeader.push_back("# ================================================================");
    out.m_FileHeader.push_back("    .globl main");

    out.m_DataSection.push_back("");
    out.m_DataSection.push_back("# ---------------- 数据段 ----------------");
    out.m_DataSection.push_back("    .data");
    for (const std::string& s : m_data)
    {
        out.m_DataSection.push_back(s);
    }
    out.m_DataSection.push_back("");
    out.m_DataSection.push_back("    .bss");
    for (const std::string& s : m_bss)
    {
        out.m_DataSection.push_back(s);
    }

    out.m_CodeSection.push_back("");
    out.m_CodeSection.push_back("# ---------------- 代码段 ----------------");
    out.m_CodeSection.push_back("    .text");
    for (const std::string& s : m_text)
    {
        out.m_CodeSection.push_back(s);
    }
    out.m_CodeSection.push_back("");
    out.m_CodeSection.push_back("# ---- end of generated code ----");
}

} // namespace

// ============================================================================
// CCodeGen 对外实现
// ============================================================================
CCodeGen::CCodeGen(const CodeList& codes)
    : m_codes(codes)
{
}

void CCodeGen::BindSymTab(const SymbolTable& symtab)
{
    m_symtab = &symtab;
}

bool CCodeGen::Generate(AsmOutput& out)
{
    // 挑战版入口：绑定 IR 输入已由构造完成，生成汇编文本
    return IRtoASM(m_codes, out);
}

bool CCodeGen::IRtoASM(const CodeList& codes, AsmOutput& out)
{
    AsmEmitter emitter(codes, m_symtab);
    emitter.Run(out);
    return true;
}
