# 符号表模块（symbol/）

> 上游：`parser/`（AST）、`sema/`（语义分析登记）　下游：`sema/ir.cpp`、`codegen/`
> 规模：2 个文件 601 行（`symbol.cpp` 417 / `symbol.h` 184）

## 1. 职责

集中存放编译过程中所有「有名实体」的信息：变量、常量、类型、过程、形参、标号、枚举，并维护作用域栈。
它同时是**目标代码生成的唯一数据源**——栈帧分配、数组元素宽度、全局变量声明都从这里查。

## 2. 七张分表

| 表 | 元素类型 | 用途 |
|---|---|---|
| `m_VarInfoTbl` | `VarEntry` | 变量（含数组：`m_bIsArray` / `m_iArraySize`） |
| `m_ConstInfoTbl` | `ConstEntry` | 常量（保留词法原文 `m_szVal`） |
| `m_TypeInfoTbl` | `TypeEntry` | 类型（内建 5 个 + 数组类型） |
| `m_ProcInfoTbl` | `ProcEntry` | 过程/函数（含各自的四元式序列 `m_Codes`） |
| `m_ParamInfoTbl` | `ParamEntry` | 形参 |
| `m_LabelInfoTbl` | `LabelEntry` | 标号（由 IR 阶段的 `LABEL` 四元式登记） |
| `m_EnumInfoTbl` | `EnumEntry` | 枚举（挑战版预留，当前为空） |

所有表项共享基类 `SymbolEntry` 的四个字段：`m_szName` / `m_iProcIndex`（所属过程，-1 为全局）/ `m_iScope`（作用域编号）/ `m_iLine` + `m_iCol`（声明位置）。

## 3. 对外接口（节选）

```cpp
class SymbolTable
{
public:
    bool EnterScope(ScopeType t);      // 进入作用域
    void ExitScope();                  // 退出作用域

    VarEntry*   RecVar(const std::string& name, TokenType type, int line, int col);
    VarEntry*   RecArray(const std::string& name, TokenType type, int size, int line, int col);
    ConstEntry* RecConst(const std::string& name, TokenType type, double value, int line, int col);
    const VarEntry*   SearchVar(const std::string& name) const;
    const ConstEntry* SearchConst(const std::string& name) const;

    void DumpSymTab(std::ostream& os) const;   // --dump-symtab

    int         RecVarTbl(const std::string& name, TokenType type, int line);
    int         RecConstTbl(const std::string& szVal, TokenType constType);
    VarEntry*   SearchVarInfoTbl(const std::string& name);
    ConstEntry* SearchConstInfoTbl(const std::string& name);

    TypeEntry*  RecType(const std::string& name, TokenType baseType);
    TypeEntry*  SearchTypeInfoTbl(const std::string& name);
    TypeEntry*  RecArrayType(const std::string& name, TokenType baseType, int size);
    void        RecBuiltinTypes();             // 登记 int/float/bool/char/void
    static int  SizeOfType(TokenType baseType);// char=1，int/float/bool=4，void=0

    LabelEntry* RecLabel(const std::string& name, int labelIdx, int line, int col);
    LabelEntry* SearchLabelInfoTbl(const std::string& name);
};
```

## 4. 作用域

三层：`SCOPE_GLOBAL` → `SCOPE_FUNCTION` → `SCOPE_BLOCK`。
进入作用域时压栈，退出时弹栈；**变量表保留全量记录**（不随作用域退出删除），否则 `--dump-symtab` 看不到完整清单。
同名查找走「由内向外」逐层遮蔽。

## 5. `--dump-symtab` 的六节

```
-- Variables --        name : TYPE [array N]  scope S  @line:L
-- Constants --        value : TYPE = 原文
-- Types --            name : TYPE  size N  /  name : TYPE  array N  size N
-- Procedures (challenge) --   name : RET  @line L
-- Params (challenge) --       proc.param : TYPE  #N  @line L scope S
-- Labels (challenge) --       L1  @line L
```

三个实现要点：

1. **`RecBuiltinTypes()` 必须在语义分析之前调用**（`SemanticAnalyzer::Analyze` 开头），否则类型表恒空。
2. **`DumpSymTab` 必须在 IR 生成之后调用**（`src/main.cpp`），因为标号由 `LABEL` 四元式登记，早于 IR 写出会永远是空节。
3. **`ParamEntry` 没有「所属过程」字段**，`-- Params --` 节按「声明行号 ≤ 形参行号且最近的 `ProcEntry`」回连过程名。

## 6. 测试

| 脚本 | 覆盖 |
|---|---|
| `tests/run_symbol_tests.ps1` | 符号表单元断言 |
| `tests/run_symtab_tests.ps1` | `--dump-symtab` 产物逐字比对（`tests/test.sym.txt`、`tests/symtab_full.sym.txt`） |

## 7. 修改指引

- 新增表项类型：派生 `SymbolEntry` → 加表成员 → 加 `RecXxx` / `SearchXxx` → 在 `DumpSymTab` 里加一节 → 更新 `tests/*.sym.txt` 基线。
- **数组元素宽度只有一处口径**：`SizeOfType()`（符号表）与 `ElemBytesOfType()`（`codegen.cpp`）必须一致（char=1，其余=4），否则栈帧分配与访问侧会错位。
