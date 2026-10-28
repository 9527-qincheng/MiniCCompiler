# 词法分析模块（lexer/）

> 上游：源文件 `.mc`　下游：`parser/`（`TokenList`）
> 规模：4 个文件 979 行（`lexer.cpp` 540 / `lexer.h` 62 / `token.cpp` 229 / `token.h` 148）

## 1. 职责

把源程序字符流切分成 Token 序列，并给出每个 Token 的行号与列号。
只做「认词」，不做任何语义判断——同一个标识符是变量还是函数名，由 `sema/` 决定。

## 2. 文件

| 文件 | 职责 |
|---|---|
| `lexer.h` | `CLex` 类声明 |
| `lexer.cpp` | 手写扫描器实现（关键字表、标识符、数字、字符常量、字符串、运算符、注释跳过） |
| `token.h` | `TokenType` 枚举与编码、`Token`、`LineInfo`、`CompileError`、诊断接口 |
| `token.cpp` | `TokenTypeToString` / `TokenTypeToCode` / `CodeToTokenType` 映射、`EmitError` / `EmitWarning` 实现 |

## 3. 对外接口

```cpp
class CLex
{
public:
    CLex();
    bool OpenSource(const std::string& szFileName);  // 打开源文件（读入 m_szSource）
    bool Run();                                      // 扫描源文件，产出 Token 序列
    const TokenList& GetTokenList() const;           // 获取 Token 序列（供 CParser）
    void        SetFileName(const std::string& szFileName);
    const std::string& GetFileName() const;
    const std::string& GetSource()   const;
};
```

调用顺序固定为 `OpenSource` → `Run` → `GetTokenList`，见 `src/main.cpp`。

## 4. Token 编码

| 区段 | 编码 | 内容 |
|---|---|---|
| 关键字 | 001–012 | `int float bool char void if else while print return true false` |
| 界符 | 021–028 | `( ) { } [ ] ; ,` |
| 运算符 | 031–046 | `+ - * / = == != < <= > >= && \|\| ! ++ --` |
| 标识符与常量 | 051–054 | 标识符、整型常量、实型常量、字符常量 |
| 结束 | 099 | `#`（`TERM_CODE_EOF`） |

编码的唯一用途是作为 LL(1) 预测分析表 `g_PredictTable[MAX_NT][MAX_TERM]` 的第二维下标。
`TOKEN_STRING` 只由词法产生、不进文法——挑战版的 `print` 不支持字符串字面量。

## 5. Token 结构

```cpp
struct Token
{
    TokenType   m_type;       // 单词类型
    std::string m_szContent;  // 词法原文
    LineInfo    m_line;       // 行列号
    int         m_iConstIdx;  // 常量表下标（-1 表示不是常量）
    bool        m_bIsFloat;   // 实型常量标记
    double      m_fVal;       // 实型值
    long long   m_iVal;       // 整型值
    char        m_cVal;       // 字符常量值
};
```

## 6. 诊断

统一格式 `minic: error: 行:列: 消息`。词法阶段会报：

- `malformed number: extra decimal point`（`1.2.3`）
- `unterminated /* comment (started here)`（注释未闭合，位置指向 `/*` 处）
- 空字符常量 / 多字符常量 / 未闭合字符常量 / 未知转义

`EmitError` 把诊断同时写入 `g_ErrorList` 与 stderr；`EmitWarning`（语义阶段使用）只写 stderr，不影响退出码。
错误总数上限 `g_iMaxErrors = 100`，超出部分只计数不打印，末尾汇总一行说明省略了多少条。

## 7. 测试

`tests/run_lexer_tests.ps1`——6 套词法用例，比对 `--dump-tokens` 产物与期望文件。

## 8. 修改指引

- **不要靠调整 `TokenType` 的枚举顺序来改编码**：编码由 `TokenTypeToCode()` 显式映射，枚举顺序与编码无关。
- 新增终结符要同步三处：`token.h` 的枚举、`token.cpp` 的 `TokenTypeToString` / `TokenTypeToCode`、`parser/grammar.cpp` 的预测分析表（用 `tools/generate_grammar.py` 重新生成，不要手改）。
- 扫描器是手写的（`ScanIdent` / `ScanNumber` / `ScanCharConst` / `ScanString` / `ScanOperator`），**不要改成正则**——精确的行列号与错误定位依赖逐字符推进。
