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

    // --- 调用现场 ---
    std::vector<Operand> m_pendingArgs;         // Q_PARA 暂存（Q_CALL 时统一落栈）

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

    // --- 四元式翻译 ---
    void GenQuad(const Quad& q);
    void GenBinary(const Quad& q, const char* iOp, const char* fOp);
    void GenCompare(const Quad& q, const char* iCc, const char* fCc);
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
        const int bytes = v.m_bIsArray ? 4 * (v.m_iArraySize > 0 ? v.m_iArraySize : 1) : 8;
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
        int bytes = v.m_bIsArray ? 4 * (v.m_iArraySize > 0 ? v.m_iArraySize : 1) : 8;
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

void AsmEmitter::LoadInt(const Operand& op, const std::string& reg)
{
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
    LoadInt(q.m_Op1, "%eax");
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
        LoadInt(q.m_Op2, "%ecx");
        Emit("cmpl %ecx, %eax");
        Emit(std::string("set") + iCc + " %al");
    }
    Emit("movzbl %al, %eax");
    StoreInt("%eax", q.m_Rslt);
}

void AsmEmitter::GenBranch(const Quad& q, bool bJumpIfZero)
{
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
    const bool bStore = (q.m_Rslt.m_kind == OperandKind::OP_VAR && m_sym &&
                         q.m_Rslt.m_iLink >= 0 &&
                         static_cast<std::size_t>(q.m_Rslt.m_iLink) < m_sym->m_VarInfoTbl.size() &&
                         m_sym->m_VarInfoTbl[static_cast<std::size_t>(q.m_Rslt.m_iLink)].m_bIsArray);

    if (bStore)
    {
        const int w = ElemWidth(q.m_Rslt);
        LoadAddr(q.m_Rslt, "%rax");
        LoadInt(q.m_Op2, "%ecx");
        Emit("movslq %ecx, %rcx");
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
        return;
    }

    // 读：元素宽度由数组声明类型决定
    const int w = ElemWidth(q.m_Op1);
    bool bFloatElem = IsFloatType(q.m_Rslt.m_type);
    if (q.m_Op1.m_kind == OperandKind::OP_VAR && m_sym && q.m_Op1.m_iLink >= 0 &&
        static_cast<std::size_t>(q.m_Op1.m_iLink) < m_sym->m_VarInfoTbl.size())
    {
        bFloatElem = IsFloatType(
            m_sym->m_VarInfoTbl[static_cast<std::size_t>(q.m_Op1.m_iLink)].m_type);
    }
    LoadAddr(q.m_Op1, "%rax");
    LoadInt(q.m_Op2, "%ecx");
    Emit("movslq %ecx, %rcx");
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
}

void AsmEmitter::GenReturn(const Quad& q)
{
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
    Emit("jmp " + m_szRetLabel);
}

void AsmEmitter::GenQuad(const Quad& q)
{
    switch (q.m_eOp)
    {
    case QuadOp::Q_NONE:
        return;

    case QuadOp::Q_ASSIGN:
    {
        // 目标/源类型可能不一致：中间代码在 int→float 处未插 Q_ITOF（IR 已知缺陷），
        // 此处按两端类型就地补转换，避免把整数的位模式直接当浮点数存。
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
            LoadInt(q.m_Op1, "%eax");
            StoreInt("%eax", q.m_Rslt);
        }
        return;
    }

    case QuadOp::Q_ADD: GenBinary(q, "addl", "addss"); return;
    case QuadOp::Q_SUB: GenBinary(q, "subl", "subss"); return;
    case QuadOp::Q_MUL: GenBinary(q, "imull", "mulss"); return;
    case QuadOp::Q_DIV: GenBinary(q, "idiv", "divss"); return;

    case QuadOp::Q_JMP:
        Emit("jmp " + Lbl(q.m_Rslt.m_iLink));
        return;
    case QuadOp::Q_JZ:
        GenBranch(q, true);
        return;
    case QuadOp::Q_JNZ:
        GenBranch(q, false);
        return;
    case QuadOp::Q_LABEL:
        EmitRaw(Lbl(q.m_Op1.m_iLink) + ":");
        return;

    case QuadOp::Q_RETURN:
        GenReturn(q);
        return;
    case QuadOp::Q_CALL:
        GenCall(q);
        return;
    case QuadOp::Q_PARA:
        m_pendingArgs.push_back(q.m_Op1);
        return;
    case QuadOp::Q_PRINT:
        GenPrint(q);
        return;
    case QuadOp::Q_INDEX:
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

    case QuadOp::Q_EQ: GenCompare(q, "e", "e");   return;
    case QuadOp::Q_NE: GenCompare(q, "ne", "ne"); return;
    case QuadOp::Q_LT: GenCompare(q, "l", "b");   return;
    case QuadOp::Q_LE: GenCompare(q, "le", "be"); return;
    case QuadOp::Q_GT: GenCompare(q, "g", "a");   return;
    case QuadOp::Q_GE: GenCompare(q, "ge", "ae"); return;
    }
}

// ----------------------------------------------------------------------------
// 单函数生成
// ----------------------------------------------------------------------------
void AsmEmitter::EmitFunction(const std::string& name, const CodeList& codes, bool bIsMain)
{
    CollectLocals(codes);
    AssignOffsets();
    m_pendingArgs.clear();
    m_szRetLabel = ".Lret" + std::to_string(m_iRetSeq++);

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

    EmitParams();

    for (const Quad& q : codes)
    {
        GenQuad(q);
    }

    // 统一出口
    EmitRaw(m_szRetLabel + ":");
    if (bIsMain)
    {
        Emit("xorl %eax, %eax");        // main 默认返回 0
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

    // ---- 2. 代码段：main（顶层语句）+ 各过程 ----
    EmitFunction("main", m_top, true);

    if (m_sym)
    {
        for (const ProcEntry& pe : m_sym->m_ProcInfoTbl)
        {
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
    if (m_bUseIntFmt)
    {
        m_data.push_back("    .align 4");
        m_data.push_back(".LCfmt_i:");
        m_data.push_back("    .string \"%d\\n\"");
    }
    if (m_bUseFltFmt)
    {
        m_data.push_back("    .align 4");
        m_data.push_back(".LCfmt_f:");
        m_data.push_back("    .string \"%g\\n\"");
    }
    if (m_bUseChrFmt)
    {
        m_data.push_back("    .align 4");
        m_data.push_back(".LCfmt_c:");
        m_data.push_back("    .string \"%c\\n\"");
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
