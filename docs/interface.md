
# MiniC 编译器 —— 接口备忘与冲突记录

本文件为骨架生成过程中的接口决策备忘，供后续开发同步使用。

## 一、模块接口速览（第 6 章契约，定稿）

| 模块 | 对外接口 | 数据传递 |
|---|---|---|
| CLex | `bool OpenSource(const string&)` / `bool Run()` / `const TokenList& GetTokenList() const` | TokenList → CParser |
| CParser | `CParser(const TokenList&)` / `ASTNodePtr Parse()` / `bool HasError() const` / `const vector<CompileError>& GetErrors() const` / `const vector<ParseStep>& GetTrace() const`（阶段④ 新增，供 `--dump-parse`） / `void SetTraceEnabled(bool)`（2026-10-27 新增，默认关；不开时 `RecordStep` 直接返回） | ASTNodePtr → 语义/IR |
| SymbolTable | `EnterScope(ScopeType)` / `ExitScope()` / `RecVar(name,type,line,col)→VarEntry*` / `RecArray(name,type,size,line,col)→VarEntry*`（阶段⑥ 新增） / `RecConst(name,type,value,line,col)→ConstEntry*` / `SearchVar/SearchConst` / `BackfillConst(iConstIdx,name,type)` / `CheckAssign(lhs,rhs,line,col)` / `DumpSymTab(os)` / `RecBuiltinTypes()`（2026-10-27 新增，登记 int/float/bool/char/void 五行） / `RecArrayType(name,baseType,size)→TypeEntry*`（2026-10-27 新增，登记 `INT[5]` 这类数组类型行） | 符号信息 → IRGenerator |
| SemanticAnalyzer | `explicit SemanticAnalyzer(SymbolTable&)` / `void Analyze(const ASTNodePtr&)`（阶段⑥ 新增，遍历 AST 填充符号表；不新增独立产物） | ASTNodePtr → 符号表 |
| IRGenerator | `explicit IRGenerator(SymbolTable&)` / `Generate(const ASTNodePtr&)` / `GetTmpVar(TokenType)` / `GetTmpLabel()` / `EmitIR(op,op1,op2,rslt)` / `GetCodes() const` / `GetCodesMutable()`（2026-10-27 新增，供 `-O` 原地改写） / `OperandToString(op)` | CodeList → CCodeGen / --dump-quads |
| CCodeGen | `explicit CCodeGen(const CodeList&)` / `void BindSymTab(const SymbolTable&)` / `bool Generate(AsmOutput&)` | 汇编输出（挑战版） |
| minic_opt（新增） | `void Optimize(CodeList&, const SymbolTable&, OptStats&)`（2026-10-27 新增，仅 `-O` 时调用；四轮窥孔，原地改写 CodeList） | CodeList 原地改写 → CCodeGen |

## 二、main 约定（第 6.7 节）

- 命令行：`minic <源文件> [--dump-tokens] [--dump-ast] [--dump-symtab] [--dump-quads] [--dump-parse] [-O|--optimize] [-h|--help] [-V|--version]`（后三项 2026-10-27 新增）
- 产物：`<源文件名>.token.txt / .ast.txt / .sym.txt / .quad.txt / .parse.txt`（与源文件同目录）+ `<源文件名>.asm`
- 流水线：CLex.Run → CParser.Parse → SymbolTable 语义 → IRGenerator.Generate → CCodeGen.Generate
- 错误：g_ErrorList（含行列号），非零退出码；成功退出码 0
- 错误条数上限：`g_iMaxErrors`（默认 100），超出的仍进 `g_ErrorList`（退出码与计数不变）但不打 stderr，汇总行前补 `minic: (N more error(s) suppressed; limit is 100)`
- 非致命诊断：`EmitWarning()`（2026-10-27 新增）只打 stderr、**不进 `g_ErrorList`**，因此不影响退出码；语义用例用 `/* expect: warn:N */` 断言它

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
15. **中间代码模块缺陷修复（阶段⑦ 收尾，鲍方乐，2026-10-24）**：修复 IR 三处类型转换/短路缺陷，
    均为实现缺陷而非接口变更，`IRGenerator` 对外签名不变。
    - **P1 `&&` / `||` 未真短路**：原 `EvalExpr` 在判断操作符之前就先把左右操作数都 `EvalExpr` 展开，
      右操作数里的函数调用/副作用在跳转前已执行。改为 `TOKEN_AND`/`TOKEN_OR` 分支内先只求左操作数、
      发 `Q_JZ`/`Q_JNZ`，**再**展开右操作数，之后才 `Q_JMP`/`Q_LABEL` 收口。
      实测 `a=false; b = a && side();`（side 打印 888）只输出 `0`，四种真假组合全部符合短路语义。
    - **P2 实参不做类型转换**：`(PARA, 实参, -, -)` 原先只 `EvalExpr` 不看形参类型，
      `float half(float x); half(5)` 把整数 5 的位模式当浮点传进去（输出 `2.8026e-45`）。
      根因是符号表的 `ParamEntry` **没有过程归属**（`m_ParamInfoTbl` 是所有函数形参混在一起的一张扁平表），
      无法按被调函数定位形参类型。故新增私有方法 `BuildParamTypeTable(root)`，
      在 `Generate` 入口扫 AST 的 `FuncDefNode`/`ParamNode` 建「函数名 → 形参类型列表」，
      再由 `ParamTypeOf(procName, pos)` 查询，`Q_PARA` 前按需插 `Q_ITOF`/`Q_FTOI`。
      **这是 IR 内部的临时补丁**：若后续给 `ParamEntry` 补过程归属字段，应改为直接查符号表。
    - **P3 返回值不做类型转换**：`float f() { return 7; }` 直接把整数 7 放进 `%eax`，
      调用方按 float 读 `%xmm0` 得到 `0`（实测）。新增私有成员 `m_curRetType`，
      由 `GenFuncDef` 在翻译函数体前设置为 `ProcEntry::m_retType`，`return` 语句据此插 `Q_ITOF`/`Q_FTOI`。
    - **P4 赋值 int→float 补 ITOF**：`f = i`（f 为 float、i 为 int）原先不插 `Q_ITOF`，
      靠目标代码侧 `Q_ASSIGN` 兜底转换。现在 IR 侧补插，`Q_ASSIGN` 的两端类型在正常路径下已一致。
      数组写 `arr[k] = i`（元素为 float）同样补插。
    - 回归：新增 `tests/codegen/short_circuit.mc`、`arg_conv.mc`、`ret_conv.mc` 三个用例，
      `tests/run_codegen_tests.ps1` 从 17 例增至 **20 例全通过**；前端回归中
      词法 6/6、语法自检、40 例 AST 基线均无差异。
    - 顺带清理：`src/main.cpp` 末尾的完成提示由 `minic: compilation finished (skeleton demo)`
      改为 `minic: compilation finished`（骨架期的残留措辞）。
16. **目标代码模块缺陷修复（阶段⑧ 收尾，鲍方乐，2026-10-24）**：三处缺陷，均不改变对外签名。
    - **C1 嵌套调用实参错序**（根因在 IR，暴露在目标代码）：原 `EvalExpr` 的 `AST_NODE_CALL` 分支
      「边求值边发 `Q_PARA`」，遇到嵌套调用时四元式错序：
      `add3(inc(0), inc(10), inc(20))` 产出
      `(PARA,0)(CALL,inc,_T4)(PARA,_T4)(PARA,10)(CALL,inc,_T5)...`——
      内层 `(PARA,0)` 被内层 `(CALL,inc)` 消费，外层实参 `_T4` 又混进下一次内层调用的实参列表
      （`Q_CALL` 取走当时 `m_pendingArgs` 的全部内容），从第二个实参起全部错乱
      （实测输出 `-1719689673`，期望 `33`）。
      **改为两段式**：先把全部实参求值进 `std::vector<Operand>`（含 `Q_ITOF`/`Q_FTOI` 插入），
      全部求值完再逐个发 `Q_PARA`，`Q_PARA` 与 `Q_CALL` 严格配对。
      注意：**2 个实参的嵌套恰好正确**，只有 ≥3 个实参且有嵌套才暴露，故原 20 例全绿也发现不了。
    - **C2 空函数缺定义**：`AsmEmitter::Run()` 原先 `if (pe.m_Codes.empty()) continue;` 跳过无四元式的
      过程表项。`void f() { }` 没有任何四元式 → 汇编里没有 `f:` 标号，而调用点仍生成 `call f`，
      链接期报 `undefined reference`。MiniC 不支持函数原型，`ProcEntry` 只为**已定义**的函数创建，
      故改为无条件为每个 `ProcEntry` 输出函数体（空函数只留序言 + 出口）。
    - **C3 出错时仍产出 .asm**：`main.cpp` 原先无条件调 `CCodeGen::Generate`。语法/语义报错的输入
      （如 `print(1); int late(int x){...}`）会让残缺 AST 走完 IR 与代码生成，落下一份**只有空 `main`**
      的 `.asm`——它能被 gcc 汇编、也能跑起来，但和用户写的程序毫无关系，属误导性产物。
      改为 `if (g_ErrorList.empty())` 内才生成汇编。
      **注（2026-10-27 起）**：该门禁现在**只保留在目标代码这一层**。语义阶段已改为无条件执行
      （见 `src/main.cpp` 的「报错三修」注释），理由是一个语法错不该把语义该报的问题全部掩盖。
      目标代码仍然保持门禁——它的产物是可直接汇编、链接、运行的 `.asm`，拿残缺 AST 生成
      半成品汇编比「不生成」更危险；而 `.quad.txt`/`.asm` 不产出时用户看到的是明确的错误列表。

    - 回归：新增 `tests/codegen/call_nested.mc`（2/3 实参嵌套 6 组断言，期望 13/106/106/33/32/6）与
      `tests/codegen/empty_fn.mc`（空函数连续调用，期望 7/8/1），`tests/run_codegen_tests.ps1`
      从 20 例增至 **22 例全通过**。
17. **`Q_PARA` 的顺序约定（对目标代码模块的硬约束）**：`Q_PARA` 必须**紧邻**在对应的 `Q_CALL` 之前，
      且多个实参的 `Q_PARA` 按实参从左到右的次序连续排列，中间不得插入任何其他四元式
      （包括内层调用的 `Q_PARA`/`Q_CALL`）。`AsmEmitter` 依赖该约定：`Q_PARA` 只做
      `m_pendingArgs.push_back`，`Q_CALL` 一次性取走整个列表并按 `i*8(%rsp)` 落栈。
      违反该约定会静默产生错误实参（C1 即此情形）。
18. **目标代码模块第二批缺陷修复（阶段⑧ 收尾，鲍方乐，2026-10-24）**：四处缺陷，同样不改变对外签名。
    - **C4 用户自定义 `int main()` 产出重复 `main:` 标号**：`AsmEmitter::Run()` 原先无条件
      `EmitFunction("main", m_top, true)`，随后又为 `m_ProcInfoTbl` 里每个过程各编一份，
      于是**文档《MiniC语法支持清单》7.1~7.3 推荐的写法**（`int main(){...}`）会产出两个
      `main:` 标号，`as` 直接报 `Error: symbol 'main' is already defined`——
      三个官方示例程序全部无法汇编。改为：用户定义了 `main` 时，把顶层语句**接在其函数体之前**
      合并成唯一的 `main`（顶层语句先执行，等价于入口代码）；未定义 `main` 时行为完全不变。
    - **C5 显式 `return 5;` 被出口清零**：原实现把 `xorl %eax, %eax` 发在出口标号**之后**且无条件，
      把 `Q_RETURN` 已装进 `%eax` 的返回值清掉——实测 `int main() { return 5; }` 的进程退出码是
      **0** 而不是 5。改为：仅当控制流能**落到**出口时（最后一条四元式不是 `Q_RETURN`）
      才在标号**之前**补默认值。
      注意 **`Q_JMP` 不能当不可达判据**：循环体末尾回跳循环头的 `Q_JMP` 之后，循环的出口标号
      仍会落到函数末尾（实测按 `Q_JMP` 判定会让 `arg_conv`/`ret_conv`/`short_circuit`
      三个用例 `main` 的退出码变成残值，全部报 `run exit≠0`）。
    - **C6 裸 `return;` 带出残值**：`Q_RETURN` 的操作数为 `OP_NONE` 时原 `GenReturn` 什么都不发，
      直接把 `%eax`/`%xmm0` 里的残值当返回值。顶层语句与 `void` 函数里的裸 `return` 很常见
      （既有用例 `arg_conv`/`ret_conv`/`short_circuit` 的顶层结尾都是 `return;`）。
      改为按当前函数返回类型显式清零：`main` 与整型类返回清 `%eax`，`float` 返回清 `%xmm0`。
    - **C7 无 `return` 的 `int` 函数返回垃圾值**：`int f() { }` 修复前实测 `print(f())` 输出
      **2043900272**（`%eax` 残值）。由 C5 的落空出口默认值覆盖：非 `main` 按
      `ProcEntry::m_retType` 区分 `float`（清 `%xmm0`）与整型（清 `%eax`），统一返回确定的 0。
    - 回归：新增 `tests/codegen/user_main.mc`（数组+递归+算术，期望 9/120/7）与
      `tests/codegen/ret_semantics.mc`（无 return / 有 return / 裸 return / 早退，期望 0/42/2.5/0/1/0），
      `tests/run_codegen_tests.ps1` 从 22 例增至 **24 例全通过**。
    - 实现细节：`AsmEmitter` 新增两个成员 `m_bCurIsMain` / `m_bCurRetFloat`，在 `EmitFunction`
      开头按过程表回填，供 `GenReturn`（裸 return）与出口默认值共用。
 19. **目标代码模块的差分测试验证（2026-10-24，鲍方乐）**：为回答"目标代码是否真的正确"，
      另建一套不依赖手写期望值的验证手段——同一份程序写 MiniC 版与 C 版，C 版由 gcc 编译，
      两者 stdout 逐行比对。**结论：零缺陷。**
    - 整数随机 6 个种子 × 40 例 = **240 例全部一致**；浮点随机 6 个种子 × 40 例 = **240 例全部一致**。
    - 定向用例：Ackermann `ack(3,3)=61`（这是对第 16 条 C1 嵌套调用修复的最强验证，
      因为 `ack(m-1, ack(m,n-1))` 正是"嵌套调用 + 多实参"）、深递归 20000 层、100 个函数、
      50 层括号嵌套、5000 元素局部数组（栈帧 `subq $20032`）、1000 元素全局数组求和。
    - 汇编确定性：同一源码编译 3 次 SHA256 完全一致（`AssignOffsets` 里手写冒泡排序保证顺序稳定）。
    - 栈对齐：所有 `subq $N, %rsp` 的 N 均为 16 的倍数，`call` 时满足 Win64 ABI。
    - 浮点打印精度与 C 完全一致（`1.0/3.0`→`0.333333`、`1000000.0`→`1e+06`）。
    - **重要陷阱（写给后续做差分测试的人）**：C 参考版的浮点字面量**必须加 `f` 后缀**。
      否则 `3.1 / 12.8` 在 C 里按 double 算得 `0.242188`，而 MiniC 的 `float` 是真单精度
      算得 `0.242187`（精确值 `0.2421874851`）——这是 C 的默认类型提升导致的**语义差异，不是缺陷**。
      同理，C 参考版的 `print` 宏必须含 `double:` 分支，否则 `f/2.0` 这类 double 提升的实参
      会落到 `default` 分支被当整数打印。
    - 回归：新增 `tests/codegen/scope_ret.mc` 覆盖此前无回归的三类场景——
      `char`/`bool` 作函数返回类型、局部变量与形参遮蔽同名全局变量、1000 元素局部数组的大栈帧，
      期望输出 15 行（`100/10/0/a/b/c/1/0/0/x/y/z/7.5/499500/Z`），
      `tests/run_codegen_tests.ps1` 从 24 例增至 **25 例全通过**。
 20. **挑战版新增 `++` / `--` 运算符（2026-10-27，鲍方乐）**：语义与 C 一致——`b = a++` 取**旧值**、
      `b = ++a` 取**新值**；单独成句 `a++;`、数组元素 `arr[i]++;` 均可。
      - **词法**：新增终结符 `TOKEN_INC = 045`、`TOKEN_DEC = 046`（插在 `TOKEN_NOT = 044` 之后，
        **不改动任何既有编码**）。`ScanOperator()` 的双字符表在 `&&`/`||` 之后追加 `++`/`--`，
        最长匹配优先，因此 `++ +` 不会被拆成三个 `+`。
      - **文法**：产生式 92 → **102 条**（新增 93–102，1–92 的编号与内容一字未改），
        终结符 39 → **41 个**，非终结符仍 45 个，语义动作 28 → **30 个**（`semantic028` 自增、
        `semantic029` 自减）。**SELECT 冲突数不变**（仍只有悬挂 else 那一处），由
        `tests/grammar_selfcheck.cpp` 的 `conflicts == 1` 断言守住。
        - 前缀：`一元表达式 → ++ 一元表达式`（93）、`--`（94）——与一元 `-` 同层；
        - 后缀：`后缀表达式1 → ++ 后缀表达式1`（95）、`--`（96）——与 `[]` `()` 同层；
        - 语句：`标识符后续 → ++ ;`（97/98，`a++;`）、`语句 → ++ 一元表达式 ;`（99/100，`++a;`）、
          `赋值 → ++ ;`（101/102，`arr[i]++;`）。
      - **AST**：**不新增节点类别**（仍 18 类，`ASTNodeKind` 枚举 19 项含 `AST_NODE_NONE` 哨兵），
        复用 `UnOpNode`；新增 `bool m_bPostfix` 区分前后缀，
        构造函数签名扩为 `UnOpNode(TokenType op, ASTNodePtr operand, const LineInfo& loc, bool isPostfix = false)`
        （**带默认值，既有调用点无需改动**）。`FillOpenBase` 的下钻条件补上 `AST_NODE_UNOP`，
        否则 `arr[i]++` 的数组基址填不进去。
      - **中间代码**：**不新增 `QuadOp`**（该枚举的追加规则要求新码只能加在末尾，本次全部复用）。
        `IRGenerator::GenIncDec(const UnOpNode*, bool bReturnOld)` 产出「旧值快照 → 原值 ±1 → 写回」，
        自增用 `Q_ADD`、自减用 `Q_SUB`；变量走 `Q_ASSIGN`、数组元素走 `Q_INDEX` 读写两支。
      - **目标代码**：**零改动**（复用 `Q_ADD`/`Q_SUB`/`Q_ASSIGN`/`Q_INDEX` 的既有翻译）。
      - **语义检查**：操作数必须是可修改左值（`AST_NODE_IDENT` / `AST_NODE_INDEX`），否则报
        `'++' requires a modifiable lvalue`；类型必须是 int/float（`IsNumeric`，char 与 bool 不接受），
        否则报 `'++' requires a numeric operand`。
      - 回归：`tests/parser/incdec.mc`（正例，前/后缀/语句/数组四形态）、
        `tests/parser/err_incdec_operand.mc`（非左值 + 非数值两条）、`tests/quad/incdec.mc`、
        `tests/codegen/incdec.mc`（期望 `5/6/7/7`，验证前后缀取值差异）。
        parser 47 → **49 例**、quad 14 → **15 例**、codegen 25 → **26 例**，全绿。
      - **变异测试**：把 `EvalExpr` 里的 `GenIncDec(u, u->m_bPostfix)` 改成 `!u->m_bPostfix`
        （即前后缀语义对调），`tests/codegen/incdec.mc` 立刻报
        `expect: 5|6|7|7 / actual: 6|6|6|7`——证明该用例确实锁住了语义。
 21. **常量折叠（挑战版 C4 落地，2026-10-27，鲍方乐）**：设计文档特性表 C4 与 §9.2 的
      `TypeSysInfo + OpConstSemantic` 都写了「常量折叠」，但 `src/sema/ir.cpp` 此前**一行实现都没有**。
      本次补齐，**不改变任何对外签名、不新增 `QuadOp`、目标代码层零改动**。
     - **新增私有接口**（`src/sema/ir.h`）：
       `Operand ConstFloat(double value, const LineInfo& loc)`、
       `bool ConstValueOf(const Operand& op, bool& bFloat, long long& iVal, double& fVal) const`、
       `Operand FoldBinary(TokenType op, const Operand& lo, const Operand& ro, const LineInfo& loc)`。
     - **插入点**：`EvalExpr` 的 `AST_NODE_BINOP` 分支，在 `OperandType(lo)`/`OperandType(ro)`
       之后、关系与算术分支**之前**。必须在**类型转换之前**折叠——`FoldBinary` 内部已按 §8.1
       的 Promote 规则处理 int/float；若先插 `ITOF`/`FTOI` 再折叠，会白生成一条转换四元式。
     - **覆盖范围**：`+ - * /` 与六个关系运算符（`== !=` 在相等层、`< <= > >=` 在关系层）；两侧都是 `OP_CONST` 才折叠。
       浮点走 `ConstFloat`（登记进 `m_ConstInfoTbl`，同时回填 `ConstEntry::m_szVal`，
       否则 `--dump-quads` 会把常量打印成空串），整型走 `ConstInt`；除法为 C 语义向零截断
       （`7 / 2` → `3`，不是 `3.5`）。
     - **三条故意不折叠的边界**（理由写在代码注释里）：
       ①**除数为 0** 不折叠——编译期算出 `Inf` 会把「运行时除零」变成「静默的错结果」；
       ②**整型结果越界**（`> 2147483647 || < -2147483648`）不折叠——交给运行时按 x86 `addl`
       自然回绕，与未折叠时**逐位一致**；③**`&&` / `||`** 不折叠——短路有副作用语义，
       右侧可能不执行，必须保留 `Q_JZ`/`Q_JNZ` 分支。
     - 回归：新增 `tests/quad/const_fold.mc`（折叠后的四元式结构）与
       `tests/codegen/const_fold.mc`（期望 `7/3/3/1/5/11/-3`，证明折叠前后运行结果一致）；
       既有 `tests/quad/arith_int.expected.quad.txt` 的 `(/, 4, 2, _T3)` 一行被折叠掉，
       基线同步更新为 `(-, _T2, 2, _T3)`。quad 15 → **16 例**、codegen 26 → **27 例**，全绿。
     - **变异测试**：把 `FoldBinary(op, lo, ro, loc)` 改成恒返回空 `Operand`，
       `arith_int` 与 `const_fold` 两个用例立刻 FAIL——证明基线确实锁住了折叠行为。

 22. **`char` 参与算术与关系运算（C 的整型提升，2026-10-27，鲍方乐）**：`char` 自 v4.0 起就是一
     等数据类型（§8.1 类型表、词法 004），但语义侧的 `IsNumeric` 只认 `TOKEN_INT` / `TOKEN_FLOAT`，
     导致 `c + 1` 报 `arithmetic operands must be numeric`、`c == 'x'` 报
     `relational operands must be numeric`。而 IR 侧的 `IsIntType`（`src/sema/ir.cpp`）**早就**把
     `TOKEN_CHAR` 算作整数，注释写明「char 在算术中按整数处理（C 语义）」——两层口径分裂，
     实际表现是 char 只能 `print`。本次放宽，**不新增接口、不改动任何已冻结签名**。
     - **改动的唯一一处**：`src/sema/semantic.cpp` 的
       `bool SemanticAnalyzer::IsNumeric(TokenType t) const`，返回值加上 `t == TokenType::TOKEN_CHAR`。
       `bool` 仍然被排除——布尔不参与算术是 §8.1 类型表定下的规则。
     - **语义**：`char` 参与算术时按 `int` 参与（C 的整型提升），`Promote(CHAR, INT)` 返回 `INT`；
       与 `float` 混算提升为 `float`。设计文档 §8.1 的类型表已补 `T_CHAR` 两行（算术 → `T_INT`、
       关系 → `T_BOOL`，转换列写「整型提升」）。
     - **目标代码零改动**：`char` 数组元素占 1 字节（写 `movb`、读 `movsbl` 符号扩展），
       `char` 变量占 4 字节（`movl`），两者在四元式层都是整数操作数。
     - **回归**：新增 `tests/quad/char_arith.mc`（18 行四元式基线，覆盖 `c + 1`、`c < 'a'`、
       `c == 65`、`arr[0] + 1`、`c++`）、`tests/codegen/char_arith.mc`（期望 `66/1/1/y/B`）、
       `tests/sema/ok_char_arith.mc`（空诊断）与 `tests/sema/err_bool_arith.mc`
       （两行 `arithmetic/relational operands must be numeric`，锁住 bool 仍被拒）。
       quad 16 → **17 例**、codegen 27 → **28 例**、sema 12 → **14 例**，全绿。
     - **变异测试**：把 `IsNumeric` 里的 `t == TokenType::TOKEN_CHAR` 改成 `false`，
       `char_arith` 的 quad 与 codegen 两个用例立刻 FAIL——证明基线锁住了这条放宽。

 23. **`print` 不自动换行 + 实参个数与 `return` 类型检查（2026-10-27，鲍方乐）**：三项语义/代码
     生成层的口径调整，**不新增任何公开接口、不改动 `docs/interface.md` 第 6 章已冻结的签名**。
     - **`print` 不再追加换行**：`src/codegen/codegen.cpp` 收尾处三张格式串由 `"%d\n"` / `"%g\n"`
       / `"%c\n"` 改为 `"%d"` / `"%g"` / `"%c"`。理由：原来每个值独占一行，无法拼出
       `1 2` 这种同行输出；现在 `print(a); print(b);` 输出 `ab`，要换行写 `print('\n')`
       （词法的 `ScanCharConst` 已支持 `\n \t \r \0 \\ \' \"` 六个转义）。
       **这是行为变更**：所有原本依赖「每条 print 自动换行」的用例都已显式补 `print('\n');`
       —— `tests/codegen/` 下 29 个 `.mc` 共补 94 处，28 个既有 `.expected.txt` 一字未改仍全过；
       新增 `tests/codegen/print_no_newline.mc` 锁住「不换行」本身（期望 `12` / `3` / `ab` 三行）。
     - **`return` 类型检查**：`src/sema/semantic.h` 新增私有成员 `m_curRetType` / `m_szCurFunc`
       与私有方法 `TokenType VisitReturn(ReturnNode* ret);`（**私有，不是对外接口**）；
       `src/sema/semantic.cpp` 把 `AST_NODE_RETURN` 从原先与 `PRINT` 共用的分支里拆出，
       `VisitFuncDef` 在进入函数作用域时保存当前返回类型、退出时恢复。规则：
       `void` 函数带值 → `void function '<名>' cannot return a value`；
       非 `void` 函数裸 `return;` → `non-void function '<名>' must return a value`；
       带值时复用 `SymbolTable::CheckAssign`（`int ← float` 仍是 Q9 告警 + 截断，不阻断）。
       **顶层（函数体外）的裸 `return;` 不检查** —— `tests/parser/return_stmt.mc`、
       `tests/codegen/{arg_conv,ret_conv,short_circuit}.mc` 都在顶层写过裸 `return;`，
       收紧会把既有合法用例打成非法。
     - **实参个数检查**：`SemanticAnalyzer` 新增私有成员 `m_FuncSigs`
       （`std::map<std::string, std::vector<TokenType>>`）与私有方法
       `void CollectSignatures(const ASTNodePtr& root);`。`Analyze()` 的入口先
       `CollectSignatures(root)` 再 `Visit(root)` —— 必须两遍，因为 MiniC 允许「函数体里调用
       后面才定义的函数」，边 Visit 边登记会漏。`VisitCall` 末尾比对个数，不符报
       `function '<名>' expects N argument(s), but M given`。
     - **回归**：`tests/sema/` 新增 `err_arg_count.mc`、`err_ret_void.mc`、`err_ret_novalue.mc`、
       `ok_param_ret.mc` 四例（首行 `/* expect: err */` 或 `/* expect: ok */`，
       golden 在 `tests/sema/<名>.expected.diag.txt`，**不在 `expected/` 子目录**）。
       sema 14 → **18 例**、codegen 28 → **29 例**，`build.bat test` 11 套全绿。
     - **GUI 手册与示例库同步**：`gui/src/samples.js`（15 条示例，补 41 处 `print('\n')`、
       8 处给无花括号控制体加 `{ }`，`output` 字段全部按实跑结果重算）与
       `gui/src/syntax-book.js`（45 条 `example` 补 96 处换行，60 条 `output`/`err` 全部按
       实测重写，`ret-mismatch` 由 `limit` 改 `no`、`stmt-if` 的 else 分支补花括号）。
       `book-smoke` 60 条、`sample-smoke` 22 条断言全 PASS。
 24. **关系运算符按 C++ 分层（2026-10-27，鲍方乐）**：`==` `!=` 的优先级由「与 `<` `<=` `>` `>=`
     同级」改为**低于**它们，与 C/C++ 一致。**不改动任何冻结签名**，但**是文法改动**，
     需要同步「两处代码 + 一份工作簿 + 一份权威基线 + 全套文档」。
     - **文法改动（编号 1–92 一字未动）**：新增非终结符 `相等表达式`(145)、`相等表达式1`(146)、
       `相等运算符`(147)；新增产生式 103–105（`145→130 146`、`146→147 130 146 | sem 8@2`、`146→ε`）。
       同时**就地改**产生式 53（`与表达式→相等表达式 与表达式1`）、54（`与表达式1→&& 相等表达式 与表达式1`）、
       59/60（左部由 `关系运算符` 改 `相等运算符`）。非终结符 45 → **48**，产生式 102 → **105**，
       终结符仍 41。`conflicts=0`，未引入新的 SELECT 冲突。
     - **为什么只改文法不算修好**：`src/sema/semantic.cpp` 的 `DeduceBinOp` 关系分支要求
       `IsNumeric(lt) && IsNumeric(rt)`，而 `IsNumeric` 只认 int/float/char（不含 bool）。
       链式比较的中间结果都是 bool，所以改了文法后 `a == b < c` 仍会报
       `relational operands must be numeric`，**分层不产生任何可观察变化**。
       因此同批放开 bool 的比较能力：`const auto comparable = [](TokenType t) { return IsNumeric(t) || t == TOKEN_BOOL; };`
       不满足时改报 `relational operands must be numeric or bool`。**bool 仍不参与算术**
       （`b + 1` 照旧报 `arithmetic operands must be numeric`）、**仍不参与逻辑以外的地方**。
     - **同步落点（五处，缺一不可）**：①`src/parser/grammar.cpp`（由 `tools/generate_grammar.py` 从
       工作簿生成）；②`src/parser/grammar.h`（`MAX_NT=48` / `PROD_COUNT=105` / `PROD_TABLE_SIZE=106`）；
       ③`E:\计算机软件实训\MiniC挑战版文法及其相关表格.xlsx`（`extend_workbook.py` 幂等重建，
       15 张表里首/带语义/编码/编号/First/Follow/Select/预测表/说明 9 张要一起改）；
       ④`tests/parser/expected/ll1_411.parse.txt`（**外部权威基线，取自设计文档 §4.11，
       步骤 82 → 86，不能拿程序输出顶替**）；⑤`docs/MiniC编译器设计文档_v4.0_挑战版LL1.md` §4 全章。
     - **实测对照（与 g++ 逐行相同）**：`a == b < c` 解析为 `a == (b < c)`（AST 顶层是
       `BinOp(EQ) [ Ident(a), BinOp(LT)[Ident(b), Ident(c)] ]`）；`a < b == c` 解析为
       `(a < b) == c`；`a < b < c` 是 `(a < b) < c`。运行值 `a=2,b=3,c=5` 时四条链式比较
       输出 `0 / 0 / 1 / 1`，`g++` 编译等价 C++ 后输出完全相同。
     - **仍与 C 不同的一点（如实记录）**：`<` 与 `<=` 之间**同级左结合**，`a < b <= c` 是
       `(a < b) <= c`；C 里它们同级但语义上多数场景无差，这里不改（要做需再插两层）。
     - **回归**：parser 49 → **50 例**（新增 `relational_layers.mc`）、quad 17 → **18 例**、
       codegen 29 → **30 例**、sema 18 → **19 例**（`err_bool_arith.mc` 的 `if (a == b)` 现在合法，
       换成 `b = a && 1;`；新增 `ok_bool_relational.mc` 锁住「bool 可比较、只告警不报错」）。
       `build.bat test` 11 套全绿。
 25. **四元式操作码按参考书表 5-2 重建（2026-10-27，鲍方乐）**：`QuadOp` 由 35 个扁平操作码
     改为表 5-2「NPIR 的操作符」的 **21 大类 + 宽度后缀**组织，共 **151 个成员**，
     枚举值按**高 5 位大类、低 5 位类内成员**编码（`(CAT << 5) | 成员号`）。
     **不改动任何冻结的函数签名**，但**是 `QuadOp` 枚举的破坏性替换**，所有引用旧操作码的
     代码与 `.quad.txt` 基线全要跟着改。
     - **为什么要做**：参考书第 5 章表 5-2 是课程指定参考书的核心设计，原实现只有 35 个
       扁平操作码（`Q_ADD`/`Q_ASSIGN`/`Q_PRINT`），与书里「宽度是操作符的一部分」的思想脱节 ——
       书里 `ADD_4` 与 `ADD_8F` 是两个不同操作符，所以 IR 不靠类型表查宽度；
       MiniC 原来把宽度留给目标代码现查 `Operand.m_type`，与书的设计对不上。
     - **MiniC 实际产生 30 个成员**（其余 121 个是表 5-2 有、MiniC 语言里不存在的，
       如位运算 `OR_0`/`AND_0`、位移 `SHR_*`/`SHL_*`、集合差 `SUB_SET`、
       字符串连接 `ADD_S`、`CALLPTR`/`ASM`/`GETADDR` 等；`codegen` 的 `default:` 分支兜住它们）：
       - 关系 1~6 类：`EQU_1/_4/_4F`、`NEQU_1/_4/_4F`、`LS_1/_4/_4F`、`LE_1/_4/_4F`、
         `MR_1/_4/_4F`、`ME_1/_4/_4F`（char 用 `_1`，含 float 用 `_4F`，否则 `_4`）
       - 算术 8~13 类：`ADD_4/_4F`、`SUB_4/_4F`、`MUL_4/_4F`、`DIV_4/_4F`
       - 18~19 类：`NEG_4/_4F`、`NOT_4`
       - 20 类：`ASSIGN_1/_4/_4F`
       - 21 类：`JMP` `LABEL` `PARA` `RETV` `CALL` `JT` `JNT` `INDEX_1/_4/_4F`
         `PRINT_1/_4/_4F` `ITOF` `FTOI`
     - **与表 5-2 的三点差异**（`src/sema/ir.h` 注释里逐条写明）：
       ①补 5 个 `_4F` 扩展成员（`SUB_4F`/`ADD_4F`/`MUL_4F`/`DIV_4F`/`ASSIGN_4F`）——
       书里浮点后缀只有 `_8F`（64 位 double），MiniC 的 `float` 是 32 位单精度，
       目标代码用 `addss`/`movss`，对不上 `_8F`；硬塞会造成「名字说 8 字节、实际 4 字节」的错觉。
       ②`JT`/`JNT` 按书改正（旧文档里误写成 `Q_JT`/`Q_JF`）。
       ③`ITOF`/`FTOI` 是 MiniC 扩展 —— 表 5-2 没有「类型转换」大类，参考书靠
       `TypeSysInfo` 的 `m_bConvert1/m_bConvert2` 在 `Emit` 时隐式插入；MiniC 显式列出来，
       便于在 `.quad.txt` 里看见转换发生在哪一步。
     - **辅助接口（新增，均为自由函数，不改冻结签名）**：`QuadCatOf` / `QuadMemberOf` /
       `MakeQuadOp` / `SameQuadCat` / `QuadCatName` / `QuadMemberName` / `QuadOpName` /
       `QuadOpIsFloat` / `QuadOpByteWidth` / `QuadOpForType` / `QuadIndexOp` / `QuadPrintOp`。
       `QuadOpName` 由 `ir.h` 统一提供（返回 `"ADD_4"` / `"ASSIGN_4F"` 这类助记符），
       `Quad::Print` 与 `main.cpp` 的 `--dump-quads` 都改调它，不再各写一份表。
     - **`AsmEmitter` 同步**：`GenQuad` 的 switch 从「35 个扁平 case」改为「按大类分组的
       case 组」（`Q_ADD_4`/`Q_ADD_4F` 共用 `GenBinary(q,"addl","addss")` 等），
       新增 `GenNeg`（`negl` / `xorps+subss`）与 `GenNot`（`cmpl $0; sete`）两个方法 ——
       原来取负走 `0 - x` 的二元减法、取非走 `x == 0` 的关系运算，现在对应表 5-2 的
       第 18/19 类各自有专门操作码，翻译也独立成函数。
     - **实测与改造前逐条等价**：同一份综合用例（含 int/float/char/数组/函数调用/取负/取非/
       六种比较/短路/自增自减）改造前后汇编运行输出**完全一致**；
       `a=2,b=3,c=5` 时 `a == b < c` / `a < b == c` / `a < b < c` 仍输出 `0 / 0 / 1`，
       与 `g++` 相同。
     - **回归**：quad 18 个基线**全部重刷**（操作码文本从 `(=, +, -` 变成
       `(ASSIGN_4, ADD_4, SUB_4`），`codegen` 30 例**零改动**（它比对的是运行输出不是四元式）、
       parser 50 / sema 27（当时是 19，后续 C 档新增 8 例）不受影响。`build.bat test` 11 套全绿。

 26. **类型表启用：`TypeEntry` 加字节宽度 + `RecBuiltinTypes()`（2026-10-27，鲍方乐）**：
     `--dump-symtab` 的 `-- Types --` 节此前恒为空 —— `SymbolTable::RecType` 与
     `SearchTypeInfoTbl` 是**零调用的死接口**，`semantic001`（「登记基本类型到类型表」）
     从未实现，类型只内嵌在 `VarEntry::m_type` 里。这次把表真正用起来。

     - **`TypeEntry` 扩一个字段**（`src/symbol/symbol.h`）：
       ```cpp
       int m_iSize = 0;   // 该类型在内存里占几个字节
       ```
       口径与 codegen 的访问指令一致：`char = 1`（数组元素用 `movb` / `movsbl`）、
       `int / float / bool = 4`（`movl` / `movss`）、`void = 0`（无对象）。
       类型链字段（`m_iLink` / `m_iTypePos` / `m_iTypeFlag`）**仍然停用** ——
       MiniC 无结构体/指针/用户自定义类型，链式类型表无从发挥。
     - **新增两个接口**（`src/symbol/symbol.h`，均为新增，不改动已冻结的 10 项签名）：
       ```cpp
       void RecBuiltinTypes();                        // 登记 int/float/bool/char/void 五行（幂等）
       static int SizeOfType(TokenType baseType);     // 1 / 4 / 0 的字节宽度口径
       ```
     - **调用点唯一**：`SemanticAnalyzer::Analyze()` 入口，在 `CollectSignatures()` 与
       `Visit()` 之前调一次 `m_symtab.RecBuiltinTypes();`。幂等（先用
       `SearchTypeInfoTbl` 查重），所以语法出错后二次进入也不会重复登记。
     - **`DumpSymTab` 输出变化**：Types 节从空变为
       ```
       -- Types --
         int : INT  size 4
         float : FLOAT  size 4
         bool : BOOL  size 4
         char : CHAR  size 1
         void : VOID  size 0
       ```
       `.sym.txt` 从 208 B 变 318 B。
     - **顺带修掉 `char` 数组多分配**：`src/codegen/codegen.cpp` 的全局 `EmitGlobals` 与
       局部 `AssignOffsets` 原来一律按 `4 * 元素个数` 分配，`char carr[4]` 白占 12 字节。
       现在按 `ElemBytesOfType(v.m_type) * 元素个数` 分配（char=1，其余=4），
       与访问侧的 `ElemWidth()` 口径对齐。实测 `char s[4]` 的 `.zero` 从 16 变 4，
       `float farr[3]` 仍是 12，`int carr[4]` 仍是 16；标量仍走 8 字节槽不变。
     - **验证**：`tests/test.sym.txt` 基线刷新（9 行 → 14 行）；
       char 数组端到端用例（全局 `char g[9]`、局部 `char a[9]` / `char b[5]` 与 `int c`
       混排）运行输出 `AZpq|777|aaaaaaaaa|777` 与预期一致，未出现元素互相踩踏；
       `build.bat test` 11 套全绿（parser 50 / sema 27 / quad 18 / codegen 30 /
       symtab / book-smoke / sample-smoke）。
     - **文档同步**：`docs/验收报告.md` 的 `.sym.txt` 样例与产物尺寸（208 → 318 B、
       94 → 106 B、71 行 66 步 → 73 行 68 步）、`docs/项目总结.md` 的产物表、
       `README.md` 的 SymbolTable 模块状态行、`docs/MiniC编译器数据结构设计.md` 的
       `TypeEntry` 与「三张表」两处说明。

 27. **语法分析轨迹改为按需记录（2026-10-27，鲍方乐）——O(n²) 性能缺陷修复**：
      `CParser::RecordStep` 原来无条件执行，每步都调 `StackText()`（遍历整个分析栈）
      与 `InputText()`（遍历**剩余全部 Token**）拼字符串，而它有三个调用点、每 Token
      至少触发 3 次 —— 整体是 O(n²) 的字符串构造。
      - **新增** `void SetTraceEnabled(bool bOn)`（`src/parser/parser.h`），
        私有字段 `bool m_bTraceEnabled`（构造默认 `false`）；`RecordStep` 首行
        `if (!m_bTraceEnabled) { return; }`。
      - **调用点**：`src/main.cpp` 在建完 `CParser` 后 `parser.SetTraceEnabled(dumpParse);`
        —— 只有真要写 `.parse.txt` 才打开。`--dump-parse` 之外的路径全免费。
      - **防死循环改造**：`Parse()` 原来用 `m_trace.size() > kMaxSteps` 兜底，
        轨迹关掉后该判断永远为假，改用独立计数器 `std::size_t iSteps`（`kMaxSteps` 仍为 1000000）。
      - **实测**：1000 个空函数的源文件 **68 150 ms → 189 ms**（约 360 倍，且恢复线性：
        100/200/400/1000 个函数分别 125 / 46 / 72 / 189 ms）。
      - **产物不变**：`tests/parser/expected/ll1_411.parse.txt` 逐字一致（8212 字符）。

 28. **静态检查补四项（2026-10-27，鲍方乐）—— 均为新增诊断，不改任何已有签名**：
      - **数组作用域**：`VisitIdent` 里若 `VarEntry::m_bIsArray` 为真且不带下标使用，
        报 `'a' is an array; a subscript is required to read an element`。
        `VisitIndex` 相应改为「基址是 IDENT 时直接返回元素类型」，不再二次 `Visit` 基址
        （否则会被上一条自己报错）。**改写前 `b = a;` 静默取首元素，属长期未发现的语义漏洞。**
      - **常量下标越界**：新增 `void CheckConstIndex(const VarEntry&, const std::string&, const ASTNodePtr&, const LineInfo&)`
        （`src/sema/semantic.h`）。仅当下标是整型字面量时检查，报
        `array subscript 5 is out of bounds (array 'a' has 3 element(s))` 或
        `array subscript -1 is negative (...)`。变量下标不做（静态不可知）。
      - **非 void 函数缺 return**：新增 `static bool HasReturnStmt(const ASTNodePtr&)`
        （递归进 block/if/while，**不进嵌套函数定义**）。`VisitFuncDef` 收尾处若不满足则
        `EmitWarning("non-void function 'f' has no return statement; ...")`。
        **刻意做成警告而非错误**：`tests/codegen/ret_semantics.mc` 有两个用例专门固定
        「落空出口返回 0」的既有语义，做成错误会直接推翻它。
      - **无入口提醒**：`CollectSignatures` 里若函数名是 `main` 就置 `m_bHasMain`；
        `Analyze` 末尾若「没有 main + 有函数定义 + 无顶层语句 + 语法零错误」则
        `EmitWarning("no 'main' function and no top-level statements; ...")`。
        **三个前提缺一不可**：`codegen.cpp` 在未定义 main 时用顶层语句合成入口是既有合法设计，
        且全仓 127 个用例里有 10 个没有 main，做成硬错误会全部崩掉；
        语法已失败时也不追这条（否则空文件、错误文件都会被刷一条）。
      - **配套**：`src/lexer/token.{h,cpp}` 新增 `void EmitWarning(const std::string&, const LineInfo&)`，
        格式 `minic: warning: 行:列: 消息`，**不进 `g_ErrorList`**（退出码仍为 0）。
      - **新增 6 个用例**（`tests/sema/`）：`err_array_oob` / `err_array_name`（err）、
        `ok_no_return_warn` / `ok_no_main_warn`（`warn:1`）、`ok_return_nested` /
        `ok_no_main_toplevel`（ok，验证不误报）。**旧 19 个语义基线一个字都没改**，
        证明没有引入任何新诊断。

 29. **符号表三张死表接上（2026-10-27，鲍方乐）**：
      - **类型表补数组形状**：`TypeEntry` 的 `m_bIsArray` / `m_iArraySize` 此前**全仓零写入**，
        `int arr[5]` 在 Types 节只显示一行 `int : INT  size 4`。新增
        `TypeEntry* RecArrayType(const std::string& name, TokenType baseType, int size)`，
        `VisitArrayDecl` 里按 `"INT[5]"` 这类名字登记，`m_iSize = SizeOfType(baseType) * size`
        （与 codegen 的 `ElemBytesOfType × m_iArraySize` 同口径）。`DumpSymTab` 输出
        `INT[5] : INT  array 5  size 20`。
      - **新增 `-- Params (challenge) --` 节**：`m_ParamInfoTbl` 一直存在但**从来没被 dump 过**
        （7 张表只出 5 张）。`ParamEntry` 没有「所属过程」字段，故按「声明行 ≤ 形参行且最近的
        ProcEntry」回连过程名，输出 `add.x : INT  #1  @line 6 scope 2`。
      - **`RecLabel` 接入**：该接口此前全仓零调用，`-- Labels --` 恒空，但 `while`/`if` 明明
        产生 LABEL 四元式。改在 `IRGenerator::EmitIR(...)` 的五参重载里登记 ——
        **注意标号在 `Op1` 不是 `Rslt`**（`EmitIR(QuadOp::Q_LABEL, Operand::Label(lMid), ...)`），
        第一版写成 `rslt` 导致 Labels 节仍空。同时 `src/main.cpp` 把 `DumpSymTab` 整段
        **从语义阶段之后移到中间代码之后**，否则标号还没登记就 dump 了。
      - `DumpSymTab` 的 Labels 节改为**按标号号升序**输出（`std::sort`），格式 `L1  @line 16`。
      - **新增第 2 个 symtab 用例** `tests/symtab_full.mc`（23 行，覆盖全局/局部数组、函数、
        形参、while + if/else 标号）与基线 `tests/symtab_full.sym.txt`（37 行）；
        `tests/run_symtab_tests.ps1` 从单用例改为 `$cases = @("test", "symtab_full")` 循环。
        `tests/test.sym.txt` 从 14 行 318 B → 16 行 343 B（多一个空 Params 节）。

 30. **命令行增强：`-h` / `-V` / 错误上限（2026-10-27，鲍方乐）**：
      - `-h|--help` 打完整用法并 `return 0`；`-V|--version` 打
        `MiniC Compiler 4.0 (challenge, x86-64)` 并 `return 0`。
        **两者都必须在把 `argv[1]` 当源文件打开之前处理**，否则 `minic --help`
        会去打开一个叫 `--help` 的文件。
      - 错误条数上限 `g_iMaxErrors = 100`（`src/lexer/token.{h,cpp}`）；超出的错误
        **仍然 push_back 进 `g_ErrorList`**（退出码与 `N error(s) found` 计数都依赖它），
        只是不再打 stderr；汇总行前补
        `minic: (100 more error(s) suppressed; limit is 100)`。
        **取 100 的依据**：全仓基线里错误最多的 `.err.txt` 只有 4 条，100 足够覆盖真问题，
        只挡「同一个语法错连报几十条」的刷屏。

 31. **四元式窥孔优化 `-O / --optimize`（2026-10-27，鲍方乐）**：
      新增 `src/sema/optimizer.{h,cpp}`（`namespace minic_opt`），
      `void Optimize(CodeList& codes, const SymbolTable& symtab, OptStats& st)` 四轮原地改写：
      ①常量条件跳转折叠（条件查 `m_ConstInfoTbl` 拿真实值，为整型 0 时 `Q_JNT`→`Q_JMP`、
      `Q_JT`→删）②删不可达（块结束符之后到下一个 `Q_LABEL` 之前整段删）
      ③空跳（`JMP L` 紧跟 `LABEL L` 删 JMP）④死临时量与死标号（先扫全段收集活跃集再删，
      **收集时须排除 `Q_LABEL` 自身的 Op1**，否则标号永远删不掉）。
      `main.cpp` 只加 `-O` 时，对 `ir.GetCodesMutable()` 与 `symtab.m_ProcInfoTbl` 各
      `p.m_Codes` 逐个调用，并向 stderr 打
      `minic: -O removed N redundant quad(s) [branch .., unreachable .., jumps .., temps .., labels ..]`。
      - **最重要的一条约束**：判「后续不可达」时**只有 `Q_JMP` 与 `Q_RETV` 算块结束**，
        **`Q_JT` / `Q_JNT` 绝不能算**。`JNT cond -> L` 是「条件为假才跳」，为真时继续往下走，
        紧随其后的正是 `if` 的真分支。第一版把条件跳转也当转移，结果 `-O` 把真分支当死代码删掉 ——
        全用例 sweep 里 16 个用例输出变化（`if_else` 从 `1 4` 变 `2 4`，`recur` / `user_main`
        直接栈溢出 `-1073741571`）。修正后 89 个可编译用例 `-O` 前后 stdout 与退出码
        **零差异**，`build.bat test` 11 套全绿（默认路径不优化，产物逐字不变）。
      - 默认不优化，`.asm` 与四元式产物在无 `-O` 时与改动前完全一致。

  32. **目标代码生成质量：累加器直通 + 立即数直用（2026-10-28，鲍方乐）——纯内部改动，不改任何签名**：
       `AsmEmitter` 原先对每个四元式严格「load → 运算 → store」，临时量也落栈，最常见的
       `b = a * 4` 会生成「存 `_T1` / 立刻读 `_T1`」这样一对纯搬运指令。
       - **累加器直通**：新增私有成员 `std::string m_szAcc`（记录 `%eax` 当前装的是哪个操作数）、
         `std::unordered_set<int> m_forwardedTemps`（免落栈的临时量）、`AnalyzeTempReuse()`、
         `ConsumesEaxFirst()`。`LoadInt` 发现要读的正是 `m_szAcc` 且目标寄存器是 `%eax` 时直接返回；
         `AnalyzeTempReuse` 只把「只被读一次 + 唯一读者就是紧邻的下一条四元式 + 该读者是 Op1」
         的临时量放进免落栈集合。
       - **白名单约束**：只对 `%eax` 生效；`Q_PRINT`（读 `%edx`）、`Q_INDEX`（先把基址装 `%rax`）、
         `Q_PARA`（不发射代码、延后到 `Q_CALL` 才消费）**不在白名单**，否则会读到从未写过的栈槽。
       - **`ClobberAcc()` 插入点**：`Q_LABEL`、`Q_JMP`、`GenIndex` 入口、`call` 之后、
         `EmitParams()` 之后、`GenReturn` 的裸 `return` 分支。
         **`GenReturn` 绝不能在函数入口清缓存** —— `RETV` 的操作数常是上一条刚算出、
         因免落栈而未写回栈槽的临时量，清了会返回栈垃圾（实测 5 个用例 `output mismatch`，
         `func_call` 输出 `1937766560` 而不是 `7`）。
       - **立即数直用**：新增 `bool IntConstOf(const Operand&, long long&) const`（浮点常量返回
         false，因为 `addss` 没有立即数形式）与 `void StoreImmInt(long long, const Operand&)`。
         `imull $4, %eax` 取代「`movl $4, %ecx` + `imull %ecx, %eax`」；`cmpl $3, %eax` 同理；
         整数常量赋值直接 `movl $7, slot(%rbp)`。`idiv` 没有立即数形式，仍走寄存器。
       - **实测**：31 个用例的汇编指令总数 **3246 → 3010，减少 236 条（7.3%）**；
         `demo.mc` 单文件 177 → 162。剩余空间需要寄存器分配，`-O` 与本改动无关（默认路径同样受益）。

  33. **静态检查再补三项（2026-10-28，鲍方乐）——新增诊断，不改签名**：
       - **实参类型**：新增文件内 `bool ArgCompatible(TokenType want, TokenType got)`，
         `int` / `float` / `char` 互转放行，`bool` 与数值不兼容。`VisitCall` 收集每个实参的
         `Visit` 结果后逐参比对，不兼容报
         `argument 1 of function 'f' expects INT, but FLOAT given`（**警告**，退出码仍为 0）。
       - **整型字面量越界**：`VisitNum` 的整型分支加检查，报
         `integer literal 2147483649 is out of range for int (-2147483648..2147483647)`（**错误**）。
         **阈值取 2147483648 而不是 2147483647**：`-2147483648` 在 AST 里是
         `UnOp(TOKEN_MINUS, Num(2147483648))`，只看 `Num` 节点无法区分「INT_MIN 的绝对值」
         与真溢出，宁可放过这一个值也不误报 INT_MIN。
       - **`char` 字面量越界**：`VisitAssign` 里仅当右值是 `AST_NODE_NUM` 且非浮点/字符/布尔字面量
         时检查，报 `value 300 cannot be represented in char (-128..127); it will be truncated to 44`（**警告**）。
       - **两个辅助函数必须放在文件最前面的匿名命名空间** —— 第一次插在「类型工具」小节，
         而 `VisitAssign` / `VisitCall` 在其前面，编译报 `'TruncateToChar' was not declared`。
       - **新增 5 个用例**：`err_literal_overflow` / `ok_int_min` / `warn_char_overflow` /
         `warn_arg_type` / `ok_arg_conv`，语义用例 **27 → 32**。
         `-Update` 会重写全部基线，实测只有 `ok_array_char_index.expected.diag.txt`
         从 0 字节变 1 字节（多一个换行），已回退，其余一字未改。

  34. **诊断附带源码上下文（2026-10-28，鲍方乐）**：
       原先只打印 `minic: error: 行:列: 消息`，要定位还得自己数行。现在每条诊断下面多两行：
       ```
       minic: error: 4:9: integer literal 2147483649 is out of range for int (-2147483648..2147483647)
         4 |     a = 2147483649;
           |         ^
       ```
       - **新增** `extern const std::string* g_pSourceText;`（`src/lexer/token.h`）。
         `src/main.cpp` 在 `OpenSource` **成功之后、`Run()` 之前**设置 —— 词法错误就是在 `Run()`
         里报的，放早了拿不到源码（第一次误插进 `if (!OpenSource(...))` 的失败分支，一行都没打印）。
       - `src/lexer/token.cpp` 新增匿名命名空间里的 `SourceLineOf(int, std::string&)` /
         `ExpandTabs` / `PrintDiagContext(const LineInfo&)`：按 `
` 数到目标行、去掉尾部 ``、
         制表符展开成 4 空格以保证「源码行」与「`^` 行」列对齐、空行不打印。
       - **指针为空时自动跳过**，直接调 `EmitError` 的单元测试行为与从前一致。
       - **影响面**：语义基线按 `minic: error:` 前缀过滤，32 例一字未改；语法基线是整段 stderr
         逐字比对，23 个 `.err.txt` 用 `-Update` 刷新；GUI 的 `DIAG_RE` 是逐行锚定的正则，不受影响。

  35. **清两处死代码（2026-10-28，鲍方乐）**：
       - **删除** `VarEntry::m_iOffset`（`src/symbol/symbol.h`，全仓零读写，栈帧偏移实际由
         codegen 的 `AssignOffsets` 维护）与 `SymbolTable::GetSerialId()`（零调用，
         MiniC 无匿名类型/结构体）。
       - **加「零调用」标注但保留** `RecConstTbl` / `BackfillConst`：Q12 遗留，已被 `RecConst`
         取代，但在 `docs/interface.md`、设计文档 §6、数据结构设计 §4 都有签名，
         删了会让四份正式文档与代码不一致。
       - **保留不动** `CCodeGen::m_TemplateLib`（设计文档 §4.6.2 的模板库预留）与
         `SymbolTable::m_EnumInfoTbl`（项目书/PPT/README 多处写到的「七张分表」之一）。

> 说明：本节第 8–10 条为 2026-10-13 阶段⑤ 收尾时同步；**下面的"四、词法识别集合"仍是 v3.1 骨架版内容**，
> 词法 v4.0 的现状（17 个挑战版终结符等）见 `README.md` 的模块状态表与
> [词法模块待改进清单.md](词法模块待改进清单.md)，
> 本次未越界改动词法章节。

## 四、词法识别集合（骨架版已实现）

- 关键字：int / float / if / else / while / return / void
- 标识符：`[A-Za-z_][A-Za-z0-9_]*`
- 常量：整数 `[0-9]+`、浮点 `[0-9]+.[0-9]+`（登记入 m_ConstTexts，Token.m_iConstIdx 记录位序）
- 运算符：+ - * / =  < 以及 v4.0 挑战版新增的 == != <= >= && || ! ++ -- 与 `char`/`bool` 字面量、`char` 关键字符号
- 界符：( ) { } ; ,
- 结束：EOF
