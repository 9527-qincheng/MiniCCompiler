// ============================================================================
// MiniC 编译器 —— LL(1) 文法表（《设计文档 v4.0 挑战版》§5.3.1 / §4.3~§4.9）
// 纯数据层：产生式表、FIRST / FOLLOW / SELECT 集合、预测分析表。
// 本文件只有声明，不含任何解析逻辑；驱动器（CParser，§4.10）查表时使用。
//
// 数组下标约定（与 §4.1 / §4.2 的编码表一致）：
//   · 终结符    编码 001~054、099，直接作为下标（g_PredictTable 第二维）
//   · 非终结符  编码 100~144，数组下标 = 编码 - NTERM_CODE_BASE，即 0~44
//   · 产生式    编号 1~92，  数组下标 = 编号（下标 0 为哨兵，不使用）
//   · 空串 ε    编码 000；在 m_Rhs 中表示为空序列，表中不再出现 000
//
// 数据来源：codex/MiniC挑战版文法及其相关表格.xlsx，
// 由 tools/generate_grammar.py 机械生成 grammar.cpp，请勿手工修改该 .cpp。
// ============================================================================
#ifndef MINIC_PARSER_GRAMMAR_H
#define MINIC_PARSER_GRAMMAR_H

#include <set>
#include <vector>

// ----------------------------------------------------------------------------
// 表规模常量（MAX_NT / MAX_TERM 即 §5.3.1 中 g_PredictTable 的两个维度）
// ----------------------------------------------------------------------------
constexpr int MAX_NT          = 45;   // 非终结符个数：编码 100..144
constexpr int MAX_TERM        = 100;  // 终结符编码上界 + 1：编码 001..054、099
constexpr int NTERM_CODE_BASE = 100;  // 非终结符编码基址（下标 = 编码 - 100）
constexpr int TERM_CODE_EOF   = 99;   // 输入结束符 '#' 的编码
constexpr int PROD_COUNT      = 92;   // 产生式条数（编号 1..92）
constexpr int PROD_TABLE_SIZE = 93;   // g_Grammar / g_SelectSet 元素个数（下标 0 为哨兵）

// ----------------------------------------------------------------------------
// 5.3.1 产生式表元素
// ----------------------------------------------------------------------------
struct Production
{
    int m_iNum;                  // 产生式编号 1..92
    int m_iLhs;                  // 左部非终结符编码（100..144）
    std::vector<int> m_Rhs;      // 右部符号编码序列（终结符 <100，非终结符 >=100）
                                 // ε 产生式：m_Rhs 为空
    std::vector<int> m_SemPos;   // 语义动作插入位置：取值 0..m_Rhs.size()
    std::vector<int> m_SemAct;   // 与 m_SemPos 一一对应的语义动作编号（第 8 章）
};

// m_SemPos / m_SemAct 两条向量等长，第 k 项表示：在 m_Rhs 中已扫描过
// m_SemPos[k] 个符号之后，执行编号为 m_SemAct[k] 的语义动作 semanticNNN。
// 位置取 0 表示插在最左端，取 m_Rhs.size() 表示插在最右端；
// 该产生式不带任何语义动作时两条向量均为空。

// ----------------------------------------------------------------------------
// 5.3.1 五张表（定义见 grammar.cpp）
// ----------------------------------------------------------------------------

// 产生式表：下标 = 产生式编号（1..92），下标 0 为哨兵
extern const Production g_Grammar[PROD_TABLE_SIZE];

// FIRST / FOLLOW / SELECT（§4.5~§4.7）
// 下标 = 非终结符编码 - NTERM_CODE_BASE；集合元素为终结符编码，
// 其中 0 表示 ε（仅出现在可空非终结符的 FIRST 集合中）
extern const std::set<int> g_FirstSet[MAX_NT];
extern const std::set<int> g_FollowSet[MAX_NT];

// 下标 = 产生式编号（1..92），下标 0 为哨兵
extern const std::set<int> g_SelectSet[PROD_TABLE_SIZE];

// 预测分析表（§4.9）：M[非终结符下标][终结符编码] = 产生式编号，-1 为出错项
extern const int g_PredictTable[MAX_NT][MAX_TERM];

#endif // MINIC_PARSER_GRAMMAR_H
