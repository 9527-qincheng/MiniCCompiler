
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
     - **覆盖范围**：`+ - * /` 与六个关系运算符；两侧都是 `OP_CONST` 才折叠。
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
