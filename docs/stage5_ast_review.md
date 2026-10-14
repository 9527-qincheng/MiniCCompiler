# 阶段⑤「AST 多子节点重构」接口评审稿（v1 草案）

> ## ✅ 评审结果与实施状态（2026-10-13 收尾补记）
>
> - **D1–D9 全部按本稿的推荐默认值通过**（用户 2026-10-13 评审答复）。
> - **实施已完成**：`src/parser/ast.h`、`src/parser/ast.cpp` 全量重写；`src/parser/parser.h` / `parser.cpp`
>   去掉全部桥接节点并改用新节点；`tests/parser_unit_test.cpp` 增补新 AST 不变量断言。
> - **验证实测（全部通过）**：`cmake --build build` 零警告；文法自检 21 项全过；单元测试 17/17（新增 8 条不变量断言）；
>   回归 **41/41**；`tests/test.mc` 五个 `--dump-*` 产物齐全、退出码 0。
> - **基线影响面**：37 条 `expected/*.ast.txt` + `tests/test.ast.txt` 重定；
>   **`expected/*.err.txt` 一条未变**（错误定位与错误恢复未动）；
>   `expected/ll1_411.parse.txt`（§4.11 的 82 步轨迹）**逐字未变**。
> - 验收证据与命令见 `README.md` 的"行为变更记录"第 3 条与"下一步（TODO 路线）"阶段⑤ 条目。

| 项 | 内容 |
|---|---|
| 评审对象 | `src/parser/ast.h` / `src/parser/ast.cpp` 的**公开结构**（节点类别集合、基类字段、Print 约定） |
| 变更性质 | **破坏性（设计文档 §5.3.2 标题即标注【变更·破坏性】）**，影响语义（04）、中间代码（05）两个下游模块 |
| 发起 | 语法模块（杨呈旭 / 郭晨赞 分工线） |
| 分支 | `feature/parser-stage5`（基于 `origin/dev` = `2a46767`，工作区干净） |
| 依据 | 设计文档 v4.0 §5.3.2（第 1211–1280 行）、§9.1 对照表（第 1553 行）、§13 阶段⑤（第 1669 行）、§13 文件清单第 7 行（第 1649 行）；`02_语法分析模块` 任务书 §5.1–§5.3 与 DoD |
| 流程依据 | 设计文档 §2.4「接口冻结：改动公开接口须全员评审」 |
| 状态 | **草案已获通过并实施完毕**（2026-10-13；评审时为"尚未改动任何代码"） |
| 需签字 | 组长（鲍方乐）、语义模块、中间代码模块、语法模块内部两位成员 |

---

## 一、为什么必须先评审

1. `ast.h` 的节点类别与基类字段属**公开接口**（04 语义要按 `m_kind` 分派遍历，05 IR 要按 `children` 取操作数）。
2. `04_语义分析模块` §五 逐字要求：`关键：语法模块的阶段⑤（AST 多子节点重构）会改变节点结构，语义模块的遍历代码必须在改造完成后定稿，否则要返工。`；`05_中间代码生成模块` §五 同样写明 `Generate() 需在改造后定稿`。
3. 改造会**重刷全部 40 条 `--dump-ast` 黄金基线**，必须先把新形状定下来，否则基线会反复失效。
4. 设计文档自身在节点数量上存在 3 处不一致（见 §3.3），需要评审拍板，不能由语法模块单方面选定。

---

## 二、现状（代码实测）

### 2.1 `src/parser/ast.h`（161 行）

- `enum class ASTNodeKind` 共 **11 项**：`AST_NODE_NONE / NUM / IDENT / DECL / ASSIGN / BINOP / UNARY / IF / WHILE / RETURN / LIST`。
- `class ASTNode` 只有**两个**子指针：`std::unique_ptr<ASTNode> m_left; m_right;`（第 44–45 行），`Print(std::ostream&, int indent = 0)` 为纯虚。
- 10 个派生类：`NumNode`（`m_fValue/m_bIsFloat/m_iConstIdx`）、`IdentNode`（`m_szName/m_iSymIdx`）、`DeclNode`（`m_type/m_szName/m_iSymIdx`）、`AssignNode`、`BinOpNode`（`m_op`）、`UnaryOpNode`（`m_op`）、`IfNode`（额外 `m_else`）、`WhileNode`、`ReturnNode`、`ListNode`。
- `using ASTNodePtr = std::unique_ptr<ASTNode>;`（第 50 行）——**评审建议保持不变**。

### 2.2 `src/parser/parser.cpp` 内的桥接节点（阶段④ 占位）

`parser.cpp:176–196` 定义了 `BridgeNode : public ASTNode`：`m_kind = AST_NODE_NONE`，用字符串 `m_szLabel` 承载语义、用 `m_left/m_right` 挂子节点、另有内部标志 `m_bOpenLeft`。当前用它承载 7 种挑战版结构：

| 桥接标签 | 构造位置 | 现状子节点形状 |
|---|---|---|
| `ArrayDecl(TYPE name[N])` | `parser.cpp:628–645` | left = `DeclNode`，right = 维度 `NumNode` |
| `FuncDef(TYPE name)` | `parser.cpp:655–680` | left = 形参链（`ListNode` of Param），right = 函数体（`ListNode`） |
| `Param(TYPE name[N])` | `parser.cpp:756–764` | 无子节点（类型/名/维度全在标签字符串里） |
| `Print` | `parser.cpp:883–889` | left = 表达式 |
| `Index` | `parser.cpp:840–851`、`964–970` | left = 基址 `Ident`，right = 下标表达式 |
| `Call` | `parser.cpp:852–859`、`964–970` | left = 被调 `Ident`，right = 实参链（`ListNode`） |
| `Empty` | `parser.cpp:831–833` | 无子节点 |

后果：`--dump-ast` 里出现 `m_kind == AST_NODE_NONE` 的标签节点，**下游无法按 `m_kind` 分派**；复合语句目前也用 `ListNode` 表示（`parser.cpp:778–784`），不是独立节点类别。

---

## 三、拟定的目标接口（草案）

### 3.1 基类（拟）

```cpp
class ASTNode
{
public:
    ASTNode(ASTNodeKind kind, const LineInfo& loc) : m_kind(kind), m_loc(loc) {}
    virtual ~ASTNode() = default;

    ASTNodeKind m_kind;
    LineInfo    m_loc;
    std::vector<ASTNodePtr> m_children;   // 【变更】任意子节点

    virtual void Print(std::ostream& os, int indent = 0) const = 0;
};
using ASTNodePtr = std::unique_ptr<ASTNode>;
```

**删除 `m_left` / `m_right`。** 设计文档 §5.3.2 第 1216 行逐字：`保留 m_left/m_right 作为便捷访问器会造成二义，故统一用 children；二元运算节点约定 children[0]/children[1]`。

### 3.2 节点类别（拟：`AST_NODE_NONE` + 18 类）

| # | 常量 | 中文 | 现状对应物 |
|---|---|---|---|
| 0 | `AST_NODE_NONE` | 哨兵（不使用） | 同现状 |
| 1 | `AST_NODE_PROGRAM` | 程序根 | 现 `ParseProgram` 造的 `ListNode` |
| 2 | `AST_NODE_DECL` | 变量声明 | `DeclNode` / `ArrayDecl` 桥 |
| 3 | `AST_NODE_FUNCDEF` | 函数定义 | `FuncDef` 桥 |
| 4 | `AST_NODE_PARAM` | 形参 | `Param` 桥 |
| 5 | `AST_NODE_BLOCK` | 复合语句块 | 现 `ListNode`（case 25） |
| 6 | `AST_NODE_ASSIGN` | 赋值 | `AssignNode` |
| 7 | `AST_NODE_IF` | if / if-else | `IfNode` |
| 8 | `AST_NODE_WHILE` | while | `WhileNode` |
| 9 | `AST_NODE_PRINT` | print | `Print` 桥 |
| 10 | `AST_NODE_RETURN` | return | `ReturnNode` |
| 11 | `AST_NODE_EMPTY` | 空语句 `;` | `Empty` 桥 |
| 12 | `AST_NODE_BINOP` | 二元运算 | `BinOpNode` |
| 13 | `AST_NODE_UNOP` | 一元运算（`!` / 取负） | `UnaryOpNode`（改名） |
| 14 | `AST_NODE_NUM` | 数值常量（整型/实型/字符/布尔统一） | `NumNode` |
| 15 | `AST_NODE_IDENT` | 标识符引用 | `IdentNode` |
| 16 | `AST_NODE_INDEX` | 数组下标 | `Index` 桥 |
| 17 | `AST_NODE_CALL` | 函数调用 | `Call` 桥 |
| 18 | `AST_NODE_ARRAYDECL` | 数组声明 | `ArrayDecl` 桥（**仅 02 任务书要求，设计文档枚举里没有**，见 D2） |

### 3.3 节点语义约定（拟，与设计文档 §5.3.2 第 1262–1279 行对齐）

| 节点 | children | 附加字段 |
|---|---|---|
| `PROGRAM` | 顶层声明… + 顶层语句… | — |
| `DECL` | — | `m_type`、`m_szName`、`m_iArraySize`（0 = 非数组） |
| `FUNCDEF` | 形参… + 函数体（`BLOCK`） | `m_retType`、`m_szName` |
| `PARAM` | — | `m_type`、`m_szName`、`m_iArraySize` |
| `BLOCK` | 块内声明与语句 | — |
| `ASSIGN` | `[目标, 表达式]`（目标为 `IDENT` 或 `INDEX`） | — |
| `IF` | `[条件, then]` 或 `[条件, then, else]` | — |
| `WHILE` | `[条件, 循环体]` | — |
| `PRINT` | `[表达式]` | — |
| `RETURN` | `[表达式]` 或 无子节点（空 return） | — |
| `EMPTY` | — | — |
| `BINOP` | `[左, 右]` | `m_op`（TokenType） |
| `UNOP` | `[操作数]` | `m_op`（`TOKEN_NOT` / `TOKEN_MINUS`） |
| `NUM` | — | `m_fValue`、`m_bIsFloat`、`m_isChar`、`m_isBool`、`m_iConstIdx`（Q12 常量表位序） |
| `IDENT` | — | `m_szName`、`m_iSymIdx`（语义回填） |
| `INDEX` | `[基址, 下标]` | — |
| `CALL` | `[被调标识符, 实参…]` | — |
| `ARRAYDECL` | `[DECL, 维度 NUM]`（D2 备选：不用此类别） | — |

---

## 四、Print 输出格式（评审焦点：全部黄金基线随之变化）

`--dump-ast` 的文本是回归基线的比对对象，**格式必须一次定死**。拟定为：

- 统一格式 `名称(附加信息)  @行:列`，子节点按 `children` 顺序、缩进 +1 层（每层 2 空格），与现状风格一致。
- 各节点文本（拟）：`Program`、`Decl(INT a)`、`FuncDef(INT f)`、`Param(INT x)`、`Block`、`Assign`、`If`、`While`、`Print`、`Return`、`Empty`、`BinOp(PLUS)`、`UnOp(NOT)`、`Num(1)` / `Num(2.500000)`、`Ident(a)`、`Index`、`Call`、`ArrayDecl(INT a[10])`。
- **序列不再产生 `List` 节点**：`PROGRAM` / `BLOCK` 直接用 children 表达序列（D3）。

两种实现方式（D7）：
- **A**：17 个派生类各自实现 `Print`（现状风格，改动面大、易格式漂移）。
- **B（推荐）**：基类统一实现 `Print`（打印 `Label()` + children 递归），派生类只提供 `virtual std::string Label() const`；`Print` 签名不变。代码量小、格式天然一致。

---

## 五、对下游模块的影响与迁移示例

| 下游 | 现状写法 | 迁移后 |
|---|---|---|
| 04 语义 | `if (n->m_left) visit(n->m_left); if (n->m_right) visit(n->m_right);` | `for (auto& c : n->m_children) visit(c);` |
| 04 语义 | `IfNode::m_else` 单独取 else | `n->m_children.size() == 3` 时 `children[2]` 为 else |
| 04 语义 | 按 `AST_NODE_UNARY` 分派 | 改 `AST_NODE_UNOP` |
| 04 语义 | `ListNode` 遍历 | 序列已是 `PROGRAM` / `BLOCK` 的 children，直接遍历 |
| 05 IR | `AssignNode` 取 `m_left/m_right` | `children[0]` / `children[1]` |
| 05 IR | `BinOpNode::m_op` + `m_left/m_right` | `m_op` + `children[0]` / `children[1]`（文档明确约定） |
| 05 IR | 数组下标/调用靠桥接标签字符串无法识别 | 按 `AST_NODE_INDEX` / `AST_NODE_CALL` 分派，`children` 即 `[基址, 下标]` / `[被调, 实参…]` |
| 05 IR | 函数定义 | `FUNCDEF`：`children[0..n-2]` 为 `PARAM`，`children[n-1]` 为 `BLOCK` |

对齐窗口建议：**ast.h 与 parser 适配合并为一次提交（同一天内）**，下游在其后各自改遍历；窗口内下游代码不定稿（与其任务书 §五 要求一致）。

---

## 六、测试与基线重定计划

| 项 | 影响 | 处理 |
|---|---|---|
| `tests/parser/expected/*.ast.txt`（40 条） | 全部形状变化 | 用 `pwsh -File tests/run_parser_tests.ps1 -Update` 重刷，随后 `git diff --stat` 逐条抽查 |
| `tests/test.ast.txt`（legacy 固定校验，被脚本逐字比对） | 变化 | 同批重刷并复核 |
| `tests/parser/expected/ll1_411.parse.txt` | **不应变化**（分析轨迹只依赖文法与分析栈） | `-Update` 明确不刷新该文件；改造后复验仍与设计文档 §4.11 的 82 步逐行一致 |
| `tests/parser_unit_test.cpp` | 断言基于旧结构（`ListNode`、`m_left`、NUM 计数等） | 按新结构更新断言；**新增**「AST 中不存在 `AST_NODE_NONE` 节点」断言（证明桥接已消除） |
| 文法自检（21 项） | 不受影响 | 复跑确认 |

---

## 七、验收标准（映射 `02_语法分析模块` DoD）

1. `ast.h` 增加 PROGRAM / FUNCDEF / PARAM / BLOCK / PRINT / INDEX / CALL / EMPTY 等节点；
2. `ASTNode` 改为 `m_children` 多子节点，解析器内部桥接节点**全部**合并进 `ast.h`（`parser.cpp` 中不再有 `BridgeNode`）；
3. 40 条回归用例期望输出按新 AST 形状重新生成并通过；
4. `ll1_411.parse.txt` 仍与设计文档 §4.11 的 82 步逐行一致；
5. 语法分析结果不受影响（仅 AST 结构变化；`HasError()/GetErrors()` 行为不变）；
6. 与语义、IR 模块的接口对齐已完成（本稿获签字）。
7. 附加自检：`cmake --build build` 与 `g++ -Wall -Wextra` 零错误零警告。

---

## 八、待评审决策点（请逐条给出结论）

| 编号 | 决策点 | 备选 | 语法模块建议 |
|---|---|---|---|
| **D1** | 节点类别总数 | 16 / 17 / 18 类 | **18 类**（设计文档枚举 17 类 + 任务书要求的 ARRAYDECL）。设计文档两处提「20 类」（§5.3.2 第 1217 行、§9.1 第 1553 行）与一处提「新增 8 类」（§13 第 1649 行），与本枚举不一致，建议按枚举为准并在 README 冲突记录登记 |
| **D2** | 数组声明用哪种表示 | (a) 只用 `DECL.m_iArraySize`，无 ARRAYDECL；(b) `ARRAYDECL = [DECL, 维度 NUM]` | **(b)**：`02_语法分析模块` §5.2 明确要求 ARRAYDECL；且维度字面量的 `m_iConstIdx` 需要保留（semantic004 登记维度、单元测试的常量计数）。`DECL.m_iArraySize` 仍同步填写，两个来源都满足 |
| **D3** | 序列如何表示 | (a) 保留 `AST_NODE_LIST`；(b) 删除，用 PROGRAM/BLOCK 的 children | **(b)**：设计文档枚举中无 LIST |
| **D4** | `AST_NODE_UNARY` 是否改名 `UNOP` | 改名 / 保留 | **改名**，与设计文档一致（下游同步改） |
| **D5** | `IF` 的 else 表示 | (a) `children.size()==2/3`；(b) 保留 `m_else` 字段 | **(a)**：设计文档第 1270 行约定「else 缺省时为 2 个子节点」 |
| **D6** | `NUM` 字段集合 | 文档 4 字段 + `m_iConstIdx` | **`m_fValue/m_bIsFloat/m_isChar/m_isBool/m_iConstIdx`**（保留常量表位序，Q12） |
| **D7** | `Print` 实现方式 | A 各派生类自实现 / B 基类统一 + `Label()` | **B**（格式一致、代码少）；`Print` 签名不变 |
| **D8** | 是否保留 `m_left/m_right` 兼容访问器 | 保留 / 删除 | **删除**（文档明确：保留会造成二义） |
| **D9** | 改造窗口 | — | ast.h + parser 适配一次合入；下游随后改遍历；`dev` 合入前先在此分支完成全部验证 |

---

## 九、批准后的实施计划

1. 本稿按评审结论修订为 v2 并归档；
2. `ast.h` / `ast.cpp` 重写（一次提交，提交信息 `refactor(parser): ...`）；
3. `parser.cpp` 删除 `BridgeNode` 与 `AttachBase`/`m_bOpenLeft` 机制，`ReduceProduction` 改为写 `m_children`；
4. `tests/parser_unit_test.cpp` 更新断言并新增「无 NONE 节点」断言；
5. 重刷 40 条 AST 基线 + `tests/test.ast.txt`，复跑：构建零警告、单元测试全过、回归 41/41、`ll1_411.parse.txt` 与 §4.11 逐行一致、文法自检 21 项；
6. 更新 `README.md`（模块状态、冲突记录第 8 条「AST 重构完成」）与 `docs/interface.md`（v3.1 → v4.0 的 AST 章节）；
7. 推送 `feature/parser-stage5` 并发 PR 合入 `dev`（需具备 GitHub 凭据者在终端执行；本机此前无凭据、push 需人工）。

---

## 十、当前状态

- 已按六书 §4.1 从 `origin/dev` 创建分支 `feature/parser-stage5`（未推送）。
- 本文件为工作区新增文件，**尚未提交**；`ast.h` / `parser.cpp` 一行未改。
- 等评审结论（尤其 D1–D9）后进入实施。
