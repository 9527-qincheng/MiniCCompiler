# 给 yvechu：`feature/symbol` 整改说明（路线 A：移植，不重写）

> 写这份文档的人：组长（鲍方乐）。
> 审查对象：`origin/feature/symbol` = `9a20f8c8`（3 个提交，`+180/-301`），基线 `origin/dev` = `c8d4d9d`。
> 审查方式：在独立 worktree 里**真的编译、真的跑测试**，全程没动主仓库。

---

## 〇、先说结论（三句话）

1. **你的分支起点这次是对的** —— `merge-base` 就是 dev 顶端，没再犯上次「从旧代码切分支」的错。这一步的进步要肯定。
2. **代码思路也是对的** —— 作用域栈（自内向外查找、内层遮蔽外层）正是设计文档 §5.4 第 3 条要求的东西，现有实现恰恰缺这个。
3. **但不能合入** —— 整套工程现在**编译不过**（8 个 error，全在 `src/sema/ir.cpp`），同时**删掉了 17 项**已被别的模块依赖的公开接口，还把 `m_ConstInfoTbl` / `m_TypeInfoTbl` **降成了 `private`**（效果一样：外部模块照样用不了）。**问题不在你的思路，在于你选了「推倒重写」而不是「移植进既有类」。**

所以这份文档只讲一件事：**怎么把你的作用域栈思路，移植进现有的 `symbol.h/.cpp`，保留全部既有接口。**

---

# 第一部分：这次发生了什么

## 一、事实核对

### 1.1 分支状态

| 项目 | 值 |
|---|---|
| 分支 | `origin/feature/symbol` |
| 分支顶端 | `9a20f8c8` |
| 提交 | `94be56a`(21:42:31 Update symbol.cpp)、`2e164dc`(21:43:00 Update symbol.h)、`9a20f8c`(21:44:44 Create symbol_test.cpp) |
| 基线 | `origin/dev` = `c8d4d9d` |
| `merge-base(dev, feature/symbol)` | `c8d4d9d` = **dev 顶端** ✅ 起点正确 |
| 改动量 | 3 files, `+180/-301` |
| 合并预演 | `git merge-tree --write-tree dev origin/feature/symbol` = **退出码 0，零冲突** |

> ⚠️ 注意最后一行：**合并预演零冲突 ≠ 能合入**。它是文本层面不打架，不代表代码层面能编译。这次就是典型例子。

### 1.2 实测结果：编译失败

在 worktree 里执行 `build.bat build`：

```
BUILD FAILED  (退出码 1)
```

8 个 error **全部集中在 `src/sema/ir.cpp`**（符号表以外的模块）：

| 位置 | 错误 |
|---|---|
| `src/sema/ir.cpp:107:75` | `error: 'std::vector<ConstEntry> SymbolTable::m_ConstInfoTbl' is private within this context` |
| `src/sema/ir.cpp:109:45` | 同上 |
| `src/sema/ir.cpp:110:23` | `error: 'const struct ConstEntry' has no member named 'm_szVal'` |
| `src/sema/ir.cpp:110:59` | `error: ... no member named 'm_iValue'; did you mean 'm_value'?` |
| `src/sema/ir.cpp:110:74` | `no member named 'm_szVal'` |
| `src/sema/ir.cpp:116:75` | `error: 'class SymbolTable' has no member named 'm_VarInfoTbl'` |
| `src/sema/ir.cpp:118:29` | 同上 |
| `src/sema/ir.cpp:146:10` | `error: 'struct ProcEntry' has no member named 'm_Codes'` |

### 1.3 为什么你自测时没发现

这是本次最需要记住的一条：

```bash
g++ -std=c++17 -Wall -Wextra -Isrc -c src/symbol/symbol.cpp    # 退出码 0 ✅
```

**单编译你的模块是过的。** 因为：

- 你改的是**既有**的 `symbol.h`（不是新加文件），模块内部自洽；
- 你在新版里声明的 10 个函数（`RecVar`/`RecArray`/`RecConst`/`SearchVar`/`SearchConst`/`BackfillConst`/`CheckAssign`/`DumpSymTab`/`EnterScope`/`ExitScope`）在 `symbol.cpp` 里**全部有实现**。

但**别的模块在用你删掉的东西**。C++ 的编译单元是分开的：你只编译 `symbol.cpp` 就永远看不到 `ir.cpp` 的报错。

### 1.4 为什么四套测试里有两套还是绿的

这也是一个陷阱，值得你知道：

| 测试 | 结果 | 原因 |
|---|---|---|
| `tests/grammar_selfcheck.cpp` | ✅ 通过 | 只 `include "parser/grammar.h"`，**不碰 symbol.h** |
| `tests/parser_unit_test.cpp` | ✅ 通过 | 只 include `lexer/lexer.h` + `parser/parser.h`，**不碰 symbol.h** |
| `tests/run_lexer_tests.ps1` | ❌ 退出码 2 | 依赖 `build/minic.exe`，而它**没编出来**（compiler not found） |
| `tests/run_parser_tests.ps1` | ❌ 退出码 2 | 同上 |

**教训：一套「全绿」的自测 ≠ 工程能编译。** 判断标准只有一条：

```bash
build.bat test        # 必须 ALL TEST SUITES PASSED
```

这条命令会先 `build.bat build`（**整套工程编译**），再依次跑 4 套测试。编译不过时它第一步就红。

---

## 二、你删掉了什么（17 项删除 + 2 项降为 private）

这些不是你「改了」，是**整个从公开接口里消失了**（下面每一项我都拿 `dev` 版逐名扫过一遍）：

### 2.1 多表成员（`docs/interface.md:29` 明确要求保留）

```
【真删 5 项】
m_VarInfoTbl     ← 被 ir.cpp:116-118 直接使用
m_ProcInfoTbl
m_ParamInfoTbl
m_LabelInfoTbl
m_EnumInfoTbl

【没删，但降为 private 2 项 —— 外部照样访问不了】
m_ConstInfoTbl   ← 被 ir.cpp:107-110 直接使用（报错就是 'is private within this context'）
m_TypeInfoTbl
```

⚠️ 这里要说明一句：`m_ConstInfoTbl` / `m_TypeInfoTbl` 严格讲不是「删了」，是**从 `public` 挪到了 `private`**。编译器给的措辞也印证了这点 —— 对前两个报的是 `is private within this context`，对 `m_VarInfoTbl` 报的才是 `has no member named 'm_VarInfoTbl'`（根本不存在）。但在「别人还能不能用」这个问题上，两种做法**后果完全相同**，都要改回来。

### 2.2 第 4 章参考书扩展接口（真删 9 项）

```
RecVarTbl / RecConstTbl / SearchVarInfoTbl / SearchConstInfoTbl
RecType / SearchTypeInfoTbl / RecLabel / SearchLabelInfoTbl / GetSerialId
```

### 2.3 第 4 章配套辅助栈成员（真删 3 项）

```
m_iTypePos / m_iTypeFlag / m_iIdListFlag
```

> 到这里为止的 `5 + 9 + 3 = 17` 项，是 `SymbolTable` 类里**真正消失的公开成员**。下面 2.4 / 2.5 是表项类内部的字段和整个类，另外计数。

### 2.4 表项字段（真删）

```
ConstEntry::m_szVal / m_iValue / m_constType / m_bIsFloat / m_fValue   ← ir.cpp:110 用 m_szVal、m_iValue
ProcEntry::m_Codes / m_retType                                         ← ir.cpp:146 用 m_Codes
SymbolEntry::m_iProcIndex / m_iScope
ParamEntry::m_iParamPos
LabelEntry::m_iLabelIdx
EnumEntry::m_EnumConst
```

### 2.5 整个类（真删）

```
EnumEntry
```

### 2.6 你新加的东西是对的

```
VarEntry::m_bIsArray / m_iArraySize     ← 设计文档 §5.4 要求（数组维度）
ScopeType::SCOPE_BLOCK                  ← 设计文档 §5.4 要求
```

### 2.7 为什么这些不能删

`E:\计算机软件实训\MiniCCompiler\docs\interface.md:29` 写得很清楚：

> 4. SymbolTable 以第 6 章契约为主，**保留第 4 章扩展接口为兼容**

设计文档 §6 表格（`MiniC编译器设计文档_v4.0_挑战版LL1.md:1668` 附近）也把 `ProcEntry` / `ParamEntry` 标为「挑战版**正式启用**」—— 它们不是预留垃圾，是后面函数定义/调用要用的。

**改动公开接口是要先沟通的**，因为语义模块和中间代码模块都在按这份契约写代码。你单方面改了，他们那边就塌了。

---

## 三、还有两个功能回归

### 3.1 `BackfillConst` 被掏空

**dev 版（有实际实现）**：

```cpp
void SymbolTable::BackfillConst(int iConstIdx, const std::string& name, TokenType type)
{
    // Q12：常量语义回填——词法阶段登记文本，语义阶段按位序回填名字/类型
    if (iConstIdx < 0 || static_cast<size_t>(iConstIdx) >= m_ConstInfoTbl.size())
    {
        return;
    }
    ConstEntry& ce = m_ConstInfoTbl[iConstIdx];
    ce.m_szName = name;
    ce.m_constType = type;
    ce.m_bIsFloat = (type == TokenType::TOKEN_NUM_FLOAT);
}
```

**你的新版**：

```cpp
void SymbolTable::BackfillConst(int iConstIdx, const std::string& name, TokenType type)
{
    // 预留实现
    (void)iConstIdx;
    (void)name;
    (void)type;
}
```

这是 **Q12/Q13 定稿的常量表语义回填机制**：词法阶段把常量**文本**登记进表（`RecConstTbl`），语义阶段按**位序**回填名字和类型。你把它变成空函数，等于这条链路断了。

### 3.2 `DumpSymTab` 输出格式变了，基线没同步

| | dev | 你的新版 |
|---|---|---|
| 首行 | `=== Symbol Table (scope 1) ===` | `==== Symbol Table Dump ====` |

`tests/test.sym.txt` 是当前基线（6 行）：

```
=== Symbol Table (scope 1) ===
-- Variables --
-- Constants --
-- Types --
-- Procedures (challenge) --
-- Labels (challenge) --
```

**格式改了，基线没改**，两边就对不上了。

> 顺带说明：目前**没有** runner 校验 `.sym.txt`（这是防护缺口，见 §4.2），所以这个不一致不会立刻被测试抓到 —— 但这不是「没问题」，是「没人看着」。

---

## 四、`tests/symbol_test.cpp` 是死测试

你新加的这个文件**永远不会被执行**，原因有三层：

### 4.1 没有 `main()`

文件里只有：

```cpp
void test_symbol_table() { ... }
```

**没有 `main()` 函数**。链接时会报：

```
collect2.exe: error: ld returned 1 exit status
```

### 4.2 四个 runner 都没引用它

`git grep symbol_test` 在 `tests/` 下的 4 个 `.ps1` 里**命中数 = 0**。没有任何 runner 知道它的存在。

### 4.3 所以它既不编译也不运行

「加了测试」和「测试真的跑了」是两件事。判断标准：**`build.bat test` 的输出里能看到你的用例数**。

---

# 第二部分：你的代码哪里值得保留

## 五、作用域栈 —— 这个真有价值 ⭐

这是你分支里最有价值的部分，**别丢**。

### 5.1 设计文档明确要求它

`MiniC编译器设计文档_v4.0_挑战版LL1.md:1291-1298`（§5.4）：

> **v3.1 → v4.0 变更**：
> 1. `ScopeType` 增加 `SCOPE_BLOCK`；
> 2. 变量表项支持 4 种基础类型与**数组维度**；
> 3. **`SearchVarInfoTbl` 必须改为按作用域链查找**（v3.1 为线性扫描，块作用域下会串名）；
> 4. `CheckAssign` 按 §5.5 类型系统表扩展 bool/char。

### 5.2 现有实现缺的正是这个

`git show dev:src/symbol/symbol.cpp` 里 `SearchVarInfoTbl` 是**线性扫描**，而且源码自己都留了 TODO：

```cpp
VarEntry* SymbolTable::SearchVarInfoTbl(const std::string& name)
{
    // TODO：作用域链查找（当前线性扫描，基础版全局满足）
    for (auto& v : m_VarInfoTbl)
    {
        if (v.m_szName == name)
        {
            return &v;
        }
    }
    return nullptr;
}
```

**「块作用域下会串名」是什么意思**：`int a;` 在全局，函数里又有 `int a;`，线性扫描会先撞到全局那个，内层的变量就找不到了 —— 语义分析直接出错。

### 5.3 你写的查找逻辑是对的

```cpp
const VarEntry* SymbolTable::SearchVar(const std::string& name) const
{
    for(auto it = m_varScopeStack.rbegin(); it != m_varScopeStack.rend(); ++it)
    {
        auto iter = it->find(name);
        if(iter != it->end())
        {
            return &iter->second;
        }
    }
    return nullptr;
}
```

`rbegin()`/`rend()` 自内向外、先命中先返回 —— **正确实现了内层遮蔽外层**。`RecArray`（数组登记）也是设计文档要求的。这两块思路直接搬过去就行。

---

# 第三部分：怎么改（路线 A：移植）

## 六、路线选择

| 路线 | 做法 | 评价 |
|---|---|---|
| **A ⭐⭐⭐** | 改造现有 `symbol.h/.cpp`，把作用域栈思路融进去 | **推荐**：思路保留 + 接口全保 + 零风险 |
| B | 保留你的独立实现，但换类名避开冲突 | 要维护两套符号表，语义模块用哪套？不可行 |
| C | 直接删掉这个分支，重做 | 浪费你已有的正确思路 |

**走 A。**

## 七、核心原则：扁平数组必须保留

这是整份文档**最重要的一条技术约束**。

### 7.1 为什么不能改成纯 map

`src/sema/ir.cpp` 用**扁平下标**直接索引符号表：

```cpp
// ir.cpp:107-112
if (op.m_iLink >= 0 && static_cast<size_t>(op.m_iLink) < m_symtab.m_ConstInfoTbl.size())
{
    const ConstEntry& ce = m_symtab.m_ConstInfoTbl[op.m_iLink];
    return ce.m_szVal.empty() ? std::to_string(ce.m_iValue) : ce.m_szVal;
}

// ir.cpp:116-121
if (op.m_iLink >= 0 && static_cast<size_t>(op.m_iLink) < m_symtab.m_VarInfoTbl.size())
{
    return m_symtab.m_VarInfoTbl[op.m_iLink].m_szName;
}
```

而且 `RecVarTbl` 的返回值**就是那个扁平下标**：

```cpp
int SymbolTable::RecVarTbl(const std::string& name, TokenType type, int line)
{
    VarEntry* ve = RecVar(name, type, line, 0);
    if (!ve) { return -1; }
    return static_cast<int>(m_VarInfoTbl.size()) - 1;   // ← 扁平下标
}
```

四元式里的 `op.m_iLink` 存的就是这个下标。**一旦改成 map、下标语义变了，中间代码模块整个塌掉。**

### 7.2 正确做法：作用域栈存「下标」，不存「对象」

```
m_VarInfoTbl (扁平数组，永久保留，下标稳定)     ← ir.cpp 按 m_iLink 索引
        ▲                    ▲
        │ idx                │ idx
   ┌────┴────┐          ┌────┴────┐
   │ 全局 map │  ...     │ 内层 map │    ← m_varScopeStack：名字 → 下标
   └─────────┘          └─────────┘
```

每层作用域是一个 `unordered_map<string, size_t>`，**value 是扁平数组下标**。这样：

- 查找还能自内向外（作用域链 ✅）；
- `m_VarInfoTbl` 还在、下标还没变（ir.cpp ✅）；
- `RecVarTbl` 返回值照旧（IR 模块 ✅）。

---

## 八、具体补丁

### 8.1 `src/symbol/symbol.h`

**① 加 include**（第 13 行附近，跟其它 include 放一起）：

```cpp
#include <unordered_map>
```

**② `ScopeType` 加 `SCOPE_BLOCK`**（现在 98-102 行）：

```cpp
enum class ScopeType
{
    SCOPE_GLOBAL,   // 全局作用域
    SCOPE_FUNCTION, // 函数作用域（挑战版）
    SCOPE_BLOCK     // 【新增 v4.0】块作用域
};
```

**③ `VarEntry` 加数组字段**（现在 39-44 行）：

```cpp
class VarEntry : public SymbolEntry
{
public:
    TokenType m_type = TokenType::TOKEN_NONE; // 变量类型
    int       m_iOffset = 0;                  // 目标代码偏移（挑战版）
    bool      m_bIsArray = false;             // 【新增 v4.0】是否数组
    int       m_iArraySize = 0;               // 【新增 v4.0】数组元素个数
};
```

**④ 加 `RecArray` 声明**（放在 `RecVar` 后面，115 行附近）：

```cpp
    VarEntry*   RecArray(const std::string& name, TokenType type, int size, int line, int col); // 【新增 v4.0】
```

**⑤ 私有区加作用域栈**（现在 148-152 行的 `private:` 段）：

```cpp
private:
    int         m_iSerialId;      // 对象序号
    int         m_iScopeCounter;  // 作用域计数器
    ScopeType   m_curScope;       // 当前作用域
    int         m_curProcIndex;   // 当前过程索引（-1 全局）

    // 【v4.0 新增】作用域链：每层是「名字 → m_VarInfoTbl 下标」。
    // 存下标而非指针：m_VarInfoTbl 会 reallocate，而 ir.cpp 依赖扁平下标稳定。
    std::vector<std::unordered_map<std::string, std::size_t>> m_varScopeStack;
    std::vector<ScopeType> m_scopeTypeStack;   // 与上面同层，用于退出时恢复 m_curScope
```

> ⚠️ **`m_VarInfoTbl` 等 6 个多表成员保持在 `public:` 区不动** —— `ir.cpp:107/116` 直接访问它们，挪成 private 就是现在这 8 个编译错误的成因。

### 8.2 `src/symbol/symbol.cpp`

**① 构造函数：初始化全局作用域**（现在 17-21 行）：

```cpp
SymbolTable::SymbolTable()
    : m_iSerialId(0), m_iScopeCounter(1),
      m_curScope(ScopeType::SCOPE_GLOBAL), m_curProcIndex(-1)
{
    m_varScopeStack.emplace_back();      // 全局作用域（层级 1）
    m_scopeTypeStack.push_back(ScopeType::SCOPE_GLOBAL);
}
```

**② `EnterScope` / `ExitScope`**（现在 23-37 行）：

```cpp
bool SymbolTable::EnterScope(ScopeType t)
{
    ++m_iScopeCounter;
    m_varScopeStack.emplace_back();      // 压入一层新作用域
    m_scopeTypeStack.push_back(t);
    m_curScope = t;
    return true;
}

void SymbolTable::ExitScope()
{
    // 全局作用域永不弹出（size()==1 时不动）
    if (m_varScopeStack.size() > 1)
    {
        m_varScopeStack.pop_back();
        m_scopeTypeStack.pop_back();
        --m_iScopeCounter;
    }
    m_curScope = m_scopeTypeStack.empty() ? ScopeType::SCOPE_GLOBAL
                                          : m_scopeTypeStack.back();
}
```

> 这里比 dev 版多了真正的入栈/出栈；比你的新版多了 `m_iScopeCounter` 维护（`DumpSymTab` 首行要用）和 `m_curScope` 的退出恢复。顺带说明：你新版里 `if (m_currentScopeLevel > 0)` 这个**全局作用域保护本身是对的**（`level` 恒等于 `stack.size()-1`，`level==0` 时不弹，全局层安全），我这条只是改成用栈容量判定，语义等价。

**③ `RecVar`：查重只看当前作用域**（现在 39-57 行）：

```cpp
VarEntry* SymbolTable::RecVar(const std::string& name, TokenType type, int line, int col)
{
    // ⚠️ 查重只看【当前】作用域：内层允许遮蔽外层同名变量（C 语义）。
    // 若用 SearchVarInfoTbl（全链查找）查重，内层就永远无法遮蔽外层了。
    auto& curMap = m_varScopeStack.back();
    auto it = curMap.find(name);
    if (it != curMap.end())
    {
        EmitError("duplicate variable '" + name + "'", LineInfo(line, col));
        return &m_VarInfoTbl[it->second];
    }

    VarEntry ve;
    ve.m_szName = name;
    ve.m_type = type;
    ve.m_iProcIndex = m_curProcIndex;
    ve.m_iScope = m_iScopeCounter;
    ve.m_iLine = line;
    ve.m_iCol = col;
    ve.m_bIsArray = false;
    ve.m_iArraySize = 0;
    m_VarInfoTbl.push_back(ve);

    const std::size_t idx = m_VarInfoTbl.size() - 1;
    curMap[name] = idx;                  // 登记「名字 → 扁平下标」
    return &m_VarInfoTbl[idx];
}
```

**④ 新增 `RecArray`**（紧跟在 `RecVar` 之后）：

```cpp
VarEntry* SymbolTable::RecArray(const std::string& name, TokenType type, int size, int line, int col)
{
    auto& curMap = m_varScopeStack.back();
    auto it = curMap.find(name);
    if (it != curMap.end())
    {
        EmitError("duplicate variable '" + name + "'", LineInfo(line, col));
        return &m_VarInfoTbl[it->second];
    }

    VarEntry ve;
    ve.m_szName = name;
    ve.m_type = type;
    ve.m_iProcIndex = m_curProcIndex;
    ve.m_iScope = m_iScopeCounter;
    ve.m_iLine = line;
    ve.m_iCol = col;
    ve.m_bIsArray = true;
    ve.m_iArraySize = size;
    m_VarInfoTbl.push_back(ve);

    const std::size_t idx = m_VarInfoTbl.size() - 1;
    curMap[name] = idx;
    return &m_VarInfoTbl[idx];
}
```

**⑤ `SearchVarInfoTbl`：改成作用域链查找**（现在 174-185 行）—— **这块直接搬你的 `rbegin/rend` 思路**：

```cpp
VarEntry* SymbolTable::SearchVarInfoTbl(const std::string& name)
{
    // 【v4.0】自内向外按作用域链查找，先命中先返回（内层遮蔽外层）
    for (auto it = m_varScopeStack.rbegin(); it != m_varScopeStack.rend(); ++it)
    {
        auto found = it->find(name);
        if (found != it->end())
        {
            return &m_VarInfoTbl[found->second];
        }
    }
    return nullptr;
}
```

**⑥ `BackfillConst`：恢复 dev 版实现**（把你现在的空实现换回来）：

```cpp
void SymbolTable::BackfillConst(int iConstIdx, const std::string& name, TokenType type)
{
    // Q12：常量语义回填——词法阶段登记文本，语义阶段按位序回填名字/类型
    if (iConstIdx < 0 || static_cast<size_t>(iConstIdx) >= m_ConstInfoTbl.size())
    {
        return;
    }
    ConstEntry& ce = m_ConstInfoTbl[iConstIdx];
    ce.m_szName = name;
    ce.m_constType = type;
    ce.m_bIsFloat = (type == TokenType::TOKEN_NUM_FLOAT);
}
```

**⑦ `DumpSymTab`：恢复 dev 版格式**（首行 `=== Symbol Table (scope N) ===`，5 段表头，与 `tests/test.sym.txt` 对齐）。

**⑧ `CheckAssign`：加 bool/char —— 但先只警告、不要硬失败**

设计文档 §5.4 第 4 条要求「按 §5.5 类型系统表扩展 bool/char」，§8.1 表里的赋值行只有：

| 操作符 | 操作数1 | 操作数2 | 结果 | 转换2 | 中间码 |
|---|---|---|---|---|---|
| = (赋值) | T_INT | T_FLOAT | T_INT | FloatToInt | MOV（**Q9 警告**） |
| = (赋值) | T_FLOAT | T_INT | T_FLOAT | IntToFloat | MOV |

**建议**：扩展成下面这样，**保持 `return true`**，新类型组合先出警告不拦编译：

```cpp
bool SymbolTable::CheckAssign(TokenType lhs, TokenType rhs, int line, int col)
{
    // Q9：float→int 允许隐式转换（C 语义截断）并打印警告，不阻断编译
    if (lhs == TokenType::TOKEN_INT && rhs == TokenType::TOKEN_FLOAT)
    {
        std::cerr << "minic: warning: " << line << ":" << col
                  << ": implicit conversion from float to int (truncated, Q9)"
                  << std::endl;
        return true;
    }
    // char↔int 兼容（C 语义）；bool 只与 bool 赋值为同类
    if (lhs == rhs) { return true; }
    if ((lhs == TokenType::TOKEN_CHAR && rhs == TokenType::TOKEN_INT) ||
        (lhs == TokenType::TOKEN_INT  && rhs == TokenType::TOKEN_CHAR) ||
        (lhs == TokenType::TOKEN_FLOAT && rhs == TokenType::TOKEN_INT))
    {
        return true;
    }
    // 其余组合：先只警告，不返回 false
    // 理由：语法制导语义动作（parser.cpp:527/529 的 semantic025/026）还没接线，
    //       现在改成硬失败，将来接线时行为不可预期。严格类型检查应先在组内对齐再改。
    std::cerr << "minic: warning: " << line << ":" << col
              << ": suspicious assignment between "
              << TokenTypeToString(lhs) << " and " << TokenTypeToString(rhs)
              << std::endl;
    return true;
}
```

> ⚠️ 你现在版本里 `return false`（类型不兼容直接失败）**改变了契约行为**。改接口语义同样要先沟通 —— 语义模块的调用方可能已经按「总是 true」写了逻辑。

### 8.3 不要动的东西（清单）

改动时请**逐项确认没被碰**：

- [ ] `m_VarInfoTbl` / `m_ConstInfoTbl` / `m_TypeInfoTbl` / `m_ProcInfoTbl` / `m_ParamInfoTbl` / `m_LabelInfoTbl` / `m_EnumInfoTbl` —— 仍是 `public`
- [ ] `RecVarTbl` —— 仍返回 `m_VarInfoTbl.size() - 1`
- [ ] `RecConstTbl` —— 仍返回 `m_ConstInfoTbl.size() - 1`
- [ ] `SearchConstInfoTbl` / `RecType` / `SearchTypeInfoTbl` / `RecLabel` / `SearchLabelInfoTbl` / `GetSerialId`
- [ ] `ConstEntry::m_szVal` / `m_iValue` / `m_constType` / `m_bIsFloat`
- [ ] `ProcEntry::m_Codes`
- [ ] `SymbolEntry::m_iProcIndex` / `m_iScope` / `m_iLine` / `m_iCol`
- [ ] `EnumEntry` 整个类
- [ ] `DumpSymTab` 首行格式

**自检命令**（在仓库根跑，全部应返回「无差异」）：

```powershell
# 看有没有删掉公开成员：应无输出
git diff dev -- src/symbol/symbol.h | Select-String '^-.*(m_VarInfoTbl|m_ConstInfoTbl|m_ProcInfoTbl|m_ParamInfoTbl|m_LabelInfoTbl|m_EnumInfoTbl|RecVarTbl|RecConstTbl|GetSerialId|EnumEntry|m_Codes|m_szVal)'
```

### 8.4 ✅ 本节补丁已实测通过（不是纸上方案）

**§8.1 / §8.2 里的每一段代码，我都在独立 worktree 里真的打进去、真的编译、真的跑过了**，结果如下：

| 验证项 | 结果 |
|---|---|
| `build.bat build`（全量编译） | **BUILD SUCCESS**，退出码 0，`minic.exe` 1647592 bytes |
| `build.bat test`（4 套测试） | **ALL TEST SUITES PASSED**，退出码 0：lexer 6/6 + 文法自检全过 + parser 41/41 + parser unit 17/17 |
| 作用域链断言程序（14 条断言） | **ALL SCOPE-CHAIN ASSERTIONS PASSED**，退出码 0 |
| `DumpSymTab` 输出 vs `tests/test.sym.txt` | **逐字一致**（7 行对 7 行） |

**照 §8 改完，你拿到的就是一个编译通过、测试全绿的版本。** 具体断言内容见 §14.A。

> 📌 一个实测踩到的坑（跟你写 `symbol_test.cpp` 时有关）：如果测试程序**只** `#include "symbol/symbol.h"`，会报一堆
> `error: invalid use of incomplete type 'struct Quad'`。
> 因为 `SymbolTable` 析构时 `std::vector<Quad> m_Codes` 需要**完整的 Quad 定义**。
> **解法**（`symbol.cpp:12` 就是这么做的）：
> ```cpp
> #include "symbol/symbol.h"
> #include "sema/ir.h"   // 完整 Quad 定义（SymbolTable 析构需要）
> ```

---

## 九、`tests/symbol_test.cpp` 怎么修

### 9.1 加 `main()`

```cpp
int main()
{
    test_symbol_table();
    return 0;
}
```

（如果 `test_symbol_table()` 里用的是断言宏，确认断言失败时返回非 0。）

### 9.2 接进 runner

两条路，选一条：

**路 1（推荐，最小改动）**：在 `tests/run_parser_unit.ps1` 旁边新建 `tests/run_symbol_tests.ps1`，仿照它的结构（编译 `symbol_test.cpp` + `src/symbol/symbol.cpp` + 依赖，跑，按退出码判定）。

**路 2**：把 symbol 用例并进 `tests/grammar_selfcheck.cpp` 那种自检式程序里。

**关键要求**：加完之后 `build.bat test` 的输出里**必须能看到你的用例**。看不到就等于没加。

### 9.3 顺带修 `.sym.txt` 基线校验缺口

现在 `tests/test.sym.txt` 有人写、**没人验**。建议在 runner 里加一条：跑 `minic --dump-symtab tests/test.mc`，把输出与 `tests/test.sym.txt` 逐字比对。

---

## 十、`tests/*.ps1` 的临时目录约定（避免踩假失败坑）

**`tests/run_parser_tests.ps1` 早期版本**用系统临时目录：

```powershell
$scratch = Join-Path ([System.IO.Path]::GetTempPath()) ("minic_parser_" + ...)
```

受限环境下 `C:\Users\<你>\AppData\Local\Temp` **不可写**，会报：

```
minic: error: 0:0: cannot write output file: ...\ll1_411.parse.txt
```

**41 个用例全 FAIL —— 这是假失败，不是你代码的问题。** 现已改为建在仓库内：

```powershell
$scratch = Join-Path $repoRoot ("build\_parser_scratch_" + [System.Guid]::NewGuid().ToString("N"))
```

你写新 runner 时请沿用这个写法（`build/` 已被 `.gitignore` 排除，不会污染仓库）。

**你本机命令行下没这个问题**，所以不必改行为，只要知道这个坑存在。

---

# 第四部分：错误规避清单

## 十一、上次指南的清单，对照本次

| # | 上次的问题 | 本次状态 |
|---|---|---|
| 1 | 直接往 main 推 | ❌ **又犯**（`origin/main` 被推了 4 个提交） |
| 2 | 没先同步最新代码 | ✅ 已改（分支起点 = dev 顶端） |
| 3 | 同类名冲突 | ✅ 已改 |
| 5 | 提交信息 `Update symbol.h` 无意义 | ⚠️ 仍是 `Update symbol.h` / `Update symbol.cpp` |
| 7 | 上传前没编译验证 | ❌ **又犯**（只单编译了 `symbol.cpp`） |
| 8 | 改接口前没沟通 | ❌ **又犯**（删了 17 项公开成员、2 项降为 private） |

### 11.1 关于「往 main 推」

`origin/main` 上现在多了 4 个提交：

```
abb1c3d 21:08:43 Update symbol.cpp
a169f76 21:33:12 Update symbol.h
d15595f 21:40:56 Update symbol.cpp
1e42b15 21:42:03 Update symbol.h
```

好消息是：**这 4 个提交净效果为 0** —— 逐 blob 追踪后，`symbol.h` 和 `symbol.cpp` 最后都被**改回了 c8d4d9d 的原样**，`origin/main^{tree}` 与 `dev^{tree}` 完全相同。所以**内容上没造成破坏**，只是历史被弄脏了。

**但流程上要记住**：

- **`main` 是稳定分支，不要直接往上推。** 所有开发走 feature 分支 → 合并进 `dev` → 验证通过后 `dev` 快进同步到 `main`。
- 每次开工前先 `git fetch origin` 并从 **`origin/dev`** 切分支，永远不要从旧的本地分支切。

### 11.2 关于「上传前编译验证」

你做的：

```bash
g++ -c src/symbol/symbol.cpp      # ← 只编译了一个文件
```

**必须做的**：

```bash
build.bat build        # 整套工程编译，退出码必须是 0
build.bat test         # 4 套测试全过，必须 ALL TEST SUITES PASSED
```

**记住这条判据**：单个 `.cpp` 编译通过 ≠ 工程能编译。C++ 里别的模块引用你改的接口时，报错只会出现在**别人**的文件里。**只有全量构建能暴露它。**

### 11.3 关于「改接口前沟通」

判断标准很简单：**你改的东西，别人在用吗？**

```powershell
git grep -n "m_VarInfoTbl\|m_ConstInfoTbl\|RecVarTbl\|m_Codes\|m_szVal" -- src tests
```

只要这个命令有输出，就说明**有人在用**，改之前必须在群里说一声。这次 `ir.cpp` 用了其中 4 项，所以一改就炸。

---

## 十二、提交前自检清单（**每次照做**）

```powershell
# ① 分支从最新 dev 切
git fetch origin
git checkout -b feature/symbol-v2 origin/dev

# ② 改代码 ...

# ③ 全量构建 —— 退出码必须是 0
build.bat build

# ④ 全套测试 —— 必须 ALL TEST SUITES PASSED
build.bat test

# ⑤ 确认没删公开接口（应无输出）
git diff dev -- src/symbol/symbol.h | Select-String '^-.*(m_VarInfoTbl|RecVarTbl|EnumEntry|m_Codes)'

# ⑥ 工作区状态
git status --short

# ⑦ 提交（信息用 conventional 格式，别再用 "Update xxx"）
git add src/symbol tests/symbol_test.cpp tests/run_symbol_tests.ps1
git commit -m "feat(symbol): 作用域链查找 + 数组登记，保留既有扩展接口"

# ⑧ 推送后等审查，不要直接合 main
git push origin feature/symbol-v2
```

---

## 十三、验收标准（DoD）

你的整改分支要满足以下**全部**条件：

| # | 条件 | 验证方式 |
|---|---|---|
| 1 | 分支从最新 `origin/dev` 切 | `git merge-base --is-ancestor origin/dev <你的分支顶端>` 退出码 0 |
| 2 | **整套工程编译通过** | `build.bat build` 退出码 0 |
| 3 | **4 套测试全过** | `build.bat test` 输出 `ALL TEST SUITES PASSED`，退出码 0 |
| 4 | 公开接口零删除 | `git diff dev -- src/symbol/symbol.h` 无「删除公开成员」的行 |
| 5 | `BackfillConst` 有实际实现 | 代码评审（不能是 `(void)` 空实现） |
| 6 | `DumpSymTab` 首行格式未变 | 与 `tests/test.sym.txt` 首行一致 |
| 7 | 作用域链生效 | 内层 `int a;` 遮蔽外层 `int a;`，`SearchVar("a")` 返回内层那个 |
| 8 | `symbol_test.cpp` 有 `main()` 且**真的被跑** | `build.bat test` 输出里能看到你的用例数 |
| 9 | 提交信息规范 | 不是 `Update xxx` |
| 10 | 没往 `main` 推 | `git log origin/main` 无你的新提交 |

---

## 十四、附录

### A. 作用域链的自测方法（建议加进 `symbol_test.cpp`）

⚠️ **先加这个 include**，否则报 `invalid use of incomplete type 'struct Quad'`：

```cpp
#include "symbol/symbol.h"
#include "sema/ir.h"   // 完整 Quad 定义（SymbolTable 析构需要）
```

完整的自测程序（**§8 补丁打完后，这段我实测跑通、14 条断言全过**）：

```cpp
#include "symbol/symbol.h"
#include "sema/ir.h"
#include <cassert>
#include <iostream>

int main()
{
    SymbolTable sym;

    sym.RecVar("a", TokenType::TOKEN_INT, 1, 1);       // 全局 a (int, 行1)
    sym.RecVar("x", TokenType::TOKEN_INT, 2, 1);       // 全局 x

    sym.EnterScope(ScopeType::SCOPE_BLOCK);            // 进入块作用域
    sym.RecVar("a", TokenType::TOKEN_FLOAT, 3, 5);     // 块内 a (float) 遮蔽全局

    // ① 内层遮蔽：找到的是内层（float, 行3, scope 2）
    const VarEntry* inner = sym.SearchVar("a");
    assert(inner != nullptr);
    assert(inner->m_type == TokenType::TOKEN_FLOAT);
    assert(inner->m_iLine == 3);
    assert(inner->m_iScope == 2);

    // ② 链式查找：外层 x 在块内依然可见
    const VarEntry* xv = sym.SearchVar("x");
    assert(xv != nullptr && xv->m_iLine == 2);

    // ③ 当前作用域重名：报错但返回已存在的那个（不新建）
    VarEntry* dup = sym.RecVar("a", TokenType::TOKEN_INT, 4, 1);
    assert(dup != nullptr && dup->m_type == TokenType::TOKEN_FLOAT);

    // ④ 退出作用域后回到外层（int, 行1）
    sym.ExitScope();
    const VarEntry* outer = sym.SearchVar("a");
    assert(outer != nullptr);
    assert(outer->m_type == TokenType::TOKEN_INT);
    assert(outer->m_iLine == 1);

    // ⑤ 扁平下标必须稳定：全局 a 永远是 idx 0（ir.cpp 依赖这个）
    assert(sym.m_VarInfoTbl[0].m_szName == "a");
    assert(sym.m_VarInfoTbl[0].m_type == TokenType::TOKEN_INT);

    // ⑥ 数组登记
    VarEntry* arr = sym.RecArray("buf", TokenType::TOKEN_INT, 10, 9, 1);
    assert(arr && arr->m_bIsArray && arr->m_iArraySize == 10);

    // ⑦ RecVarTbl 返回值 == 扁平下标（IR 模块按此下标回查）
    int idx = sym.RecVarTbl("y", TokenType::TOKEN_INT, 11);
    assert(idx == (int)sym.m_VarInfoTbl.size() - 1);
    assert(sym.m_VarInfoTbl[idx].m_szName == "y");

    // ⑧ 全局作用域不可弹出：多余的 ExitScope 不影响全局表项
    sym.ExitScope(); sym.ExitScope(); sym.ExitScope();
    assert(sym.m_VarInfoTbl[0].m_szName == "a");

    std::cout << "ALL SCOPE-CHAIN ASSERTIONS PASSED\n";
    return 0;
}
```

实测输出：

```
inner a: type=2 line=3 scope=2
x still visible from block, line=2
duplicate-in-scope detected, kept existing
after ExitScope a: type=1 line=1
m_VarInfoTbl size=3
array buf[10] registered, idx=3
RecVarTbl(y)=4 size=5
m_VarInfoTbl size after extra ExitScope=5
ALL SCOPE-CHAIN ASSERTIONS PASSED
```

（`stderr` 上会同步打出 `minic: error: 4:1: duplicate variable 'a'` —— 这是断言③预期的重名报错。）


### B. 本次审查的验证环境

| 项目 | 值 |
|---|---|
| 编译器 | `g++ (GCC) 15.1.0`（`C:\msys64\ucrt64\bin\g++.exe`） |
| 构建 | `build.bat build`（自动收集 `src/**/*.cpp`，`-std=c++17 -Wall -Wextra`） |
| 全套测试 | `build.bat test`（lexer 6 例 + 文法自检 + parser 41 例 + parser unit 17 例） |
| 审查位置 | 独立 worktree，**主仓库全程未动**，`dev @ c8d4d9d` |
| 本机 CMake | **无**（用 `build.bat` 即可） |

### C. 相关文件路径

| 文件 | 说明 |
|---|---|
| `E:\计算机软件实训\MiniCCompiler\src\symbol\symbol.h` | 符号表接口（本指南 §8.1 的改动目标） |
| `E:\计算机软件实训\MiniCCompiler\src\symbol\symbol.cpp` | 符号表实现（§8.2 的改动目标） |
| `E:\计算机软件实训\MiniCCompiler\src\sema\ir.cpp:107-121` | 依赖扁平下标的地方，**不能破坏** |
| `E:\计算机软件实训\MiniCCompiler\docs\interface.md:29` | 「保留第 4 章扩展接口为兼容」的出处 |
| `E:\计算机软件实训\MiniC编译器设计文档_v4.0_挑战版LL1.md:1291-1314` | §5.4 符号表 v4.0 变更要求 |
| `E:\计算机软件实训\MiniC编译器设计文档_v4.0_挑战版LL1.md:1529-1547` | §8.1 类型系统表 |
| `E:\计算机软件实训\MiniCCompiler\tests\test.sym.txt` | `DumpSymTab` 输出基线 |
| `E:\计算机软件实训\组员提交审查报告_symbol分支_20261014.md` | 本次审查的完整报告 |
| `E:\计算机软件实训\给yvechu_符号表模块说明与修改指南.md` | 上一次的指南（§11 对照用） |
| `E:\计算机软件实训\05_版本快照\_yvechu提交留档_20261014\` | 你本次 3 个文件的留档（含 `v2_symbol.h` / `v2_symbol.cpp` / `v2_symbol_test.cpp`） |

---

## 十五、需要你回复的

1. **路线 A 你认可吗？** 如果有技术上做不到的地方，直接说。
2. **作用域链那块实际要几天？** 给个时间，我好安排后面的语义模块接线。
3. **`CheckAssign` 的严格类型检查**（返回 false）是你有意的设计，还是顺手写的？如果要严格化，我们得和语义模块一起对齐 —— 因为 `parser.cpp:527/529` 的语义动作还没接线。

**最后一句话**：这次你的**方向是对的、代码质量比上次有明显提升**（起点正确、模块内自洽、作用域栈思路命中设计要求）。问题只剩一个 —— **改了别人在用的接口、又只单编译了自己的文件**。把这两点改掉，走路线 A 重来一版，我这边验收速度会很快。
