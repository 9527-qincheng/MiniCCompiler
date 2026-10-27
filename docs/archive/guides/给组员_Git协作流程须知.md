# MiniC 编译器项目 —— Git 协作流程须知

**生效日期**：2026-10-14
**适用对象**：全体组员
**发布人**：组长

---

## 一、最重要的一条

从今天起，**`main` 和 `dev` 两条分支都被锁定，任何人都不能直接 `git push` 上去。**

所有代码只能这样进入主干：

```
你自己的分支  →  Pull Request（PR）  →  组长审核批准  →  合并进 dev
```

原因是之前已经发生过**两次覆盖事故**：
- 有组员从过期的旧分支开发后上传，把别人的成果覆盖成了旧版本；
- 有组员把**编译不通过**的代码直接推进了仓库。

GitHub 的分支保护已经从服务器层面把这条路堵死了。你如果直接推送，会看到：

```
remote: error: GH006: Protected branch update failed for refs/heads/dev.
remote: error: Changes must be made through a pull request.
 ! [remote rejected] dev -> dev (protected branch hook declined)
```

**这个报错不是你的 Git 坏了，也不是网络问题**，是保护规则在正常工作。

---

## 二、你该怎么做（标准流程）

### 第 1 步：切自己的分支，且必须从最新代码切

```bash
git fetch origin
git checkout -b feature/你的模块 origin/dev    # ← 关键：origin/dev，不是本地旧的
```

> ⚠️ **这一步做错，就会重演覆盖事故。**
> 永远从 `origin/dev` 切分支，不要从你本地的旧分支切。

### 第 2 步：本地写完，**先编译，再跑测试**

```bash
build.bat list      # ← 先看这个！确认 File count 把你自己新增的 .cpp 算进去了
build.bat build
build.bat test
```

**三条都必须看，才能提交。** 只编译自己那个 `.cpp` 是不够的——
你要确认**整个工程**能编译（`g++ -c 单个文件` 通过 ≠ 工程能链接）。

> 🔴 **`build.bat list` 的 `File count` 是最重要的一行。**
> `build.bat` 只自动收集 `src/**/*.cpp`。**如果你的新文件不在 `src/` 下，
> 编译器根本看不到它**——此时 `build.bat build` 会报 `BUILD SUCCESS`、
> `build.bat test` 会报 `ALL TEST SUITES PASSED`，**全是假绿灯**。
> 判据很简单：新增了 `.cpp`，`File count` 就必须变大。

### 第 3 步：提交（写清楚提交信息）

```bash
git add <你改的文件>
git commit -m "feat(symbol): 增加块作用域栈查找"
```

提交信息请写**做了什么**，不要用 `Update symbol.cpp` 这种没信息量的格式。

### 第 4 步：推送你自己的分支（这一步不受保护限制）

```bash
git push -u origin feature/你的模块
```

### 第 5 步：在 GitHub 上开 Pull Request

打开 <https://github.com/9527-qincheng/MiniCCompiler>，点 **Pull requests → New pull request**：

- **base（合并到）** 选 **`dev`** —— ⚠️ **不要选 main**
- **compare（来自）** 选你刚推的分支
- 标题和正文写清楚：改了哪些文件、为什么改、**是否动了别人的接口**

### 第 6 步：等组长审核

组长会：
1. 在本地把你的分支拉下来**实际编译 + 跑测试**；
2. 检查你有没有动到公共接口（如 `symbol.h`、`ast.h`、语法树的对外契约）；
3. 批准并通过 PR 合并。

**在组长批准之前，PR 不会被合并。** 合并后组长会把 `dev` 同步到 `main`。

---

## 三、几条硬规矩

| # | 规矩 | 原因 |
|---|---|---|
| 1 | **不许直接推 `main` 或 `dev`** | 服务器已锁定，推也推不上去 |
| 2 | **PR 的 base 必须是 `dev`**，不是 `main` | `main` 是发布快照，只由组长从 `dev` 同步 |
| 3 | **分支必须从 `origin/dev` 切** | 防止拿旧代码覆盖别人的成果 |
| 4 | **上传前必须 `build.bat list` + `build.bat build` + `build.bat test` 全绿** | 仓库没有 CI，编译不过没人拦得住，只能靠你自己 |
| 5 | **改公共接口前先在 PR 里说明** | `symbol.h` / `ast.h` / `docs/interface.md` 是多方约定，不能单方面改 |
| 6 | **不要在仓库里放个人沟通文档** | 仓库文档是给项目用的，沟通件私下发 |
| 7 | **不要提交生成物** | `build/` 已在 `.gitignore` 里；`tests/*.token.txt` 等基线只有**确认行为变更**后才刷新 |
| 8 | 🔴 **不要用 GitHub 网页版 `Add files via upload`** | 见下方专门说明 |

---

## 三之二、🔴 为什么不能用网页版「Add files via upload」

**症状**：网页拖拽上传后，文件**全部落在仓库根目录**，而不是你想要的 `src/xxx/`；
`build.bat build` 报成功、`build.bat test` 报全过，但**你的代码根本没被编译**；
而且分支基准永远是 `main`（不是 `dev`），落后一大截。

**原因**：网页上传是"把文件丢进仓库根目录然后提交"，它

- **不保留你本地的目录结构**（除非你在网页上手动新建文件夹，很容易忘）；
- 基于仓库**默认分支 `main`** 建分支 —— 而不是最新的 `dev`；
- 提交信息固定为 `Add files via upload`，`committer` 是 `GitHub <noreply@github.com>`；
- **你没法在提交前本地预检**，所以假绿灯永远发现不了。

**正确做法**：在本地用 git 命令行（`git clone` 一次，之后 `git checkout -b ...` + 编辑器保存），
这样文件会老老实实落在你建的目录里，且提交前能跑 `build.bat list` 自查。

```bash
git clone https://github.com/9527-qincheng/MiniCCompiler.git
cd MiniCCompiler
git fetch origin --prune
git checkout -b feature/你的模块 origin/dev
# ...用编辑器把文件保存到 src/xxx/ 下...
build.bat list && build.bat build && build.bat test
git add -A && git commit -m "feat(xxx): 做了什么" && git push origin feature/你的模块
```

> 真实案例：有位同学连续两轮（`SHB6667-patch-1` 与 `SHB6667-patch-2`）都走网页上传，
> 两个分支的 tree hash **完全相同**（`2cbc95d6…`），6 个文件**始终在仓库根目录**，
> 45 个编译错误**一条没少**——因为文件从来没进过编译器。
> 换 git 命令行后这个问题会立刻消失。


## 四、特别提醒：别删接口

**在改任何模块的头文件之前，先问自己三个问题：**

1. 这些函数/成员，**别的模块有没有在用**？（用 `git grep "名字" src/ tests/` 查）
2. 我删掉之后，**整个工程还能编译吗**？（`build.bat build` 实测）
3. 如果确实要改接口，**我在 PR 里说清楚了吗**？

> 教训：有同学的符号表改造删掉了 17 项公开成员，其中 `m_VarInfoTbl`、
> `m_ConstInfoTbl`、`ConstEntry::m_szVal`、`ProcEntry::m_Codes` 等
> **正被 `src/sema/ir.cpp` 直接使用**，导致工程编译失败
> （8 处 `error`，全是 `is private within this context` /
> `has no member named`）。
> 他自己的 `symbol.cpp` 单独编译是通过的，所以没发现——
> **单文件编译通过不代表工程能编译。**

改造的正确姿势是**在现有类里做增量改造**，而不是推倒重写一个新类。

---

## 五、常用命令速查

```bash
# 看远程现在是什么状态
git fetch --all --prune
git ls-remote origin

# 从最新 dev 切分支
git checkout -b feature/xxx origin/dev

# 构建 + 全套测试（上传前必做）
build.bat build
build.bat test

# 同步你的分支到最新 dev（在 PR 之前做，减少冲突）
git fetch origin
git merge origin/dev

# 查看自己分支相对 dev 有哪些提交
git log --oneline origin/dev..HEAD

# 查看自己改了哪些文件
git diff --stat origin/dev
```

---

## 六、遇到问题怎么办

| 现象 | 原因 | 处理 |
|---|---|---|
| `GH006: Protected branch update failed` | 你在推 `main` 或 `dev` | 推你自己的分支，然后开 PR |
| `non-fast-forward` / 推送被拒 | 远程有你没有的提交 | `git fetch` 后 `git merge origin/dev` |
| `build.bat test` 个别用例 FAIL | 你的改动影响了行为 | 先确认是有意变更，再刷新对应基线 |
| 测试报 `cannot write output file: ...\ll1_411.parse.txt` | 系统 `%TEMP%` 不可写 | 脚本已改为用仓库内 `build\_*_scratch`；若仍报，把 `$env:TEMP` 指向仓库内目录 |
| 不确定能不能合 | —— | **先开 PR**，组长来判 |

---

**一句话**：从 `origin/dev` 切分支 → 本地编译测试全绿 → 推自己的分支 → 对 `dev` 开 PR → 等组长批准。
