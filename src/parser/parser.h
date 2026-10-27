// ============================================================================
// MiniC 编译器 —— 语法分析模块 CParser（设计文档 v4.0 §4.10 / §7.2）
// Q4【v4.0 变更】：LL(1) 预测分析表驱动（非递归，分析栈 + 预测分析表）
//                   取代 v3.1 的递归下降（见 §4.10 / §7.2 / §9 决策 Q4）
//
// 公开签名（§7.2，冻结）：
//   explicit CParser(const TokenList&) / ASTNodePtr Parse() /
//   bool HasError() const / const std::vector<CompileError>& GetErrors() const
//   另按 §7.2【新增】提供 GetTrace()，供 --dump-parse 输出预测分析过程（§4.11）。
//
// 内部实现：以 §4.1/§4.2 的数值编码为下标查 §4.9 预测分析表，显式分析栈逐符号
//           推导；语义动作（§8）在文档规定的位置调度执行，语法分析器只负责
//           构建 AST，符号表 / 四元式等语义效果留给语义分析与中间代码阶段。
// ============================================================================
#ifndef MINIC_PARSER_PARSER_H
#define MINIC_PARSER_PARSER_H

#include "lexer/token.h"
#include "parser/ast.h"

#include <cstddef>
#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// §4.10 分析栈元素
//
//   TERM / NTERM / ACTION 与文档结构一致；
//   · REDUCE 为阶段④内部扩展：LL(1) 表驱动没有“规约”事件，而 §8 的语义动作把
//     AST 组装推迟到“产生式右部全部扫描完”之时，故在展开产生式时于最深处压入
//     一个 REDUCE 哨兵，标记该右部的结束（不出现在 §4.11 的栈打印里）。
//   · m_iValueBase 为阶段④扩展字段：语义值栈基址。规约时据此取出本产生式的
//     语义值；恐慌式恢复弹栈时据此截断语义值栈，保证两个栈始终同步。
// ----------------------------------------------------------------------------
struct StackItem
{
    enum Kind { TERM, NTERM, ACTION, REDUCE };

    Kind        m_kind       = TERM;
    int         m_iCode      = 0;   // TERM/NTERM：编码（§4.1/§4.2）；REDUCE：产生式编号
    int         m_iSemantic  = -1;  // ACTION：语义动作编号 0..27（§8）
    std::size_t m_iValueBase = 0;   // 【扩展】入栈时的语义值栈大小
};

// ----------------------------------------------------------------------------
// §4.11 分析过程行（--dump-parse 用）
// ----------------------------------------------------------------------------
struct ParseStep
{
    int         m_iStep  = 0;   // 步骤号（1 起）
    std::string m_szStack;      // 符号栈（自栈顶向右，含语义动作标记，不含内部 REDUCE）
    std::string m_szInput;      // 剩余输入串（当前符号 → #）
    std::string m_szAction;     // 本步动作（展开 / 匹配 / 语义动作 / 出错）
};

// ----------------------------------------------------------------------------
// §7.2 CParser（第 6 章接口契约：构造绑定 TokenList，Parse 返回 AST）
// ----------------------------------------------------------------------------
class CParser
{
public:
    explicit CParser(const TokenList& tokens);

    // --- 第 6 章接口契约（签名冻结） ---
    ASTNodePtr Parse();                       // LL(1) 预测分析表驱动，构建 AST
    bool HasError() const;                    // 是否出现语法错误
    const std::vector<CompileError>& GetErrors() const; // 语法错误列表

    // --- §7.2【新增】供 --dump-parse 输出预测分析过程（§4.11） ---
    const std::vector<ParseStep>& GetTrace() const;

private:
    // ---------- 语义值（驱动器内部实现，不对外暴露） ----------
    struct DeclItem     // 声明符：变量名 + 数组长度（0 = 非数组）
    {
        std::string m_szName;
        int         m_iArraySize = 0;
        ASTNodePtr  m_dim;          // 数组维度常量节点（NumNode），非数组为 null
        LineInfo    m_loc;
    };

    struct PendingOp    // 右递归表达式尾 ⌈op 操作数⌋ 的悬挂二元运算
    {
        TokenType   m_op = TokenType::TOKEN_NONE;
        ASTNodePtr  m_operand;
        LineInfo    m_loc;
    };

    struct SuffixItem   // 后缀表达式1 的一个后缀：[] 下标、() 调用、++/-- 自增自减
    {
        bool        m_bCall = false;
        ASTNodePtr  m_operand;                  // [] 下标表达式
        std::vector<ASTNodePtr> m_args;         // () 实参序列（CALL 节点 children[1..]）
        TokenType   m_unary = TokenType::TOKEN_NONE;  // TOKEN_INC / TOKEN_DEC：后缀自增自减
        LineInfo    m_loc;
    };

    enum class SemKind { NONE, TOKEN, TYPE, NODE, LIST, INT, DECLS, PAIRS, SUFFIX };

    struct SemValue
    {
        SemKind     m_kind = SemKind::NONE;
        Token       m_token;                                  // TOKEN
        TokenType   m_type = TokenType::TOKEN_NONE;           // TYPE
        ASTNodePtr  m_node;                                   // NODE
        std::vector<ASTNodePtr> m_items;                      // LIST
        int         m_iInt = 0;                               // INT / DECLS 首声明符数组长度
        ASTNodePtr  m_dim;                                    // INT（数组后缀）：维度常量节点
        std::vector<DeclItem> m_decls;                        // DECLS 其余声明符
        std::vector<PendingOp> m_pairs;                       // PAIRS
        std::vector<SuffixItem> m_suffixes;                   // SUFFIX
        LineInfo    m_loc;
    };

    // ---------- §7.2 私有字段 ----------
    std::vector<StackItem> m_SymbolStack;   // 分析栈（§4.10）
    const TokenList*       m_pTokenList;    // 绑定 Token 序列
    std::size_t            m_iListPos;      // 输入指针 ip
    std::vector<CompileError> m_errors;     // 语法错误列表
    std::vector<ParseStep>    m_trace;      // 预测分析过程（§4.11）

    // ---------- §7.2 私有方法 ----------
    void PushProduction(int iProdNum);      // 按产生式压栈（语义动作按位置入栈）
    bool ExecuteAction(int iSemantic);      // 执行语义动作（§8）
    void Synchronize();                     // 恐慌式错误恢复（§4.10）

    // ---------- 阶段④实现补充 ----------
    std::vector<SemValue> m_ValueStack;     // 语义值栈（与 m_SymbolStack 同步）
    ASTNodePtr            m_Root;           // Parse() 返回值（最外层归约结果）
    bool                  m_bAborted;       // 步数上限保护触发标志
    LineInfo              m_ProgramLoc;     // 程序起始位置（AST 根位置，semantic000 记录）
    std::size_t           m_iLastErrorPos;  // 上次报错的输入位置（重复报错抑制）
    std::size_t           m_iLastErrorDepth;// 上次报错时的分析栈深度

    // 输入 / 栈访问辅助
    const Token& Cur() const;               // 当前输入符号（越界时回落到 EOF 静态量）
    int          CurCode() const;           // 当前输入符号编码（§4.1）
    bool         AtInputEnd() const;        // 输入指针已到末尾
    bool         NextToken();               // 前进一个 Token；已到末尾返回 false

    // 错误与轨迹
    void ReportError(const std::string& msg, const LineInfo& loc);
    void ReportError(const std::string& msg);             // 用当前符号位置
    void HandleError(const std::string& msg);             // 报错 + 去重 + 恐慌式恢复
    void RecordStep(const std::string& action);           // 追加一行 §4.11 分析过程
    std::string StackText() const;                        // 符号栈渲染（栈顶在左）
    std::string InputText() const;                        // 剩余输入串渲染

    // 语义动作 / 规约
    void TruncateValues(std::size_t iBase);               // 语义值栈截断（错误恢复用）
    void ReduceProduction(int iProdNum, std::size_t iValueBase);  // 右部结束：组装语义值
};

#endif // MINIC_PARSER_PARSER_H
