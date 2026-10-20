
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
| CCodeGen | `explicit CCodeGen(const CodeList&)` / `bool Generate(AsmOutput&)` | 汇编输出（挑战版） |

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
