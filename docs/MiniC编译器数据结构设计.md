


# MiniC 编译器数据结构设计文档（修订版 v3.1）

> 版本：v3.1（与 MiniCCompiler 代码实际实现对齐）
> 编制日期：2026-10-08（v3.0 原日期；v3.1 修订于 2026-10-10）
> 修订说明：
> - v1.0 以课程 PPT 为主要依据、参考书为辅助，采用 C 风格结构体；
> - v2.0 依小组决议修订：**实现语言明确为 C++**，**数据结构设计以《编译器设计之路》为参考基准**，优先借鉴其 NeoPascal 工程（多表符号表、四元式 IR、LL(1) 递归下降、指令模板代码生成）的设计思路；
> - v3.0：与 MiniCCompiler 骨架实现对齐（组长决议全部采纳）：参数表项统一为 ParamEntry（原 ParaEntry）；TokenType 补充 TOKEN_LBRACE/TOKEN_RBRACE（花括号界符，挑战版控制流必需）；AST 节点指针明确 std::unique_ptr<ASTNode> 管理（Q10 定稿）；SymbolTable 接口以第 6 章契约为准、第 4 章扩展接口保留兼容；IRGenerator 构造签名明确为 explicit IRGenerator(SymbolTable&)；补充 ScopeType 枚举；汇编产物后缀固定 .asm。
> - v3.1：与代码实际实现对齐（组员反馈文档与代码不一致，以 src/lexer/token.h、src/parser/parser.h/.cpp 实际实现为准）：TokenType 枚举名统一为 TOKEN_NUM_INT/TOKEN_NUM_FLOAT（原文档 TOKEN_INT_CONST/TOKEN_FLOAT_CONST），并改为 enum class + TOKEN_NONE=0 起始、按代码顺序分段；Token 成员改为 m_line(LineInfo 行列结构)/m_bIsFloat/m_fVal(double)/m_iVal(long long)，移除 m_iRow/m_iCol 独立行列成员；补充 NextToken() 语义（先 ++ 再取、遍历到末尾停在最后一个 token 即 TOKEN_EOF，实现位于 CParser 而非 CLex）与 Parse() 预置约定（m_CurToken = tokens[0]，消费后自行调用 NextToken()）；确认 parser.h 无 Check/Expect 辅助函数，组员可自行添加私有 helper（第 6.3 节公开接口签名不可改）。
> - 课程 PPT 中的硬性约束（MiniC 为 ANSI C 子集、Token 序列/AST/符号表/四元式为必交内容）**全部保留，优先级不变**。
>
> 编制依据：
> - 参考书《编译器设计之路》（裘巍，扫描版 PDF）——设计基准
> - 课程资料《计算机软件实训——MiniC 编译器设计与实现》（PPT）——硬性约束

---

## 目录

1. [修订说明与文档依据](#1-修订说明与文档依据)
2. [MiniC 语言子集与实训要求（PPT 硬性约束）](#2-minic-语言子集与实训要求ppt-硬性约束)
3. [编译器总体架构（以参考书为基准）](#3-编译器总体架构以参考书为基准)
4. [各模块核心数据结构（C++ 定义）](#4-各模块核心数据结构c-定义)
   - 4.1 公共基础结构
   - 4.2 词法分析模块
   - 4.3 语法分析模块（AST）
   - 4.4 语义分析与符号表
   - 4.5 中间代码生成（四元式）
   - 4.6 目标代码生成（扩展设计）
5. [模块间数据结构关系](#5-模块间数据结构关系)
6. [模块接口定义（定稿）](#6-模块接口定义定稿)
7. [设计决策说明](#7-设计决策说明)
8. [待确认问题清单（定稿）](#8-待确认问题清单定稿)
9. [附录 A：参考书 C++ 结构 ↔ MiniC 结构对照表](#9-附录-a参考书-c-结构--minic-结构对照表)
10. [附录 B：模块接口速览（定稿）](#10-附录-b模块接口速览定稿)

---

## 1. 修订说明与文档依据

- 本文档面向小组开发成员，用于统一 MiniC 编译器各模块的 C++ 数据结构约定，作为 `docs/interface.md` 的输入素材。
- **设计基准**：《编译器设计之路》以 NeoPascal（Pascal 子集编译器）为完整工程示例，全部程序以 C++ 描述（参考书第 2 章原文："本书所有的程序都将使用 C++ 描述"）。其数据结构命名、组织方式、辅助栈设计在本文档中作为首要借鉴来源。
- **约束锚点**：课程 PPT 规定的语言范围与交付物（Token/AST/符号表/四元式）是硬性约束，参考书设计不得与之冲突；PPT 未明确处，一律以参考书 NeoPascal 设计为默认取向。
- 标注说明：
  - **【PPT】**：课程 PPT 明确规定，直接采用；
  - **【参考书】**：参考书 NeoPascal 设计，作为基准借鉴，按 MiniC 简化落地；
  - **【扩展预留】**：挑战版（if/while/函数/数组）预留，基础版可不实现；
  - **【待确认】**：两资料均未明确、影响设计方向的事项，原见第 7 节清单，**已于 2026-10-08 定稿**（现第 8 节，全部按建议默认值确认）。

---

## 2. MiniC 语言子集与实训要求（PPT 硬性约束）

### 2.1 语言范围（基础版，PPT 规定）

| 项目 | 内容 |
|---|---|
| 语言定位 | ANSI C 的子集，实现"读得懂、编得出"的最小闭环 |
| 变量声明 | 仅支持**全局变量声明**，类型限 `int`、`float` |
| 语句 | 仅支持**赋值语句** |
| 表达式 | 仅支持**算术表达式**：`+ - * /` 及括号 `( )` |
| 文法（BNF） | `Program → DeclList StmtList`；`DeclList → Decl DeclList | ε`；`StmtList → Stmt StmtList | ε` 等（PPT 原文法为准） |

### 2.2 必交交付物（PPT 规定）

1. **Token 序列**（词法分析输出）；
2. **AST 语法树**（语法分析输出）；
3. **符号表**（语义分析期间构建）；
4. **四元式**（中间代码输出）。

要求打通"源码输入 → 中间代码输出"完整流水线；前端各阶段结果可打印、可调试、可提交验收。

### 2.3 工程约束

- 开发语言：**C++**（v2.0 决议；参考书程序亦全部以 C++ 描述，两者一致）；
- 模块接口冻结于 `docs/interface.md`（PPT 要求）；
- 挑战版（扩展）：条件语句 `if`、循环 `while`、函数、数组等（见 Q5）。

---

## 3. 编译器总体架构（以参考书为基准）

以参考书经典五阶段模型（第 1 章）为骨架，采用 NeoPascal 的工程组织方式：

- **词法分析作为独立的一遍扫描**（参考书 2.2 节）：词法分析器扫描整个源文件，一次性生成单词流 `vector<Token>`，再交由语法分析器消费；
- **语法分析采用 LL(1) 非递归预测分析 / 递归下降**（参考书第 3 章）：通过 First/Follow 集合与预测分析表驱动，必要时消除左递归、提取公共左因子；
- **语法制导翻译 + 语义子程序**（参考书第 4 章）：语义动作以"语义子程序"（如 `semantic0xx`）形式安插在产生式中，随推导执行；辅助栈（iTypePos / iTypeFlag / iIdListFlag 等）用于解决非终结符复用与嵌套语境；
- **中间代码采用三地址四元式**（参考书第 5 章 NPIR）：以过程为单位组织 `m_Codes`，统一经 `EmitIR` 生成；
- **目标代码生成（扩展）基于指令模板**（参考书第 9 章）：IR 四元式 → 模式串匹配指令模板 → 寄存器分配 → 输出汇编。

```
┌──────────┐  单词流    ┌──────────┐  AST    ┌──────────┐  符号表    ┌──────────┐ 四元式 IR ┌──────────┐
│ 源程序    │ ─────────▶ │ 词法分析  │ ──────▶ │ 语法分析  │ ───────▶ │ 语义分析  │ ───────▶ │ 中间代码  │ ─(扩展)─▶ │ 目标代码  │
│ .minic   │ (独立一遍)  │  CLex    │         │ CParser   │          │ CSymbolTbl│          │ IRGenerator│          │ CCodeGen │
└──────────┘            └──────────┘         └──────────┘          └──────────┘          └──────────┘          └──────────┘
     │                        │                    │                    │                     │                    │
     ▼                        ▼                    ▼                    ▼                     ▼                    ▼
  ┌────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
  │                       错误输出 EmitError（参考书全局错误处理，全模块共享）                                        │
  └────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

模块职责与依赖：

| 模块 | 输入 | 输出 | 依赖 |
|---|---|---|---|
| 词法分析 | 源文件字符流 | `vector<Token>` + 常量登记 | 关键字表、转换表、错误输出 |
| 语法分析 | Token 流 | AST + 语义子程序触发 | 词法、错误输出 |
| 语义分析 | AST | 符号表（多表）、类型检查 | 语法、符号表 |
| 中间代码生成 | AST + 符号表 | 四元式序列 `m_Codes` | 语义、符号表、类型系统表 |
| 目标代码生成（扩展） | 四元式序列 | 汇编文本 | 中间代码、指令模板库、寄存器描述 |

---

## 4. 各模块核心数据结构（C++ 定义）

### 4.1 公共基础结构

#### 4.1.1 单词行号信息 LineInfo（参考书声明 2-5）

```cpp
// 单词行列信息：参考书【声明 2-5】LineInfo 的 MiniC 实际实现；
// 含行号与列号（PPT 要求列号），用于出错定位与调试（v3.1：与 src/lexer/token.h 对齐）
struct LineInfo
{
    int  m_iRow = 1;          // 行号（1-based）
    int  m_iCol = 1;          // 列号（PPT 扩展，1-based）

    LineInfo() = default;
    LineInfo(int row, int col) : m_iRow(row), m_iCol(col) {}
};
```

#### 4.1.2 错误输出（参考书 EmitError 全局函数）

```cpp
// 参考书语义子程序使用全局 EmitError(msg, token) 报告错误，MiniC 沿用该约定
// 【待确认 Q8】错误恢复策略见问题清单
void EmitError(const string& szMsg, const Token& tok);
void EmitError(const string& szMsg, const LineInfo& loc);

// 全局错误记录（中间产物，供打印/验收）
extern vector<CompileError> g_ErrorList;   // CompileError{ szMsg; LineInfo loc; }

#### 4.1.3 作用域类型 ScopeType（v3.0 决议补充）

第 6 章接口契约 `EnterScope(ScopeType t)` 需要作用域类型参数，此处补充定义（与骨架实现 `ScopeType{SCOPE_GLOBAL, SCOPE_FUNCTION}` 对齐）：

```cpp
// 作用域类型：基础版仅全局作用域，挑战版函数作用域（参考书过程嵌套）
enum class ScopeType
{
    SCOPE_GLOBAL,    // 全局作用域（基础版仅此一种）
    SCOPE_FUNCTION   // 函数作用域（挑战版）
};
```
```

### 4.2 词法分析模块（参考书第 2 章 + PPT Token 约束）

**职责**：独立一遍扫描源文件，识别标识符/常量/系统单词（关键字、运算符、界符）三类单词，输出 Token 流；常量字面量实时登记入常量表（参考书"识别常量即调用 `RecConstTbl` 登记"）。

#### 4.2.1 TokenType 枚举（PPT 类别 + 参考书单词 ID 思想）

参考书将同类单词编在同一段 ID 中并预留扩展段（如 029~039 预留）；MiniC 采用 C++ 枚举并保持分段编号风格，便于扩展挑战版语法。

```cpp
// 单词类别枚举：enum class 分段编号（关键字段 / 标识符-常量段 / 运算符段 / 界符段），便于按段扩展
// v3.1：与 src/lexer/token.h 实际实现逐项对齐（enum class + TOKEN_NONE=0 起始；整型/浮点常量名
//       统一为 TOKEN_NUM_INT / TOKEN_NUM_FLOAT，原文档 TOKEN_INT_CONST / TOKEN_FLOAT_CONST 废弃）
enum class TokenType
{
    TOKEN_NONE = 0,
    // 关键字
    TOKEN_INT,          // int
    TOKEN_FLOAT,        // float
    TOKEN_IF,           // if（挑战版）
    TOKEN_ELSE,         // else（挑战版）
    TOKEN_WHILE,        // while（挑战版）
    TOKEN_RETURN,       // return（挑战版）
    TOKEN_VOID,         // void（挑战版）
    // 标识符 / 常量
    TOKEN_IDENT,        // 标识符
    TOKEN_NUM_INT,      // 整型常量
    TOKEN_NUM_FLOAT,    // 浮点常量
    // 运算符
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_MUL,          // *
    TOKEN_DIV,          // /
    TOKEN_ASSIGN,       // =
    // 界符
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_LBRACE,       // {（扩展：挑战版控制流）
    TOKEN_RBRACE,       // }（扩展：挑战版控制流）
    TOKEN_SEMI,         // ;
    TOKEN_COMMA,        // ,
    // 结束
    TOKEN_EOF
};

// TokenType 文本名（用于 --dump-tokens / 调试）
const char* TokenTypeToString(TokenType t);
```

#### 4.2.2 Token 类（参考书声明 2-4 CToken + PPT 字段）

以参考书 `CToken` 为基础（m_iKind / m_szContent / m_LineInfo 三元组），扩展 PPT 要求的列号与常量表索引。

```cpp
// Token：词法分析核心输出
// 参考书【声明 2-4】CToken 采用 (m_iKind, m_szContent, m_LineInfo) 三元组；
// 单词流元素即此结构（参考书【声明 2-6】vector<CToken> TokenList）
// v3.1：与 src/lexer/token.h 实际实现对齐——行列信息合并为 m_line(LineInfo)，
//       常量语义值用 m_bIsFloat/m_fVal(double)/m_iVal(long long) 存储
struct Token
{
    TokenType   m_type = TokenType::TOKEN_NONE; // 单词类型（参考书 m_iKind）
    std::string m_szContent;                    // 单词原文（参考书 m_szContent）
    LineInfo    m_line;                         // 行列信息（参考书 m_LineInfo + PPT 列号）
    int         m_iConstIdx = -1;               // 常量表位序（Q12：词法登记 m_ConstTexts 索引）

    // 常量的语义值（词法阶段登记，供后续语义回填使用）
    bool        m_bIsFloat = false;             // 是否为浮点常量
    double      m_fVal = 0.0;                   // 浮点值
    long long   m_iVal = 0;                     // 整型值

    Token() = default;
    Token(TokenType type, const std::string& content, int row, int col)
        : m_type(type), m_szContent(content), m_line(row, col) {}
};

// 单词流（参考书【声明 2-6】：vector<CToken> TokenList）
using TokenList = std::vector<Token>;
```

#### 4.2.3 词法分析器 CLex 类（参考书程序 2-2 Lex.h）

```cpp
// 词法分析器：参考书【程序 2-2】CLex 类结构，成员与接口对齐
class CLex
{
private:
    int         m_szLexTbl[50][130];   // 【参考书声明 2-2】转换表：状态 × 字符集(ASCII 0-127 与高字节统一列)
    string      m_szSource;            // 源代码字符串（读入后在尾部补空格便于最后单词的超前搜索）
    string      m_szFileName;          // 源程序文件名
    map<string, TokenType> m_KeywordTbl; // 【参考书声明 2-3】关键字表：STL map（红黑树，查找效率高）
    string      m_szBuffer;            // 单词缓存区
    int         m_iRow;                // 当前行号
    int         m_iCol;                // 当前列号
    int         m_iNonTerminal;        // 当前转换状态
    int         m_iPos;                // 源程序当前读取位置
    TokenList*  m_pTokenList;          // 指向单词流（输出）

    bool Process(int iTag);                          // 词法处理核心（参考书程序 2-1 算法）
    void EmitToken(TokenType kind, const string& content, int row, int col); // 记录单词（参考书程序 2-7）
    bool SearchKeyword(const string& szKeyWord, TokenType& outType) const;   // 查询关键字表（程序 2-5）
    void BufferBack();                              // 单词缓存区/读取位置回退（程序 2-6）

public:
    CLex();
    ~CLex() = default;

    bool GenToken(TokenList* pTokenList);           // 生成单词流（程序 2-3）
    void SetKeywords(const string& szStr);          // 设置关键字表
    void SetLexTbl(const string& szStr);            // 设置转换表
    void SetFileName(const string& szStr);          // 设置源程序文件名
    const int (&GetLexTbl() const)[50][130] { return m_szLexTbl; }  // 只读访问转换表（调试用）
};

// 常量登记（参考书 RecConstTbl 语义）：词法阶段将字面常量登记入常量表，返回表项位序
// 由语义阶段 CSymbolTbl 维护（见 4.4），词法通过单例访问
```

**设计要点**：
- 单词分三类（参考书 2.2 节）：标识符（ID+名字）、常量（ID+常量表位序）、系统单词（ID）；`m_szContent` 对标识符存名字、对常量存常量表位序字符串（参考书 EmitToken 重载语义）；
- 关键字识别：标识符识别完成后（终止状态 -01）再查 `m_KeywordTbl`（参考书 2.3.3）；
- 超前搜索：MiniC 词法尽量设计为最多超前 1 字符；若出现需超前 2 字符的界定符组合，参照参考书 -42 终止状态的双回退处理（程序 2-4）；
- 转换表可由工具生成或手写，表项内容属于词法定义数据（见 Q11）。

### 4.3 语法分析模块（参考书第 3 章 + PPT AST 要求）

**职责**：LL(1) 预测分析/递归下降消费 Token 流，构建 AST；语义子程序在推导过程中被同步调用（参考书语法制导翻译）。

#### 4.3.1 分析器结构

```cpp
// 语法分析器：基于参考书第 3 章 LL(1) 设计
class CParser
{
private:
    const TokenList*  m_pTokenList;   // 输入单词流（词法独立一遍产出的整体 TokenList）
    int               m_iListPos;     // 当前单词游标（参考书语义子程序大量使用 iListPos）
    Token             m_CurToken;     // 当前单词（超前搜索 1 个单词）

    /* 预测分析辅助（参考书第 3 章 First/Follow 集合与预测分析表） */
    map<string, set<TokenType>>  m_FirstSet;    // 非终结符 First 集合
    map<string, set<TokenType>>  m_FollowSet;   // 非终结符 Follow 集合
    // 预测分析表：非终结符 × 终结符 -> 产生式右部符号串（参考书 3.4 节）
    map<string, map<TokenType, vector<string>>> m_PredictTable;

    /* 辅助栈（参考书第 4 章，解决非终结符复用与嵌套语境） */
    stack<int>    m_iTypePos;      // 【参考书声明 4-13】当前类型指针栈（类型复合声明）
    stack<int>    m_iTypeFlag;     // 类型标志栈（数组/记录/集合等语境标识）
    stack<int>    m_iIdListFlag;   // 标识符列表标志栈（区分标识符列表复用语境）

    /* AST 构建 */
    std::unique_ptr<ASTNode> m_pRoot;  // 语法树根（v3.0 决议：unique_ptr 管理，满足 PPT 交付物要求）

    void NextToken();              // 取下一个单词（v3.1 与代码对齐：先 ++ 再取；遍历到末尾停在最后一个 token 即 TOKEN_EOF）
    // 注（v3.1）：parser.h 无 Check/Expect/Match 终结符匹配辅助函数；组员可自行添加私有 helper（公开接口签名不可改）

    // 非终结符对应分析函数（递归下降形式；也可由预测表驱动，二选一，见 Q4）
    bool ParseProgram();           // Program  → DeclList StmtList
    bool ParseDeclList();          // DeclList → Decl DeclList | ε
    bool ParseDecl();              // Decl      → int id ; | float id ;
    bool ParseStmtList();          // StmtList  → Stmt StmtList | ε
    bool ParseStmt();              // Stmt      → id = Expr ;
    bool ParseExpr();              // Expr      → Term {(+|-) Term}
    bool ParseTerm();              // Term      → Factor {(*|/) Factor}
    bool ParseFactor();            // Factor    → id | 常量 | ( Expr )

public:
    CParser();
    ~CParser() = default;

    // 以单词流为输入完成语法分析，返回 AST 根（失败返回 nullptr）
    std::unique_ptr<ASTNode> Parse(const TokenList* pTokenList);   // v3.0 决议：unique_ptr 管理
};
```

**LL(1) 处理要点（参考书第 3 章）**：
- 对 PPT 文法先求 First/Follow 集合，检查预测冲突；出现左递归（如 `Expr → Expr + Term` 形式）需消除左递归改写为右递归/迭代形式；
- 公共左因子提取：多个候选式首符号相同时提取（参考书文法 3-5→3-7）；
- 表达式按优先级分层：`Expr → Term → Factor`，语法层次即运算符优先级（参考书 3.1.3 语法树）。

#### 4.3.2 AST 节点（ASTNode 基类 + 派生类方案）

参考书第 3 章以语法树描述推导过程（叶结点自左至右构成句子）；PPT 要求显式 AST 输出。采用 **C++ 抽象基类 + 派生类**方案（任务指定方案之一），字段对齐 PPT 参考定义（NODE_DECL/NODE_OP、left/right/name/value）。

```cpp
// AST 节点类别枚举（对应 PPT NODE_DECL/NODE_OP 分类并扩展）
enum class ASTNodeKind
{
    Program, DeclList, Decl, StmtList, Assign,
    Expr, Op, Num, Ident,           // 基础版
    If, While, FuncDef, Call, Array // 挑战版扩展
};

// 抽象基类
class ASTNode
{
public:
    ASTNodeKind  m_kind;            // 节点类别
    LineInfo     m_loc;             // 节点对应源码位置
    std::unique_ptr<ASTNode> m_left;   // 【PPT】左子节点（v3.0 决议：unique_ptr 管理，Q10 定稿）
    std::unique_ptr<ASTNode> m_right;  // 【PPT】右子节点（v3.0 决议：unique_ptr 管理，Q10 定稿）

    explicit ASTNode(ASTNodeKind kind) : m_kind(kind), m_left(nullptr), m_right(nullptr) {}
    virtual ~ASTNode() = default;

    // 打印/导出 AST（交付物打印）
    virtual void Print(ostream& os, int indent = 0) const = 0;
};

// 数值常量节点（PPT：NODE_NUM / value 字段）
class NumNode : public ASTNode
{
public:
    TokenType  m_type;        // 常量类型：int / float
    int        m_iVal;        // 整型值
    float      m_fVal;        // 浮点值
    int        m_iConstIdx;   // 常量表位序（与词法/语义常量表一致）

    NumNode(TokenType t, int iVal, float fVal)
        : ASTNode(ASTNodeKind::Num), m_type(t), m_iVal(iVal), m_fVal(fVal), m_iConstIdx(-1) {}
    void Print(ostream& os, int indent) const override;
};

// 标识符节点（PPT：NODE_IDENT / name 字段）
class IdentNode : public ASTNode
{
public:
    string  m_szName;         // 【PPT】变量名
    int     m_iSymIdx;        // 语义阶段回填：符号表项索引

    explicit IdentNode(const string& name)
        : ASTNode(ASTNodeKind::Ident), m_szName(name), m_iSymIdx(-1) {}
    void Print(ostream& os, int indent) const override;
};

// 变量声明节点（PPT：NODE_DECL / name 字段）
class DeclNode : public ASTNode
{
public:
    string    m_szName;       // 【PPT】声明的变量名
    TokenType m_declType;     // 声明类型：TOKEN_INT / TOKEN_FLOAT
    int       m_iSymIdx;      // 语义阶段回填：符号表项索引

    DeclNode(const string& name, TokenType t)
        : ASTNode(ASTNodeKind::Decl), m_szName(name), m_declType(t), m_iSymIdx(-1) {}
    void Print(ostream& os, int indent) const override;
};

// 赋值语句节点
class AssignNode : public ASTNode
{
public:
    // m_left = IdentNode(左值)，m_right = Expr 子树
    AssignNode() : ASTNode(ASTNodeKind::Assign) {}
    void Print(ostream& os, int indent) const override;
};

// 二元算术运算节点（PPT：NODE_OP / op、left、right 字段）
class BinOpNode : public ASTNode
{
public:
    TokenType m_op;           // 【PPT】运算符：+ - * /

    explicit BinOpNode(TokenType op)
        : ASTNode(ASTNodeKind::Op), m_op(op) {}
    void Print(ostream& os, int indent) const override;
};

// 程序/列表节点：左子=首元素，右子=剩余列表（链表式表达列表，配合 PPT 文法递归形式）
class ListNode : public ASTNode
{
public:
    // 用于 DeclList / StmtList / Program 等列表式节点
    explicit ListNode(ASTNodeKind kind) : ASTNode(kind) {}
    void Print(ostream& os, int indent) const override;
};
```

> **备选方案（variant）**：若小组倾向非多态方案，可用 `std::variant<NumNode, IdentNode, DeclNode, AssignNode, BinOpNode, ListNode>` 承载节点内容，配 `std::visit` 遍历；基类+派生方案与 variant 方案等价，**建议二选一后冻结于 interface.md（见 Q10）**。

#### 4.3.3 文法到 AST 的映射约定

| BNF 产生式（PPT） | AST 构造 |
|---|---|
| `Program → DeclList StmtList` | `ListNode(Program)`，左=DeclList，右=StmtList |
| `DeclList → Decl DeclList \| ε` | `ListNode(DeclList)` 链表式 |
| `Decl → int id ;` / `float id ;` | `DeclNode(name, type)` |
| `Stmt → id = Expr ;` | `AssignNode`：left=IdentNode，right=Expr 树 |
| `Expr → Expr + Term` 等 | `BinOpNode(op)`：left=左操作数树，right=右操作数树 |
| `Factor → ( Expr )` | 直接返回内层 Expr 树（不建冗余节点） |
| `Factor → 常量` | `NumNode(type, val)` |
| `Factor → id` | `IdentNode(name)` |

---

### 4.4 语义分析与符号表（参考书第 4 章多表组织）

**职责**：随语法分析（或遍历 AST）执行语义子程序，登记符号表、查重、类型检查、构造类型链；符号表是编译器中心数据库（参考书第 4 章）。

#### 4.4.1 符号表项基类 SymbolEntry（参考书 ObjectInfo）

参考书以 `ObjectInfo` 作为符号表项的公共基类（含名字、所属过程索引）；MiniC 保留该组织，并补充 PPT 要求的 scope/line 字段。

```cpp
// 符号表项公共基类：对应参考书 ObjectInfo
class SymbolEntry
{
public:
    string  m_szName;        // 【PPT】符号名
    int     m_iProcIndex;    // 所属过程/作用域索引（参考书 m_iProcIndex；基础版恒为主程序 0）
    int     m_scope;         // 【PPT】作用域编号（基础版全局=0；挑战版随 ProcStack 递增）
    int     m_line;          // 【PPT】声明所在行
    bool    m_bUsed;         // 是否被引用（参考书 VarInfo::m_bUsed，供"声明未使用"警告）

    SymbolEntry() : m_iProcIndex(0), m_scope(0), m_line(0), m_bUsed(false) {}
    virtual ~SymbolEntry() = default;
};

// 变量表项：参考书 VarInfo（含类型链接、存储分配、寄存器绑定等）
class VarEntry : public SymbolEntry
{
public:
    int       m_iTypeLink;    // 类型链接：指向 TypeInfoTbl 中该变量的类型表项（参考书 m_iTypeLink）
    TokenType m_baseType;     // 基础版直接冗余类型：TOKEN_INT / TOKEN_FLOAT（便于快速访问）
    int       m_eRank;        // 符号种类：VAR=0 / PARA=1 / RET=2（参考书 eRank；挑战版用）
    bool      m_bRef;         // 是否为引用参数（参考书 m_bRef；挑战版用）
    int       m_iMemoryAlloc; // 存储分配偏移（参考书 m_iMemoryAlloc；目标代码阶段回填）
};

// 常量表项：参考书 ConstInfo（含值、存储类型、枚举索引等）
class ConstEntry : public SymbolEntry
{
public:
    string    m_szVal;        // 常量值原文（参考书 m_szVal）
    TokenType m_constType;    // 常量类别：TOKEN_NUM_INT / TOKEN_NUM_FLOAT（v3.1：与代码枚举名对齐）
    int       m_iVal;         // 整型值
    float     m_fVal;         // 浮点值
    int       m_iEnumIdx;     // 所属枚举类型表项索引（挑战版枚举用）
    int       m_storeType;    // 存储类型（参考书 StoreType，基础版=INTEGER/REAL）
};

// 类型表项：参考书 TypeInfo（类型链核心）
class TypeEntry : public SymbolEntry
{
public:
    int         m_eDataType;    // 类型种类（参考书 StoreType：T_INTEGER/T_REAL/T_ARRAY...）
    int         m_iLink;        // 类型链：指向基类型/匿名类型表项（参考书 m_iLink）
    int         m_eBaseType;    // 基类型（基本类型时等于自身）
    int         m_iState;       // 状态标志：0=未分析完成（不可引用）/ 1=分析完成（参考书 m_iState）
    string      m_szContent;    // 指针类型暂存基类型名（参考书 m_szContent，挑战版指针用）
    vector<FieldInfo> m_FieldInfo; // 记录字段列表（挑战版）
    vector<ArrayInfo> m_ArrayInfo; // 数组维度信息列表（挑战版；ArrayInfo{m_iStart,m_iEnd}）
};

// 过程/函数表项：参考书 ProcInfo（挑战版）
class ProcEntry : public SymbolEntry
{
public:
    int   m_eRank;          // Main=0 / Sub=1
    int   m_eType;          // Procedure / Function / Type
    vector<int>  m_ParaTbl; // 形参表（ParamEntry 索引，v3.0 决议统一命名）
    int   m_iReturnType;    // 返回类型
    int   m_iReturnVar;     // 返回变量（VarEntry 索引）
    int   m_eFlag;          // Forward / Extern / None
    vector<Quad> m_Codes;   // 该过程对应的四元式序列（参考书 ProcInfo::m_Codes，中间代码按过程组织）
};

// 形参表项：参考书 ParaInfo（挑战版），v3.0 决议统一命名为 ParamEntry
class ParamEntry : public SymbolEntry
{
public:
    int m_iParaType;   // 形参类型链接
    int m_iParaVar;    // 形参变量（VarEntry 索引）
    int m_eAssignType; // 传值 VAL / 传址 VAR
};

// 标号表项：参考书 LabelInfo（挑战版控制流）
class LabelEntry : public SymbolEntry
{
public:
    bool m_bDef;   // 是否已定义
    bool m_bUse;   // 是否被引用
};
```

#### 4.4.2 符号表容器 SymbolTable（参考书 CSymbolTbl 多表组织）

参考书采用多表组织（变量表、常量表、类型表、过程表、枚举表、标号表），并配有 `ProcStack` 管理过程嵌套；MiniC 基础版只需变量/常量/类型三张主表，其余表保留为挑战版扩展，**结构完全对齐参考书**。

```cpp
// 符号表：参考书 CSymbolTbl 多表组织
// 【v3.0 决议】对外接口以第 6 章契约（EnterScope/ExitScope/RecVar/RecConst/SearchVar/SearchConst/BackfillConst/CheckAssign/DumpSymTab）为准；
// 本段（SearchVarInfoTbl/RecVar/RecConstTbl/RecType/Print 等）为参考书风格扩展接口，保留作兼容层，不参与接口冻结。
class SymbolTable
{
public:
    /* ---- 多张符号表（参考书 CSymbolTbl 成员） ---- */
    vector<VarEntry>      m_VarInfoTbl;     // 变量信息表
    vector<ConstEntry>    m_ConstInfoTbl;   // 常量信息表
    vector<TypeEntry>     m_TypeInfoTbl;    // 类型信息表
    vector<ProcEntry>     m_ProcInfoTbl;    // 过程信息表（挑战版）
    vector<LabelEntry>    m_LabelInfoTbl;   // 标号信息表（挑战版）
    vector<EnumEntry>     m_EnumInfoTbl;    // 枚举信息表（挑战版）
    stack<int>            m_ProcStack;      // 过程栈：栈顶=当前过程索引（参考书 m_ProcStack）

    /* ---- 辅助栈（参考书第 4 章） ---- */
    stack<int>            m_iTypePos;       // 当前类型指针栈（类型复合声明）
    stack<int>            m_iTypeFlag;      // 类型标志栈
    stack<int>            m_iIdListFlag;    // 标识符列表标志栈

    /* ---- 查询接口（参考书 SearchXxxTbl 系列） ---- */
    int SearchVarInfoTbl(int iProcIndex, const string& szName) const;
    int SearchConstInfoTbl(int iProcIndex, const string& szName) const;
    int SearchTypeInfoTbl(int iProcIndex, const string& szName, bool bGlobal = true) const;
    int SearchLabelInfoTbl(int iProcIndex, const string& szName) const;

    /* ---- 登记接口 ---- */
    int RecVar(const string& szName, TokenType baseType, int line);       // 登记变量
    int RecConstTbl(const string& szVal, TokenType constType);            // 登记常量（词法/语义共用）
    int RecType(const string& szName);                                    // 登记类型表项（匿名类型传入 "_noname"+序号）

    /* ---- 类型链/引用检查 ---- */
    bool PtrCheck(int& iPos);        // 指针类型 miLink 回填（参考书程序 4-28，挑战版）
    string GetSerialId();             // 匿名类型唯一序号（参考书 "_noname" + GetSerialId）

    /* ---- 输出：符号表打印（交付物） ---- */
    void Print(ostream& os) const;
};

extern SymbolTable g_SymbolTbl;   // 全局符号表单例（参考书全局 SymbolTbl）
```

#### 4.4.3 语义分析关键动作（参考书第 4 章语义子程序）

| 阶段 | 动作 | 参考书对应 |
|---|---|---|
| 声明 | 遇 `DeclNode`：`SearchVarInfoTbl` 查重 → `RecVar` 登记 → 回填 `m_iSymIdx` | semantic 处理变量定义部分 |
| 常量 | 词法识别常量即 `RecConstTbl` 登记，语义阶段按名回填 `m_szName/m_iProcIndex` | semantic004（常量定义→标识符=常量） |
| 类型 | 基础版类型为内建 int/float（TypeEntry 预置 T_INTEGER/T_REAL）；挑战版类型声明用 `m_iTypePos` 栈 + `m_iTypeFlag` 栈构造类型链 | semantic006/015/010 |
| 语句 | 遇 `IdentNode`：查变量表，未声明报错；标记 `m_bUsed = true` | 变量引用的语义检查 |
| 类型检查 | 赋值/表达式类型相容：int↔float 按 C 语义转换；挑战版引入类型系统表（见 4.5.4） | 参考书第 6 章 |

**MiniC 简化说明**：参考书为 Pascal 完整类型系统设置六张表 + 复杂辅助栈；MiniC 基础版仅全局 int/float 变量，**仅启用 `m_VarInfoTbl / m_ConstInfoTbl / m_TypeInfoTbl` 三张表**，辅助栈可不实例化；挑战版按需启用其余表与栈（避免过度设计，见第 6 节决策说明）。

### 4.5 中间代码生成（参考书第 5 章 NPIR）

**职责**：遍历 AST + 查符号表，按语义动作生成三地址四元式，以过程为单位组织（参考书 `ProcInfo::m_Codes` / `EmitIR`）。

#### 4.5.1 四元式操作码 QuadOp（参考书 NPIR 操作符）

```cpp
// 四元式操作码：参考书第 5 章 NPIR 操作符（EQU/ADD/SUB/MUL/DIV/JMP/LABEL/PARA/RETV/CALL/JT/JNT 等）
enum class QuadOp
{
    /* 运算类 */
    Q_ASSIGN,   // res = arg1
    Q_ADD,      // res = arg1 + arg2
    Q_SUB,      // res = arg1 - arg2
    Q_MUL,      // res = arg1 * arg2
    Q_DIV,      // res = arg1 / arg2

    /* 类型转换（含 float 运算时插入；参考书类型系统表生成转换 IR） */
    Q_ITOF,     // res = (float)arg1

    /* 控制流（挑战版） */
    Q_LABEL,    // 标号定义
    Q_JMP,      // 无条件跳转
    Q_JT,       // 真跳转
    Q_JF,       // 假跳转

    /* 函数（挑战版） */
    Q_PARA,     // 实参传递（参考书 PARAs）
    Q_CALL,     // 调用
    Q_RET       // 返回（参考书 RETV）
};
```

> **⚠ 2026-10-27 变更（以代码为准）**：上面这段枚举是 v3.1 骨架版的设计稿。
> 现行 `src/sema/ir.h` 已按参考书**表 5-2「NPIR 的操作符」**重建为 **21 大类 + 宽度后缀**，
> 共 **151 个成员**，枚举值按「高 5 位大类、低 5 位类内成员」编码：
>
> ```cpp
> namespace QuadCat { enum : int { CAT_EQU = 1, CAT_NEQU = 2, CAT_LS = 3, ..., CAT_ASSIGN = 20, CAT_OTHER = 21 }; }
> enum class QuadOp : int
> {
>     Q_ADD_4  = (QuadCat::CAT_ADD  << 5) | 2,   // 4 字节整型加法
>     Q_ADD_4F = (QuadCat::CAT_ADD  << 5) | 6,   // 4 字节单精度加法（MiniC 扩展）
>     ...
> };
> ```
>
> 设计稿里的 `Q_JT` / `Q_JF` 按书改正为 `Q_JT` / `Q_JNT`；`Q_ASSIGN` / `Q_ADD` / `Q_PRINT` / `Q_INDEX`
> 分别对应当前的 `Q_ASSIGN_*` / `Q_ADD_*` / `Q_PRINT_*` / `Q_INDEX_*`（按宽度选后缀）。
> 完整枚举、MiniC 实际产生的 30 个成员、以及与表 5-2 的三处差异，
> 见设计文档 §5.5 与 [interface.md](interface.md) 第 25 条。

#### 4.5.2 操作数 Operand 类（参考书 OpInfo）

参考书 `OpInfo` 含操作数类型（CONST/VAR/PTR/LABEL/NONE/PROC）、链接索引、引用标记、UD/DU 链等；MiniC 保留核心字段。

```cpp
// 操作数类型（参考书 OpInfo::m_iType）
enum class OperandKind
{
    OP_NONE,    // 空操作数
    OP_CONST,   // 常量：索引指向 m_ConstInfoTbl
    OP_VAR,     // 变量：索引指向 m_VarInfoTbl
    OP_TEMP,    // 临时变量：表达式中间结果（参考书 GetTmpVar / _Txxx）
    OP_LABEL,   // 标号（挑战版控制流）
    OP_PROC     // 过程/函数（挑战版 CALL）
};

// 操作数：参考书 OpInfo
class Operand
{
public:
    OperandKind m_kind;       // 操作数种类（参考书 m_iType）
    int         m_iLink;      // 链接索引（参考书 m_iLink：常量表/变量表/临时编号/标号编号）
    TokenType   m_type;       // 数据类型：TOKEN_INT / TOKEN_FLOAT（供目标代码确定字节宽度）
    bool        m_bRef;       // 是否为间接引用（参考书 m_bRef，指针/数组挑战版用）
    int         m_iDetailType;// 详细类型信息（参考书 m_iDetailType，栈式类型链用）
    // UD/DU 链（参考书 m_udChain / m_duChain）：优化阶段回填，基础版暂不启用

    Operand() : m_kind(OperandKind::OP_NONE), m_iLink(-1), m_type(TOKEN_EOF), m_bRef(false), m_iDetailType(0) {}

    // 便捷构造
    static Operand Const(int idx, TokenType t);   // 常量
    static Operand Var(int idx, TokenType t);     // 变量
    static Operand Temp(int idx, TokenType t);    // 临时变量
    static Operand Label(int idx);                // 标号
};
```

#### 4.5.3 四元式 Quad 类（参考书 IRCode）

参考书 `IRCode { m_eOpType, m_Op1, m_Op2, m_Rslt }` 与 PPT 四元式 `(op, arg1, arg2, res)` 一致。

```cpp
// 四元式：参考书【IRCode】{m_eOpType, m_Op1, m_Op2, m_Rslt}
class Quad
{
public:
    QuadOp      m_eOp;       // 【PPT】op（参考书 m_eOpType）
    Operand     m_Op1;       // 【PPT】arg1（参考书 m_Op1）
    Operand     m_Op2;       // 【PPT】arg2（参考书 m_Op2）
    Operand     m_Rslt;      // 【PPT】res （参考书 m_Rslt）
    LineInfo    m_loc;       // 对应源码位置（调试/报错）

    Quad(QuadOp op, const Operand& op1, const Operand& op2, const Operand& rslt)
        : m_eOp(op), m_Op1(op1), m_Op2(op2), m_Rslt(rslt) {}
    void Print(ostream& os) const;   // 打印四元式（交付物）
};

// 四元式序列：以过程为单位（参考书 ProcInfo::m_Codes）
using CodeList = vector<Quad>;
```

#### 4.5.4 中间代码生成器 IRGenerator（参考书 EmitIR / GetTmpVar / GetTmpLabel）

```cpp
// 中间代码生成器：参考书第 5 章（EmitIR、GetTmpVar、GetTmpLabel、CurrentStatement）
class IRGenerator
{
private:
    int         m_iTmpVarCounter;    // 临时变量计数器（参考书 GetTmpVar：生成 _T1,_T2,...）
    int         m_iLabelCounter;     // 标号计数器（参考书 GetTmpLabel：生成 L1,L2,...）
    CodeList    m_Codes;             // 当前过程四元式序列（参考书 m_Codes）

public:
    explicit IRGenerator(SymbolTable& symtab);   // v3.0 决议：构造绑定符号表（与第 6 章接口契约一致，确定操作数类型）

    int  GetTmpVar(TokenType t);       // 申请临时变量，返回临时编号（参考书 GetTmpVar）
    int  GetTmpLabel();                // 申请标号，返回标号编号（参考书 GetTmpLabel）
    void EmitIR(QuadOp op, const Operand& op1, const Operand& op2, const Operand& rslt); // 生成四元式（参考书 EmitIR）

    /* 表达式翻译（参考书第 6 章：操作数栈 + 运算符栈） */
    stack<Operand>  m_OperandStack;    // 操作数栈（参考书 Operand 栈）
    stack<int>      m_OperationStack;  // 运算符栈（存 TokenType 对应运算符 ID）

    /* 语句翻译（参考书第 7 章：CurrentStatement 栈，挑战版） */
    stack<StatementState> m_StmtStack; // 控制结构嵌套翻译栈

    CodeList& GetCodes() { return m_Codes; }        // 当前过程代码序列
    void FlushToProc(ProcEntry& proc);              // 将 m_Codes 挂到过程表项（挑战版）
};

// 语句翻译状态：参考书 CurrentStatement / LabelIdx
struct LabelIdx
{
    int TrueLabel;    // 真出口
    int FalseLabel;   // 假出口
    int ExitLabel;    // 出口
    int EntryLabel;   // 入口（循环）
};

struct StatementState
{
    int  m_eType;        // IF / WHILE / FOR / REPEAT / CASE
    LabelIdx m_Labels;   // 相关标号
    int  m_iLoopVar;     // 循环变量（FOR）
    bool m_bIsDownto;    // FOR downto（参考书 m_bIsDownto）
    bool m_bIsElse;      // IF 的 else 分支（参考书 m_bIsElse）
};
```

#### 4.5.5 类型系统表（参考书第 6 章 TypeSysInfo，MiniC 简化版）

参考书类型系统表 `TypeSysInfo { m_iOp, m_iOp1Type, m_iOp2Type, m_iRsltType, m_iProcessId, m_iVarProcessId, m_iIR, m_Convert1, m_Convert2 }` 定义"操作符 × 操作数类型 → 结果类型 + 生成的中间码 + 转换动作"。MiniC 仅 int/float，规则表简化为：

```cpp
// 类型系统规则表项：参考书 TypeSysInfo（MiniC 简化：两种类型 → 规则极少）
struct TypeSysInfo
{
    TokenType m_iOp;         // 运算符（TOKEN_PLUS/MINUS/MUL/DIV/ASSIGN）
    TokenType m_iOp1Type;    // 左操作数类型（TOKEN_INT/TOKEN_FLOAT）
    TokenType m_iOp2Type;    // 右操作数类型
    TokenType m_iRsltType;   // 结果类型（int op int → int；含 float → float）
    QuadOp    m_iIR;         // 生成的中间码（Q_ADD/Q_SUB/Q_MUL/Q_DIV/Q_ITOF...）
    bool      m_bConvert1;   // 是否需要将 arg1 转换为结果类型（int→float）
    bool      m_bConvert2;   // 是否需要将 arg2 转换为结果类型
};

// 类型系统表（MiniC 规则集合，全局只读）
extern const vector<TypeSysInfo> g_TypeSysTable;

// 表达式翻译主流程（参考书第 6 章）：
// 1) 按 AST 后序遍历，操作数压 m_OperandStack，运算符压 m_OperationStack
// 2) 归约时查 g_TypeSysTable 确定结果类型与转换（必要时 EmitIR(Q_ITOF)）
// 3) 常量折叠（参考书 OpConstSemantic）：两个常量运算直接在语义期求值，不生成 IR
```

**基础版表达式翻译示例**（`a = b * 2 + 1`，int 型）：

```
1.  (Q_MUL,   Var(b),   Const(2),  Temp(_T1))      // GetTmpVar(int) → _T1
2.  (Q_ADD,   _T1,      Const(1),  Temp(_T2))      // GetTmpVar(int) → _T2
3.  (Q_ASSIGN,_T2,      (none),    Var(a))         // 赋值
```

### 4.6 目标代码生成（参考书第 9 章，扩展设计）

> 课程基础版验收产物止于四元式，目标代码生成非基础版必做（PPT 未要求，见 Q3）。本节依据参考书第 9 章（i386 示例）给出可扩展 C++ 骨架，**暂不承诺实现**。

#### 4.6.1 指令模板库（参考书 Pattern / Code / RegFlg）

```cpp
// 指令行：参考书第 9 章 Code 结构（Label/Op/Operand1-3/Comment）
struct CodeLine
{
    string  m_szLabel;      // 指令标号
    string  m_szOp;         // 操作码（mov/add/...）
    string  m_szOperand1;   // 操作数 1（含 %V.1% 等占位符）
    string  m_szOperand2;
    string  m_szOperand3;
    string  m_szComment;    // 注释
};

// 操作数来源信息：参考书 RegFlg { cFlg, cReg, iSize }
struct RegFlag
{
    char m_cFlg;    // 0=直接内存访问 / 1=寄存器 / 2=优先寄存器
    char m_cReg;    // 特定寄存器约束（-1 表示不限定）
    int  m_iSize;   // 操作数字节数（1/2/4）
};

// 指令模板：参考书 Pattern { szPattern, CodeList, Flg, SaveReg }
struct InstPattern
{
    string            m_szPattern;     // 模式串（如 "ADD_4%V.1%%V.2%%V.3%"）
    vector<CodeLine>  m_CodeList;      // 指令序列
    vector<RegFlag>   m_Flg;           // 各操作数来源信息
    vector<int>       m_SaveReg;       // 使用本模板前需回存的寄存器列表
};
```

#### 4.6.2 寄存器描述与分配（参考书声明 9-1 ~ 9-5）

```cpp
// i386 寄存器编号（参考书声明 9-1）
enum { EAX=0, EBX, ECX, EDX, AX, BX, CX, DX, AH, BH, CH, DH, AL, BL, CL, DL, ESI, EDI };

// 字节数 -> 寄存器号映射（参考书声明 9-2 Size2Reg）
extern const int Size2Reg[3][8];    // 1 字节 / 2 字节 / 4 字节寄存器组
// 寄存器 -> 字节数（参考书声明 9-3 Reg2Size）
extern const int Reg2Size[];        // 寄存器号 -> 字节数
// 寄存器交织依赖（参考书声明 9-4 RegRela：eax 与 ax/ah/al 相关）
extern const int RegRela[18][5];

// 寄存器绑定跟踪（参考书声明 9-5 RegVal）
extern map<int, vector<Operand>> g_RegVal;   // 寄存器号 -> 绑定的操作数
extern set<int> g_ForbidRegs;                // 当前指令模板禁用的寄存器

// 寄存器分配：参考书 9.3.4（优先未绑定寄存器 → 已绑定中选出并 SaveReg 回存 → 不可用绝不分配）
class CRegisterAllocator { ... };
```

#### 4.6.3 目标代码生成器（参考书 9.3.5 流程）

```cpp
class CCodeGen
{
public:
    // 数据流分析（变量活跃区间）→ 初始化模板库 → 按过程遍历基本块 IR →
    // 生成模式串 → 检索指令模板 → 寄存器分配/回存 → 文本替换 → 输出汇编
    bool IRtoASM(const CodeList& codes, AsmOutput& out);

    // 指令模板库（参考书 AsmScheme.txt 加载）
    vector<InstPattern> m_TemplateLib;
};

// 目标代码输出容器（产物文件：`<源文件名>.asm`，v3.0 决议后缀固定 .asm）
struct AsmOutput
{
    vector<string> m_FileHeader;   // 文件头（含库引用）
    vector<string> m_DataSection;  // 数据段声明（.data）
    vector<string> m_CodeSection;  // 代码段指令序列（.code）
};
```

---

## 5. 模块间数据结构关系

```
                    ┌──────────────────────────────────────────┐
                    │          g_ErrorList / EmitError          │
                    │      （参考书全局错误处理，全模块共享）      │
                    └──────────────────────────────────────────┘
                                    ▲              ▲
                                    │              │
  ┌─────────┐  TokenList  ┌──────────┐  ASTNode* ┌──────────┐  SymbolEntry  ┌──────────┐  Quad[]  ┌──────────┐
  │  CLex   │ ──────────▶ │  CParser  │ ────────▶│ 语义分析  │ ────────────▶ │IRGenerator│ ───────▶ │ CCodeGen │
  │(独立一遍)│ (vector<Token>)│ (LL(1))  │           │(语法制导)  │ (多表登记)    │ (NPIR)   │ (扩展)    │          │
  └─────────┘            └──────────┘           └──────────┘               └──────────┘         └──────────┘
      │                        │                       │                       │                     │
      ▼                        ▼                       ▼                       ▼                     ▼
  m_szLexTbl                (AST 存储)          g_SymbolTbl             IRContext                 InstPattern
  m_KeywordTbl              ASTNode 多态          (VarInfo/ConstInfo/     (m_Codes + 临时变量/      CodeLine/RegFlag
  RecConstTbl→常量表         派生类树               TypeInfo 多表)           标号计数器)             Size2Reg/RegRela
```

**核心关系说明**：

| 关系 | 说明 |
|---|---|
| 词法 → 语法 | `TokenList` 一次性生成后整体传递（参考书独立一遍模型）；`m_iListPos` 游标消费；`m_iConstIdx` 沟通常量表 |
| 语法 → 语义 | 语义子程序在推导过程中同步执行（参考书语法制导翻译）；AST 由 `CParser` 显式构建（PPT 交付物） |
| 语义 → 符号表 | 语义子程序登记 `g_SymbolTbl` 各表（`RecVar/RecConstTbl/RecType`），查重与类型链构造均走 `SearchXxxTbl` 系列 |
| 语义 → 中间代码 | 遍历 AST 时查符号表确定操作数类型；临时变量 `GetTmpVar`；类型转换查 `g_TypeSysTable` |
| 中间代码 → 目标代码 | `Quad[]` 逐条匹配 `InstPattern`；`Operand.m_type` 决定寄存器字节宽度与数据段声明 |

---

## 6. 模块接口定义（定稿）

> 本章为五个模块的**对外接口契约**（对应 PPT interface.md 验收要求，Q10 已定稿：接口按本章冻结）。组员可依据本章列出的类名、函数签名与数据流独立开发各自模块，模块间仅依赖本章接口，不感知内部实现。
>
> 定稿依据（详见第 8 节 Q1-Q12）：C++17 + CMake；多表符号表；AST 基类+派生类+unique_ptr；递归下降；四元式操作数引用式、打印转原文；错误报告行列号并尽量恢复；float→int 隐式转换打印警告；常量表词法登记、语义回填挂符号表；挑战版做 x86 汇编。

### 6.1 总览：模块间数据流与调用链

```
main(argc, argv)   ← 命令行解析（--dump-tokens / --dump-ast / --dump-symtab / --dump-quads）
  │
  ▼
CLex::Run() ──TokenList──▶ CParser::Parse() ──ASTNode*──▶ SymbolTable（语义登记/查重/类型检查）
                                                                        │ 类型/符号信息
                                                                        ▼
                                                          IRGenerator::Generate() ──CodeList(Quad[])──▶ CCodeGen::IRtoASM()（挑战版）
```

**数据传递链**（与第 5 章一致）：`TokenList → AST → 符号表 → Quad → 汇编`，每两个模块之间仅通过下表所示的数据结构解耦。

| 阶段 | 数据载体 | 生产者 → 消费者 |
|---|---|---|
| 词法 → 语法 | `TokenList`（vector<Token>） | CLex → CParser |
| 语法 → 语义 | `ASTNode*`（基类+派生类树，unique_ptr 管理） | CParser → SymbolTable/IRGenerator |
| 语义 → 中间代码 | `SymbolEntry*` 查询结果 + `g_TypeSysTable` 规则 | SymbolTable → IRGenerator |
| 中间代码 → 目标代码 | `CodeList`（vector<Quad>） | IRGenerator → CCodeGen |

### 6.2 CLex 词法分析模块（接口约定）

```cpp
// 职责：读取源文件，按 m_szLexTbl 转换表扫描并产出 Token 序列；
//       识别关键字/标识符/整数/浮点/运算符/分隔符；登记字面量常量（Q12 词法登记）；
//       出错时上报行列号（Q8）并尽量继续扫描。
class CLex
{
public:
    bool    OpenSource(const std::string& szFileName);  // 打开源文件，失败返回 false
    bool    Run();                                      // 执行词法扫描，填充 m_TokenList；错误不中断
    const TokenList& GetTokenList() const;              // 对外输出完整 Token 序列（供 CParser 消费）

    // 对外可见数据（详细字段见 §4.2）
    TokenList               m_TokenList;                // 输出：Token 序列
    std::vector<std::string> m_ConstTexts;              // 常量字面量文本登记（Q12，供语义回填）
};
```

**词法流消费约定（v3.1 与代码对齐）**：
- CLex **不提供 NextToken()**：词法分析为“独立一遍”，`Run()` 一次性扫描并产出完整 `TokenList`（扫描完毕在末尾追加 `TOKEN_EOF` 结束符），整体经 `GetTokenList()` 传给 CParser 消费；
- Token 流的逐个消费由 CParser 的私有 `NextToken()` 完成（语义见 §6.3），词法模块不感知语法消费过程。

### 6.3 CParser 语法分析模块（接口约定）

```cpp
// 职责：递归下降（Q4 定稿）；自顶向下推导，同步执行语法制导语义动作；
//       显式构建 AST（基类+派生类+unique_ptr，Q10 定稿）；
//       语义动作经 SymbolTable 接口完成登记与查重，不直接操作符号表内部结构。
class CParser
{
public:
    explicit CParser(const TokenList& tokens);          // 构造：绑定词法输出
    ASTNodePtr Parse();                                 // 启动语法分析，返回程序根节点（unique_ptr）
    bool    HasError() const;                           // 是否已发生语法错误
    const std::vector<CompileError>& GetErrors() const; // 语法错误清单（含行列号，Q8）
};
```

**调用约定与辅助函数（v3.1 与代码对齐）**：
- `Parse()` 预置约定：先 `m_iListPos = 0`、清空 `m_errors`，随后 `m_CurToken = (*m_pTokenList)[0]`（预置为 Token 流首元素 tokens[0]；Token 流为空时报错并返回 nullptr）；解析函数消费完当前 token 后**自行调用私有 `NextToken()`** 推进游标；
- 私有 `NextToken()` 语义：先判断 `m_iListPos + 1 < m_pTokenList->size()`，成立则 `++m_iListPos`（**先 ++ 再取**），再取 `m_CurToken = (*m_pTokenList)[m_iListPos]`；遍历到末尾时游标不再前进，**停在最后一个 token（即 `TOKEN_EOF`）**，不会越界；
- 辅助函数：parser.h 当前**无 Check / Expect** 等终结符匹配辅助函数（也无 Match）；组员实现递归下降时可自行添加私有 helper（如 `bool Check(TokenType t)` / `bool Expect(TokenType t)`），但**本节 4 个公开接口签名不可改**（构造、`Parse`、`HasError`、`GetErrors`）。

### 6.4 SymbolTable 语义分析与符号表模块（接口约定）

```cpp
// 职责：维护多表符号表（Q1 定稿：变量表/常量表/类型表分表）；
//       登记、查重、类型链构造；常量语义回填（Q12）；float→int 隐式转换警告（Q9）。
class SymbolTable
{
public:
    bool    EnterScope(ScopeType t);                    // 进入作用域（基础版仅全局，挑战版函数）
    void    ExitScope();                                // 退出作用域
    VarEntry*    RecVar(const std::string& name, TokenType type, int line, int col);   // 登记变量（重名报错）
    ConstEntry*  RecConst(const std::string& name, TokenType type, double value, int line, int col); // 登记常量
    const VarEntry*   SearchVar(const std::string& name) const;    // 查变量（按作用域链）
    const ConstEntry* SearchConst(const std::string& name) const;  // 查常量
    void    BackfillConst(int iConstIdx, const std::string& name, TokenType type); // Q12：常量语义回填
    bool    CheckAssign(TokenType lhs, TokenType rhs, int line, int col); // Q9：float→int 允许并打印警告
    void    DumpSymTab(std::ostream& os) const;         // 符号表输出（供 --dump-symtab）
};
```

### 6.5 IRGenerator 中间代码生成模块（接口约定）

```cpp
// 职责：遍历 AST 生成四元式序列（Q6 定稿：操作数引用式、打印转原文）；
//       申请临时变量/标号；类型转换按 g_TypeSysTable（int/float 少量规则）。
class IRGenerator
{
public:
    explicit IRGenerator(SymbolTable& symtab);          // 构造：绑定符号表（确定操作数类型）
    void    Generate(const ASTNodePtr& root);           // 入口：以程序根节点生成代码
    int     GetTmpVar(TokenType t);                     // 申请临时变量（参考书 GetTmpVar，_Txxx 命名）
    int     GetTmpLabel();                              // 申请标号（挑战版控制流）
    void    EmitIR(QuadOp op, const Operand& op1, const Operand& op2, const Operand& rslt); // 追加四元式
    const CodeList& GetCodes() const;                   // 输出四元式序列（供 CCodeGen / --dump-quads）
    std::string OperandToString(const Operand& op) const; // Q6：操作数索引转原文（打印用）
};
```

### 6.6 CCodeGen 目标代码生成模块（接口约定）

```cpp
// 职责：四元式 → x86 汇编（Q3 定稿：基础版止于四元式，挑战版做 x86 汇编）；
//       指令模板匹配 + 寄存器分配（§4.6 骨架）。
class CCodeGen
{
public:
    explicit CCodeGen(const CodeList& codes);           // 构造：绑定 IR 输入
    bool    Generate(AsmOutput& out);                   // 生成汇编文本（挑战版；基础版不调用）
};
```

### 6.7 main 入口与命令行解析约定（Q7 定稿）

- 命令行格式：`minic <源文件> [--dump-tokens] [--dump-ast] [--dump-symtab] [--dump-quads]`
- 四类产物落盘文件名（源文件去扩展名后追加后缀，与源文件同目录）：
  | 选项 | 产物文件 |
  |---|---|
  | `--dump-tokens` | `<源文件名>.token.txt` |
  | `--dump-ast` | `<源文件名>.ast.txt` |
  | `--dump-symtab` | `<源文件名>.sym.txt` |
  | `--dump-quads` | `<源文件名>.quad.txt` |
  | 挑战版汇编 | `<源文件名>.asm`（v3.0 决议：后缀固定 .asm） |
- main 流水线约定：
  1. 解析命令行参数（未知选项报错并打印用法）；
  2. `CLex` 打开源文件并 `Run()`；若指定 `--dump-tokens` 落盘 Token 序列；
  3. `CParser::Parse()` 构建 AST；若指定 `--dump-ast` 落盘 AST 文本（前序遍历+缩进）；
  4. 语义阶段：`SymbolTable` 登记/查重/类型检查（可由 CParser 语法制导触发）；若指定 `--dump-symtab` 落盘符号表；
  5. `IRGenerator::Generate()`；若指定 `--dump-quads` 落盘四元式（`OperandToString` 转原文）；
  6. 挑战版：`CCodeGen::Generate()` 输出汇编文件（固定后缀 `<源文件名>.asm`，v3.0 决议）；
  7. 汇总 `g_ErrorList`：有错误按非零退出码结束，错误信息含行列号（Q8）。

---

## 7. 设计决策说明

### 6.1 v2.0 相对 v1.0 的变更

| 维度 | v1.0（C 风格） | v2.0（C++ 修订版） |
|---|---|---|
| 实现语言 | C 风格结构体 | **C++ 类/结构体**（参考书全部程序以 C++ 描述，保持一致） |
| 设计基准 | PPT 为主、参考书为辅 | **参考书 NeoPascal 工程为基准**，PPT 为硬性约束锚点 |
| 符号表 | 单张表 + 哈希索引 | **多表组织**（VarInfoTbl/ConstInfoTbl/TypeInfoTbl/...，参考书 CSymbolTbl） |
| AST | 单一节点 + kind 判别 | **ASTNode 基类 + 派生类**（多态），备选 variant 方案 |
| 四元式 | Quad + Operand（简化） | Quad(IRCode) + Operand(OpInfo)，含 UD/DU 链、引用标记等参考书字段 |
| 词法 | 手写状态机概述 | **转换表 m_szLexTbl + CLex 类**（参考书程序 2-1/2-2） |
| 语法 | 递归下降概述 | **LL(1) 预测分析**（First/Follow/预测表 + 辅助栈） |
| 目标代码 | 简单骨架 | **指令模板 + 寄存器描述**（参考书第 9 章完整骨架） |
| 语义辅助 | 未涉及 | **iTypePos/iTypeFlag/iIdListFlag 辅助栈、类型系统表**（参考书第 4/6 章） |

### 6.2 参考书借鉴映射

| 参考书元素 | MiniC 落地 | 取舍说明 |
|---|---|---|
| 单词三元组 CToken (m_iKind, m_szContent, m_LineInfo) | `Token` 类 | 保留 m_iKind/m_szContent/行号，扩展 PPT 列号与常量表位序 |
| 转换表 m_szLexTbl[50][130]、关键字表 map<string,int> | `CLex` 成员 | 直接采用 |
| LL(1) + First/Follow + 预测表 + 辅助栈 | `CParser` | 直接采用；MiniC 文法简单，辅助栈多为挑战版启用 |
| 多表符号表 CSymbolTbl（Var/Const/Type/Proc/Label/Enum） | `SymbolTable` 多表 | 基础版启用 3 张主表，其余挑战版启用 |
| ObjectInfo 基类 | `SymbolEntry` 基类 | 保留 m_szName/m_iProcIndex，扩展 PPT scope/line |
| VarInfo/ConstInfo/TypeInfo | `VarEntry/ConstEntry/TypeEntry` | 字段对齐参考书 |
| IRCode + OpInfo + EmitIR | `Quad + Operand + EmitIR` | 直接采用，操作码按 MiniC 收窄 |
| GetTmpVar/GetTmpLabel/_Txxx 命名 | `IRGenerator` 计数器 | 直接采用 |
| 类型系统表 TypeSysInfo + OpConstSemantic | `g_TypeSysTable` + 常量折叠 | 规则数从参考书规模简化为 int/float 少量规则 |
| CurrentStatement + LabelIdx | `m_StmtStack + LabelIdx` | 挑战版启用 |
| Pattern/Code/RegFlg/Size2Reg/RegRela/RegVal | `InstPattern/CodeLine/RegFlag/...` | 扩展设计，暂不承诺实现 |

### 6.3 刻意简化（避免过度设计，与 v1.0 结论一致）

- 参考书类型系统含 1300+ 规则；MiniC 仅 int/float，`g_TypeSysTable` 规则数个位数；
- 参考书六张符号表为完整 Pascal 语言服务；MiniC 基础版启用三张表即可满足 PPT 验收；
- 参考书目标代码基于 i386 完整工程；MiniC 是否启用取决于课程要求（Q3）。

---

## 8. 待确认问题清单（定稿）

> **定稿说明**：以下 12 项问题已于 2026-10-08 全部按"建议默认"值定稿，**不再需要确认**；实现时严格按下表"定稿结论"执行（v1.0 原有 10 项与 v2.0 新增 Q11/Q12 一并定稿）。

| 编号 | 问题（定稿结论） | 影响范围 | 状态 |
|---|---|---|---|
| Q1 | **符号表采用多表组织**（变量表/常量表/类型表分表，参考书 CSymbolTbl），不回退单表 | §4.4 | **已定稿** |
| Q2 | **开发语言 C++17 + CMake** 构建（VS/MinGW 兼容） | 全部代码 | **已定稿** |
| Q3 | **目标代码**：基础版止于四元式；**挑战版做 x86 汇编**（启用 §4.6 骨架） | §4.6 | **已定稿** |
| Q4 | **语法分析采用递归下降**（保留 LL(1) 思路，不另做预测表驱动） | §4.3 CParser | **已定稿** |
| Q5 | **挑战版功能顺序**：控制流(if/while) → print → 数组 → 函数 | 各模块扩展预留 | **已定稿** |
| Q6 | **四元式操作数引用式**（表索引）；打印时按索引转原文（OperandToString） | §4.5 Operand | **已定稿** |
| Q7 | **命令行 + 四类产物落盘**：--dump-tokens/ast/symtab/quads → *.token.txt/*.ast.txt/*.sym.txt/*.quad.txt | 工程框架、I/O | **已定稿** |
| Q8 | **错误处理**：报告行列号并**尽量错误恢复**（参考书 EmitError 风格） | 错误输出 | **已定稿** |
| Q9 | **float→int 允许隐式转换（C 语义截断）并打印警告** | 类型检查规则 | **已定稿** |
| Q10 | **AST 采用基类 + 派生类 + unique_ptr**（弃用 variant）；接口冻结以第 6 章为准 | 工程文档 | **已定稿** |
| Q11 | **词法转换表手写简化版**（m_szLexTbl），与参考书对齐并提交 | §4.2 CLex | **已定稿** |
| Q12 | **常量表：词法登记、语义回填**（RecConstTbl + m_szName 回填），常量表挂 g_SymbolTbl | §4.2/§4.4 | **已定稿** |

---

## 9. 附录 A：参考书 C++ 结构 ↔ MiniC 结构对照表

| 参考书（NeoPascal） | 参考书出处 | MiniC（本文档） | 状态 |
|---|---|---|---|
| `int m_szLexTbl[50][130]` 转换表 | 声明 2-2 | `CLex::m_szLexTbl` | 采用 |
| `map<string,int> m_KeywordTbl` | 声明 2-3 | `CLex::m_KeywordTbl` | 采用 |
| `CToken { m_iKind; m_szContent; m_LineInfo }` | 声明 2-4 | `Token` 类 | 采用+扩展 col/constIdx |
| `LineInfo { m_iRow; m_szFileName }` | 声明 2-5 | `LineInfo` | 采用 |
| `vector<CToken> TokenList` | 声明 2-6 | `using TokenList = vector<Token>` | 采用 |
| `CLex`（程序 2-2） | 程序 2-2 | `CLex` 类 | 采用 |
| LL(1) 预测分析 + First/Follow | 第 3 章 | `CParser` + 预测表 | 采用 |
| 语义子程序 semantic0xx + 辅助栈 | 第 4 章 | 语法制导翻译 + 辅助栈 | 采用 |
| `ObjectInfo { m_szName; m_iProcIndex }` | 第 4 章 | `SymbolEntry` 基类 | 采用+扩展 PPT 字段 |
| `VarInfo / ConstInfo / TypeInfo / ProcInfo / ParaInfo / LabelInfo` | 第 4 章 | `VarEntry / ConstEntry / TypeEntry / ProcEntry / ParamEntry / LabelEntry` | 采用 |
| `CSymbolTbl`（多表 + ProcStack + SearchXxxTbl） | 第 4 章 | `SymbolTable` 类 | 采用 |
| `iTypePos / iTypeFlag / iIdListFlag` 辅助栈 | 声明 4-13 | 同名列 | 采用（挑战版为主） |
| `IRCode { m_eOpType; m_Op1; m_Op2; m_Rslt }` | 第 5 章 | `Quad` 类 | 采用 |
| `OpInfo { m_iType; m_iLink; m_bRef; m_iDetailType; UD/DU 链 }` | 第 5 章 | `Operand` 类 | 采用（UD/DU 暂不启用） |
| NPIR 操作符 | 第 5 章表 5-2 | `QuadOp` 枚举 | **采用（21 大类 + 宽度后缀，151 个成员）**，见 §4.5.1 的变更说明 |
| `EmitIR / GetTmpVar / GetTmpLabel` | 第 5 章 | `IRGenerator` | 采用 |
| `TypeSysInfo + OpConstSemantic` | 第 6 章 | `g_TypeSysTable` + 常量折叠 | 采用（规则简化） |
| `CurrentStatement + LabelIdx` | 第 7 章 | `m_StmtStack + LabelIdx` | 挑战版启用 |
| `Pattern / Code / RegFlg` | 第 9 章 | `InstPattern / CodeLine / RegFlag` | 扩展预留 |
| `Size2Reg / Reg2Size / RegRela / RegVal` | 声明 9-1~9-5 | 同名结构 | 扩展预留 |
| `AsmScheme.txt` 指令模板文件 | 第 9 章 | 模板库加载 | 扩展预留 |
---

## 10. 附录 B：模块接口速览（定稿）

> 速览表为第 6 章接口契约的一页摘要，供组员分工开发时快速查阅；以第 6 章完整签名与注释为准。

### 10.1 五模块接口速览

| 模块 | 对外接口（类方法/签名） | 职责 | 数据传递 |
|---|---|---|---|
| CLex（词法） | `bool OpenSource(const string&)`；`bool Run()`；`const TokenList& GetTokenList() const` | 转换表扫描源文件，产出 Token 序列，登记常量文本（Q12） | `TokenList` → CParser |
| CParser（语法） | `CParser(const TokenList&)`；`ASTNodePtr Parse()`；`bool HasError() const`；`const vector<CompileError>& GetErrors() const` | 递归下降 + 语法制导语义动作，显式构建 AST | `ASTNode*` → 语义/IRGenerator |
| SymbolTable（语义/符号表） | `EnterScope/ExitScope`；`RecVar/RecConst`；`SearchVar/SearchConst`；`BackfillConst`；`CheckAssign`；`DumpSymTab(os)` | 多表登记/查重/类型检查，常量回填，float→int 警告 | 符号信息 → IRGenerator |
| IRGenerator（中间代码） | `Generate(const ASTNodePtr&)`；`GetTmpVar/GetTmpLabel`；`EmitIR(op,op1,op2,rslt)`；`GetCodes() const`；`OperandToString(op)` | 遍历 AST 生成四元式，临时变量/标号，类型转换 | `CodeList(Quad[])` → CCodeGen / --dump-quads |
| CCodeGen（目标代码） | `CCodeGen(const CodeList&)`；`bool Generate(AsmOutput&)` | 四元式 → x86 汇编（挑战版，Q3） | 汇编输出（挑战版） |

### 10.2 main 入口速览

- 命令行：`minic <源文件> [--dump-tokens] [--dump-ast] [--dump-symtab] [--dump-quads]`
- 产物落盘：`<源文件名>.token.txt / .ast.txt / .sym.txt / .quad.txt`（与源文件同目录）
- 流水线：CLex.Run() → CParser.Parse() → SymbolTable 语义检查 → IRGenerator.Generate() → （挑战版）CCodeGen.Generate()；错误汇总 `g_ErrorList`，含行列号（Q8），非零退出码。
