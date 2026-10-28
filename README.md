
# MiniC 编译器（挑战版，C++17）—— 项目说明

**当前进度**：六个阶段全部打通，**编译器已可端到端运行**。词法模块已按 v4.0 挑战版完成（17 个新终结符、编码映射、双字符运算符、字符常量）；语法模块已按 v4.0 挑战版落地为 **LL(1) 预测分析表驱动**（105 条产生式，非递归分析栈 + `src/parser/grammar.cpp` 预测表，`--dump-parse` 输出分析过程）；符号表模块已实现多表登记、作用域链查找与数组登记；语义分析模块已实现并接入主流程（`src/sema/semantic.cpp`，遍历 AST 登记变量/数组/函数/常量并做类型检查）；**中间代码模块已实现**（`src/sema/ir.cpp`，四元式含 ITOF/FTOI 类型转换、`&&`/`||` 真短路、数组读写、函数调用与递归，`--dump-quads` 输出）；**目标代码模块已实现**（`src/codegen/codegen.cpp`，x86-64 / GNU as 汇编，产物可直接用 gcc 汇编链接成 exe 运行）。

依据：[docs/MiniC编译器数据结构设计.md](docs/MiniC编译器数据结构设计.md) 第 4 章（数据结构）与第 6 章（接口契约，Q1~Q12 定稿）；挑战版文法见 [docs/MiniC编译器设计文档_v4.0_挑战版LL1.md](docs/MiniC编译器设计文档_v4.0_挑战版LL1.md)。

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
│   ├── generate_grammar.py # 由文法规格生成 src/parser/grammar.h/.cpp（阶段③）
│   └── random_diff.py      # 随机差分：同一棵程序树渲染成 MiniC 与等价 C，比对两条链路的输出
├── src/
│   ├── main.cpp            # 命令行解析 + 流水线 + 六类产物落盘
│   ├── lexer/
│   │   ├── token.h/cpp     # Token/TokenType/LineInfo/CompileError/g_ErrorList（公共基础）
│   │   └── lexer.h/cpp     # CLex（词法）
│   ├── parser/
│   │   ├── ast.h/cpp       # ASTNode + 18 类节点（PROGRAM/DECL/FUNCDEF/PARAM/ARRAYDECL/BLOCK/ASSIGN/IF/WHILE/PRINT/RETURN/EMPTY/BINOP/UNOP/NUM/IDENT/INDEX/CALL，统一 m_children 多子节点）
│   │   ├── grammar.h/cpp   # 105 条产生式 + FIRST/FOLLOW/SELECT + 预测分析表（脚本生成）
│   │   └── parser.h/cpp    # CParser（LL(1) 预测分析表驱动，分析栈 + 语义动作 + 轨迹）
│   ├── symbol/
│   │   └── symbol.h/cpp    # SymbolTable 多表（Var/Const/Type/Proc/Param/Label）+ 语义接口
│   ├── sema/
│   │   ├── semantic.h/cpp  # SemanticAnalyzer（遍历 AST 填充符号表 + 类型检查）
│   │   └── ir.h/cpp        # Quad/Operand/QuadOp（表 5-2 的 21 大类 + 宽度后缀）+ IRGenerator
│   └── codegen/
│       └── codegen.h/cpp   # AsmOutput + CCodeGen（x86-64 / GNU as 汇编生成）
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
│   ├── parser/             # Parser 回归用例（49 个 .mc）与期望结果
│   │   ├── *.mc            #   合法 25 条 + 非法 24 条
│   │   └── expected/       #   *.ast.txt / *.err.txt / ll1_411.parse.txt
│   ├── codegen/            # 目标代码端到端用例（30 个 .mc + 期望输出）
│   ├── grammar_selfcheck.cpp      # 文法表自检（21 项断言）
│   ├── parser_unit_test.cpp       # Parser 单元测试（AST 字段级）
│   ├── symbol_test.cpp            # 符号表作用域链断言
│   ├── run_lexer_tests.ps1        # 词法回归（6 例）
│   ├── run_grammar_selfcheck.ps1  # 文法表自检
│   ├── run_parser_tests.ps1       # 语法回归（50 例 AST / 错误基线）
│   ├── run_parser_unit.ps1        # Parser 单元测试
│   ├── run_symbol_tests.ps1       # 符号表作用域链断言
│   ├── run_symtab_tests.ps1       # 符号表 DumpSymTab 回归
│   ├── run_sema_tests.ps1         # 语义诊断回归（32 例，含警告与多错并报）
│   ├── run_quad_tests.ps1         # 四元式序列回归（18 例）
│   ├── run_opt_tests.ps1          # 优化后四元式回归（-O，2 例）
│   ├── sema/                      # 语义用例 *.mc + *.expected.diag.txt
│   ├── quad/                      # 四元式用例 *.mc + *.expected.quad.txt
│   ├── opt/                       # 优化后四元式用例（-O）
│   └── run_codegen_tests.ps1      # 目标代码端到端（编译→gcc 链接→运行→比对输出）
├── examples/
│   └── example.mc          # 挑战版关键字示例
├── docs/
│   ├── interface.md                            # 接口备忘 + 冲突记录（模块接口定义）
│   ├── MiniC编译器设计文档_v4.0_挑战版LL1.md     # 系统架构设计文档：文法/LL(1) 表/接口/语义动作
│   ├── MiniC编译器数据结构设计.md               # 数据结构设计：多表符号表/四元式/AST/指令模板
│   ├── MiniC语法支持清单.md                     # 语言规格：105 条产生式实测支持矩阵、与 C 的 19 条差异
│   ├── 词法模块待改进清单.md                    # 词法模块实测复审记录（4 项问题的修复依据）
│   ├── stage5_ast_review.md                     # 阶段⑤ AST 形状评审
│   ├── 验收报告.md                              # 测试验收报告：交付物核对/五模块记录/12 套回归/偏差与遗留
│   ├── 小组分工表.md                            # 第 5 组 12 人任务分工表（对应项目书表 3）
│   ├── 项目总结.md                              # 项目总结：架构/成果/困难与解决/经验/不足
│   ├── MiniC编译器_答辩演示.pptx                # 答辩 PPT（18 页）
│   ├── 语法分析过程_示例.xlsx                   # 分析过程示例（LL(1) 逐步推导）
│   ├── 语法分析过程_ll1_411.xlsx                # §4.11 的 82 步分析轨迹
│   ├── gui-implementation/                      # GUI 实现截图（01~11）
│   ├── gui-acceptance/                          # GUI 验收截图（S1~S5）
│   ├── archive/                                 # 过程文档存档：协作说明/审查报告/整改说明/模块任务书
│   │   ├── guides/                              #   组员使用指南、Git 协作须知、build.bat 说明
│   │   ├── reviews/                             #   7 轮提交审查报告 + 分支检查记录
│   │   └── taskbriefs/                          #   整改说明 8 份 + modules/ 各模块任务书 8 份
│   └── plans/                                   # 个人任务计划（12 人各一份，见 docs/plans/README.md）
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
| `build.bat test` | 编译后跑全部 12 套测试（9 套编译阶段回归 + 2 套 GUI 冒烟），任一失败即返回 1 |
| `build.bat run tests\test.mc` | 编译后运行指定源文件 |
| `build.bat list` | 列出参与编译的源文件 |
| `build.bat clean` | 清理 `build` 目录 |
| `build.bat rebuild` | 先清理再编译 |

> `build.bat test` 等价于依次执行 `run_lexer_tests.ps1`、`run_grammar_selfcheck.ps1`、
> `run_parser_tests.ps1`、`run_parser_unit.ps1`、`run_symbol_tests.ps1`、`run_symtab_tests.ps1`、
> `run_sema_tests.ps1`、`run_quad_tests.ps1`、`run_opt_tests.ps1`、`run_codegen_tests.ps1`，再跑 GUI 的
> `gui\test\book-smoke.js`（语法手册数据与渲染）与 `gui\test\sample-smoke.js`（示例库数据与
> 可编译性），两者都是纯 Node、不需要 Electron 窗口；PATH 里没有 `node` 时整体跳过，
> 适合提交前自检与 CI。仓库已配 `.github/workflows/ci.yml`：在 `windows-latest` 上装
> MSYS2 的 MinGW-w64，跑同一套 `build.bat test`，另加六份产物节标题核对与
> `-O` 语义一致性冒烟（优化前后 stdout 与退出码必须一致），失败时上传产物供排查。
> **该工作流已在 `dev` 上启用：2026-10-28 首次运行 85 秒全绿（7 个 step 全 success）。**
> 九个编译阶段各有自己的回归网：词法比 `.token.txt`、语法比 `.ast.txt`/错误消息、
> 符号表比 `.sym.txt`、语义比 stderr 诊断（含行列号）、中间代码比 `.quad.txt`、
> 目标代码是唯一走"汇编→链接→运行→比 stdout"的端到端。
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
| `--dump-symtab` | `tests/test.sym.txt` | 已实现（语义分析填充的多表符号表） |
| `--dump-quads` | `tests/test.quad.txt` | 已实现（四元式，含顶层代码与各函数过程） |
| `-O` / `--optimize` | （无独立产物） | 四元式优化：前三轮窥孔（常量条件跳转折叠 / 删不可达 / 删空跳 / 死临时量与死标号）+ 第四轮按基本块做常量传播 / 公共子表达式 / 死存储，迭代到不动点；**默认关闭**，关时产物与未加此选项完全一致 |
| `-h` / `--help` | stdout | 打印用法后退出码 0 |
| `-V` / `--version` | stdout | 打印 `MiniC Compiler 4.0 (challenge, x86-64)` 后退出码 0 |

- 默认同时生成 `<源文件名>.asm`（x86-64 / GNU as 汇编）。
- 汇编产物可直接交给 gcc 汇编链接后运行：`Copy-Item tests\test.asm tests\test.s; gcc -x assembler -o tests\test.exe tests\test.s; .\tests\test.exe`
  （Windows 上 gcc 不认 `.asm` 后缀，须先改名为 `.s` 或用 `-x assembler` 显式指定语言）。
- 出错时退出码为 1（错误含行列号，汇总于 `g_ErrorList`）；成功退出码 0。
- 错误条数上限 100：超出的错误仍计入 `g_ErrorList`（退出码与 `N error(s) found` 计数不变），
  只是不再逐条打到 stderr，汇总行前补 `minic: (N more error(s) suppressed; limit is 100)`。
- 非致命诊断走 `minic: warning: 行:列: 消息`，**不进 `g_ErrorList`**，因此不改变退出码
  （数组名不带下标、常量下标越界是**错误**；非 void 函数缺 `return`、无 `main` 且无顶层语句是**警告**）。

### Token 输出示例（tests/test.mc）

```
1:1  INT     int
1:5  IDENT   a
1:6  SEMI    ;
2:1  FLOAT   float
...
```

## 图形界面（GUI）

`gui/` 是基于 **Electron** 的桌面界面：暗/亮双主题、多文件标签、源码行号与语法高亮、
代码格式化、7 类产物页签，以及可点击跳转的诊断列表，把「写代码 → 编译 → 看产物 → **运行看 print 输出**」
串成一个窗口。

```bash
cd gui
npm install        :: 首次安装 Electron（.npmrc 已配置 npmmirror 镜像）
启动GUI.bat        :: 或 npm start；需先 build.bat 生成 build\minic.exe
```

| 页签 | 数据来源 |
|---|---|
| Token / 语法树 / 符号表 | `--dump-tokens` / `--dump-ast` / `--dump-symtab` 产物 |
| 四元式 / 汇编 | `--dump-quads` 与默认 `.asm` 产物（均为真实输出） |
| **运行输出** | **点「运行」（F5）：minic 编译 → gcc 汇编链接 → 执行 exe，抓 stdout/stderr** |
| 诊断 | stderr 的 `minic: error/warning: 行:列: 消息`，点击跳到源码行 |

> **`print` 的输出在哪看**：`print` 是运行期语句，`minic.exe` 只做编译，本身不输出 `print` 的内容。
> 点工具栏 **运行**（或 `F5`）会走完整链路并执行程序，`print` 的结果显示在「运行输出」页签，
> 页脚给出编译/链接/运行耗时与进程退出码。只点「编译」是看不到任何 `print` 结果的。
>
> 运行需要 MinGW 的 `gcc`（自动从 PATH 查找；也可用环境变量 `MINIC_GCC` 指定完整路径）。
> 程序若死循环，5 秒后被强制结束并提示超时。

**示例库**：工具栏「示例▾」提供 15 条源码，分 6 组——基础（最小程序、最大公约数）、类型（浮点、
字符与布尔）、函数（多实参与嵌套调用、递归）、数组与控制流（素数筛选、冒泡排序、数组统计、
嵌套循环与多分支）、综合（全特性演示）、错误演示（缺分号+未声明、未声明变量、顶层声明写在语句后）。
每条都标注了期望 stdout（或错误示例的 stderr 首行），**全部在真实工具链上跑通**：
`build\minic.exe` 编译 → `gcc -x assembler` 汇编链接 → 运行比对输出。
示例数据在 `gui/src/samples.js`，回归脚本 `gui/test/sample-smoke.js`。

**内建语法手册**：工具栏「语法手册」或 `F1` 打开全屏参考书，10 章 68 条，每条标注
`✓ 支持` / `✗ 不支持` / `⚠ 有限制` / `· 说明`；不支持的直接给真实报错原文，支持的给可运行示例
（可一键插入编辑器 / 载入为新文档 / 复制）。支持多关键词搜索与目录跳转。全部 60 条示例都在
`build\minic.exe` 上实测过编译退出码与运行输出。

- 用法、快捷键、目录结构与启动器原理见 [gui/README.md](gui/README.md)。
- 界面规范板（S1~S5）与验收截图见 [`docs/gui-acceptance/`](docs/gui-acceptance/)。
- 冒烟测试：`cd gui && node test\compile-smoke.js`（校验各类产物齐全、Token 格式与错误诊断格式）；
  `node test\run-smoke.js`（校验编译→链接→执行链路、编译失败、除零崩溃与死循环超时）；
  `node test\book-smoke.js`（语法手册数据层 + 渲染层）；`node test\sample-smoke.js`（示例库数据层与实跑）。
- 注意：本机实测原生程序在含中文的路径下文件 I/O 会失效，启动脚本会把 Electron 运行时部署到
  `%LOCALAPPDATA%\minic-gui\runtime`、`lib/compiler.js` 会把 `minic.exe` 复制到 `%TEMP%\minic-gui`，首次启动稍慢。

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
- 当前用例数：49 条（含 25 条合法、24 条非法）+ 1 条既有黄金输出固定校验

新增挑战版语法用例（阶段④ 补充）：

| 用例 | 覆盖点 |
|---|---|
| `compare_ops` | `< <= > >= == !=` 六种关系运算符及优先级（`< <= > >=` 高于 `== !=`，同 C++） |
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

## 语义分析诊断回归测试

```bash
pwsh -File tests/run_sema_tests.ps1
pwsh -File tests/run_sema_tests.ps1 -Case err_duplicate
pwsh -File tests/run_sema_tests.ps1 -Update
```

- 用例输入：`tests/sema/*.mc` 共 19 条；期望诊断：`tests/sema/<case>.expected.diag.txt`
- 期望类型写在用例首行 `/* expect: ok | err | warn:<条数> */`，新增用例不必改 runner
- 比对**诊断行的完整文本**（`minic: error: 行:列: 消息`），含行列号与错误码措辞；
  每条诊断后面还会跟两行源码上下文（`  4 |     a = 2147483649;` 与 `    |         ^`），
  这两行不进基线——runner 按 `minic: error:` 前缀过滤，只比对诊断行本身；
  只比条数会漏掉位置漂移，只比有无会漏掉措辞变化
- `err_` 用例要求退出码 1 且至少一条 error；`ok_` 要求退出码 0 且零 error（warning 允许，
  `warn:N` 会额外核对条数）；尾部汇总行 `N error(s) found` 不参与比对（由条数派生）
- 覆盖：未声明标识符、重复声明、形参与局部重名、`&&` 非 bool 操作数、嵌套函数定义、
  调用未定义函数、同名错误并报（3 条一次报出）、块内遮蔽合法、bool 作条件合法、
  void 裸 return 合法、float→int 截断告警（Q9）、INT↔BOOL 可疑赋值告警

## 四元式回归测试

```bash
pwsh -File tests/run_quad_tests.ps1
pwsh -File tests/run_quad_tests.ps1 -Case arith_int
pwsh -File tests/run_quad_tests.ps1 -Update
```

- 用例输入：`tests/quad/*.mc` 共 18 条；期望序列：`tests/quad/<case>.expected.quad.txt`
- 校验收敛到 `.quad.txt` 的**结构**：表达式求值顺序、临时变量编号（`_T1`/`_T2`…）、
  标号编号（`L1`/`L2`…）、短路展开形态、`ITOF`/`FTOI` 插入位置、`PARA`/`CALL` 的相对次序。

## 优化后四元式回归测试（-O）

```bash
pwsh -File tests/run_opt_tests.ps1
pwsh -File tests/run_opt_tests.ps1 -Case licm_hoist
pwsh -File tests/run_opt_tests.ps1 -Update
```

- 与上一节的唯一区别是编译时多带 `-O`：默认路径的产物由 `run_quad_tests.ps1` 守着，
  优化路径的产物由这一套守着，两边互不影响。
- 用例：`tests/opt/licm_hoist.mc`（`gi * 2` 应被提到前导块）、
  `tests/opt/licm_not_hoist.mc`（除法、依赖循环变量、循环内有函数调用，三种都不许提）。
- 这一套同时是「优化器的『不做』也要有基线」的落地：只测「提了没有」不足以发现问题，
  反例能拦住「提过头」。
  **操作码用参考书表 5-2 的助记符**（`ADD_4` / `ASSIGN_4F` / `LS_1` / `INDEX_4` / `PRINT_4` / `RETV` / `JNT`），
  宽度后缀直接体现在基线里：`ASSIGN_1` 是 char、`ASSIGN_4` 是 int、`ASSIGN_4F` 是 float。
  这些在端到端测试里完全测不出来（只比 stdout，结构变了照样输出对）
- 覆盖：整型与实型四则及优先级、混合类型转换、一元取负与逻辑非、if-else 双标号、
  while 四段式、六种关系运算、`&&`/`||` 真短路展开、多实参求值次序、嵌套调用
  （内层实参不得与外层 `PARA` 交错）、数组 `INDEX_*` 取址、`PRINT_*`/`RETV` 发射形态、多函数分段
- golden 命名为 `*.expected.quad.txt` 而非 `*.quad.txt`：minic 会把产物写在源文件同目录，
  若基线叫 `<用例>.quad.txt`，任何人直接对 `tests/quad/*.mc` 编译一次就会覆盖基线。
  `.gitignore` 的 `*.quad.txt` 因此加了 `!tests/quad/*.expected.quad.txt` 例外
- 另查一条硬约束：产物里不得出现盘符或反斜杠路径（四元式是逻辑表示，混进路径就无法跨环境比对），
  且必须含 `// --- proc: ` 段头


## 各模块实现状态

| 模块 | 接口 | 状态 |
|---|---|---|
| CLex（词法） | OpenSource / Run / GetTokenList | **已实现（v4.0 挑战版）**：关键字(int/float/bool/char/void/if/else/while/print/return/true/false)、标识符、整型/实型/字符常量、运算符(+ - * / = == != < <= > >= && \|\| !)、界符((){}[];,)；注释与字符串；常量登记 m_ConstTexts；错误含行列号。**已补 TokenTypeToCode / CodeToTokenType 编码映射** |
| CParser（语法） | CParser(TokenList&) / Parse / HasError / GetErrors / GetTrace | **已实现（v4.0 挑战版 LL(1) 预测分析表驱动）**：105 条产生式驱动的非递归分析栈（`StackItem{TERM/NTERM/ACTION}` + 内部规约标记），语义动作 semantic000–027 按设计文档 §8 位置调度（符号表/IR 类动作登记占位，待阶段⑥⑦）；支持声明（含 `int a, b;` 多声明符与一维数组）、赋值/下标赋值/调用语句、表达式优先级与结合性、一元 `- !`、比较与逻辑运算符、`bool`/`char`/`true`/`false`、`print`、`if/else`（就近配对）、`while`、`return`、复合语句、函数定义与调用；恐慌式错误恢复可连续报错不崩溃；`GetTrace()` 输出分析轨迹供 `--dump-parse` 使用。公开签名与 §7.2 一致，`GetTrace()` 为新增 |
| SymbolTable（符号表） | EnterScope / ExitScope / RecVar / RecArray / RecConst / SearchVar / SearchConst / BackfillConst / CheckAssign / DumpSymTab | **已实现**：多表登记、作用域链自内向外查找（内层可遮蔽外层）、数组登记（m_bIsArray / m_iArraySize）、常量回填、float→int 告警（Q9）；类型表由 `RecBuiltinTypes()` 登记 int/float/bool/char/void 五行并带字节宽度（`TypeEntry::m_iSize`：char=1，int/float/bool=4，void=0），`--dump-symtab` 的 `-- Types --` 节不再是空节 |
| SemanticAnalyzer（语义分析） | SemanticAnalyzer(SymbolTable&) / Analyze(const ASTNodePtr&) | **已实现**：遍历 AST 登记变量/数组/函数/形参/常量，未声明与重复声明报错，按 §8.1 类型系统做运算与赋值检查 |
| IRGenerator（中间代码） | IRGenerator(SymbolTable&) / Generate / GetTmpVar / GetTmpLabel / EmitIR / GetCodes / OperandToString | **已实现**：遍历 AST 生成四元式（`QuadOp` 按参考书表 5-2 建 21 大类 + 宽度后缀，共 151 个成员、MiniC 实际产生 30 个），含算术/关系/逻辑、`&&`/`||` 真短路（`Q_JNT`/`Q_JT`）、`ITOF`/`FTOI` 类型转换（赋值、实参、返回值三处均插）、一维数组读写（`Q_INDEX_*` 双向）、`if/else`、`while`、`print`、函数定义（四元式存入 `ProcEntry::m_Codes`）与调用（`Q_PARA` + `Q_CALL`）、递归、`++`/`--` 前后缀（`GenIncDec`）、**常量折叠**（`FoldBinary`/`ConstFloat`，两侧皆为编译期常量时直接求值，除零与整型溢出保守不折、`&&`/`||` 不折以保短路）；`--dump-quads` 输出顶层代码与各过程 |
| CCodeGen（目标代码，挑战版） | CCodeGen(const CodeList&) / BindSymTab / Generate / IRtoASM / AsmOutput | **已实现**：x86-64 / GNU as（AT&T 语法），栈帧 + `%rbp` 基址寻址、整型 `%eax` / 浮点 `%xmm0` 双通道、数组按 `元素宽度×下标` 的 `(%rax,%rcx,4)` 寻址、**自定义栈传参**（调用方按 `i*8(%rsp)` 落第 i 个实参、槽位 16 字节对齐，被调方从 `16+8*(pos-1)(%rbp)` 取形参；不走 System V 寄存器传参）、`printf` 输出（浮点同时放 `%xmm` 与第 2 个整型槽以适配 Windows ABI）；**累加器直通**（`%eax` 里已是所需值时不重复 load）与**立即数直用**（`imull $4, %eax`）；**热点局部整型标量固定寄存器**（每函数至多两个，放 callee-saved 的 `%ebx` / `%esi`，序言 `pushq` / 尾声 `popq`）；`QuadOp` 24 个值全部覆盖；产物可被 gcc 直接汇编链接 |

> **词法现状说明**：17 个挑战版终结符（`bool` `char` `print` `true` `false` `[` `]` `==` `!=` `<` `<=` `>` `>=` `&&` `||` `!` 字符常量）**词法层已全部支持**；
> **语法层现状**：挑战版文法（105 条产生式）已由 LL(1) 预测分析驱动器全部覆盖，
> 原先"`print`/数组/函数等尚未实现"的表述已过时；仍属**预期报错**的是文法本身不允许的写法：
> `print` 必须带括号（`print a;` 非法）、数组仅一维（`int b[2][3];` 非法）、
> 声明必须全部位于语句之前（`程序 → 声明部分 语句部分`）。
> 详见 [docs/词法模块待改进清单.md](docs/词法模块待改进清单.md)（词法侧 4 个问题的实测与修复记录）。

## 交付文档

项目书表 6 列了 5 项交付物，对应到仓库里的位置如下：

| 交付物 | 位置 | 状态 |
|---|---|---|
| ① 编译器源码 | `src/`（六段流水线） | ✅ 完成 |
| ② 技术文档 | [docs/MiniC编译器设计文档_v4.0_挑战版LL1.md](docs/MiniC编译器设计文档_v4.0_挑战版LL1.md)（系统架构设计）、[docs/MiniC编译器数据结构设计.md](docs/MiniC编译器数据结构设计.md)、[docs/interface.md](docs/interface.md)（模块接口定义）、[docs/MiniC语法支持清单.md](docs/MiniC语法支持清单.md)（语言规格） | ✅ 完成 |
| ③ 测试用例集 | `tests/`，12 套回归（9 套编译阶段 + 2 套 GUI 冒烟，见「构建」一节的 `build.bat test`） | ✅ 完成 |
| ④ 验收报告 | [docs/验收报告.md](docs/验收报告.md)（交付物核对 / 五模块十字段记录 / 12 套回归 / 偏差与遗留） | ✅ 完成 |
| ⑤ 答辩材料 | 答辩 PPT：[docs/MiniC编译器_答辩演示.pptx](docs/MiniC编译器_答辩演示.pptx)（18 页）<br>项目总结：[docs/项目总结.md](docs/项目总结.md) | ✅ 完成 |
| 个人任务计划（附录 A） | [docs/plans/](docs/plans/README.md)，12 人各一份 | ✅ 完成 |
| 小组分工表（项目书表 3） | [docs/小组分工表.md](docs/小组分工表.md)，12 人 + 模块人数 + 署名差异说明 | ✅ 完成 |
| 过程文档存档 | [docs/archive/](docs/archive/README.md)，协作说明 4 + 审查报告 9 + 整改说明 8 + 模块任务书 8 | ✅ 完成 |

个人计划的模板字段为**姓名 / 负责模块 / 任务目标 / 关键交付物 / 验收标准 / 时间节点 / 完成情况**；
每份文末附「可核验的 Git 记录」，可用 `git log --author=<账号>` 或 `git log -- <路径>` 直接核对，
不是只看文档自述。12 人清单与模块对应关系见 [docs/plans/README.md](docs/plans/README.md)。

## 冲突记录与待决策项（不擅自变更接口）

生成骨架过程中发现以下与设计文档第 4 章不一致/未覆盖点，**按任务约定先记录、未擅自改接口**：

1. **ParamEntry vs ParaEntry**：任务要求类名 `ParamEntry`，设计文档第 4.4.1 为 `ParaEntry`；本骨架采用 `ParamEntry`，附录 B 对照表中亦为 ParaEntry，需确认统一名称。
2. **TokenType 缺少花括号界符**：任务要求词法识别 `{}`，设计文档 TokenType 未列 `TOKEN_LBRACE/TOKEN_RBRACE`；本骨架扩展了这两个枚举值（挑战版控制流必需），若文档需保持原枚举请在 v3.0 补充。
3. **AST 指针管理**：文档第 4.3 使用裸指针 `ASTNode* m_left/m_right`，任务/第 6 章要求 unique_ptr 管理（Q10）；本骨架采用 `std::unique_ptr<ASTNode>` 成员。
4. **SymbolTable 接口差异**：文档第 4.4 为 `RecVar(name,type,line)→int`、含 `RecConstTbl/SearchVarInfoTbl`；第 6 章契约为 `RecVar(name,type,line,col)→VarEntry*`、含 `EnterScope/ExitScope` 等。本骨架以**第 6 章契约为主**，同时保留第 4 章扩展接口（RecVarTbl/RecConstTbl/SearchXxxTbl）为兼容附加接口。
5. **IRGenerator 构造**：第 4.5.4 为无参构造，第 6.5 契约为 `explicit IRGenerator(SymbolTable&)`；按第 6 章契约实现（绑定符号表以支持 OperandToString 转原文）。
6. **ScopeType 枚举**：第 6 章 EnterScope 需要 `ScopeType t` 参数，文档未定义该枚举；本骨架补充 `ScopeType{SCOPE_GLOBAL, SCOPE_FUNCTION}`。
7. **main 输出 asm**：第 6.7 约定挑战版输出汇编文件但未给后缀；本骨架固定输出 `<源文件名>.asm`（现已按此约定实现）。
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

## 各阶段完成情况

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
- **阶段 2（已完成）**：语义动作（符号表登记/查重/类型检查/常量回填）接入 — `src/sema/semantic.cpp`
- **阶段 3（已完成）**：IRGenerator 遍历 AST 生成四元式（`--dump-quads`）— `src/sema/ir.cpp`
- **挑战版（Q5 顺序）：控制流(if/while) → print → 数组 → 函数 → x86 汇编 —— 五步全部落地**
  - **语法侧已全部落地**：复合语句/if/while/return/一元运算、比较与逻辑运算符、
    `bool`/`char`/`true`/`false`、`print(表达式);`、一维数组声明与下标、函数定义/形参/调用
  - **目标代码侧已全部落地**：`src/codegen/codegen.cpp`，`tests/run_codegen_tests.ps1` 32 例
    （算术/比较/if-else/while/浮点/混合类型/逻辑/函数调用/递归/数组读写/字符/嵌套分支/短路/实参转换/返回值转换/
    嵌套调用实参顺序/空函数/用户自定义 `main`/返回值语义/char 与 bool 返回/变量遮蔽/大栈帧）全部编译成汇编、经 gcc 链接、运行并比对输出，**30/30 通过**
  - **差分测试验证**（2026-10-24，未入库，脚本在 `build/_s2/`）：除固定用例，另用「同一程序 MiniC 版 vs gcc 编译的等价 C 版」逐行比对 stdout，
    整数随机 240 例 + 浮点随机 240 例 **全部一致**，另覆盖 Ackermann(3,3)=61、深递归 20000 层、100 个函数、50 层括号嵌套、5000 元素局部数组（栈帧 20032 字节）。
    注意 C 参考版的浮点字面量必须加 `f` 后缀，否则 `3.1/12.8` 按 double 算成 `0.242188` 而 MiniC 按 float 算成 `0.242187` —— 属语义差异不是缺陷。
  - **收尾修复的两批缺陷**（2026-10-24，详见 `docs/interface.md` 第 16、18 条）：
    - 第一批：嵌套调用实参错序（根因在 IR 边求值边发 `Q_PARA`，≥3 实参且有嵌套才暴露）、
      空函数 `void f(){}` 不产出函数体导致链接期 `undefined reference`、
      语法/语义报错时仍落下一份只有空 `main` 的误导性 `.asm`
    - 第二批：用户自定义 `int main()` 产出重复 `main:` 标号（**文档 7.1~7.3 的三个官方示例全部无法汇编**，
      `as` 报 `symbol 'main' is already defined`）、显式 `return 5;` 被出口标号后的
      `xorl %eax,%eax` 清零（`int main(){return 5;}` 退出码实测为 0）、
      裸 `return;` 带出 `%eax` 残值、无 `return` 的 `int` 函数返回垃圾值（实测 2043900272）
- **挑战版语法**：文法已定稿（105 条产生式、LL(1) 预测分析表，见 [docs/MiniC编译器设计文档_v4.0_挑战版LL1.md](docs/MiniC编译器设计文档_v4.0_挑战版LL1.md)）。
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
  - 阶段⑥ 符号表扩展 **已完成**（多表 + 作用域链 + 数组/常量/过程/形参/标号登记）
  - 阶段⑦ 语义动作与 IR **已完成**（2026-10-24 修复短路求值与三处类型转换缺陷，见 `docs/interface.md` 第 15 条）
  - 阶段⑧ 目标代码 **已完成**（2026-10-24，x86-64 / GNU as）

## 随机差分

除固定用例外，还有一组端到端差分：同一棵随机程序树渲染两次——一次成 MiniC 源、
一次成等价 C 源；前者经 `minic` 出汇编再由 `gcc -x assembler` 链接，后者直接 `gcc -O0` 编译，
两个可执行文件的 **stdout 与退出码必须逐字节一致**。

```
python tools/random_diff.py -n 1000 -s 20261028
```

退出码 0 表示全一致，可直接接 CI。基线：`seed 20261028`、1 000 例、`identical = 1000 / mismatch = 0`。

三条语义等价约束写在脚本头注释里：整型 32 位有符号、除法向零截断；实型 32 位单精度，
**C 侧字面量必须带 `f` 后缀**；**所有变量必须显式初始化**（MiniC 不清零局部变量，
不初始化就是在比较两份二进制的栈残留）。

## 已知限制

MiniC 是教学子集语言，下列写法**不在文法/语义支持范围内**，属预期行为而非缺陷：

- 支持 `++` `--`（前缀与后缀：`b = a++` 取旧值、`b = ++a` 取新值；`++5` 报 `'++' requires a modifiable lvalue`，`++true` 报 `'++' requires a numeric operand`）。
- 不支持：`+=` 等复合赋值、`%` 取模、`?:`、`for`、`do-while`、`break`、`continue`、
  `switch`、`goto`、多维数组、字符串字面量、逗号表达式、链式赋值、函数原型声明、赋值表达式（`a = b = 1`）。
- 声明必须全部位于语句之前（`int a; a=1; int b;` 报 `unexpected 'int'`）。
- 数组长度只能是整型常量（`int b[3+5];` 报 `expected ']'`）。
- 逻辑运算符 `&&` `||` 要求 `bool` 操作数（`int` 直接参与会报 `logical operators require bool operands`）。
- `char` **可以**参与算术与关系运算（按整型提升，C 语义）：`c + 1` 得 `int`、`c == 'x'` 得 `bool`、`c++` 合法；`bool` 参与**比较**也合法（`a == b`、`a < b == b < c` 都能写），但仍不参与算术（报 `arithmetic operands must be numeric`）。
- 关系运算符**已按 C++ 分层**：`<` `<=` `>` `>=` 高于 `==` `!=`，所以 `a == b < c` 是 `a == (b < c)`、`a < b == c` 是 `(a < b) == c`；`<` 与 `<=` 之间仍同级左结合（`a < b <= c` = `(a < b) <= c`）。
- 语义阶段**检查**：函数实参个数与**实参类型**（不兼容报 `argument 1 of function 'f' expects INT, but FLOAT given`，**警告**）、
  `return` 类型（void 带值、非 void 裸 return 都报错）、数组名不带下标使用、
  **常量**下标越界（`int a[3]; a[5]=1;` 报 `array subscript 5 is out of bounds`）、
  整型字面量越界（`a = 2147483648;` 报 `integer literal ... is out of range for int`，**错误**）、
  非 void 函数缺 `return`（**警告**）、无 `main` 且无顶层语句（**警告**）、
  `char` 字面量越界（`c = 300;` 报截断后的值，**警告**）。
  **不检查**：变量下标越界（静态不可知）、`if`/`while` 条件为 float、
  不可达代码与未使用变量、栈深度与递归深度。
- 代码生成**不做**寄存器分配以外的优化：热点局部整型标量固定放 `%ebx` / `%esi`（每函数至多两个），
  临时量仍全部落栈；没有循环优化，也没有图着色全局分配。
- `print` **不自动换行**：连续两条 `print` 接在同一行，要换行写 `print('\n');`。
- 浮点字面量：`1.` 合法、`.5` 不合法；不支持 `0x1F` 十六进制与 `1e5` 指数形式。

完整语法支持清单（105 条产生式、类型规则、与 C 的 19 条差异、报错速查、实测记录）见
[docs/MiniC语法支持清单.md](docs/MiniC语法支持清单.md)。
