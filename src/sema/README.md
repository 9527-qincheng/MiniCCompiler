# 语义分析与中间代码模块（sema/）

> 上游：`parser/`（AST）+ `symbol/`　下游：`codegen/`（`CodeList`）
> 规模：6 个文件 3 283 行（`ir.cpp` 1 380 / `ir.h` 439 / `semantic.cpp` 714 / `semantic.h` 99 / `optimizer.cpp` 267 / `optimizer.h` 42）

## 1. 职责

两个阶段 + 一个可选优化器：

1. **语义分析**（`SemanticAnalyzer`）：类型检查、作用域检查、实参/返回值检查、数组下标检查，产出诊断。
2. **中间代码生成**（`IRGenerator`）：把 AST 翻译成**四元式**序列（`CodeList`），并挂到符号表的过程表项上。
3. **窥孔优化**（`minic_opt::Optimize`）：`-O` 时对四元式做四轮化简。

## 2. 文件

| 文件 | 职责 |
|---|---|
| `semantic.h` / `semantic.cpp` | `SemanticAnalyzer`：两遍遍历（先收签名，再检查） |
| `ir.h` / `ir.cpp` | `QuadOp`（151 个成员）、`Operand`、`Quad`、`IRGenerator` |
| `optimizer.h` / `optimizer.cpp` | `minic_opt::Optimize` 与 `OptStats` |

## 3. 语义分析

```cpp
class SemanticAnalyzer
{
public:
    explicit SemanticAnalyzer(SymbolTable& symtab);
    void Analyze(const ASTNodePtr& root);
};
```

**必须两遍**：先 `CollectSignatures()` 收全部函数签名（因为允许调用后面才定义的函数），再 `Visit()` 逐个检查。

检查项（错误）：

- 重复声明 `duplicate variable 'a'`
- 未声明标识符 `undefined identifier b`
- 实参个数 `function 'f' expects 2 argument(s), but 3 given`
- 返回类型 `non-void function 'f' must return a value` / `void function 'g' cannot return a value`
- 数组大小 `array size must be a positive integer`
- 数组名不带下标 `'a' is an array; a subscript is required to read an element`
- 常量下标越界/为负 `array subscript 5 is out of bounds (array 'a' has 3 element(s))`
- 下标非整数 `array subscript must be an integer`

检查项（警告，`EmitWarning`，不进 `g_ErrorList`、不影响退出码）：

- 隐式转换 `implicit conversion from float to int (truncated, Q9)`
- 可疑赋值 `suspicious assignment between BOOL and INT`
- 非 void 函数完全没有 `return`
- 既无 `main` 也无顶层语句

**后两项只能做警告**：MiniC 允许不写 `main`（`codegen` 会用顶层语句合成入口，`tests/` 里有 10 个这样的用例），且 `tests/codegen/ret_semantics.mc` 专门固定「落空出口返回未定义值」的语义——做成错误会让既有基线全崩。

**`a[-1]` 的 -1 在 AST 里是 `UnOpNode(-, NumNode(1))` 不是 `NumNode`**，所以下标检查要先调 `FoldConstIndex()` 把「字面量」与「一元负号套字面量」两种形态折叠成整数。

## 4. 中间代码

```cpp
class IRGenerator
{
public:
    explicit IRGenerator(SymbolTable& symtab);
    void Generate(const ASTNodePtr& root);
    int  GetTmpVar(TokenType t);      // 申请临时变量 _T1、_T2 …
    int  GetTmpLabel();               // 申请标号 L1、L2 …
    void EmitIR(QuadOp op, const Operand& op1, const Operand& op2, const Operand& rslt);
    void EmitIR(QuadOp op, const Operand& op1, const Operand& op2, const Operand& rslt,
                const LineInfo& loc); // 携带源码位置的重载
    const CodeList& GetCodes() const;
    CodeList&       GetCodesMutable();
    std::string OperandToString(const Operand& op) const;
};
```

`QuadOp` 是 **21 大类 + 宽度后缀** 的编码（`(cat << 5) | 成员号`），共 151 个成员，MiniC 实际产生 **30 个**：

| 类别 | 例子 |
|---|---|
| 算术 | `ADD_4` / `ADD_4F` / `SUB_4` / `MUL_4` / `DIV_4F` |
| 关系 | `LS_4` / `LE_4` / `MR_4` / `ME_4` / `EQU_4` / `NEQU_4` |
| 赋值 | `ASSIGN_1` / `ASSIGN_4` / `ASSIGN_4F` |
| 下标 | `INDEX_1` / `INDEX_4` / `INDEX_4F` |
| 控制 | `LABEL` / `JMP` / `JT` / `JNT` |
| 调用 | `PARA` / `CALL` / `RETV` |
| 输出 | `PRINT_1` / `PRINT_4` / `PRINT_4F` |
| 转换 | `ITOF` / `FTOI`（MiniC 扩展，参考书表 5-2 无此大类） |

**类型转换一律显式**（`ITOF` / `FTOI`），不在算术指令里隐式做——这样目标代码只需看操作码就能决定用 `addl` 还是 `addss`。

## 5. 常量折叠

在 `EvalExpr` 的 `AST_NODE_BINOP` 分支里、**类型转换之前**插入 `FoldBinary()`。三条边界（宁可不折，也不折错）：

1. 除数为 0 → 不折；
2. 整型结果越界（超出 `[-2147483648, 2147483647]`）→ 不折；
3. `&&` / `||` → 不折（保短路语义）。

## 6. `-O` 窥孔优化

```cpp
namespace minic_opt {
struct OptStats { int iFoldedBranch, iUnreachable, iRedundantJump, iDeadTemp, iDeadLabel; int Total() const; };
void Optimize(CodeList& codes, const SymbolTable& symtab, OptStats& st);
}
```

四轮：

1. 常量条件跳转：条件为整型常量 0 时，`JNT` 改成 `JMP`，`JT` 直接删除；
2. 删不可达：块结束符之后到下一个 `LABEL` 之前的整段删除；
3. 删空跳：`JMP L` 紧跟 `LABEL L` 时删掉 `JMP`；
4. 删死临时量与死标号：先扫全段收集活跃集，再一次性删除。

> **最容易写错的一条：块结束符只能是 `Q_JMP` 与 `Q_RETV`，绝不能把 `Q_JT` / `Q_JNT` 算进去。**
> `JNT cond -> L` 的语义是「条件为假才跳」，为真时继续往下走，紧随其后的正是 `if` 的真分支。
> 把它当块结束符会删掉真分支——第一版就是这么错的，89 个可编译用例里 16 个输出变化、递归用例直接栈溢出。

## 7. 测试

| 脚本 | 覆盖 |
|---|---|
| `tests/run_sema_tests.ps1` | 27 个用例，比对诊断原文；用例首行写 `/* expect: err */` / `/* expect: warn:N */` / `/* expect: ok */` |
| `tests/run_quad_tests.ps1` | 18 个用例，比对 `--dump-quads` 产物 |

`-O` 的一致性由 `.github/workflows/ci.yml` 的冒烟步骤保证：同一份源程序分别用带/不带 `-O` 编译、链接、运行，stdout 与退出码必须一致。

## 8. 修改指引

- 新增检查项：在对应 `VisitXxx` 里调 `EmitError` / `EmitWarning` → 加一个 `tests/sema/*.mc` 用例（首行写 expect）→ 跑 `run_sema_tests.ps1 -Update` 生成基线。
- 新增 `QuadOp` 成员：`ir.h` 的 `QuadCat` 编码 → `QuadOpName` 映射 → `codegen.cpp` 的 `GenQuad` 分派分支，缺一个就会静默跳过该指令。
