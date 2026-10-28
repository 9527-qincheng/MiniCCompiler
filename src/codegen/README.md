# 目标代码生成模块（codegen/）

> 上游：`sema/`（`CodeList`）+ `symbol/`　下游：`.asm` 文件（交 `gcc -x assembler` 汇编链接）
> 规模：2 个文件 1 199 行（`codegen.cpp` 1 114 / `codegen.h` 85）

## 1. 职责

把四元式序列翻译成 **x86-64 AT&T 语法汇编**。设计取向是「正确性优先、结构简单」：

- 每个变量和临时量都分配一个固定的 `rbp` 栈槽，不做寄存器分配；
- 每条四元式独立翻译成「取操作数 → 运算 → 写回」三段，指令数量偏多但可逐条对账；
- 类型宽度由 `QuadOp` 的宽度后缀决定（`_1` → `movb`，`_4` → `movl`，`_4F` → `movss`）。

## 2. 文件

| 文件 | 职责 |
|---|---|
| `codegen.h` | `InstPattern` / `CodeLine` / `AsmOutput` / `CCodeGen` |
| `codegen.cpp` | 匿名命名空间里的 `AsmEmitter`（全部翻译逻辑）+ `CCodeGen` 对外壳 |

## 3. 对外接口

```cpp
class CCodeGen
{
public:
    explicit CCodeGen(const CodeList& codes);
    void BindSymTab(const SymbolTable& symtab);
    bool Generate(AsmOutput& out);                        // 生成汇编文本
    bool IRtoASM(const CodeList& codes, AsmOutput& out);  // 四元式 → x86 汇编
};

struct AsmOutput
{
    std::string m_FileHeader;   // 文件头注释 + .globl
    std::string m_DataSection;  // .data（浮点常量池、格式串）
    std::string m_CodeSection;  // .text（函数体）
    void Clear();
};
```

## 4. 生成流程（`AsmEmitter::Run`）

1. **收集全局量**：遍历变量表，把全局标量与数组写进 `.bss`，数组按 `ElemBytesOfType(类型) × 元素个数` 分配（`char s[4]` → `.zero 4`，`int a[4]` → `.zero 16`）。
2. **逐函数生成栈帧**：先按变量表下标升序分配局部变量槽，再分配临时量槽，`AlignUp(off, 16)` 对齐；形参按「名字 + 声明行列」回连到变量表项，在栈帧里占独立槽位。
3. **逐四元式翻译**：`GenQuad()` 按 `QuadOp` 大类 switch 分派——算术走 `GenBinary()`、比较走 `GenCompare()`、下标走 `GenIndex()`、调用走 `GenCall()`、输出走 `GenPrint()`。
4. **懒生成格式串**：`GenPrint()` 首次用到某类型时才往 `.data` 追加 `.LCfmt_i: .string "%d"` / `.LCfmt_f: "%g"` / `.LCfmt_c: "%c"`。
5. **收尾**：每个函数以 `movq %rbp, %rsp` / `popq %rbp` / `ret` 结束，返回标号 `.Lret0`、`.Lret1` …，条件标号 `.Lc1`、`.Lc2` …

## 5. 调用约定

- **用户函数调用：栈传参。** 调用前 `subq $N, %rsp`，逐个 `movl %eax, k(%rsp)`，`call f`，返回后 `addq $N, %rsp`。
- **`printf`：走 Win64 ABI。** `subq $32, %rsp` 预留影子空间，`leaq .LCfmt_i(%rip), %rcx` 传格式串，`movl ..., %edx` 传值，`movl $0, %eax`，`call printf`，`addq $32, %rsp`。浮点额外 `cvtss2sd %xmm1, %xmm1` + `movq %xmm1, %rdx`。

## 6. 类型宽度

| 类型 | 标量指令 | 数组元素指令 | 元素字节 |
|---|---|---|---|
| `int` | `movl` / `addl` / `cmpl` | `(%rax,%rcx,4)` | 4 |
| `float` | `movss` / `addss` / `ucomiss` | `(%rax,%rcx,4)` | 4 |
| `char` | `movb` / `movsbl` | `movb %dl, (%rax,%rcx,1)` | 1 |
| `bool` | `movl` | — | 4 |

**`ElemBytesOfType()` 必须与 `symbol/symbol.cpp` 的 `SizeOfType()` 保持一致**，否则栈帧分配与访问侧错位。

## 7. 测试

`tests/run_codegen_tests.ps1`——30 个用例，流程是：`minic` 出 `.asm` → 改后缀为 `.s` → `gcc -x assembler -o <exe> <s>` 链接 → 运行取 stdout 与退出码 → 与期望比对。

**不比对 `.asm` 文本**：汇编只要行为等价即可，锁死文本会让任何指令选择上的改进都无法提交。

## 8. 修改指引

- 新增 `QuadOp` 成员必须在 `GenQuad()` 里加分支；`default: return;` 会让未实现的指令**静默跳过**，症状是「编译通过但结果不对」。
- 改栈帧布局只需动 `AssignOffsets()`；改指令选择只需动 `GenBinary()` / `GenCompare()` / `GenIndex()`。
- 本模块没有做优化：`m_TemplateLib`（指令模板库）目前未填充，寄存器分配与公共子表达式消除都留给后续。
