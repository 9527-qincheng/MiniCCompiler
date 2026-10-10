---
AIGC:
    Label: "1"
    ContentProducer: 001191440300708461136T1XGW3
    ProduceID: bcd9f7e8ad8b40e1eb3fcdcc012220da_a0999f87c30e11f1884b525400cd780f
    ReservedCode1: KaAxM7ZvUXcjX+Qq2NUMgmoBLSM9fZkHjFECzs4IsyXhFjNwYPTOHJkNfYlYDW2wxCm9i9x5Gu9KAns19wu5adDIc2mFOm3G9Eq7FlpX7jRY+4/FSRjSKNGnlbSFVOL4hVJFXOeaGgjjcnL0Nm7CZpkX0vk2XgbStdqsWN3Gm/hbJCQ+P0rAQr8ak64=
    ContentPropagator: 001191440300708461136T1XGW3
    PropagateID: bcd9f7e8ad8b40e1eb3fcdcc012220da_a0999f87c30e11f1884b525400cd780f
    ReservedCode2: KaAxM7ZvUXcjX+Qq2NUMgmoBLSM9fZkHjFECzs4IsyXhFjNwYPTOHJkNfYlYDW2wxCm9i9x5Gu9KAns19wu5adDIc2mFOm3G9Eq7FlpX7jRY+4/FSRjSKNGnlbSFVOL4hVJFXOeaGgjjcnL0Nm7CZpkX0vk2XgbStdqsWN3Gm/hbJCQ+P0rAQr8ak64=
---

# MiniC 编译器（挑战版，C++17 + CMake）—— 项目骨架

半成品可演示版：**词法模块已跑通最小示例**（关键字/标识符/整数/浮点/运算符/界符识别，常量词法登记），其余模块（语法分析、语义分析、中间代码、目标代码）保留完整类声明与接口实现，核心逻辑留 TODO。

依据：《MiniC编译器数据结构设计.md》第 4 章（数据结构）与第 6 章（接口契约，v2.0 定稿，Q1~Q12 全部按定稿值执行）。

## Git 仓库

- 远程仓库：`https://github.com/9527-qincheng/MiniCCompiler.git`
- 分支说明：`main`（稳定分支，始终可运行、可演示，只接收 dev 合并）；`dev`（集成分支，组员从 `origin/dev` 切 feature 分支开发，完成后合并回 dev）
- 克隆命令：`git clone https://github.com/9527-qincheng/MiniCCompiler.git`

## 项目结构

```
MiniCCompiler/
├── CMakeLists.txt          # C++17 + GLOB_RECURSE，产出 minic
├── README.md
├── .gitignore
├── src/
│   ├── main.cpp            # 命令行解析 + 流水线 + 四类产物落盘
│   ├── lexer/
│   │   ├── token.h/cpp     # Token/TokenType/LineInfo/CompileError/g_ErrorList（公共基础）
│   │   └── lexer.h/cpp     # CLex（词法最小可演示）
│   ├── parser/
│   │   ├── ast.h/cpp       # ASTNode + NumNode/IdentNode/DeclNode/AssignNode/BinOpNode/ListNode
│   │   └── parser.h/cpp    # CParser（核心 TODO）
│   ├── symbol/
│   │   └── symbol.h/cpp    # SymbolTable 多表（Var/Const/Type/Proc/Param/Label）+ 语义接口
│   ├── sema/
│   │   └── ir.h/cpp        # Quad/Operand/QuadOp + IRGenerator（核心 TODO）
│   └── codegen/
│       └── codegen.h/cpp   # AsmOutput + CCodeGen（挑战版骨架）
├── tests/
│   └── test.mc             # 最小示例：int a; float b; a = b + 1;
├── examples/
│   └── example.mc          # 挑战版关键字示例
├── docs/
│   └── interface.md        # 接口备忘 + 冲突记录
└── build/                  # CMake 构建目录
```

## 构建

要求：CMake >= 3.10，C++17 编译器（MinGW g++ / MSVC / Clang 均可）。

```bash
# 方式一：CMake（推荐）
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build

# 方式二：直接 g++（无 CMake 环境）
g++ -std=c++17 -Isrc -o build/minic.exe \
    src/lexer/token.cpp src/lexer/lexer.cpp \
    src/parser/ast.cpp src/parser/parser.cpp \
    src/symbol/symbol.cpp src/sema/ir.cpp \
    src/codegen/codegen.cpp src/main.cpp
```

产出可执行文件：`build/minic.exe`。

## 运行

```bash
./build/minic.exe tests/test.mc --dump-tokens --dump-ast --dump-symtab --dump-quads
```

输出（与源文件同目录，源文件去扩展名后加后缀）：

| 选项 | 产物文件 | 当前状态 |
|---|---|---|
| `--dump-tokens` | `tests/test.token.txt` | 已实现（词法演示） |
| `--dump-ast` | `tests/test.ast.txt` | 骨架占位（TODO） |
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

## 各模块实现状态

| 模块 | 接口 | 状态 |
|---|---|---|
| CLex（词法） | OpenSource / Run / GetTokenList | 已实现：关键字(int/float/if/else/while/return/void)、标识符、整数、浮点、运算符(+ - * / =)、界符((){};,)；常量登记 m_ConstTexts；错误含行列号 |
| CParser（语法） | CParser(TokenList&) / Parse / HasError / GetErrors | 骨架：递归下降函数全部留 TODO |
| SymbolTable（语义） | EnterScope / ExitScope / RecVar / RecConst / SearchVar / SearchConst / BackfillConst / CheckAssign / DumpSymTab | 基础实现：多表登记/查重/作用域/常量回填/float→int 警告（Q9） |
| IRGenerator（中间代码） | Generate / GetTmpVar / GetTmpLabel / EmitIR / GetCodes / OperandToString | 骨架：计数器/EmitIR/OperandToString 可用，遍历 AST 生成留 TODO |
| CCodeGen（目标代码，挑战版） | Generate / IRtoASM / AsmOutput | 骨架：输出结构完整，x86 翻译留 TODO |

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

- 阶段 1：CParser 递归下降（声明→赋值→算术表达式），产出 AST（--dump-ast）
- 阶段 2：语义动作（符号表登记/查重/类型检查/常量回填）接入
- 阶段 3：IRGenerator 遍历 AST 生成四元式（--dump-quads）
- 挑战版（Q5 顺序）：控制流(if/while) → print → 数组 → 函数 → x86 汇编
*（内容由AI生成，仅供参考）*
