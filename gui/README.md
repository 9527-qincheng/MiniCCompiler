# MiniC Studio — MiniC 编译器图形界面

MiniC Studio 是 MiniC 编译器的桌面 IDE，基于 **Electron + React + Vite + Monaco Editor + ECharts**。
它把「写代码 → 编译 → 查看各阶段产物 → 运行程序看输出」串在同一个窗口里，
既适合课堂演示，也适合日常调试编译器本身。

## 功能亮点

- **专业暗色 IDE**：石墨底 + 青色强调，自带浅色主题，可随时切换。
- **Monaco 编辑器**：MiniC 语法高亮、关键字补全与悬停说明、括号配对、诊断波浪线、多标签、内置格式化。
- **8 个可视化页签**：词法、语法树、符号表、中间码、目标码、LL(1)、运行、诊断。
- **真实产物驱动**：所有页签都来自 `minic.exe` 的 `--dump-*` 产物，不是模拟数据。
- **LL(1) 单步演示**：完整 105 条产生式、分析栈/剩余输入/动作三栏表格，可按步骤播放。
- **四元式 + CFG**：中间码表格按操作分类着色，并自动划分基本块、绘制控制流图。
- **AST 树**：可折叠语法树，支持节点详情与源码行定位。
- **诊断联动**：编译错误在编辑器里标红，点击诊断卡跳到对应源码行。
- **完整运行链路**：`minic` 生成汇编 → `gcc -x assembler` 链接 → 执行，捕获 stdout/stderr 与耗时。
- **示例库 + 语法手册**：20 条可运行示例、10 章 68 条语法速查，均可一键插入编辑器。

## 启动

### 方式一：项目根目录一键启动（推荐）

双击项目根目录的 `一键配置并启动.bat`，脚本会检查 g++、Node.js/npm、Electron 与
`build\minic.exe`，缺什么补什么，然后启动 MiniC Studio。

### 方式二：只启动 GUI

```bat
cd gui
npm install       :: 首次运行安装依赖（Electron、React、Vite、Monaco、ECharts）
启动GUI.bat       :: 自动构建前端并启动
```

`启动GUI.bat` 每次启动都会先执行 `npm run build` 生成最新前端，再把 Electron 运行时
部署到 `%LOCALAPPDATA%\minic-gui\runtime`，最后启动窗口。

也可以手动：

```bat
cd gui
npm start
```

> `npm start` = `npm run build && electron . --no-sandbox --disable-gpu`。
> 需要先执行 `build.bat build` 生成 `build\minic.exe`。

## 界面结构

```
┌ 标题栏：MiniC Studio · 当前文件 · 最小化/最大化/关闭 ┐
├ 工具栏：新建 打开 保存 | 示例库 语法手册 | 编译(F7) 运行(F5) | -O 字号 主题 ┤
├ 左：源码编辑器（多标签）      │ 右：检查器（8 个页签）                   ┤
├ 状态栏：编译状态 · 耗时 · Token 数 · 诊断数 · 光标位置 · 编码 · 主题     ┘
```

编辑器与检查器之间的分隔条可以拖动，宽度会记住。

## 八个页签

| 页签 | 内容 |
|---|---|
| **词法** | Token 统计卡、类型分布环形图、可按类型筛选/搜索的完整 Token 表；点击行定位源码 |
| **语法树** | 可折叠 AST 树、节点详情（类型/行号/子节点数）、节点数/深度统计；点击节点跳到源码行 |
| **符号表** | 按作用域分栏展示变量、常量、类型、过程、形参、标号；支持搜索 |
| **中间码** | 四元式表格按算术/关系/逻辑/数组/调用等分类着色；基本块划分与 CFG 控制流图 |
| **目标码** | x86-64 汇编高亮、指令/伪指令/寄存器统计、寄存器使用分布 |
| **LL(1)** | 预测分析全过程：105 条产生式、分析栈/剩余输入/动作表格，支持上一步/下一步/播放 |
| **运行** | 编译 → 链接 → 运行终端，显示 stdout/stderr、编译/链接/运行耗时与退出码 |
| **诊断** | 错误/警告卡片，含行:列、消息、源码片段；点击跳转到编辑器对应位置 |

## 常用操作

| 操作 | 快捷键 |
|---|---|
| 新建标签页 | `Ctrl+N` |
| 打开文件 | `Ctrl+O` |
| 保存 | `Ctrl+S` |
| 编译 | `F7` |
| 运行 | `F5` |
| 语法手册 | `F1` |
| 格式化 | `Shift+Alt+F` |

- **优化开关**：工具栏「优化 -O」切换 `minic.exe -O`，会影响中间码与目标码产物。
- **LL(1) 分析**：首次切到 LL(1) 页签会按需附加 `--dump-parse` 重新编译；大文件的分析过程可能较大。
- **导出产物**：词法 / 语法树 / 符号表 / 中间码 / 目标码页签右上角的导出按钮可将当前产物保存为 `.txt` / `.asm` 文件。
- **主题/字号**：工具栏右侧切换深浅主题与 12–20 号字，设置会自动记住。

## 示例库与语法手册

- **示例库**：20 条源码，分 6 组（基础、类型、函数、数组与控制流、综合、错误演示），
  全部在真实工具链上验证过退出码与输出。支持一键载入为新标签页。
- **语法手册**：10 章 68 条，标注 `✓ 支持` / `✗ 不支持` / `⚠ 有限制` / `· 说明`；
  支持多关键词搜索、目录跳转、一键插入编辑器。

## 目录结构

```
gui/
├── main.js                 # Electron 主进程：窗口、IPC、截图验收钩子
├── preload.js              # 通过 contextBridge 暴露 window.minicAPI
├── lib/
│   └── compiler.js         # 调用 minic.exe / gcc 的编译与运行调度
├── renderer/               # 新版 React 前端（Vite 项目根）
│   ├── index.html
│   ├── vite.config.mjs
│   └── src/
│       ├── main.jsx        # React 入口、Monaco 初始化
│       ├── App.jsx         # 应用状态、编译/运行、快捷键、页签调度
│       ├── components/     # 标题栏、工具栏、编辑器、Modal、Toast、图表等
│       ├── panels/         # 8 个检查器页签
│       ├── lib/            # Monaco 配置、产物解析、格式化、颜色
│       ├── data/           # 示例库与语法手册数据
│       └── styles/app.css  # 完整设计系统
├── src/                    # 旧版前端（保留用于旧回归测试）
└── test/                   # compile-smoke / book-smoke / sample-smoke
```

## 工作原理

1. 渲染进程（React）不直接访问文件系统或执行程序，只调用 `window.minicAPI`。
2. `preload.js` 通过 IPC 把请求转给主进程 `main.js`。
3. `lib/compiler.js` 负责把源码写入 `%TEMP%\minic-gui`，调用 `minic.exe` 生成
   `.token.txt` / `.ast.txt` / `.sym.txt` / `.quad.txt` / `.asm`（可按需生成 `.parse.txt`）。
4. 前端 `lib/parseArtifacts.js` 把这些文本解析成表格、树、图与诊断列表。
5. 点击「运行」时，`compiler.js` 继续调用 `gcc -x assembler` 链接并执行生成的 exe，超时 5 秒。

> 由于本机原生程序在含中文的路径下文件 I/O 会失效，`compiler.js` 会把 `minic.exe`
> 复制到 `%TEMP%\minic-gui` 后再执行；Electron 运行时也部署到 `%LOCALAPPDATA%\minic-gui\runtime`。

## 构建与截图验收

```bat
cd gui
npm run build                      :: 只构建前端到 renderer\dist
npm start                          :: 构建 + 启动
npm run start:fast                 :: 不构建，直接用现有 dist 启动
```

截图模式（用于自动化验收）：

```bat
npm run shot -- --shot=build\shot.png --tab=quad --theme=dark --compile
```

支持参数：`--shot=<png>`、`--theme=dark|light`、`--tab=token|ast|sym|quad|asm|parse|run|diag`、
`--menu=book|samples`、`--compile`、`--break`、`--run`、`--parse`、
`--multifile`、`--diagjump`、`--fontsize=<n>`、`--sample=<id>`、`--diagidx=<n>`。
截图完成后会打印 `SHOT_SAVED <path>` 并以 0 退出。

## 测试

```bat
cd gui
npm test              :: compile-smoke：产物齐全、Token 格式、错误诊断格式
npm run test:book     :: 语法手册数据层 + 旧渲染层
npm run test:samples  :: 示例库数据层 + 20 条真实工具链实跑
npm run test:run      :: 编译→链接→运行链路、崩溃与超时处理
```

四套测试都不需要 Electron 窗口；`test:run` 会真实调用
`minic.exe` → `gcc` → 生成的 exe。旧版 `gui\src` 保留未动，
因此 `book-smoke.js` 的 DOM stub 测试仍然可以运行。

## 已知限制

- MiniC 语言本身不支持 `%`、`?:`、`for`、`do-while`、`break`、`continue`、`switch`、`goto`、多维数组等，
  编辑器补全会给出提示，语法手册中有对应条目。
- LL(1) 分析过程为文本表格，超大源文件可能生成很大（例如数百 KB）的 `.parse.txt`，默认关闭、按需开启。
- 「运行」需要系统 PATH 中有 MinGW `gcc`，也可用环境变量 `MINIC_GCC` 指定完整路径。
- 程序死循环会在 5 秒后被强制终止并提示超时。
