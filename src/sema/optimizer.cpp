// ============================================================================
// MiniC 编译器 —— 四元式窥孔优化实现
// ----------------------------------------------------------------------------
// 保守原则（每一条都能对上「删掉它为什么不改变行为」）：
//   1) 常量条件跳转：cond 的值在编译期取到 0，则 JNT 必跳（改成 JMP）、
//      JT 必不跳（整条删掉）。条件非 0 不折叠 —— 非 0 的判断需要常量表的
//      类型位（浮点 0.0 与整型 0 的 m_iValue 都是 0），只在整型常量上做，
//      见 ConstantIsZero。
//   2) 不可达代码：JMP / RETV 之后、下一个 LABEL 之前的四元式永远执行不到。
//      这一段里可能有 PRINT / CALL，但执行不到就是执行不到，删掉安全。
//   3) 空跳：JMP L 紧跟 LABEL L，等价于什么都不做。
//   4) 死临时量：纯计算（关系 / 算术 / 取负 / 取非 / 赋值 / ITOF / FTOI /
//      INDEX）的结果临时量在整段里没有任何出现为源操作数的地方 → 删。
//      只对 OP_TEMP 生效，变量（OP_VAR）绝不删 —— 变量可能被后续语句读，
//      而临时量按定义只在本段内使用。
//   5) 死标号：没有任何跳转指令指向的 LABEL 定义 → 删。必须排除 LABEL
//      自身的 Op1（它「定义」自己，不算被使用）。
//   6) 基本块内的常量传播 / 公共子表达式 / 死存储（见 BlockOptimize）：
//      先把序列按「标号 / 无条件或条件转移之后」切成基本块，块内没有跳转
//      进入，所以「某个变量此刻一定是常量」「某个表达式已经算过」这类状态
//      可以安全地向前传播；块边界与 CALL 处一律清空全部状态。
// 不做的事（留给后续）：循环不变式、跨块数据流、寄存器分配。
// ============================================================================
#include "sema/optimizer.h"

#include <map>
#include <set>
#include <string>

namespace minic_opt
{
namespace
{

// 条件操作数是「整型常量 0」吗？Operand.m_iLink 对 OP_CONST 是常量表下标，
// 必须查表才能拿到真实取值，不能直接拿 m_iLink 当值用。
bool ConstantIsZero(const Operand& op, const SymbolTable& symtab, bool& bKnown)
{
    bKnown = false;
    if (op.m_kind != OperandKind::OP_CONST || op.m_iLink < 0 ||
        static_cast<std::size_t>(op.m_iLink) >= symtab.m_ConstInfoTbl.size())
    {
        return false;
    }
    const ConstEntry& ce = symtab.m_ConstInfoTbl[static_cast<std::size_t>(op.m_iLink)];
    // 浮点常量不参与：0.0 与 0 在 m_iValue 上都可能是 0，但「浮点 0 当条件」
    // 的语义（MiniC 里 if 只对整数语义定义）不该由优化器来替语言定规矩。
    if (ce.m_bIsFloat || ce.m_constType == TokenType::TOKEN_FLOAT ||
        ce.m_constType == TokenType::TOKEN_NUM_FLOAT)
    {
        bKnown = false;
        return false;
    }
    bKnown = true;
    return ce.m_iValue == 0;
}

// 纯计算：有结果、无副作用。被删的唯一前提是结果没人读。
bool IsPureCompute(QuadOp op)
{
    const int cat = QuadCatOf(op);
    if (cat >= static_cast<int>(QuadCat::CAT_EQU) &&
        cat <= static_cast<int>(QuadCat::CAT_ASSIGN))
    {
        // 关系（EQU…ME）/ 算术（SUB…NOT）/ 赋值（ASSIGN）
        return true;
    }
    switch (op)
    {
    case QuadOp::Q_ITOF:
    case QuadOp::Q_FTOI:
    case QuadOp::Q_INDEX_1:
    case QuadOp::Q_INDEX_4:
    case QuadOp::Q_INDEX_4F:
        return true;
    default:
        return false;
    }
}

// 「后续代码不可达」的转移：只有无条件跳转与返回。
// **条件跳转不能算**：JNT cond -> L 是「条件为假才跳」，条件为真时会继续往下走，
// 紧随其后的就是 if 的真分支。第一版把它算进来，结果优化后
// `if (a > 3) { print(1); } else { print(2); }` 打出了 2 —— 真分支被当成
// 不可达删掉了（实测 16 个用例输出变化，recur / user_main 直接栈溢出崩掉）。
bool EndsBlock(QuadOp op)
{
    return op == QuadOp::Q_JMP || op == QuadOp::Q_RETV;
}

bool IsConditional(QuadOp op)
{
    return op == QuadOp::Q_JT || op == QuadOp::Q_JNT;
}

std::string TempKey(const Operand& op)
{
    if (op.m_kind != OperandKind::OP_TEMP || op.m_iLink < 0)
    {
        return std::string();
    }
    return "_T" + std::to_string(op.m_iLink);
}

std::string LabelKey(const Operand& op)
{
    if (op.m_kind != OperandKind::OP_LABEL || op.m_iLink < 0)
    {
        return std::string();
    }
    return "L" + std::to_string(op.m_iLink);
}

// 操作数的身份键：种类 + 表内位序。用于「同一个变量 / 同一个临时量」判定。
std::string OperandKey(const Operand& op)
{
    switch (op.m_kind)
    {
    case OperandKind::OP_CONST: return "C" + std::to_string(op.m_iLink);
    case OperandKind::OP_VAR:   return "V" + std::to_string(op.m_iLink);
    case OperandKind::OP_TEMP:  return "T" + std::to_string(op.m_iLink);
    case OperandKind::OP_LABEL: return "L" + std::to_string(op.m_iLink);
    default:                    return std::string();
    }
}

// 控制流在这里断开：块内状态不能跨过去。
bool EndsFlow(QuadOp op)
{
    return op == QuadOp::Q_JMP || op == QuadOp::Q_RETV ||
           op == QuadOp::Q_JT || op == QuadOp::Q_JNT;
}

// 二元纯计算：关系（EQU…ME）与算术（SUB…AND）。这两类没有副作用，
// 同样的操作数算两次结果必然相同，可以复用前一次的结果临时量。
bool IsBinaryCompute(QuadOp op)
{
    const int cat = QuadCatOf(op);
    return cat >= static_cast<int>(QuadCat::CAT_EQU) &&
           cat <= static_cast<int>(QuadCat::CAT_AND);
}

bool IsAssign(QuadOp op)
{
    return op == QuadOp::Q_ASSIGN_1 || op == QuadOp::Q_ASSIGN_4 ||
           op == QuadOp::Q_ASSIGN_4F;
}

// 用哪条 ASSIGN 搬运 CSE 复用的结果。
// 关系运算的结果是 int（0/1），哪怕操作数是浮点（LS_4F 比完给的是整数），
// 所以关系类固定用 ASSIGN_4；算术类才跟着操作数宽度与浮点标记走。
QuadOp AssignOpOf(QuadOp op)
{
    const int cat = QuadCatOf(op);
    if (cat >= static_cast<int>(QuadCat::CAT_EQU) &&
        cat <= static_cast<int>(QuadCat::CAT_ME))
    {
        return QuadOp::Q_ASSIGN_4;
    }
    if (QuadOpIsFloat(op))
    {
        return QuadOp::Q_ASSIGN_4F;
    }
    if (QuadOpByteWidth(op) == 1)
    {
        return QuadOp::Q_ASSIGN_1;
    }
    return QuadOp::Q_ASSIGN_4;
}

struct CseEntry
{
    Operand val;   // 之前算出的结果临时量
    int     e1;    // 当时 Op1 的写入代数
    int     e2;    // 当时 Op2 的写入代数
};

// 一个基本块内的优化：常量传播 + 公共子表达式 + 死存储。
// 三条都依赖同一个前提 —— 块内没有跳转进入，执行是严格顺序的。
void OptimizeBlock(const CodeList& src, std::size_t beg, std::size_t end,
                   OptStats& st, CodeList& out)
{
    std::map<std::string, Operand>  constOf;    // 变量 → 已知整型常量
    std::map<std::string, int>      epoch;      // 变量/临时量 → 写入代数
    std::map<std::string, CseEntry> cse;        // 表达式键 → 已算出的结果
    std::map<std::string, std::size_t> pending; // 变量 → 上一次赋值在 out 里的位置

    for (std::size_t k = beg; k < end; ++k)
    {
        Quad q = src[k];
        const QuadOp op = q.m_eOp;

        // 调用可能改任何全局量：跨语句的假设全部作废。
        if (op == QuadOp::Q_CALL)
        {
            constOf.clear();
            epoch.clear();
            cse.clear();
            pending.clear();
        }

        // 先按「原始操作数」登记读取：读过就不再是「被覆盖的死存储」。
        for (const Operand* p : {&q.m_Op1, &q.m_Op2})
        {
            if (p->m_kind == OperandKind::OP_VAR)
            {
                pending.erase(OperandKey(*p));
            }
        }

        // ---- 常量传播 ----
        for (Operand* p : {&q.m_Op1, &q.m_Op2})
        {
            if (p->m_kind != OperandKind::OP_VAR)
            {
                continue;
            }
            const auto it = constOf.find(OperandKey(*p));
            if (it != constOf.end())
            {
                *p = it->second;
                st.iConstProp++;
            }
        }

        // ---- 公共子表达式 ----
        if (IsBinaryCompute(op))
        {
            const std::string key = QuadOpName(op) + "|" +
                                    OperandKey(q.m_Op1) + "|" + OperandKey(q.m_Op2);
            const auto it = cse.find(key);
            if (it != cse.end() &&
                epoch[OperandKey(q.m_Op1)] == it->second.e1 &&
                epoch[OperandKey(q.m_Op2)] == it->second.e2)
            {
                // 同样的输入算过一遍，直接搬前一次的结果
                q.m_eOp = AssignOpOf(op);
                q.m_Op1 = it->second.val;
                q.m_Op2 = Operand();
                st.iCse++;
            }
            else
            {
                CseEntry e;
                e.val = q.m_Rslt;
                e.e1  = epoch[OperandKey(q.m_Op1)];
                e.e2  = epoch[OperandKey(q.m_Op2)];
                cse[key] = e;
            }
        }

        // ---- 写入登记 ----
        if (q.m_Rslt.m_kind == OperandKind::OP_VAR ||
            q.m_Rslt.m_kind == OperandKind::OP_TEMP)
        {
            const std::string vk = OperandKey(q.m_Rslt);
            epoch[vk] += 1;
            constOf.erase(vk);

            if (q.m_Rslt.m_kind == OperandKind::OP_VAR)
            {
                if (IsAssign(q.m_eOp))
                {
                    // 上一次赋值到这次赋值之间没人读过 → 上一次是死存储
                    const auto it = pending.find(vk);
                    if (it != pending.end())
                    {
                        out[it->second].m_eOp = QuadOp::Q_NONE;
                        st.iDeadStore++;
                    }
                    pending[vk] = out.size();
                }
                else
                {
                    // 数组元素写入等：地址基址被读过，不能当死存储处理
                    pending.erase(vk);
                }
            }

            // 整型常量赋值才记进常量表视图（浮点不参与，理由同 ConstantIsZero）
            if (q.m_eOp == QuadOp::Q_ASSIGN_4 && q.m_Op1.m_kind == OperandKind::OP_CONST)
            {
                constOf[vk] = q.m_Op1;
            }
        }

        out.push_back(q);
    }
}

// 把序列切成基本块，逐块优化。
void BlockOptimize(const CodeList& codes, OptStats& st, CodeList& out)
{
    const std::size_t n = codes.size();
    std::size_t i = 0;
    while (i < n)
    {
        std::size_t j = i + 1;
        while (j < n && codes[j].m_eOp != QuadOp::Q_LABEL && !EndsFlow(codes[j - 1].m_eOp))
        {
            ++j;
        }
        OptimizeBlock(codes, i, j, st, out);
        i = j;
    }
}

} // namespace

// ----------------------------------------------------------------------------
// Optimize
// ----------------------------------------------------------------------------
namespace
{

// 完整跑一遍 0-4 轮。外层会反复调用直到不再有变化。
void RunOnce(CodeList& codes, const SymbolTable& symtab, OptStats& st)
{

    // ---- 第 0 轮：常量条件跳转 ----
    // 只认 Q_JT / Q_JNT；条件为整型常量 0 才折叠。
    for (auto& q : codes)
    {
        if (!IsConditional(q.m_eOp))
        {
            continue;
        }
        bool bKnown = false;
        if (!ConstantIsZero(q.m_Op1, symtab, bKnown) || !bKnown)
        {
            continue;
        }
        if (q.m_eOp == QuadOp::Q_JNT)
        {
            // 条件恒假 → 一定跳
            q.m_eOp = QuadOp::Q_JMP;
            q.m_Op1 = Operand();
        }
        else
        {
            // 条件恒假 → JT 一定不跳，整条删掉
            q.m_eOp = QuadOp::Q_NONE;
        }
        st.iFoldedBranch++;
    }

    // ---- 第 1 轮：删不可达 ----
    CodeList out;
    out.reserve(codes.size());
    bool bReachable = true;
    for (const auto& q : codes)
    {
        if (q.m_eOp == QuadOp::Q_NONE)
        {
            continue;   // 第 0 轮标记删除
        }
        if (q.m_eOp == QuadOp::Q_LABEL)
        {
            bReachable = true;   // 标号是跳转目标，重新可达
        }
        if (!bReachable)
        {
            st.iUnreachable++;
            continue;
        }
        if (EndsBlock(q.m_eOp))
        {
            bReachable = false;
        }
        out.push_back(q);
    }

    // ---- 第 2 轮：空跳 JMP L; LABEL L ----
    for (std::size_t i = 0; i + 1 < out.size(); ++i)
    {
        if (out[i].m_eOp != QuadOp::Q_JMP || out[i + 1].m_eOp != QuadOp::Q_LABEL)
        {
            continue;
        }
        // **跳转目标在 Rslt 不在 Op1**：四元式打印成 (JMP, -, -, L2)，
        // 前两个字段是空操作数。第一版查 Op1，于是这一轮从来没命中过
        // （-O 的统计里 jumps 恒为 0 就是这个原因）。两处都查，兼容两种形态。
        const Operand& tgt = (out[i].m_Rslt.m_kind == OperandKind::OP_LABEL)
                                 ? out[i].m_Rslt
                                 : out[i].m_Op1;
        if (tgt.m_kind == OperandKind::OP_LABEL &&
            tgt.m_iLink == out[i + 1].m_Op1.m_iLink)
        {
            out[i].m_eOp = QuadOp::Q_NONE;
            st.iRedundantJump++;
        }
    }
    CodeList stage2;
    stage2.reserve(out.size());
    for (const auto& q : out)
    {
        if (q.m_eOp == QuadOp::Q_NONE)
        {
            continue;
        }
        stage2.push_back(q);
    }

    // ---- 第 3 轮：死临时量 + 死标号（先统计，再一次性删） ----
    std::set<std::string> liveTemp;    // 有人读的临时量
    std::set<std::string> usedLabel;   // 有跳转指令指向的标号
    for (const auto& q : stage2)
    {
        const std::string s1 = TempKey(q.m_Op1);
        const std::string s2 = TempKey(q.m_Op2);
        if (!s1.empty())
        {
            liveTemp.insert(s1);
        }
        if (!s2.empty())
        {
            liveTemp.insert(s2);
        }
        // 非纯计算指令读的是「值」：结果操作数里若放的是临时量，也要留
        // （例如 RETV _T3 把 _T3 当返回值；虽然它在 Op1 里已计入，但
        //   PRINT/INDEX 等把结果放 Rslt 的形态也要覆盖）。
        if (!IsPureCompute(q.m_eOp) && q.m_eOp != QuadOp::Q_LABEL)
        {
            const std::string r = TempKey(q.m_Rslt);
            if (!r.empty())
            {
                liveTemp.insert(r);
            }
        }
        // 跳转目标：LABEL 定义点不算「被使用」，必须排除，否则死标号删不掉。
        if (q.m_eOp != QuadOp::Q_LABEL)
        {
            for (const Operand* p : {&q.m_Op1, &q.m_Op2, &q.m_Rslt})
            {
                const std::string l = LabelKey(*p);
                if (!l.empty())
                {
                    usedLabel.insert(l);
                }
            }
        }
    }

    CodeList stage3;
    stage3.reserve(stage2.size());
    for (const auto& q : stage2)
    {
        if (IsPureCompute(q.m_eOp))
        {
            const std::string r = TempKey(q.m_Rslt);
            if (!r.empty() && liveTemp.find(r) == liveTemp.end())
            {
                st.iDeadTemp++;
                continue;   // 结果临时量没人读，且本身无副作用 → 删
            }
        }
        if (q.m_eOp == QuadOp::Q_LABEL)
        {
            const std::string l = LabelKey(q.m_Op1);
            if (!l.empty() && usedLabel.find(l) == usedLabel.end())
            {
                st.iDeadLabel++;
                continue;   // 没人跳到这个标号
            }
        }
        stage3.push_back(q);
    }

    // ---- 第 4 轮：基本块内的常量传播 / 公共子表达式 / 死存储 ----
    CodeList stage4;
    stage4.reserve(stage3.size());
    BlockOptimize(stage3, st, stage4);
    CodeList stage5;
    stage5.reserve(stage4.size());
    for (const auto& q : stage4)
    {
        if (q.m_eOp != QuadOp::Q_NONE)
        {
            stage5.push_back(q);
        }
    }

    // ---- 第 6 轮：循环不变量外提（LICM） ----
    // 只处理「最内层」循环：区间内不再包含别的循环。外层留到下一遍 RunOnce ——
    // 那时内层提上来的部分可能又变成了外层的循环不变量。
    //
    // 判据（宁可不提也不提错）：
    //   1. 只提纯算术 / 关系 / int↔float 转换，结果必须是临时量；
    //      除法与取模一律不动 —— 除数可能是 0，把计算提到循环外，就把
    //      「循环一次都没进」变成「一进来就除零」，那是可观测的行为改变。
    //   2. 操作数只能是：常量，或循环体内从未被写过的变量，或已被提出去的临时量。
    //   3. 循环体内不能有 CALL —— 调用可能改写任何全局量，变量就不再恒定。
    //   4. 该临时量在循环体内只能被定义这一次。
    // 提到哪：循环入口那个 LABEL 之前，也就是前导块（preheader）的末尾 ——
    // 所有进入循环的路径都从 LABEL 过，插在它前面就一定先于循环体执行。
    CodeList stage6;
    stage6.reserve(stage5.size());
    {
        const std::size_t n = stage5.size();
        std::map<int, std::size_t> labelAt;
        for (std::size_t i = 0; i < n; ++i)
        {
            if (stage5[i].m_eOp == QuadOp::Q_LABEL &&
                stage5[i].m_Op1.m_kind == OperandKind::OP_LABEL &&
                stage5[i].m_Op1.m_iLink >= 0)
            {
                labelAt[stage5[i].m_Op1.m_iLink] = i;
            }
        }

        struct LoopSpan
        {
            std::size_t beg;   // 入口 LABEL 的位置
            std::size_t end;   // 回到该 LABEL 的 JMP 的位置
        };
        std::vector<LoopSpan> loops;
        for (std::size_t i = 0; i < n; ++i)
        {
            if (stage5[i].m_eOp != QuadOp::Q_JMP)
            {
                continue;
            }
            // 跳转目标在 Rslt（(JMP, -, -, L2)），兼容放在 Op1 的形态
            const Operand& tgt = (stage5[i].m_Rslt.m_kind == OperandKind::OP_LABEL)
                                     ? stage5[i].m_Rslt
                                     : stage5[i].m_Op1;
            if (tgt.m_kind != OperandKind::OP_LABEL)
            {
                continue;
            }
            const auto it = labelAt.find(tgt.m_iLink);
            if (it != labelAt.end() && it->second < i)
            {
                LoopSpan sp;
                sp.beg = it->second;
                sp.end = i;
                loops.push_back(sp);
            }
        }

        std::vector<LoopSpan> inner;
        for (std::size_t a = 0; a < loops.size(); ++a)
        {
            bool bContains = false;
            for (std::size_t b = 0; b < loops.size(); ++b)
            {
                if (a != b && loops[a].beg < loops[b].beg && loops[b].end < loops[a].end)
                {
                    bContains = true;
                    break;
                }
            }
            if (!bContains)
            {
                inner.push_back(loops[a]);
            }
        }

        std::map<std::size_t, std::vector<Quad> > hoistAt;
        std::set<std::size_t> hoistedIdx;
        for (const auto& sp : inner)
        {
            bool bHasCall = false;
            std::set<std::string> written;
            for (std::size_t k = sp.beg; k <= sp.end; ++k)
            {
                if (stage5[k].m_eOp == QuadOp::Q_CALL)
                {
                    bHasCall = true;
                }
                if (stage5[k].m_Rslt.m_kind == OperandKind::OP_VAR)
                {
                    written.insert(OperandKey(stage5[k].m_Rslt));
                }
            }
            if (bHasCall)
            {
                continue;
            }

            std::map<std::string, int> defCount;
            for (std::size_t k = sp.beg; k <= sp.end; ++k)
            {
                if (!IsPureCompute(stage5[k].m_eOp))
                {
                    continue;
                }
                const std::string t = TempKey(stage5[k].m_Rslt);
                if (!t.empty())
                {
                    defCount[t]++;
                }
            }

            std::set<std::string> hoisted;
            for (std::size_t k = sp.beg + 1; k < sp.end; ++k)
            {
                const Quad& q = stage5[k];
                if (!IsPureCompute(q.m_eOp))
                {
                    continue;
                }
                const int cat = QuadCatOf(q.m_eOp);
                if (cat == static_cast<int>(QuadCat::CAT_DIV) ||
                    cat == static_cast<int>(QuadCat::CAT_MOD))
                {
                    continue;   // 可能除零，不提
                }
                if (q.m_eOp == QuadOp::Q_INDEX_1 || q.m_eOp == QuadOp::Q_INDEX_4 ||
                    q.m_eOp == QuadOp::Q_INDEX_4F ||
                    q.m_eOp == QuadOp::Q_ASSIGN_1 || q.m_eOp == QuadOp::Q_ASSIGN_4 ||
                    q.m_eOp == QuadOp::Q_ASSIGN_4F)
                {
                    continue;   // 读内存 / 写变量，不提
                }
                const std::string t = TempKey(q.m_Rslt);
                if (t.empty() || defCount[t] != 1)
                {
                    continue;
                }
                bool bInvariant = true;
                for (const Operand* p : {&q.m_Op1, &q.m_Op2})
                {
                    if (p->m_kind == OperandKind::OP_NONE || p->m_kind == OperandKind::OP_CONST)
                    {
                        continue;
                    }
                    if (p->m_kind == OperandKind::OP_VAR)
                    {
                        if (written.count(OperandKey(*p)) != 0)
                        {
                            bInvariant = false;
                            break;
                        }
                        continue;
                    }
                    if (p->m_kind == OperandKind::OP_TEMP && hoisted.count(OperandKey(*p)) != 0)
                    {
                        continue;
                    }
                    bInvariant = false;
                    break;
                }
                if (!bInvariant)
                {
                    continue;
                }
                hoistAt[sp.beg].push_back(q);
                hoistedIdx.insert(k);
                hoisted.insert(t);
                st.iHoisted++;
            }
        }

        for (std::size_t i = 0; i < n; ++i)
        {
            const auto it = hoistAt.find(i);
            if (it != hoistAt.end())
            {
                for (const auto& q : it->second)
                {
                    stage6.push_back(q);
                }
            }
            if (hoistedIdx.count(i) != 0)
            {
                continue;
            }
            stage6.push_back(stage5[i]);
        }
    }

    codes.swap(stage6);
}

} // namespace

// ----------------------------------------------------------------------------
// Optimize
// ----------------------------------------------------------------------------
// 迭代到不动点：常量传播会把变量读换成常量、CSE 会把重复表达式换成搬运，
// 这两件事本身不减少四元式条数，但会让「结果没人读的临时量」「条件恒假的
// 跳转」在下一轮浮现出来。所以整体最多跑 3 遍，直到一轮下来什么都没变。
// 每一轮内部的判断都是保守的，重复执行不会改变结论，因此收敛即可。
void Optimize(CodeList& codes, const SymbolTable& symtab, OptStats& st)
{
    if (codes.empty())
    {
        return;
    }
    for (int iPass = 0; iPass < 3; ++iPass)
    {
        const int iBefore = st.Total();
        RunOnce(codes, symtab, st);
        if (st.Total() == iBefore)
        {
            break;
        }
    }
}

} // namespace minic_opt
