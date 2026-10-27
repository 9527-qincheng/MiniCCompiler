# 给 yvechu：符号表模块说明与修改指南

> **致**：yvechu
> **来自**：第 5 组 组长（鲍方乐）
> **日期**：2026-10-12
> **附件**：你的原稿留档（`symbol_table.h` / `symbol_table.cpp` / `main_test.cpp`）
> **定位**：本文档合并了「问题说明」与「操作指南」，一份看完即可动手。

---

## 〇、先说三句实话

1. **你的符号表代码是能用的、思路是对的。** 我实测过——问题**不在你的实现质量**，在**接入方式**。
2. **你的「作用域栈」设计恰好补上了项目现有实现的一个短板**（详见第三节）。这部分**很值得保留**。
3. 这次需要你**重做一遍接入流程**，不是因为代码差，而是**集成方式需要调整**。

下面把来龙去脉讲清楚，你照着第四节做就行。

---

# 第一部分：问题说明

## 一、发生了什么

### 1.1 项目里已经有一个符号表了

`src/symbol/` 目录下**本来就有**一套符号表，是项目骨架自带的：

| 文件 | 里面装的类 |
|---|---|
| **`symbol.h`** / **`symbol.cpp`** | `class SymbolTable` ← **早已存在** |

**注意一个容易看混的地方**：

- 文件名是 **`symbol.h`**（没有 `_table`）
- 但里面的**类名**叫 **`SymbolTable`**

### 1.2 你新加的文件

| 文件 | 里面装的类 |
|---|---|
| **`symbol_table.h`** / **`symbol_table.cpp`** | `class SymbolTable` ← **同名** |

**问题来了**：

```
symbol.h        →  里有 class SymbolTable
symbol_table.h  →  里也有 class SymbolTable
                    ↑ 两个同名类
```

### 1.3 为什么这会导致整个工程编译不过

项目的 `CMakeLists.txt` 用的是自动收集：

```cmake
file(GLOB_RECURSE MINIC_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
```

`GLOB_RECURSE` 会**把 `src/` 下所有 `.cpp` 全部编译进来**，于是：

```
symbol.cpp       定义 SymbolTable::SymbolTable() / EnterScope() ...
symbol_table.cpp 也定义 SymbolTable::SymbolTable() / enterScope() ...
                 ↑ 同一个类的同一个构造函数，被定义了两遍
```

**链接器直接报错**（重复定义），整个 `minic.exe` **构建失败**。

**我实测的证据**：

```
9 个源文件【单独编译】→ 全部通过 ✅
整体【链接】           → collect2.exe: error: ld returned 1 exit status ❌
只链接这两个符号表文件 → 同样失败 ❌
```

> 打个比方：两个人各交了一份标题都叫《项目报告》的文件，装订时机器不知道用哪一份，直接卡住了。
> **你的报告本身没问题，是"标题撞了"。**

### 1.4 你的 4 个问题汇总

| 序号 | 位置 | 问题 | 后果 |
|---|---|---|---|
| **1** | `src/symbol/symbol_table.h` | 类名 `SymbolTable` 与 `symbol.h` 撞名 | 🔴 **整个工程链接失败** |
| **2** | `src/symbol/symbol_table.h/.cpp` | 未遵守 §7.3 冻结接口 | 🔴 语义/IR 模块无法对接 |
| **3** | `tests/main_test.cpp` | `#include "symbol_table.h"` 路径错 | 🔴 该文件编不过 |
| **4** | `tests/main_test.cpp` | 有 `main()`，与 `src/main.cpp` 冲突 | 🔴 两者无法同时链接 |

---

## 二、两套接口的对照

### 2.1 接口对比

| 项目现有 `symbol.h`（设计文档 §7.3 冻结） | 你的 `symbol_table.h` |
|---|---|
| `bool EnterScope(ScopeType t)` | `void enterScope()` |
| `void ExitScope()` | `void exitScope()` |
| `VarEntry* RecVar(name, type, line, col)` | `bool insertVar(const VarInfo&)` |
| `const VarEntry* SearchVar(name)` | `VarInfo* lookupVar(name)` |
| `ConstEntry* RecConst(name, type, value, line, col)` | `bool insertConst(const ConstInfo&)` |
| `const ConstEntry* SearchConst(name)` | `ConstInfo* lookupConst(name)` |
| `bool CheckAssign(lhs, rhs, line, col)` | （缺） |
| `void DumpSymTab(std::ostream&)` | （缺） |
| `void BackfillConst(...)` | （缺） |

### 2.2 为什么接口不能随便改

设计文档 §2.4 写着：

> **模块接口冻结，变更需全员评审**

这不是卡你，而是有实际原因：**语义分析和中间代码两个模块，是照这套接口来调用的**。接口一变，那两个模块就得跟着改，容易乱。

所以接口**最好保持现有的**。

### 2.3 一个技术细节：返回类型

你的 `lookupVar` 返回 `VarInfo*`（可写指针），现有 `SearchVar` 返回 `const VarEntry*`（只读）。

**只读更安全**——查符号不该顺手改符号。实现时请沿用现有的只读约定。

---

## 三、你的代码有什么值得保留的

说这些不是客套，是我实际读代码后的判断。

### 3.1 作用域栈设计 —— **这个真有价值**

你的做法：

```cpp
std::vector<std::unordered_map<std::string, VarInfo>> varTableStack;
int currentScope;

// 查找时从内层向外层遍历
for (auto it = varTableStack.rbegin(); it != varTableStack.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) return &(found->second);
}
```

**为什么好**：这符合 C 语言的**块作用域**语义——内层同名变量应遮蔽外层。

```c
int a;              // 外层 a
{
    int a;          // 内层 a，遮蔽外层
    a = 1;          // 改的是内层
}
a = 2;              // 改的是外层
```

### 3.2 而这恰好是现有实现**缺的**

现有 `symbol.cpp` 的查找是这样的，⚠️ **注意标注的问题**：

```cpp
VarEntry* SymbolTable::SearchVarInfoTbl(const std::string& name)
{
    // TODO：作用域链查找（当前线性扫描，基础版全局满足）
    for (auto& v : m_VarInfoTbl) {      // ← 线性扫全部，不认作用域
        if (v.m_szName == name) return &v;
    }
    return nullptr;
}
```

**现有的变量表是扁平数组，没有作用域链**——内层同名变量会查错。我审查时也把「作用域链查找」列为待办。

**→ 你的思路正好补上这个洞。这是你这轮工作最有价值的部分。**

### 3.3 其他可保留的点

| 你的做法 | 评价 |
|---|---|
| 查重报错清晰（`variable 'x' redefined in current scope`） | 好，建议沿用 |
| `lastError` / `clearError()` 错误状态管理 | 思路干净 |
| 覆盖 `int/float/char/bool` 四种类型 | 与挑战版要求一致 ✅ |

---

# 第二部分：操作指南

## 四、你需要怎么做

### 4.0 路线选择

| 路线 | 做法 | 推荐度 |
|---|---|---|
| **A（推荐）** | **改造现有 `symbol.h/.cpp`**，把你的作用域栈思路融进去 | ⭐⭐⭐ |
| **B** | 保留你的独立实现，但**改类名**（如 `SymbolTableV2`） | ⭐ |
| **C** | 直接删掉，不做符号表 | ❌ 浪费你的工作 |

**推荐 A**：既有价值，又不冲突。

---

### 4.1 第 1 步：切分支（**千万别直接在 main 上改**）

```bash
cd <你的仓库路径>\MiniCCompiler

git fetch origin
git checkout -b feature/symbol origin/dev
```

> **关键**：从 **`origin/dev`** 切，**不要从 `main` 切**。
> 原因：`main` 现在是很旧的初始骨架，`dev` 才是最新成果。

---

### 4.2 第 2 步：把作用域栈移植进现有实现

**不要新建 `symbol_table.h/.cpp`**，改 `src/symbol/symbol.h` 和 `symbol.cpp`。

**三条铁律**：

1. **类名保持 `SymbolTable` 不变**（语义/IR 模块已在用它）
2. **公开接口签名保持不变**（`EnterScope` / `RecVar` / `SearchVar` / `CheckAssign` / `DumpSymTab`）
3. **只改内部数据结构**，引入作用域层级

**参考改法**（示意，你按实际情况调整）：

```cpp
// symbol.h —— 改造内部结构
class SymbolTable
{
public:
    // ... 公开接口签名保持原样，不要改 ...

private:
    // 【改进】扁平数组 → 分层结构，实现作用域链
    std::vector<std::vector<VarEntry>> m_ScopeStack;
    // 或：std::vector<std::unordered_map<std::string, VarEntry>> m_ScopeStack;
};
```

```cpp
// symbol.cpp —— 查找改为自内向外
const VarEntry* SymbolTable::SearchVar(const std::string& name) const
{
    // 从最内层作用域往外找（你的思路）
    for (auto it = m_ScopeStack.rbegin(); it != m_ScopeStack.rend(); ++it) {
        // 内层找到即返回（同名变量内层遮蔽外层）
        ...
    }
    return nullptr;
}
```

> **注意**：现有 `symbol.h` 里已有一堆成员（`m_VarInfoTbl`、`m_ConstInfoTbl`、`m_TypeInfoTbl` 等）。
> 改造时**不要破坏现有功能**——建议先跑通现有测试，再加新特性。

---

### 4.3 第 3 步：修掉测试文件的三个问题

| 问题 | 现状 | 改法 |
|---|---|---|
| **头文件路径** | `#include "symbol_table.h"` → 编译报 `No such file` | 文件在 `src/symbol/` 下，改成 `#include "symbol/symbol.h"` |
| **`main()` 冲突** | 与 `src/main.cpp` 的 `main()` 重复 | 改名（如 `test_symbol_table()`），**不要带 main** |
| **`system("pause")`** | Windows 专有，破坏可移植性 | 删掉，改用返回值表示结果 |

**建议**：只想验证功能的话，写成不带 `main()` 的测试函数：

```cpp
// tests/symbol_test.cpp
#include "symbol/symbol.h"
#include <cassert>

void test_symbol_table()
{
    SymbolTable st;
    st.EnterScope(ScopeType::SCOPE_GLOBAL);
    st.RecVar("a", TokenType::TOKEN_INT, 1, 1);
    assert(st.SearchVar("a") != nullptr);
    assert(st.SearchVar("notexist") == nullptr);
    // ...
}
```

---

### 4.4 第 4 步：本地编译验证（**必须过这一关**）

```bash
g++ -std=c++17 -Wall -Wextra -Isrc -o build/minic.exe \
  src/lexer/token.cpp src/lexer/lexer.cpp \
  src/parser/ast.cpp src/parser/parser.cpp \
  src/symbol/symbol.cpp src/sema/ir.cpp \
  src/codegen/codegen.cpp src/main.cpp
```

**验收标准**：

- ✅ 编译**零错误、零警告**
- ✅ `./build/minic.exe tests/test.mc --dump-symtab` 能正常产出
- ✅ **没有新增 `.cpp` 导致链接失败**

> **如果你确实新增了 `.cpp`**，务必再确认 CMake 也能构建：
> ```bash
> cmake -S . -B build && cmake --build build
> ```
> 因为 `GLOB_RECURSE` **会自动收集所有 `.cpp`**——多一个文件就可能撞名。这是本次问题的根源，务必重视。

---

### 4.5 第 5 步：提交（走规范流程）

```bash
git add src/symbol/symbol.h src/symbol/symbol.cpp
git commit -m "feat(symbol): 引入作用域栈，实现按作用域链查找变量"
git push origin feature/symbol
```

然后到 GitHub 上**发起 Pull Request → 合入 `dev`**。

**提交信息规范**：

| 前缀 | 用途 |
|---|---|
| `feat:` | 新功能 |
| `fix:` | 修 bug |
| `docs:` | 文档 |
| `refactor:` | 重构 |

> ❌ 不要用 `Add files via upload`——看不出改了什么。

---

### 4.6 第 6 步：等审查合入

| 步骤 | 谁做 |
|---|---|
| 发起 PR → `dev` | 你 |
| 审查（编译 + 功能验证） | 组长 |
| 合入 `dev` | 组长 |
| 后续统一推进 `main` | 组长 |

---

## 五、这次的问题出在哪（错误规避清单）

### 5.1 分支操作规范

```
main  ← 稳定主干（只接收合入，不直接改）
  ↑
 dev  ← 集成分支（日常开发汇总）
  ↑
feature/xxx  ← 组员各自的功能分支（在这里开发）
```

**正确流程**：

```
① git checkout -b feature/symbol origin/dev    ← 从 dev 切分支
② 开发和本地验证
③ git push origin feature/symbol
④ GitHub 上发起 PR → 合入 dev
⑤ 组长审查后合并
```

### 5.2 ⚠️ 错误规避清单（**下次务必避免**）

| 序号 | 错误做法 | 本次后果 | 正确做法 |
|---|---|---|---|
| **1** | ❌ **直接往 `main` 分支 push** | 干扰主干，需回退 | ✅ 从 `origin/dev` 切 `feature/xxx`，提 PR |
| **2** | ❌ **没有先 `git pull` 同步最新代码** | 从旧版 `main` 拉代码，把 `dev` 的成果**覆盖成旧版** | ✅ 动手前先 `git fetch && git checkout -b feature/symbol origin/dev` |
| **3** | ❌ **新增了与现有类同名的类**（`SymbolTable`） | 整个工程**链接失败** | ✅ 先看 `src/` 下有没有同类；改造既有类，或改个新类名 |
| **4** | ❌ **用网页上传文件**（Add files via upload） | 容易整目录覆盖；提交信息无意义 | ✅ 用 `git add / commit / push` |
| **5** | ❌ **提交信息写 "Add files via upload"** | 看不出改了什么，无法追溯 | ✅ 用 `feat:` / `fix:` / `docs:` 语义化格式 |
| **6** | ❌ **测试文件带 `main()` 且 `#include` 路径不对** | 编译/链接失败 | ✅ 测试不带 `main`；包含路径用 `symbol/symbol.h` |
| **7** | ❌ **没在本地编译验证就上传** | 问题留到集成时才暴露 | ✅ 上传前跑一遍 `g++ -Wall -Wextra`，**零警告**再提交 |
| **8** | ❌ **没确认改动是否影响他人模块** | 接口一变，语义/IR 模块跟着乱 | ✅ 改公开接口前先在群里说一声 |

### 5.3 提交前的自检清单（建议每次照做）

```
□ 我在 feature/xxx 分支上（不是 main / dev）
□ 我基于 origin/dev 最新代码（不是旧版）
□ 我新增的类/函数名，在 src/ 下没有同名冲突
□ 公开接口没有擅自修改
□ 本地编译零错误、零警告
□ 程序能正常运行（--dump-tokens / --dump-ast / --dump-symtab）
□ 提交信息是语义化格式（feat:/fix:/docs:/refactor:）
□ 我没有删除别人的文件
```

---

## 六、关于这次 main 重置（需要你知道）

**`main` 分支已被重置到初始状态**（`6bcfb0c`），是我执行的（经组长确认）。

**原因**：你上传时基于的是旧版 `main`，把 `dev` 上已有的成果（parser 实现、词法功能）**覆盖成了旧版**。为了让主干恢复干净、也让你能按规范重做，我们把 `main` 回退了。

**你的代码已完整留档，一个字符都没丢**：

```
05_版本快照\_yvechu提交留档\
    ├── symbol_table.h
    ├── symbol_table.cpp
    └── main_test.cpp
```

（另外还有 git 备份标签 `backup/main-before-reset-20261012-215313`，指向你原来的提交 `988b35d`。）

**如果你本地还保留着那两个提交**，请先同步：

```bash
git fetch origin
git reset --hard origin/main
```

或者干脆重新克隆一份。

> **说明**：这件事**不是说你写得不好**，而是流程上需要调整。你的思路我们已经记下，等按规范接入后就能进主干。

---

## 七、需要你回复的

1. **是否同意路线 A**（改造现有 `symbol.h/.cpp`，融入你的作用域栈）？
   - 若选路线 B（保留独立实现），请给个**新类名**
2. **作用域栈**这部分打算怎么接入？
   - 我的建议：**你先出一版改造方案**，我们在群里过一下接口再动手
3. **还想做哪些功能**？（设计文档里预留了数组维度、函数参数表、标号表——都适合接在这一块）

有疑问随时找我，也可以把改造方案发出来一起看。

---

## 附录 A：验证方法（供你自查）

```bash
# 1. 编译
g++ -std=c++17 -Wall -Wextra -Isrc -o build/minic.exe \
  src/lexer/token.cpp src/lexer/lexer.cpp \
  src/parser/ast.cpp src/parser/parser.cpp \
  src/symbol/symbol.cpp src/sema/ir.cpp \
  src/codegen/codegen.cpp src/main.cpp

# 2. 运行
./build/minic.exe tests/test.mc --dump-tokens --dump-ast --dump-symtab

# 3. 若走 CMake（验证 GLOB 收集正常）
cmake -S . -B build && cmake --build build
```

**通过标准**：编译零错误零警告；三份产物 (`*.token.txt` / `*.ast.txt` / `*.sym.txt`) 正常生成。

---

## 附录 B：本次审查的验证环境

| 项 | 内容 |
|---|---|
| 编译器 | g++ (MinGW)，`-std=c++17 -Wall -Wextra` |
| 验证方式 | 从提交导出完整源码树 → 逐文件编译 + 整体链接 |
| 结果 | **9 个源文件单独编译全通过；整体链接失败**（`ld returned 1 exit status`） |
| 结论 | 问题在**类名重复定义**，非代码质量问题 |
