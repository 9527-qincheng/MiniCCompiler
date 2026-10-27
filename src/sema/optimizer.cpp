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
// 不做的事（留给后续）：常量传播、公共子表达式、循环不变式、寄存器分配。
// ============================================================================
#include "sema/optimizer.h"

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

} // namespace

// ----------------------------------------------------------------------------
// Optimize
// ----------------------------------------------------------------------------
void Optimize(CodeList& codes, const SymbolTable& symtab, OptStats& st)
{
    if (codes.empty())
    {
        return;
    }

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
        if (out[i].m_Op1.m_kind == OperandKind::OP_LABEL &&
            out[i].m_Op1.m_iLink == out[i + 1].m_Op1.m_iLink)
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

    codes.swap(stage3);
}

} // namespace minic_opt
