# 致 lexer-dev：feature/lexer 分支整改说明

> **发出日期**：2026-10-13
> **分支**：`feature/lexer`（`689f83f`）
> **结论**：**代码质量合格，可以合入**；但有 **3 处提交物**需要先整改。
> **审查方式**：独立 worktree 检出 → 实际编译 → 逐项实测（未改动你的分支）

---

## 一、先说结论：你的代码是好的

两处修复我都**实际编译并验证**过，扎实、有注释、有效果：

| 修复 | 我的实测 |
|---|---|
| 跳过 UTF-8 BOM（`lexer.cpp:89-96`） | ✅ 造了个带 BOM 的 `int a;`（前 3 字节 `EF BB BF`）→ 编译退出码 **0**，不再报错。并已确认 `origin/dev` 上**没有**这段逻辑，是真修复 |
| 多字符常量不产 token（`ScanCharConst()`） | ✅ `char c = 'ab';` → 报 `multi-character char literal`，**不再产出伪 token**，`if (!ok) return;` 生效 |

`token.h` 的注释订正也没问题。

**所以下面说的都是"顺手带进来的东西"，不是你的代码有问题。别慌。**

另外好消息：**你的分支合入 `origin/dev` 零冲突**，我预演过。虽然 `merge-base = a372c26`（落后 13 个提交），但落后的全是 parser/构建/文档，**不碰 `src/lexer/`**，所以**不会覆盖任何人的工作**。

---

## 二、必须整改的两项 🔴

### 整改 1：两个黄金基线文件混进了 PowerShell 报错

#### 问题

`tests/bad_char.token.txt`（21 行）和 `tests/bad_op.token.txt`（20 行）里，
**开头若干行不是 token，是 PowerShell 的报错信息**。
下面以 `bad_char`（21 行，噪声 9 行）为例：

```
.\test_lexer.exe : minic: error: 2:10: multi-character char literal     ← 第 1 行
所在位置 行:8 字符: 5                                                     ← 第 2 行
+     .\test_lexer.exe "tests\$f.mc" > "tests\$f.token.txt" 2>&1        ← 第 3 行
+     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~        ← 第 4 行
+ CategoryInfo          : NotSpecified: (minic: error: 2...er char literal:String) [], RemoteException
+ FullyQualifiedErrorId : NativeCommandError                            ← 第 6 行
                                                                        ← 第 7 行（空行）
minic: error: 3:10: multi-character char literal                        ← 第 8 行
minic: error: 4:12: unknown escape sequence '\q'                        ← 第 9 行
2:1  CHAR  char                                                         ← 第 10 行：真正的 token 从这里才开始
```

`bad_op`（20 行）结构一样，但**噪声是 8 行**（少一行 stderr，只有
`minic: error: 3:3: unexpected character '|'` 这 1 行），真实 token 从第 9 行开始。

#### 三个具体的害处

1. **混入了机器相关路径** `.\test_lexer.exe` —— 换台机器、换个 shell、换个工作目录，这行就必然对不上。
2. **混入了 PowerShell 本地化文案** —— `所在位置 行:8 字符: 5`、`CategoryInfo`、`NativeCommandError`
   都是 PowerShell 的错误壳，不是你的程序输出。
3. **stderr 被当成 stdout 存了** —— 噪声区里那几行 `minic: error: ...` 本该进 stderr，
   不该混进 token 基线（`bad_char` 混了 2 行、`bad_op` 混了 1 行）。

#### 根因

你生成基线时用了：

```powershell
.\test_lexer.exe "tests\$f.mc" > "tests\$f.token.txt" 2>&1
#                                                     ^^^^ 问题在这里
```

`2>&1` 把 **stderr 重定向并入了 stdout**。当程序退出码非 0 时（你这些是**故意**的坏例），
PowerShell 会把原生命令的 stderr 包装成 `NativeCommandError` 记录一起写进去。

#### 好消息：token 内容本身是对的

我逐行比对过——**去掉噪声后的 token 内容，与程序真实输出逐字一致**。
所以**不用重写内容，只要把噪声行删掉就行**（`bad_char` 删前 9 行、`bad_op` 删前 8 行，见下方命令）。

#### 怎么修（推荐做法：stdout 与 stderr 分开）

**方式 A —— 删掉噪声行**（最快，注意**必须先读入变量再写回**）：

```powershell
# ⚠️ 不能写成 Get-Content file | ... | Set-Content file —— 同一管道里读写同一文件会报
#    "The process cannot access the file ... because it is being used by another process"
#    正确做法：先全部读进变量，再写回。

# bad_char: 21 行，噪声 9 行 → 删后 12 行
$p = 'tests\bad_char.token.txt'
$c = Get-Content $p -Encoding UTF8
$c | Select-Object -Skip 9 | Set-Content $p -Encoding UTF8

# bad_op: 20 行，噪声 8 行 → 删后 12 行   ★注意这里是 8，不是 9
$p = 'tests\bad_op.token.txt'
$c = Get-Content $p -Encoding UTF8
$c | Select-Object -Skip 8 | Set-Content $p -Encoding UTF8
```

> ⚠️ **两个文件噪声行数不同**：`bad_char` 是 9 行、`bad_op` 是 8 行。
> 删完**都应是 12 行**（每例的真实 token 恰好都是 12 行）。删完请核对行数。

**方式 B —— 重新生成（推荐，最省事）**：

程序本身就会把产物写成**与源文件同目录**的 `<源文件名>.token.txt`，
所以**根本不需要 `>` 重定向**，直接跑就行：

```powershell
.\build\minic.exe tests\bad_char.mc --dump-tokens    # 自动覆盖：tests\bad_char.token.txt
.\build\minic.exe tests\bad_op.mc   --dump-tokens    # 自动覆盖：tests\bad_op.token.txt
```

> ✅ **我已实测**：这样跑完，两个文件都从 21/20 行变成正确的 **12 行**，
> 且与程序输出**逐字一致**，噪声全部消失。
>
> ⚠️ **千万不要写 `> tests\bad_char.token.txt`**：shell 会**先**把该文件截断为 0 字节，
> 而程序随后又要往同一个路径写产物——你这次的根因正是重定向引起的，**直接不用重定向最安全**。

> 📌 **注意你分支上没有 `build.bat`**：它是你切分支之后才在 dev 上加的
> （你的 `merge-base = a372c26`，落后 13 个提交）。
> 所以 `.\build\minic.exe` 这个路径在你当前分支上**还不存在**。两个办法：
>
> 1. **推荐**：先同步最新 dev（`git fetch origin && git merge origin/dev`），
>    这样既拿到 `build.bat`，也顺带解决了"落后 13 个提交"的问题；
> 2. 或者自己用 g++ 编译一个（`build.bat` 里第 78 行的命令）：
>    ```powershell
>    mkdir build
>    g++ -std=c++17 -Wall -Wextra -Isrc -o build\minic.exe (Get-ChildItem src -Recurse -Filter *.cpp).FullName
>    ```

**方式 C —— 想要重定向就用 Start-Process 显式分流**（等价于 parser 测试脚本的做法）：

```powershell
Start-Process -FilePath .\build\minic.exe `
    -ArgumentList @('tests\bad_char.mc','--dump-tokens') `
    -Wait -NoNewWindow `
    -RedirectStandardOutput out.txt `
    -RedirectStandardError  err.txt     # stderr 单独一个文件，绝不混入产物
```

#### 正确的基线内容（我已用你的分支编译实测取得，可直接对照）

**`tests/bad_char.token.txt`（12 行）**：

```
2:1  CHAR  char
2:6  IDENT  a
2:8  ASSIGN  =
3:1  CHAR  char
3:6  IDENT  b
3:8  ASSIGN  =
3:14  SEMI  ;
4:1  CHAR  char
4:6  IDENT  c
4:8  ASSIGN  =
4:14  SEMI  ;
5:1  EOF  <eof>
```

**`tests/bad_op.token.txt`（12 行）**：

```
2:1  IDENT  a
2:5  IDENT  b
2:6  SEMI  ;
3:1  IDENT  a
3:5  IDENT  b
3:6  SEMI  ;
4:1  IDENT  a
4:3  LT  <
4:4  LT  <
4:6  IDENT  b
4:7  SEMI  ;
5:1  EOF  <eof>
```

> 注：这两个用例退出码是 **1**（故意设计的错误用例），这是正常的。

#### 对照组：同批另外三个基线是干净的

所以这是个局部失误，不是系统性问题：

| 文件 | 首行 | 状态 |
|---|---|---|
| `tests/bad_char.token.txt` | `.\test_lexer.exe : minic: error: ...` | ❌ 需修 |
| `tests/bad_op.token.txt` | `.\test_lexer.exe : minic: error: ...` | ❌ 需修 |
| `tests/lex_challenge.token.txt` | `2:1  BOOL  bool` | ✅ 干净 |
| `tests/lex_char.token.txt` | `2:1  CHAR  char` | ✅ 干净 |
| `tests/lex_op_edge.token.txt` | `2:1  IDENT  a` | ✅ 干净 |

---

### 整改 2：删掉过期设计文档 `docs/v4/设计文档v4.md`

#### 问题

你在这次提交里新建了 `docs/v4/` 目录，放进 3 个文件。其中一个**是过期版本**：

| 仓库内路径 | 大小 | 情况 | 判断 |
|---|---|---|---|
| `docs/v4/设计文档v4.md` | 84102 | 这是 **v4.0**；本地早已更新到 **v4.0.1**（94668 字节） | 🔴 **过期，会误导人** |
| `docs/v4/文法及表格.xlsx` | 212987 | blob `1baab5c9a5` 与本地 `MiniC挑战版文法及其相关表格.xlsx` **完全相同** | 🟡 重复 |
| `docs/v4/词法改造任务书.md` | 16636 | blob `425dea0096` 与本地 `词法模块改造任务书.md` **完全相同** | 🟡 重复 |

#### 为什么过期文档最危险

v4.0.1 相对 v4.0 修正了这些内容：

- §13 中遗留的**绝对路径** → 改为相对路径表述
- §9.1 增加**「数量口径说明表」**
- 补充说明**「语法树工具不纳入 C++ 仓库」**

**仓库里放一份 v4.0，下次谁照它做，就会踩已经修正过的坑。**

#### 怎么修

**推荐做法：整个 `docs/v4/` 删掉。**

仓库 `docs/` 现在只有 `docs/interface.md`，保持这个状态就够了——
设计文档、任务书、表格属于**交付物**，不进代码仓库（这也是 v4.0.1 自己写明的主张）。

```powershell
git rm -r docs/v4
git commit -m "chore(docs): 移除误提交的 v4 文档副本（设计文档已过期为 v4.0）"
```

**如果组长要求文档入库**，那么：
1. **必须换成 v4.0.1**，不能用仓库里这份 v4.0；
2. 文件名与本地保持一致，`设计文档v4.md` 这种名字容易和 v4.0.1 混淆，建议
   `MiniC编译器设计文档_v4.0.1_挑战版LL1.md`；
3. 表格 `.xlsx` 是二进制、212KB、且与本地重复，**建议不要入库**。

---

## 三、建议整改的三项 🟡

### 建议 1：`.editorconfig` 第 11 行语法有误

```ini
[*.mc]
charset = utf-8-bom = false      ← 这一行非法
indent_style = tab
```

`charset` 的**合法取值只有**：`latin1` / `utf-8` / `utf-8-bom` / `utf-16be` / `utf-16le`。
`utf-8-bom = false` 不在其中，**编辑器解析器会告警并忽略这整行**。

看你的意图应该是「`.mc` 文件不要带 BOM」，正确写法就是：

```ini
[*.mc]
charset = utf-8
indent_style = tab
```

顺带一提：`.editorconfig` 里 `[*]` 段已写 `charset = utf-8`，而 `[*.mc]` 段是想覆盖它——
直接写 `charset = utf-8` 即可，语义一致且合法。

### 建议 2：补一个词法测试脚本，让 5 个基线"活"起来

**现状**：你新增了 5 个 `.token.txt` 基线，但**仓库里没有任何词法测试运行脚本**：

```
tests/ 下没有 run_lexer*.ps1
全仓库含 "test_lexer" 字样的文件只有：bad_char.token.txt、bad_op.token.txt（就是那两处噪声）
```

从噪声里能看出，你是在**仓库外自己编了个 `test_lexer.exe`** 手工跑的——
**脚本没入库，这 5 个基线就无法自动验证**，只能靠人工比对。

**这正是整改 1 能溜进来的原因**：如果有自动比对脚本，混进 9 行噪声立刻就会 FAIL。

**建议**：参照已有的 `tests/run_parser_tests.ps1` 写一个 `tests/run_lexer_tests.ps1`，
做到：自动编译 → 逐例跑 → 比对基线 → 输出 `PASS/FAIL` 与统计 → 非零退出码表示失败。

已有的 parser 脚本可以直接抄结构（它已经把"跑例 → 比对 → 统计 → 退出码"都写好了）。

> 小提示：parser 脚本用系统 `%TEMP%` 建临时目录，在**受限环境**下会因写不了
> `C:\Users\...\Temp` 而**全部假失败**。建议你在脚本里把临时目录建在**仓库内**
> （比如 `build/_test_scratch/`），既避开权限问题，也方便出问题时翻现场。

### 建议 3：下次从最新 dev 切分支

这次 `merge-base = a372c26`，落后 `origin/dev` **13 个提交**。

**这次没出事**（落后的提交不碰 `src/lexer/`，合并零冲突），但这是运气好。
**流程上建议**：

```powershell
git fetch origin
git checkout -b feature/xxx origin/dev     # 从最新 dev 切，不要从旧分支切
```

---

## 四、整改后的提交建议

```powershell
# 0.（推荐）先同步最新 dev —— 既拿到 build.bat，又解决"落后 13 个提交"
git fetch origin
git merge origin/dev

# 1.（推荐）重新生成两个基线 —— 直接跑，不要重定向
#    ← 已实测：跑完自动变成正确的 12 行，与程序输出逐字一致
.\build\minic.exe tests\bad_char.mc --dump-tokens
.\build\minic.exe tests\bad_op.mc   --dump-tokens

#    若你暂时不想同步 dev（分支上没有 build.bat），就手工编译再跑：
#    mkdir build
#    g++ -std=c++17 -Wall -Wextra -Isrc -o build\minic.exe (Get-ChildItem src -Recurse -Filter *.cpp).FullName
#
#    或者用「方式 A」直接删噪声行（bad_char 删前 9 行、bad_op 删前 8 行）：
#    $p='tests\bad_char.token.txt'; $c=Get-Content $p -Encoding UTF8; $c | Select-Object -Skip 9 | Set-Content $p -Encoding UTF8
#    $p='tests\bad_op.token.txt';   $c=Get-Content $p -Encoding UTF8; $c | Select-Object -Skip 8 | Set-Content $p -Encoding UTF8

# 2. 核对行数（两个都应是 12 行）
(Get-Content tests\bad_char.token.txt).Count
(Get-Content tests\bad_op.token.txt).Count

# 3. 确认噪声已清干净（以下四条都应无输出）
git grep -l 'test_lexer'
git grep -l 'CategoryInfo'

# 4. 删掉过期文档目录
git rm -r docs/v4

# 5. 修 .editorconfig（charset = utf-8-bom = false → charset = utf-8）

# 6. 提交（已推送的分支用 --force-with-lease，别用 --force）
git add -A
git commit -m "fix(lexer): 清理基线中的 PowerShell 噪声，移除过期的 v4 文档副本，修正 .editorconfig"
git push --force-with-lease origin feature/lexer
```

---

## 五、checklist

整改完请自查：

- [ ] `tests/bad_char.token.txt` 首行是 `2:1  CHAR  char`（**不是** `.\test_lexer.exe : ...`）
- [ ] `tests/bad_op.token.txt` 首行是 `2:1  IDENT  a`
- [ ] 两个文件内容与第二节给出的正确内容**逐字一致**
- [ ] 全文搜索 `test_lexer` 已无命中
- [ ] 全文搜索 `CategoryInfo` / `NativeCommandError` / `所在位置` 已无命中
- [ ] `docs/v4/` 已删除（或 `设计文档v4.md` 已换成 v4.0.1）
- [ ] `.editorconfig` 中 `charset` 取值合法
- [ ] 编译仍然通过（`build.bat build`，或手工 g++）
- [ ] `src/lexer/` 下的代码**没有**被改动（那部分本来就对）

> 自查命令（在仓库根目录跑）：
>
> ```powershell
> # 两个基线都应是 12 行，且首行应为 token 而不是报错
> (Get-Content tests\bad_char.token.txt).Count      # → 12
> (Get-Content tests\bad_op.token.txt).Count        # → 12
> Get-Content tests\bad_char.token.txt -TotalCount 1  # → 2:1  CHAR  char
> Get-Content tests\bad_op.token.txt   -TotalCount 1  # → 2:1  IDENT  a
>
> # 以下四条都应「无输出」
> git grep -l 'test_lexer'
> git grep -l 'CategoryInfo'
> git grep -l 'NativeCommandError'
> git grep -l '所在位置'
> ```

---

## 六、一句话

**你的 BOM 修复和多字符常量修复都很棒，直接合；**
**但顺手提交的 2 个基线文件（混了 PowerShell 报错）和 1 个过期设计文档（v4.0）得先清掉，**
**再顺手补个词法测试脚本，以后这类问题就不会再溜进来了。**
