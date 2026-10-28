# 语法分析模块（parser/）

> 上游：`lexer/`（`TokenList`）　下游：`sema/`（`ASTNodePtr`）
> 规模：6 个文件 2 859 行（`parser.cpp` 1 292 / `parser.h` 161 / `grammar.cpp` 784 / `grammar.h` 68 / `ast.cpp` 295 / `ast.h` 259）

## 1. 职责

用 **LL(1) 预测分析表** 把 Token 序列归约成抽象语法树（AST），并在出错时给出可读诊断、尽量恢复后继续分析。

## 2. 文件

| 文件 | 职责 |
|---|---|
| `grammar.h` | 表规模常量（`MAX_NT` / `MAX_TERM` / `PROD_COUNT`）与 `Production` 结构 |
| `grammar.cpp` | **机械生成**的产生式表、FIRST / FOLLOW / SELECT 集合、预测分析表（`tools/generate_grammar.py` 生成，勿手改） |
| `parser.h` | `CParser` 类声明、`StackItem` / `ParseStep` / `SemValue` 等内部结构 |
| `parser.cpp` | 双栈驱动器、错误恢复、19 类 AST 节点的构造 |
| `ast.h` | AST 节点类声明（`ASTNode` 基类 + 18 个派生类） |
| `ast.cpp` | 节点析构、`DumpAST` 与 `--dump-ast` 的文本格式 |

## 3. 对外接口

```cpp
class CParser
{
public:
    explicit CParser(const TokenList& tokens);
    ASTNodePtr Parse();                                  // LL(1) 预测分析表驱动，构建 AST
    bool HasError() const;                               // 是否出现语法错误
    const std::vector<CompileError>& GetErrors() const;  // 语法错误列表
    const std::vector<ParseStep>& GetTrace() const;
    void SetTraceEnabled(bool bOn);                      // 轨迹开关（默认关闭）
};
```

## 4. 文法规模

| 项 | 值 |
|---|---|
| 产生式 | **105** 条（编号 1–105） |
| 非终结符 | **48** 个（编码 100–147） |
| 终结符 | 编码 001–054 与 099（`#`） |
| FIRST 集合 | 89 项 |
| FOLLOW 集合 | 48 项 |
| 预测分析表 | 48 × 100（`g_PredictTable`） |
| 冲突 | **0** |

9 级优先级（低 → 高）：`||` < `&&` < `== !=` < `< <= > >=` < `+ -` < `* /` < 一元与 `++ --` 前缀 < 后缀 < 基本表达式。

## 5. 驱动器要点

- **双栈**：符号栈（`StackItem`）与语义栈（`SemValue`）。每归约一条产生式，按 `m_SemPos` / `m_SemAct` 依次执行语义动作 `semanticNNN`，由 `ExecuteAction()` 建出 AST 节点。
- **`m_trace` 默认关闭**：`RecordStep()` 每一步都要拼「分析栈全部内容 + 剩余全部 Token 文本」，是 O(步数 × 输入长度) 的开销。1000 个空函数时开启轨迹会让编译时间从 189 ms 涨到 68 150 ms。只有 `--dump-parse` 才调用 `SetTraceEnabled(true)`。
- **防死循环**：`Parse()` 用独立步数计数器 `iSteps` 与 `kMaxSteps = 1000000` 兜底，与轨迹行数解耦（轨迹关掉后行数恒为 0，不能用它做保护）。

## 6. 错误恢复

`HandleError()` 报出「`unexpected 'x'; expected one of: ...`」后由 `Synchronize()` 决定如何恢复：

- 常规做法是按 FOLLOW 集合弹栈；
- **遇到复合语句（编码 113）时不再按 FOLLOW 弹栈，而是跳到下一个 `}` 或文件尾直接返回**——否则一次函数体内的错误会把整个函数体连同后续函数一起弹掉，症状是「后面的代码全部被吞、只剩一条报错」。

## 7. 测试

| 脚本 | 覆盖 |
|---|---|
| `tests/run_parser_tests.ps1` | 50 个用例，比对 `--dump-ast` 产物与 `.err.txt` 诊断原文 |
| `tests/run_parser_unit.ps1` | 17 × 20 组单点断言 |
| `tests/run_grammar_selfcheck.ps1` | 文法自检（产生式条数、FIRST / FOLLOW、表无冲突） |

`tests/parser/expected/ll1_411.parse.txt` 是**外部权威基线**（取自《设计文档》§4.11 的 86 步分析过程），任何文法改动都必须同步它。

## 8. 修改指引

- **`grammar.cpp` 不要手改**。改 `tools/generate_grammar.py` 的输入后重新生成，脚本能逐字节复现现有文件（`Compare-Object` 已验证 IDENTICAL）。
- 改文法必然牵动三处：`grammar.cpp`、`tests/parser/expected/ll1_411.parse.txt`（86 步）、设计文档 §4.11 的表格与 trace。
- 新增 AST 节点要同步 `ast.h`（枚举 + 类）、`ast.cpp`（析构 + `DumpAST`）、`parser.cpp`（`ExecuteAction` 的构造分支）与 `sema/semantic.cpp`（`Visit` 分派）。
