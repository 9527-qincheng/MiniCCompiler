// ============================================================================
// MiniC 编译器 —— 文法表自检（第 5.3.1 节纯数据层）
// 校验 grammar.h / grammar.cpp 五张表内部一致、且与 §4.3~§4.9 的约定相符：
//   · 产生式编号、左右部编码、语义动作位置是否合法；
//   · FIRST / FOLLOW / SELECT 集合的编码与易空性；
//   · 预测分析表是否与 SELECT 集合逐格对应（悬挂 else 一处为例外）；
//   · ε 产生式的 SELECT 是否等于其左部的 FOLLOW。
// 用法：grammar_selfcheck
//   退出码：0 全部通过 / 1 存在失败断言
// ============================================================================
#include "parser/grammar.h"

#include <iostream>
#include <set>
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

bool IsTerminalCode(int code)
{
    return code > 0 && code < MAX_TERM;
}

bool IsNonterminalCode(int code)
{
    return code >= NTERM_CODE_BASE && code < NTERM_CODE_BASE + MAX_NT;
}

bool IsValidSymbolCode(int code)
{
    return IsTerminalCode(code) || IsNonterminalCode(code);
}

const int kSemanticActionCount = 30;   // §8：semantic000..semantic029

} // namespace

int main()
{
    // --- 编号、左右部与语义动作 -------------------------------------------
    Check(PROD_COUNT == 102, "PROD_COUNT is 102");
    Check(g_Grammar[0].m_iNum == 0 && g_Grammar[0].m_Rhs.empty(),
          "g_Grammar[0] is an unused sentinel");

    bool numOk = true, lhsOk = true, rhsOk = true, semOk = true;
    std::set<int> lhsCodes;
    for (int n = 1; n <= PROD_COUNT; ++n)
    {
        const Production& p = g_Grammar[n];
        if (p.m_iNum != n)
        {
            numOk = false;
        }
        if (!IsNonterminalCode(p.m_iLhs))
        {
            lhsOk = false;
        }
        else
        {
            lhsCodes.insert(p.m_iLhs);
        }
        for (size_t i = 0; i < p.m_Rhs.size(); ++i)
        {
            if (!IsValidSymbolCode(p.m_Rhs[i]))
            {
                rhsOk = false;
            }
        }
        if (p.m_SemPos.size() != p.m_SemAct.size())
        {
            semOk = false;
            continue;
        }
        int previous = 0;
        for (size_t k = 0; k < p.m_SemPos.size(); ++k)
        {
            const int pos = p.m_SemPos[k];
            const int act = p.m_SemAct[k];
            if (pos < previous || pos < 0 || pos > static_cast<int>(p.m_Rhs.size()))
            {
                semOk = false;
            }
            if (act < 0 || act >= kSemanticActionCount)
            {
                semOk = false;
            }
            previous = pos;
        }
    }
    Check(numOk, "every production carries its own number (g_Grammar[n].m_iNum == n)");
    Check(lhsOk, "every production has a valid nonterminal LHS code");
    Check(rhsOk, "every RHS symbol is a valid terminal/nonterminal code");
    Check(semOk, "semantic-action positions/ids are ordered and in range");
    Check(static_cast<int>(lhsCodes.size()) == MAX_NT,
          "all 45 nonterminals appear on the left of some production");

    // --- FIRST / FOLLOW ----------------------------------------------------
    bool firstOk = true, followOk = true, firstHasEps = false, followHasEps = false;
    for (int idx = 0; idx < MAX_NT; ++idx)
    {
        if (g_FirstSet[idx].empty())
        {
            firstOk = false;
        }
        for (int code : g_FirstSet[idx])
        {
            if (code == 0)
            {
                firstHasEps = true;
            }
            else if (!IsTerminalCode(code))
            {
                firstOk = false;
            }
        }
        if (g_FollowSet[idx].empty())
        {
            followOk = false;
        }
        for (int code : g_FollowSet[idx])
        {
            if (!IsTerminalCode(code))
            {
                followOk = false;
                if (code == 0)
                {
                    followHasEps = true;
                }
            }
        }
    }
    Check(firstOk, "FIRST(A) is non-empty and holds terminal codes (plus 0 = epsilon)");
    Check(firstHasEps, "FIRST of a nullable nonterminal does contain 0 (epsilon)");
    Check(followOk, "FOLLOW(A) is non-empty and holds terminal codes only");
    Check(!followHasEps, "FOLLOW never contains 0 (epsilon)");
    Check(g_FollowSet[0].size() == 1 && g_FollowSet[0].count(TERM_CODE_EOF) == 1,
          "FOLLOW(program) == { # }");

    // --- SELECT ------------------------------------------------------------
    bool selectOk = true, selectEmpty = false;
    for (int n = 1; n <= PROD_COUNT; ++n)
    {
        if (g_SelectSet[n].empty())
        {
            selectEmpty = true;
        }
        for (int code : g_SelectSet[n])
        {
            if (!IsTerminalCode(code))
            {
                selectOk = false;
            }
        }
    }
    Check(!selectEmpty, "every production has a non-empty SELECT set");
    Check(selectOk, "SELECT sets hold terminal codes only (no epsilon)");
    Check(g_SelectSet[0].empty(), "g_SelectSet[0] sentinel is empty");

    // --- ε 产生式：SELECT == FOLLOW(左部) ---------------------------------
    bool epsilonOk = true;
    int epsilonCount = 0;
    for (int n = 1; n <= PROD_COUNT; ++n)
    {
        const Production& p = g_Grammar[n];
        if (!p.m_Rhs.empty())
        {
            continue;
        }
        ++epsilonCount;
        if (g_SelectSet[n] != g_FollowSet[p.m_iLhs - NTERM_CODE_BASE])
        {
            epsilonOk = false;
        }
    }
    Check(epsilonCount > 0, "grammar contains epsilon productions");
    Check(epsilonOk, "for every epsilon production, SELECT == FOLLOW(LHS)");

    // --- 预测分析表 vs SELECT ---------------------------------------------
    bool cellOk = true, reverseOk = true;
    int conflicts = 0;
    for (int idx = 0; idx < MAX_NT; ++idx)
    {
        const int nt = idx + NTERM_CODE_BASE;
        std::vector<int> owners;
        for (int n = 1; n <= PROD_COUNT; ++n)
        {
            if (g_Grammar[n].m_iLhs == nt)
            {
                owners.push_back(n);
            }
        }
        for (int term = 0; term < MAX_TERM; ++term)
        {
            std::vector<int> hits;
            for (int n : owners)
            {
                if (g_SelectSet[n].count(term) == 1)
                {
                    hits.push_back(n);
                }
            }
            const int got = g_PredictTable[idx][term];
            if (got != -1 && (got < 1 || got > PROD_COUNT))
            {
                cellOk = false;
                continue;
            }
            if (hits.empty())
            {
                if (got != -1)
                {
                    reverseOk = false;
                }
            }
            else if (hits.size() == 1)
            {
                if (got != hits[0])
                {
                    reverseOk = false;
                }
            }
            else
            {
                // 唯一允许的冲突：否则部分 × else，表内取产生式 42（§4.8）
                ++conflicts;
                const bool danglingElse =
                    (nt == 120 && term == 7 && got == 42 &&
                     hits.size() == 2 && hits[0] == 42 && hits[1] == 43);
                if (!danglingElse)
                {
                    reverseOk = false;
                }
            }
        }
    }
    Check(cellOk, "every prediction-table cell is -1 or a valid production number");
    Check(reverseOk,
          "prediction table matches the SELECT sets cell by cell "
          "(only exception: 否则部分 x else)");
    Check(conflicts == 1, "exactly one SELECT conflict exists (dangling else)");

    // --- 表项必须能反查到对应产生式 ---------------------------------------
    bool forwardOk = true;
    for (int idx = 0; idx < MAX_NT; ++idx)
    {
        const int nt = idx + NTERM_CODE_BASE;
        for (int term = 0; term < MAX_TERM; ++term)
        {
            const int prod = g_PredictTable[idx][term];
            if (prod == -1)
            {
                continue;
            }
            if (g_Grammar[prod].m_iLhs != nt || g_SelectSet[prod].count(term) != 1)
            {
                forwardOk = false;
            }
        }
    }
    Check(forwardOk,
          "every table entry points at a production for that LHS selecting that terminal");

    if (g_fail == 0)
    {
        std::cout << "grammar_selfcheck: ALL PASSED\n";
        return 0;
    }
    std::cout << "grammar_selfcheck: " << g_fail << " check(s) FAILED\n";
    return 1;
}
