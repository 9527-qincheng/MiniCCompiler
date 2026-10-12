
# MiniC 编译器 —— 接口备忘与冲突记录

本文件为骨架生成过程中的接口决策备忘，供后续开发同步使用。

## 一、模块接口速览（第 6 章契约，定稿）

| 模块 | 对外接口 | 数据传递 |
|---|---|---|
| CLex | `bool OpenSource(const string&)` / `bool Run()` / `const TokenList& GetTokenList() const` | TokenList → CParser |
| CParser | `CParser(const TokenList&)` / `ASTNodePtr Parse()` / `bool HasError() const` / `const vector<CompileError>& GetErrors() const` | ASTNodePtr → 语义/IR |
| SymbolTable | `EnterScope(ScopeType)` / `ExitScope()` / `RecVar(name,type,line,col)→VarEntry*` / `RecConst(name,type,value,line,col)→ConstEntry*` / `SearchVar/SearchConst` / `BackfillConst(iConstIdx,name,type)` / `CheckAssign(lhs,rhs,line,col)` / `DumpSymTab(os)` | 符号信息 → IRGenerator |
| IRGenerator | `explicit IRGenerator(SymbolTable&)` / `Generate(const ASTNodePtr&)` / `GetTmpVar(TokenType)` / `GetTmpLabel()` / `EmitIR(op,op1,op2,rslt)` / `GetCodes() const` / `OperandToString(op)` | CodeList → CCodeGen / --dump-quads |
| CCodeGen | `explicit CCodeGen(const CodeList&)` / `bool Generate(AsmOutput&)` | 汇编输出（挑战版） |

## 二、main 约定（第 6.7 节）

- 命令行：`minic <源文件> [--dump-tokens] [--dump-ast] [--dump-symtab] [--dump-quads]`
- 产物：`<源文件名>.token.txt / .ast.txt / .sym.txt / .quad.txt`（与源文件同目录）+ `<源文件名>.asm`
- 流水线：CLex.Run → CParser.Parse → SymbolTable 语义 → IRGenerator.Generate → CCodeGen.Generate
- 错误：g_ErrorList（含行列号），非零退出码；成功退出码 0

## 三、冲突记录（未擅自变更接口，待确认）

1. `ParamEntry` vs 文档 `ParaEntry`（任务要求 ParamEntry，文档 4.4.1 为 ParaEntry）
2. TokenType 缺花括号界符（骨架扩展 `TOKEN_LBRACE/TOKEN_RBRACE`）
3. AST 成员用 `unique_ptr<ASTNode>`（文档 4.3 为裸指针，Q10 要求 unique_ptr 管理）
4. SymbolTable 以第 6 章契约为主，保留第 4 章扩展接口为兼容
5. IRGenerator 构造按第 6 章 `explicit IRGenerator(SymbolTable&)`
6. 补充 `ScopeType{SCOPE_GLOBAL, SCOPE_FUNCTION}` 枚举（第 6 章需要，文档未定义）
7. 汇编产物后缀取 `<源文件名>.asm`（第 6.7 未明确）

## 四、词法识别集合（骨架版已实现）

- 关键字：int / float / if / else / while / return / void
- 标识符：`[A-Za-z_][A-Za-z0-9_]*`
- 常量：整数 `[0-9]+`、浮点 `[0-9]+.[0-9]+`（登记入 m_ConstTexts，Token.m_iConstIdx 记录位序）
- 运算符：+ - * / =
- 界符：( ) { } ; ,
- 结束：EOF
