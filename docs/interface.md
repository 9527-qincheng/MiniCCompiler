
# MiniC 编译器 —— 接口备忘与冲突记录

本文件为骨架生成过程中的接口决策备忘，供后续开发同步使用。

## 一、模块接口速览（第 6 章契约，定稿）

| 模块 | 对外接口 | 数据传递 |
|---|---|---|
| CLex | `bool OpenSource(const string&)` / `bool Run()` / `const TokenList& GetTokenList() const` | TokenList → CParser |
| CParser | `CParser(const TokenList&)` / `ASTNodePtr Parse()` / `bool HasError() const` / `const vector<CompileError>& GetErrors() const` / `const vector<ParseStep>& GetTrace() const`（阶段④ 新增，供 `--dump-parse`） | ASTNodePtr → 语义/IR |
| SymbolTable | `EnterScope(ScopeType)` / `ExitScope()` / `RecVar(name,type,line,col)→VarEntry*` / `RecArray(name,type,size,line,col)→VarEntry*`（阶段⑥ 新增） / `RecConst(name,type,value,line,col)→ConstEntry*` / `SearchVar/SearchConst` / `BackfillConst(iConstIdx,name,type)` / `CheckAssign(lhs,rhs,line,col)` / `DumpSymTab(os)` | 符号信息 → IRGenerator |
| SemanticAnalyzer | `explicit SemanticAnalyzer(SymbolTable&)` / `void Analyze(const ASTNodePtr&)`（阶段⑥ 新增，遍历 AST 填充符号表；不新增独立产物） | ASTNodePtr → 符号表 |
| IRGenerator | `explicit IRGenerator(SymbolTable&)` / `Generate(const ASTNodePtr&)` / `GetTmpVar(TokenType)` / `GetTmpLabel()` / `EmitIR(op,op1,op2,rslt)` / `GetCodes() const` / `OperandToString(op)` | CodeList → CCodeGen / --dump-quads |
| CCodeGen | `explicit CCodeGen(const CodeList&)` / `void BindSymTab(const SymbolTable&)` / `bool Generate(AsmOutput&)` | 汇编输出（挑战版） |

## 二、main 约定（第 6.7 节）

- 命令行：`minic <源文件> [--dump-tokens] [--dump-ast] [--dump-symtab] [--dump-quads] [--dump-parse]`
- 产物：`<源文件名>.token.txt / .ast.txt / .sym.txt / .quad.txt / .parse.txt`（与源文件同目录）+ `<源文件名>.asm`
- 流水线：CLex.Run → CParser.Parse → SymbolTable 语义 → IRGenerator.Generate → CCodeGen.Generate
- 错误：g_ErrorList（含行列号），非零退出码；成功退出码 0

## 三、冲突记录（未擅自变更接口，待确认）

1. `ParamEntry` vs 文档 `ParaEntry`（任务要求 ParamEntry，文档 4.4.1 为 ParaEntry）
2. TokenType 缺花括号界符（骨架扩展 `TOKEN_LBRACE/TOKEN_RBRACE`）
3. AST 成员用 `unique_ptr<ASTNode>`（文档 4.3 为裸指针，Q10 要求 unique_ptr 管理）；
   **阶段⑤ 起子节点统一为 `vector<ASTNodePtr> m_children`**（原 `m_left/m_right/m_else` 删除，见第 8 条）
4. SymbolTable 以第 6 章契约为主，保留第 4 章扩展接口为兼容
5. IRGenerator 构造按第 6 章 `explicit IRGenerator(SymbolTable&)`
6. 补充 `ScopeType{SCOPE_GLOBAL, SCOPE_FUNCTION}` 枚举（第 6 章需要，文档未定义）
7. 汇编产物后缀取 `<源文件名>.asm`（第 6.7 未明确）
8. **阶段⑤ AST 多子节点重构（已实施，2026-10-13）**：设计文档 §5.3.2【变更·破坏性】要求
   `m_children` 多子节点与 18 类节点（文档枚举 17 类 + 02 任务书 §5.2 的 ARRAYDECL）；
   `ast.h` 原为 v3.1 的 `m_left/m_right` + 11 类。现状：`m_children` + 18 类，删除 `AST_NODE_LIST`，
   `UNARY` 改名 `UNOP`，新增 `PROGRAM/DECL/FUNCDEF/PARAM/BLOCK/ASSIGN/IF/WHILE/PRINT/RETURN/EMPTY/
   BINOP/UNOP/NUM/IDENT/INDEX/CALL/ARRAYDECL`；`Print` 由基类统一实现（各节点提供 `Label()`）。
   评审稿与子节点语义表见 `docs/stage5_ast_review.md`。
9. **词法未登记字符/布尔常量表下标**（跨模块记录）：`'x'` / `true` / `false` 的 `Token::m_iConstIdx` 为 -1，
   Parser 只按透传处理；语义阶段 semantic003 需按 token 自行登记（未改词法）。
10. **分析栈元素结构扩展**（阶段④ 记录）：`StackItem` 在文档 §4.10 三字段之外增加
    `m_iValueBase` 与 `m_loc`，并新增内部 `Kind::REDUCE` 哨兵（静默、不进 `GetTrace()`，
    以保证 §4.11 的 82 步轨迹与文档逐行一致）。
11. **符号表新增 `RecArray` 与 `SCOPE_BLOCK`**（阶段⑥ 符号表模块，2026-10-15）：
    设计文档 §5.4【变更】要求变量表项支持数组维度、作用域增加块级。
    变更内容：`ScopeType` 增 `SCOPE_BLOCK`；`VarEntry` 增 `m_bIsArray`/`m_iArraySize`；
    新增 `VarEntry* RecArray(name, type, size, line, col)`；`SearchVarInfoTbl` 由线性扫描改为
    **按作用域链自内向外查找**（内层可遮蔽外层）；`RecVar` 查重范围收窄为**当前作用域**；
    `CheckAssign` 扩展 char↔int、同类型，其余组合改为告警不阻断。
    兼容性：**未删除任何既有公开接口**，`m_VarInfoTbl` 仍为 public 且下标语义不变。
12. **新增语义分析模块 `SemanticAnalyzer`**（阶段⑥ 语义分析模块，2026-10-20）：
     设计文档 §5.5 与 `src/sema/` 原先只有中间代码（`ir.h/ir.cpp`），未为语义分析预定义目录；
     本模块落在 `src/sema/semantic.h` + `semantic.cpp`（与 `ir.h` 同目录，可被 `build.bat` 收集）。
     接口：`explicit SemanticAnalyzer(SymbolTable&)` + `void Analyze(const ASTNodePtr&)`，
     构造函数注入符号表（不修改 `IRGenerator` / `CParser` 的任何既有签名）。
     接线点：`src/main.cpp` 第 4 阶段，**仅当 `g_ErrorList` 为空时执行**——语法阶段已报错时
     AST 可能残缺，继续语义分析会产生连锁误报并破坏 `tests/parser/expected/*.err.txt` 黄金基线。
     常量表：第 9 条的遗留问题（`'x'`/`true`/`false` 的 `m_iConstIdx` 为 -1）由语义阶段
     自行 `RecConst` 登记解决，词法未改动。
     行为影响：语义报错会使退出码为 1（此前不报错）；`--dump-symtab` 的 `.sym.txt`
     从空表变为实际登记内容。
13. **中间代码模块 IRGenerator 实现**（阶段⑦ 中间代码模块，张灏宇，2026-10-21）：
     `Generate()` 递归遍历 AST 生成四元式（交付物④ `--dump-quads`），包含：
     - 表达式：NUM/IDENT/BINOP（算术/关系/逻辑短路）/UNOP（取负、逻辑非）/INDEX（Q_INDEX）/CALL（Q_PARA+Q_CALL）；
     - 语句：ASSIGN / IF（JZ+JMP+LABEL 标号）/ WHILE（入口 LABEL+JZ+回跳）/ PRINT / RETURN / FUNCDEF→FlushToProc；
     - 类型转换（§8.1 表）：算术 int/float 混合插入 ITOF；赋值 FLOAT→INT 插入 FTOI；
     - 控制流标号经 `GetTmpLabel()` 预先申请编号后直接引用（编号体系下无需两趟回填）。
     **接口变更（均为新增，未破坏既有签名/值）**：
     - `EmitIR` 新增带 `LineInfo` 的重载（原四元式 `m_loc` 恒为 0,0，无法定位源码；重载属新增，已登记）；
     - `QuadOp` 追加 `Q_PARA / Q_INDEX / Q_ITOF / Q_FTOI / Q_EQ / Q_NE / Q_LT / Q_LE / Q_GT / Q_GE`
       （追加在枚举末尾，既有值不变；`Quad::Print` 的 opName 表与 main.cpp 的 `QuadOpName` 同步扩展）；
     - `IRGenerator` 增加私有辅助方法（EvalExpr/GenStmt/GenFuncDef/OperandType/类型转换/ConstInt）与
       `m_TmpTypes` 临时量类型登记（GetTmpVar 补完类型记录 TODO）；
     - `OperandToString` 的 OP_CONST 分支补浮点原文输出（语义 RecConst 未存 `m_szVal` 时按类型格式化，
       修复浮点常量打印被截断为整数的缺陷）；
     - main.cpp `--dump-quads` 改为 Q6 原文打印（去掉硬编码 TODO 首行），函数四元式按过程分组输出。
     联调约定（与目标代码组王逸凡）：CodeList 只读消费；`(INDEX, 数组, 下标, t)` 为取值、
     `(INDEX, 值, 下标, 数组)` 为存值（元素寻址，下标×元素宽度由目标代码处理）；新增操作码如需调整先对齐。
14. **目标代码模块 CCodeGen 实现**（阶段⑧ 目标代码模块，王逸凡，2026-10-24）：
     `IRtoASM()` 把四元式翻译为 x86-64 汇编（AT&T 语法 / GNU as / Windows MinGW 链接）：
     - 朴素栈式翻译：每个变量与临时量固定 rbp 栈槽，逐四元式 load→运算→store，不做寄存器分配；
     - 入口为 `main`（`.globl main` + `main:`，与已敲定的 A 方案一致），可用 `gcc -x assembler` 直接汇编链接；
     - 全局变量/数组按 `VarEntry.m_iScope == 1` 且名字不在过程表中判定，落 `.bss`；
       局部变量与临时量入栈帧，栈帧按 16 字节对齐，分配顺序按变量表下标/临时编号升序以保证输出稳定；
     - 过程调用用自定义栈传参：调用方按 `i*8(%rsp)` 落第 i 个实参（槽位 16 字节对齐），
       被调用方从 `16+8*(i-1)(%rbp)` 取形参；`print` 走 Win64 ABI 调用 `printf`；
     - 数组寻址 `(%rax,%rcx,元素宽度)`，元素宽度由数组声明类型决定（char 1 / int·float 4）；
     - 关系运算支持 int 与 float 混用（IR 不插转换，目标代码就地 `cvtsi2ss`）；
     - `Q_ASSIGN` 两端类型不一致时就地补 `cvtsi2ss` / `cvttss2si`（兜住 IR 在 int→float 处缺 ITOF 的缺陷）。
     **接口变更（新增，未破坏既有签名）**：
     - `CCodeGen` 新增 `void BindSymTab(const SymbolTable&)`：函数四元式存放在 `SymbolTable::m_ProcInfoTbl[i].m_Codes`，
       而构造函数只接收顶层 CodeList；main.cpp 在 `CCodeGen cg(ir.GetCodes());` 后补一行 `cg.BindSymTab(symtab);`。
       未绑定符号表时退化为「仅生成顶层语句」，不崩溃。
     - `src/symbol/symbol.cpp` 的 `RecConst()` 修正 `m_bIsFloat` 判定：语义阶段传入 `TOKEN_FLOAT`
       （`NumType()` 的结果），原先只认词法阶段的 `TOKEN_NUM_FLOAT`，导致浮点常量被当作整数截断。
       该修复同时改善 `--dump-quads` 的浮点常量原文输出。
     - 产物后缀 `<源文件名>.asm`（第 7 条约定），已在 `.gitignore` 中排除 `*.asm` 与 `*.quad.txt`。

> 说明：本节第 8–10 条为 2026-10-13 阶段⑤ 收尾时同步；**下面的"四、词法识别集合"仍是 v3.1 骨架版内容**，
> 词法 v4.0 的现状（17 个挑战版终结符等）见 `README.md` 的模块状态表与《词法模块待改进清单.md》，
> 本次未越界改动词法章节。

## 四、词法识别集合（骨架版已实现）

- 关键字：int / float / if / else / while / return / void
- 标识符：`[A-Za-z_][A-Za-z0-9_]*`
- 常量：整数 `[0-9]+`、浮点 `[0-9]+.[0-9]+`（登记入 m_ConstTexts，Token.m_iConstIdx 记录位序）
- 运算符：+ - * / =
- 界符：( ) { } ; ,
- 结束：EOF
