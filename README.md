
# MiniC 编译器（挑战版，C++17）—— 项目说明

**当前进度**：词法模块已按 v4.0 挑战版完成（17 个新终结符、编码映射、双字符运算符、字符常量）；语法模块为基础版递归下降（声明→赋值→算术表达式，产出 AST）；其余模块（语义接入、中间代码、目标代码）保留完整接口实现，核心逻辑留 TODO。

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
├── src/
│   ├── main.cpp            # 命令行解析 + 流水线 + 四类产物落盘
│   ├── lexer/
│   │   ├── token.h/cpp     # Token/TokenType/LineInfo/CompileError/g_ErrorList（公共基础）
│   │   └── lexer.h/cpp     # CLex（词法）
│   ├── parser/
│   │   ├── ast.h/cpp       # ASTNode + NumNode/IdentNode/DeclNode/AssignNode/BinOpNode/ListNode
│   │   └── parser.h/cpp    # CParser（声明/赋值/算术表达式递归下降）
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
│   └── bad_parser.mc       # 语法错误恢复
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
| `build.bat run tests\test.mc` | 编译后运行指定源文件 |
| `build.bat list` | 列出参与编译的源文件 |
| `build.bat clean` | 清理 `build` 目录 |
| `build.bat rebuild` | 先清理再编译 |

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

## 运行

```bash
build.bat run tests\test.mc --dump-tokens --dump-ast --dump-symtab --dump-quads
```

或直接调用已编译好的程序：

```bash
build\minic.exe tests\test.mc --dump-tokens --dump-ast --dump-symtab --dump-quads
```

输出（与源文件同目录，源文件去扩展名后加后缀）：

| 选项 | 产物文件 | 当前状态 |
|---|---|---|
| `--dump-tokens` | `tests/test.token.txt` | 已实现（词法演示） |
| `--dump-ast` | `tests/test.ast.txt` | 已实现（递归下降 AST） |
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

## Parser 回归测试

语法分析模块的自动化回归测试（覆盖优先级/结合性/嵌套括号/合法与非法输入/错误恢复/既有黄金输出）：

```bash
# 校验（比对退出码、AST 输出、错误输出）
powershell -ExecutionPolicy Bypass -File tests/run_parser_tests.ps1

# 确认行为变更是有意为之后，刷新期望结果
powershell -ExecutionPolicy Bypass -File tests/run_parser_tests.ps1 -Update
```

- 用例输入：`tests/parser/*.mc`；期望结果：`tests/parser/expected/*.ast.txt`、`*.err.txt`
- 额外固定校验：`tests/test.mc` 的 AST 必须与 `tests/test.ast.txt` 逐字一致
- 需要已构建的 `build/minic.exe`（`cmake --build build`）

直接检查 AST 字段的 Parser 单元测试（常量表位序 `m_iConstIdx` 透传、`m_iSymIdx` 默认值）：

```bash
powershell -ExecutionPolicy Bypass -File tests/run_parser_unit.ps1
```

- 需要 `g++` 在 PATH 中（可用 `-Cxx <path>` 指定编译器）

## 各模块实现状态

| 模块 | 接口 | 状态 |
|---|---|---|
| CLex（词法） | OpenSource / Run / GetTokenList | **已实现（v4.0 挑战版）**：关键字(int/float/bool/char/void/if/else/while/print/return/true/false)、标识符、整型/实型/字符常量、运算符(+ - * / = == != < <= > >= && \|\| !)、界符((){}[];,)；注释与字符串；常量登记 m_ConstTexts；错误含行列号。**已补 TokenTypeToCode / CodeToTokenType 编码映射** |
| CParser（语法） | CParser(TokenList&) / Parse / HasError / GetErrors | 已实现：声明/赋值/算术表达式递归下降（+ - * / 及括号优先级）；扩展一元 +/-、复合语句、if/else、while、return；错误恢复可连续报错。条件中的比较运算符待词法模块补充 TokenType |
| SymbolTable（语义） | EnterScope / ExitScope / RecVar / RecConst / SearchVar / SearchConst / BackfillConst / CheckAssign / DumpSymTab | 基础实现：多表登记/查重/作用域/常量回填/float→int 警告（Q9） |
| IRGenerator（中间代码） | Generate / GetTmpVar / GetTmpLabel / EmitIR / GetCodes / OperandToString | 骨架：计数器/EmitIR/OperandToString 可用，遍历 AST 生成留 TODO |
| CCodeGen（目标代码，挑战版） | Generate / IRtoASM / AsmOutput | 骨架：输出结构完整，x86 翻译留 TODO |

> **词法现状说明**：17 个挑战版终结符（`bool` `char` `print` `true` `false` `[` `]` `==` `!=` `<` `<=` `>` `>=` `&&` `||` `!` 字符常量）**词法层已全部支持**，
> 语法侧控制流骨架（复合语句 / `if` / `while` / `return`）已可用；`print`、数组、函数等尚未实现的语法结构仍会报错——这是**预期行为**，不是 bug。
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

## 下一步（TODO 路线）

- 阶段 1（已完成）：CParser 递归下降（声明→赋值→算术表达式），产出 AST（--dump-ast）
- **阶段 1.5（进行中）**：词法 v4.0 挑战版已完成（17 个终结符、编码映射、双字符运算符、字符常量）。
  遗留待修见《词法模块待改进清单.md》：
  1. **UTF-8 BOM 识别**（记事本保存的 `.mc` 会报 3 个 `unexpected character`）— P0
  2. 多字符常量 `'ab'` 报错后仍产出 token（同一错误报两遍）— P1
  3. 字符串 `STRING` 定位说明（词法支持、文法不支持）— P1
  4. 5 个新用例缺配套 `*.token.txt` 期望输出 — P2
- 阶段 2：语义动作（符号表登记/查重/类型检查/常量回填）接入
- 阶段 3：IRGenerator 遍历 AST 生成四元式（--dump-quads）
- 挑战版（Q5 顺序）：控制流(if/while) → print → 数组 → 函数 → x86 汇编
  - 语法侧控制流骨架（复合语句/if/while/return/一元运算）已完成；条件比较运算符（`< > <= >= == !=`）需词法模块先补 TokenType，数组需 `[ ]` 界符，均属跨模块依赖
- **挑战版语法**：文法已定稿（92 条产生式、LL(1) 预测分析表，见《MiniC编译器设计文档_v4.0_挑战版LL1.md》），
  待实现：AST 多子节点改造 → 预测分析驱动器 → 控制流/print/数组/函数 → x86 汇编
