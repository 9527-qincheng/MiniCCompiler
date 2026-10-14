
# MiniC 编译器（挑战版，C++17）—— 项目说明

**当前进度**：词法模块已按 v4.0 挑战版完成（17 个新终结符、编码映射、双字符运算符、字符常量）；语法模块已按 v4.0 挑战版落地为 **LL(1) 预测分析表驱动**（92 条产生式，非递归分析栈 + `src/parser/grammar.cpp` 预测表，`--dump-parse` 输出分析过程）；其余模块（语义接入、中间代码、目标代码）保留完整接口实现，核心逻辑留 TODO。

依据：《MiniC编译器数据结构设计.md》第 4 章（数据结构）与第 6 章（接口契约，Q1~Q12 定稿）；挑战版文法见《MiniC编译器设计文档_v4.0_挑战版LL1.md》。

## 快速开始

```bash
git clone https://github.com/9527-qincheng/MiniCCompiler.git
cd MiniCCompiler
build.bat                                  # 一键编译（Windows）
build.bat run tests\test.mc                # 编译并运行
```

## Git 仓库

- 远程仓库：`https://github.com/9527-qincheng/MiniCCompiler.git`
- 分支说明：`main`（稳定分支，始终可运行、可演示，只接收 dev 合并）；`dev`（集成分支，组员从 `origin/dev` 切 feature 分支开发，完成后合并回 dev）
- 克隆命令：`git clone https://github.com/9527-qincheng/MiniCCompiler.git`

## 项目结构

```
MiniCCompiler/
├── build.bat               # 一键构建脚本（Windows，自动收集 src 下所有 .cpp）
├── CMakeLists.txt          # C++17 + GLOB_RECURSE，产出 minic
├── README.md
├── .gitignore
├── .gitattributes          # 统一换行符（LF）与二进制保护
├── tools/
│   └── generate_grammar.py # 由文法规格生成 src/parser/grammar.h/.cpp（阶段③）
├── src/
│   ├── main.cpp            # 命令行解析 + 流水线 + 六类产物落盘
│   ├── lexer/
│   │   ├── token.h/cpp     # Token/TokenType/LineInfo/CompileError/g_ErrorList（公共基础）
│   │   └── lexer.h/cpp     # CLex（词法）
│   ├── parser/
│   │   ├── ast.h/cpp       # ASTNode + 18 类节点（PROGRAM/DECL/FUNCDEF/PARAM/ARRAYDECL/BLOCK/ASSIGN/IF/WHILE/PRINT/RETURN/EMPTY/BINOP/UNOP/NUM/IDENT/INDEX/CALL，统一 m_children 多子节点）
│   │   ├── grammar.h/cpp   # 92 条产生式 + FIRST/FOLLOW/SELECT + 预测分析表（脚本生成）
│   │   └── parser.h/cpp    # CParser（LL(1) 预测分析表驱动，分析栈 + 语义动作 + 轨迹）
│   ├── symbol/
│   │   └── symbol.h/cpp    # SymbolTable 多表（Var/Const/Type/Proc/Param/Label）+ 语义接口
│   ├── sema/
│   │   └── ir.h/cpp        # Quad/Operand/QuadOp + IRGenerator（核心 TODO）
│   └── codegen/
│       └── codegen.h/cpp   # AsmOutput + CCodeGen（挑战版骨架）
├── tests/
│   ├── test.mc             # 最小示例：int a; float b; a = b + 1;
│   ├── lex_test.mc         # 词法测试（注释/字符串/关键字）
│   ├── lex_challenge.mc    # 挑战版词法（17 个新终结符全覆盖）
│   ├── lex_op_edge.mc      # 单/双字符运算符边界
│   ├── lex_char.mc         # 字符常量与转义
│   ├── bad_lex.mc          # 词法错误恢复
│   ├── bad_char.mc         # 字符常量错误
│   ├── bad_op.mc           # 运算符错误
│   ├── parser_expr.mc      # 表达式优先级
│   ├── bad_parser.mc       # 语法错误恢复
│   ├── parser/             # Parser 回归用例（40 个 .mc）与期望结果
│   │   ├── *.mc            #   合法 21 条 + 非法 19 条
│   │   └── expected/       #   *.ast.txt / *.err.txt / ll1_411.parse.txt
│   ├── grammar_selfcheck.cpp      # 文法表自检（21 项断言）
│   ├── parser_unit_test.cpp       # Parser 单元测试（AST 字段级）
│   ├── run_lexer_tests.ps1        # 词法回归测试脚本（需 PowerShell 7）
│   ├── run_parser_tests.ps1       # 回归测试脚本（需 PowerShell 7）
│   ├── run_parser_unit.ps1        # 单元测试脚本（需 PowerShell 7）
│   └── run_grammar_selfcheck.ps1  # 文法自检脚本（需 PowerShell 7）
├── examples/
│   └── example.mc          # 挑战版关键字示例
├── docs/
│   └── interface.md        # 接口备忘 + 冲突记录
└── build/                  # 构建产物目录（已被 .gitignore 排除）
```

## 构建

要求：C++17 编译器（MinGW g++ 实测通过，版本 15.1.0）。

### 方式一：build.bat（Windows 推荐，一键构建）

```bat
build.bat
```

即产出 `build\minic.exe`。

**其他子命令**：

| 命令 | 作用 |
|---|---|
| `build.bat` | 编译，产出 `build\minic.exe` |
| `build.bat test` | 编译后跑全部 4 套测试，任一失败即返回 1 |
| `build.bat run tests\test.mc` | 编译后运行指定源文件 |
| `build.bat list` | 列出参与编译的源文件 |
| `build.bat clean` | 清理 `build` 目录 |
| `build.bat rebuild` | 先清理再编译 |

> `build.bat test` 等价于依次执行 `run_lexer_tests.ps1`、`run_grammar_selfcheck.ps1`、
> `run_parser_tests.ps1`、`run_parser_unit.ps1`，适合提交前自检与 CI。
> 退出码会如实传播（**注意**：`build.bat run` 也传播 `minic` 的退出码，
> 早前版本因 `endlocal`/`exit` 分行书写会吞掉退出码恒返回 0，现已修复）。

**特点**：

- **自动递归扫描 `src/` 下所有 `.cpp`**，新增源文件**无需修改脚本**
  （与 `CMakeLists.txt` 的 `GLOB_RECURSE` 行为一致）
- 编译失败时识别两类常见链接错误并给出排查提示：

  | 报错 | 含义 |
  |---|---|
  | `multiple definition of 'X'` | **撞名冲突**（两个 `.cpp` 定义了同名类/函数） |
  | `undefined reference to 'X'` | **漏文件**（某个 `.cpp` 未参与编译） |

- 额外检查产物大小，避免 **0 字节 exe** 被误判为构建成功
- 纯 ASCII 编码，避免 cmd 下中文乱码

> **注意**：`g++` 默认用 `collect2` 包装链接器，真实错误可能只显示
> `ld returned 1 exit status`，看不出原因。若需完整信息，可逐个编译后直接调用 `ld`：
> ```bat
> g++ -std=c++17 -Isrc -c src\lexer\token.cpp -o token.o
> ld -o build\minic.exe *.o
> ```

### 方式二：CMake（需自行安装 CMake）

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

> 本项目 `CMakeLists.txt` 按标准语法编写，但**开发环境未安装 CMake，尚未实测**。
> 若安装：`winget install Kitware.CMake`

### 方式三：手动 g++（不推荐）

```bash
g++ -std=c++17 -Wall -Wextra -Isrc -o build/minic.exe ^
    src/lexer/token.cpp src/lexer/lexer.cpp ^
    src/parser/ast.cpp src/parser/parser.cpp ^
    src/symbol/symbol.cpp src/sema/ir.cpp ^
    src/codegen/codegen.cpp src/main.cpp
```

> ⚠️ 此方式**需手动列举全部源文件**——挑战版新增 `.cpp` 时若忘记加入，
> 会报 `undefined reference`。**建议改用 `build.bat`**。

产出可执行文件：`build/minic.exe`。

### 源文件编码与换行约定

| 项 | 约定 | 说明 |
|---|---|---|
| 文本编码 | **UTF-8 无 BOM** | 词法器现已能跳过源文件开头的 BOM（`lexer.cpp` OpenSource），但仍统一无 BOM 以免歧义 |
| 换行符 | **LF**（`.gitattributes` 已强制） | 由 `.gitattributes` 统一，避免各人 `core.autocrlf` 配置不同产生的全文件 diff |
| `.bat` 脚本 | 纯 ASCII + CRLF | cmd 按本地代码页解析，含中文会导致命令被拆解 |
| `.ps1` 脚本 | UTF-8 无 BOM + LF | 5.1 会按 ANSI 解码中文注释并报误导性语法错，故要求 PowerShell 7 |

> **注意**：仓库内文本统一以 LF 存储；工作区检出时的实际换行符取决于各人 `core.autocrlf`。
> 解析测试脚本已内置 `Normalize()` 归一化，比对不受影响。

## 运行

```bash
build.bat run tests\test.mc --dump-tokens --dump-ast --dump-symtab --dump-quads --dump-parse
```

或直接调用已编译好的程序：

```bash
build\minic.exe tests\test.mc --dump-tokens --dump-ast --dump-symtab --dump-quads --dump-parse
```

输出（与源文件同目录，源文件去扩展名后加后缀）：

| 选项 | 产物文件 | 当前状态 |
|---|---|---|
| `--dump-tokens` | `tests/test.token.txt` | 已实现（词法演示） |
| `--dump-ast` | `tests/test.ast.txt` | 已实现（LL(1) 表驱动解析产出的 AST） |
| `--dump-parse` | `tests/test.parse.txt` | 已实现（分析栈/剩余输入/动作 逐步轨迹，对齐设计文档 §4.11） |
| `--dump-symtab` | `tests/test.sym.txt` | 基础登记/查重可用 |
| `--dump-quads` | `tests/test.quad.txt` | 骨架占位（TODO） |

- 默认同时生成 `<源文件名>.asm`（挑战版汇编骨架占位）。
- 出错时退出码为 1（错误含行列号，汇总于 `g_ErrorList`）；成功退出码 0。

### Token 输出示例（tests/test.mc）

```
1:1  INT     int
1:5  IDENT   a
1:6  SEMI    ;
2:1  FLOAT   float
...
```

## Lexer 回归测试

词法模块的自动化回归测试（覆盖关键字/标识符/常量/运算符与界符/注释/字符串/字符转义/错误恢复）：

```bash
pwsh -File tests/run_lexer_tests.ps1

# 确认行为变更是有意为之、且人工核对过 token 流之后，刷新期望结果
pwsh -File tests/run_lexer_tests.ps1 -Update
```

- 用例输入：`tests/<case>.mc` 共 6 条；期望结果：`tests/<case>.token.txt`（与输入同目录）
- 对每个用例执行 `minic <case>.mc --dump-tokens`，逐字比对 token 流
- **不校验退出码**：`lex_char`/`lex_challenge`/`lex_op_edge` 是刻意的词法片段
  （如 `a == b;`、`char a = 'a';` 单独成行），语法阶段必然报错并返回 1；该退出码来自
  parser 而非 lexer，拿它判定词法正确性会产生误导
- 对 `bad_char`/`bad_op` 额外断言 stderr 至少含一条 `minic: error:`，防止词法把非法输入
  **静默吞掉**却输出"干净但不完整"的 token 流（这正是二者曾经的缺陷形态）
- 临时目录建在仓库内 `build\_lexer_scratch\`，不依赖系统 `%TEMP%`
- 需要已构建的 `build/minic.exe`（`build.bat build`）

| 用例 | 覆盖点 |
|---|---|
| `test` | 最小示例：`int`/`float`/赋值/算术/分号 |
| `lex_char` | 字符常量与转义 `'\n' '\t' '\'' '\\'` |
| `lex_challenge` | 挑战版词法全覆盖：`bool`/`true`/`char`/`==`/`!=`/`<=`/`>=`/`&&`/`\|\|`/`!` |
| `lex_op_edge` | 单/双字符运算符边界（`<` vs `<=`、`=` vs `==`、`!` vs `!=`） |
| `bad_char` | 字符常量错误恢复（未闭合、多字符、非法转义） |
| `bad_op` | 非法单字符运算符（`&`、`\|`、`<<`） |

## Parser 回归测试

语法分析模块的自动化回归测试（覆盖优先级/结合性/嵌套括号/合法与非法输入/错误恢复/既有黄金输出/LL(1) 分析轨迹）：

```bash
# 必须用 PowerShell 7（pwsh）：tests\*.ps1 为 UTF-8 无 BOM + 中文注释，5.1 会按 ANSI
# 解码而报语法错误；三个脚本已加 `#Requires -Version 7.0`，用 powershell.exe 5.1 运行会
# 直接提示版本不符（5.1 另有一个已知缺陷：Start-Process 在同时存在 NO_PROXY/no_proxy
# 时抛 "Item has already been added"，PowerShell 7 已修复）

# 校验（比对退出码、AST 输出、错误输出、分析轨迹）
pwsh -File tests/run_parser_tests.ps1

# 确认行为变更是有意为之后，刷新期望结果
pwsh -File tests/run_parser_tests.ps1 -Update
```

- 用例输入：`tests/parser/*.mc`；期望结果：`tests/parser/expected/*.ast.txt`、`*.err.txt`
- 对每个用例执行 `minic <case>.mc --dump-ast --dump-parse`，比对四项：退出码、AST、stderr、分析轨迹
- 分析轨迹 golden：`tests/parser/expected/ll1_411.parse.txt` **直接取自设计文档 §4.11 的 82 步表格**（不是程序自身输出），
  输入 `tests/parser/ll1_411.mc` 即 §4.11 的示例 `int a, b;  a = 2;  b = a + 1;`；
  该 golden 不随 `-Update` 刷新，需手工从文档重新提取
- 额外固定校验：`tests/test.mc` 的 AST 必须与 `tests/test.ast.txt` 逐字一致
- 需要已构建的 `build/minic.exe`（`cmake --build build`）
- 当前用例数：40 条（含 21 条合法、19 条非法）+ 1 条既有黄金输出固定校验

新增挑战版语法用例（阶段④ 补充）：

| 用例 | 覆盖点 |
|---|---|
| `compare_ops` | `< <= > >= == !=` 六种关系运算符及优先级 |
| `logic_ops` | `&& \|\| !` 与 `true/false` |
| `bool_char` | `bool`/`char` 类型、字符常量、`print` |
| `print_stmt` | `print(表达式);` 输出语句 |
| `array_1d` | `int a[10];` 数组声明与 `a[i]` 下标读写 |
| `func_def` | 函数定义、形参列表、带实参调用 |
| `nested_ctrl` | `while` + `if/else` + 数组下标 + 关系运算的嵌套结构 |
| `err_print_no_paren` | `print a;` 缺括号 |
| `err_array_no_size` | `int a[];` 缺维度常量 |
| `err_decl_after_stmt` | 声明必须位于语句之前（挑战版 `程序 → 声明部分 语句部分`） |
| `err_2d_array` | 挑战版文法数组仅一维（`int b[2][3];` 非法） |
| `err_func_bad_param` | 形参列表畸形 |

直接检查 AST 字段的 Parser 单元测试（常量表位序 `m_iConstIdx` 透传、`m_iSymIdx` 默认值）：

```bash
pwsh -File tests/run_parser_unit.ps1
```

- 需要 `g++` 在 PATH 中（可用 `-Cxx <path>` 指定编译器）
- 常量比对覆盖 v4.0 四类常量（整型/实型/字符/布尔）与数组维度常量
- 阶段⑤ 起新增**新 AST 不变量**断言：根为 `PROGRAM`、树中无 `AST_NODE_NONE` 哨兵、每个节点
  `Label()` 非空、各节点类别子节点个数符合语义表、`ARRAYDECL` 子节点为 `[DECL, 维度 NUM]`、
  `ASSIGN`/`INDEX` 基址槽与 `CALL` 被调名均已回填、`IF` 的 else 位于 `children[2]`

### 行为变更记录（golden 基线重定）

阶段④ 把递归下降替换为 LL(1) 预测分析表驱动后，语法行为有两处**有意**变化，期望文件已按新行为重定：

1. **`int a, b;` 由报错改为合法**：设计文档 §4.11 的示例输入即 `int a, b;`，挑战版文法产生式 7/8
   （`变量声明尾 → 数组后缀 变量声明尾1`、`变量声明尾1 → , 标识符 数组后缀 变量声明尾1`）明确支持一条声明多个声明符。
   因此原 `err_multi_decl`（`int a, b;`，期望退出码 1）**用例改名并转为正向用例** `multi_decl`（期望退出码 0），
   其 AST 包含两个 `Decl(INT a)` / `Decl(INT b)`。
   为不丢失"声明符列表畸形"的覆盖，新增 `err_decl_tail_no_name`（`int a, ;`，期望退出码 1）。
   **未删除用例、未放宽断言**。
2. **数组声明的 AST 形状**：`int a[10];` 产出 `ArrayDecl(INT a[10])`，子节点为 `Decl(INT a)` 与 `Num(10)`（维度常量）。
   阶段④ 先用解析器内部的桥接节点承载；**阶段⑤ 已合并为 `ast.h` 的正式节点 `AST_NODE_ARRAYDECL`**
   （02 任务书 §5.2 要求，设计文档枚举 17 类之外新增的第 18 类）。

其余用例的 AST 与错误定位（行列号）在换驱动后保持一致，未改动期望；错误恢复仍可单次运行报多个错误。

3. **阶段⑤ AST 文本基线整体重定**（2026-10-13，`ast.h` 破坏性重构，设计文档 §5.3.2【变更·破坏性】）：
   `List` 序列节点删除——根节点与块节点改为 `Program` / `Block`（子节点直接缩进，比 v3.1 少一层 `List` 包裹）；
   一元运算 `UnaryOp(...)` 改名 `UnOp(...)`；新增 `FuncDef`/`Param`/`ArrayDecl`/`Print`/`Index`/`Call`/`Empty` 等正式节点。
   本次**只改 AST 的文本形状**：37 条 `expected/*.ast.txt` 与 `tests/test.ast.txt` 重定，
   **无任何 `*.err.txt` 变化**（错误定位与错误恢复未动），`ll1_411.parse.txt`（§4.11 的 82 步轨迹）**逐字未变**。
   节点清单与子节点语义见 `docs/stage5_ast_review.md`。

## 各模块实现状态

| 模块 | 接口 | 状态 |
|---|---|---|
| CLex（词法） | OpenSource / Run / GetTokenList | **已实现（v4.0 挑战版）**：关键字(int/float/bool/char/void/if/else/while/print/return/true/false)、标识符、整型/实型/字符常量、运算符(+ - * / = == != < <= > >= && \|\| !)、界符((){}[];,)；注释与字符串；常量登记 m_ConstTexts；错误含行列号。**已补 TokenTypeToCode / CodeToTokenType 编码映射** |
| CParser（语法） | CParser(TokenList&) / Parse / HasError / GetErrors / GetTrace | **已实现（v4.0 挑战版 LL(1) 预测分析表驱动）**：92 条产生式驱动的非递归分析栈（`StackItem{TERM/NTERM/ACTION}` + 内部规约标记），语义动作 semantic000–027 按设计文档 §8 位置调度（符号表/IR 类动作登记占位，待阶段⑥⑦）；支持声明（含 `int a, b;` 多声明符与一维数组）、赋值/下标赋值/调用语句、表达式优先级与结合性、一元 `- !`、比较与逻辑运算符、`bool`/`char`/`true`/`false`、`print`、`if/else`（就近配对）、`while`、`return`、复合语句、函数定义与调用；恐慌式错误恢复可连续报错不崩溃；`GetTrace()` 输出分析轨迹供 `--dump-parse` 使用。公开签名与 §7.2 一致，`GetTrace()` 为新增 |
| SymbolTable（语义） | EnterScope / ExitScope / RecVar / RecConst / SearchVar / SearchConst / BackfillConst / CheckAssign / DumpSymTab | 基础实现：多表登记/查重/作用域/常量回填/float→int 警告（Q9） |
| IRGenerator（中间代码） | Generate / GetTmpVar / GetTmpLabel / EmitIR / GetCodes / OperandToString | 骨架：计数器/EmitIR/OperandToString 可用，遍历 AST 生成留 TODO |
| CCodeGen（目标代码，挑战版） | Generate / IRtoASM / AsmOutput | 骨架：输出结构完整，x86 翻译留 TODO |

> **词法现状说明**：17 个挑战版终结符（`bool` `char` `print` `true` `false` `[` `]` `==` `!=` `<` `<=` `>` `>=` `&&` `||` `!` 字符常量）**词法层已全部支持**；
> **语法层现状**：挑战版文法（92 条产生式）已由 LL(1) 预测分析驱动器全部覆盖，
> 原先"`print`/数组/函数等尚未实现"的表述已过时；仍属**预期报错**的是文法本身不允许的写法：
> `print` 必须带括号（`print a;` 非法）、数组仅一维（`int b[2][3];` 非法）、
> 声明必须全部位于语句之前（`程序 → 声明部分 语句部分`）。
> 详见《词法模块待改进清单.md》。

## 冲突记录与待决策项（不擅自变更接口）

生成骨架过程中发现以下与设计文档第 4 章不一致/未覆盖点，**按任务约定先记录、未擅自改接口**：

1. **ParamEntry vs ParaEntry**：任务要求类名 `ParamEntry`，设计文档第 4.4.1 为 `ParaEntry`；本骨架采用 `ParamEntry`，附录 B 对照表中亦为 ParaEntry，需确认统一名称。
2. **TokenType 缺少花括号界符**：任务要求词法识别 `{}`，设计文档 TokenType 未列 `TOKEN_LBRACE/TOKEN_RBRACE`；本骨架扩展了这两个枚举值（挑战版控制流必需），若文档需保持原枚举请在 v3.0 补充。
3. **AST 指针管理**：文档第 4.3 使用裸指针 `ASTNode* m_left/m_right`，任务/第 6 章要求 unique_ptr 管理（Q10）；本骨架采用 `std::unique_ptr<ASTNode>` 成员。
4. **SymbolTable 接口差异**：文档第 4.4 为 `RecVar(name,type,line)→int`、含 `RecConstTbl/SearchVarInfoTbl`；第 6 章契约为 `RecVar(name,type,line,col)→VarEntry*`、含 `EnterScope/ExitScope` 等。本骨架以**第 6 章契约为主**，同时保留第 4 章扩展接口（RecVarTbl/RecConstTbl/SearchXxxTbl）为兼容附加接口。
5. **IRGenerator 构造**：第 4.5.4 为无参构造，第 6.5 契约为 `explicit IRGenerator(SymbolTable&)`；按第 6 章契约实现（绑定符号表以支持 OperandToString 转原文）。
6. **ScopeType 枚举**：第 6 章 EnterScope 需要 `ScopeType t` 参数，文档未定义该枚举；本骨架补充 `ScopeType{SCOPE_GLOBAL, SCOPE_FUNCTION}`。
7. **main 输出 asm**：第 6.7 约定挑战版输出汇编文件但未给后缀；本骨架固定输出 `<源文件名>.asm`（挑战版占位）。
8. **v3.1 二叉树 AST 无法表达挑战版结构**（阶段④ 记录 → **阶段⑤ 已解决**）：挑战版的函数定义/形参、
   `print`、下标 `a[i]`、调用 `f(x)`、数组声明、空语句在 v3.1 的 11 类节点中没有对应类型。
   阶段④ 曾在 `src/parser/parser.cpp` 内用局部桥接节点 `BridgeNode : ASTNode`（`m_kind = AST_NODE_NONE`）承载；
   阶段⑤ 已按设计文档 §5.3.2【变更·破坏性】把 `ast.h` 重构为 `m_children` 多子节点 + 18 类节点
   （新增 `PROGRAM`/`FUNCDEF`/`PARAM`/`BLOCK`/`PRINT`/`INDEX`/`CALL`/`EMPTY`/`ARRAYDECL`，
   `UNARY` 改名 `UNOP`，删除 `LIST`），**桥接节点已全部删除**，AST 文本基线整体重定
   （见"行为变更记录"第 3 条）。
9. **字符/布尔常量未登记常量表下标**：词法 Q12 只为整型/实型字面量回填 `Token::m_iConstIdx`，
   `'x'`/`true`/`false` 的 `m_iConstIdx` 为 -1；Parser 按"透传 token 字段"原则原样保留，
   语义阶段 semantic003 需自行按 token 登记常量（跨模块记录，未改词法）。
10. **分析栈元素结构扩展**（阶段④ 记录）：设计文档 §4.10 的 `StackItem` 只有
    `{Kind{TERM,NTERM,ACTION}, m_iCode, m_iSemantic}`；为在错误恢复时同步截断内部语义值栈、
   并为每个符号保留源位置，实现时增加了 `m_iValueBase`（语义值栈基址）与 `m_loc` 两个字段，
    并新增内部 `Kind::REDUCE` 哨兵（用于在规约时组装 AST，静默处理、不进 `GetTrace()` 轨迹，
    以保证 §4.11 的 82 步与文档逐行一致）。文档字段语义与算法未变。

## 下一步（TODO 路线）

- 阶段 1（已完成，v4.0 已变更）：CParser 最初为递归下降（声明→赋值→算术表达式）；
  **v4.0 起改为 LL(1) 预测分析表驱动**（详见下方"挑战版语法"）
- **阶段 1.5（进行中）**：词法 v4.0 挑战版已完成（17 个终结符、编码映射、双字符运算符、字符常量）。
  1. ~~**UTF-8 BOM 识别**（记事本保存的 `.mc` 会报 3 个 `unexpected character`）~~ — ✅ 已修
     （`lexer.cpp` 的 `OpenSource` 检测并跳过 `EF BB BF`）
  2. ~~多字符常量 `'ab'` 报错后仍产出 token（同一错误报两遍）~~ — ✅ 已修
     （`ScanCharConst` 出错不再 `emplace_back`，改由语法阶段自然发现缺失）
  3. 字符串 `STRING` 定位说明（词法支持、文法不支持）— P1（待办，语义/文法侧定夺）
  4. ~~5 个新用例缺配套 `*.token.txt` 期望输出~~ — ✅ 已补，并新增
     `tests/run_lexer_tests.ps1` 纳入自动回归（6/6 通过）
  5. 字符/布尔常量未回填 `m_iConstIdx`（词法只为整型/实型登记常量表下标），
     语义阶段 semantic003 需按 token 自行登记 — P2（跨模块，已在 Parser 单元测试注释中记录）
- 阶段 2：语义动作（符号表登记/查重/类型检查/常量回填）接入
- 阶段 3：IRGenerator 遍历 AST 生成四元式（--dump-quads）
- 挑战版（Q5 顺序）：控制流(if/while) → print → 数组 → 函数 → x86 汇编
  - **语法侧已全部落地**：复合语句/if/while/return/一元运算、比较与逻辑运算符、
    `bool`/`char`/`true`/`false`、`print(表达式);`、一维数组声明与下标、函数定义/形参/调用
- **挑战版语法**：文法已定稿（92 条产生式、LL(1) 预测分析表，见《MiniC编译器设计文档_v4.0_挑战版LL1.md》）。
  - 阶段③ 编码层 + 文法表（`src/parser/grammar.h/.cpp`）**已完成**：产生式/FIRST/FOLLOW/SELECT/预测分析表，
    由 `tools/generate_grammar.py` 从 xlsx 生成（**禁止手工编辑 grammar.cpp**），
    自检 `pwsh -File tests/run_grammar_selfcheck.ps1` 全绿
  - 阶段④ 预测分析驱动器（`CParser` 重写 + `--dump-parse`）**已完成**：
    非递归分析栈、语义动作 semantic000–027 调度、恐慌式错误恢复（可连续报错）、
    §4.11 的 82 步轨迹与文档逐行一致
  - 阶段⑤ AST 多子节点重构（设计文档 §5.3.2【变更·破坏性】）**已完成**（2026-10-13）：
    基类 `m_children` 取代 `m_left/m_right`，节点类别 11 → 18 类（新增 PROGRAM/FUNCDEF/PARAM/BLOCK/
    PRINT/INDEX/CALL/EMPTY/ARRAYDECL，UNARY 改名 UNOP，删除 LIST），解析器内的桥接节点全部删除，
    Print 由基类统一实现（各节点只提供 `Label()`）；37 条 `expected/*.ast.txt` 与 `tests/test.ast.txt` 重定，
    **`*.err.txt` 与 §4.11 的 82 步轨迹逐字未变**；单元测试新增新 AST 不变量断言（17/17 通过）。
  - 待实现：阶段⑥ 符号表扩展 → 阶段⑦ 语义动作与 IR → 阶段⑧ 目标码
