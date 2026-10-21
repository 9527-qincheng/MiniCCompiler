# MiniC 编译器 GUI（Electron）

按设计规范板 v1.0（S1~S5）实现的高保真桌面界面：暗/亮双主题、自定义标题栏、
源码编辑器（行号 + 语法高亮）、7 类编译产物标签、诊断列表（点击跳转源码行）。

## 启动

双击 `启动GUI.bat`（推荐），或命令行：

```bat
cd /d MiniCCompiler\gui
npm install          :: 首次
启动GUI.bat
```

需要先构建编译器后端：

```bat
cd /d MiniCCompiler
build.bat            :: 生成 build\minic.exe
```

## 快速上手（3 分钟）

1. **启动**：双击 `启动GUI.bat`，窗口打开（首次运行会先解压运行时，约 30 秒）。
2. **写代码**：启动后是一个空白的新文件（`未命名.mc`）。可以直接输入，或点 **示例▾** 载入示例，
   也可以点 **打开** 选择已有 `.mc` 文件。
3. **多文件**：编辑器上方是文件标签栏。点 **新建**（或 `+` / `Ctrl+N`）新建空白文件，
   每个文件独立保存内容、路径、编译产物和诊断；切换标签互不干扰。
   标签上的蓝点表示该文件有未保存修改，点 × 或中键可关闭（有改动会先确认）。
4. **编译**：点工具栏蓝色 **▶ 编译**（或 `Ctrl+Enter`）。改动过代码但还没重新编译时，
   右上角状态会变成「已修改 · 待编译」。
5. **保存**：`Ctrl+S`。没打开过文件时 → 弹出对话框选位置和文件名；已打开/已保存过 → 直接覆盖原文件。
5. **看产物**：右侧 7 个标签分别是 Token / 语法分析 / 语法树 / 符号表 / 四元式 / 汇编 / 诊断，
   点标签切换；Token 表按记号类型着色，标签上的数字是条目数。
6. **看错误**：编译失败会自动切到 **诊断** 标签，列出错误（行、列、阶段、原因），
   **点一下错误行**，光标就跳到源码对应位置，编辑器里对应行有红色底纹。
7. **调字号/主题**：编辑器内 `Ctrl+滚轮` 缩放字号；右上角 **主题** 或 `Ctrl+Shift+L` 切换暗/亮。

## 为什么需要 启动GUI.bat

本机实测：**原生程序位于含中文的路径时（如 `E:\计算机软件实训\...`），其文件 I/O 会整体失效**——
`minic.exe` 会报 `cannot open source file`，Electron 会直接崩溃。同一份二进制放到纯 ASCII
路径则完全正常。启动脚本因此做两件事：

1. 把 Electron 运行时部署到 `%LOCALAPPDATA%\minic-gui\runtime`（纯 ASCII 路径，仅首次）；
2. 用该运行时加载本目录（应用目录可以留在中文路径，实测无影响）。

同一原因，`lib/compiler.js` 在调用前会把 `minic.exe` 复制到 `%TEMP%\minic-gui\minic.exe`。

另外本机 GPU 进程无法在沙箱中启动，启动参数固定带 `--no-sandbox --disable-gpu`
（`package.json` 的 start 脚本已包含）。

## 目录结构

```
gui/
  main.js            Electron 主进程：窗口/IPC/文件对话框/截图模式
  preload.js         contextBridge 暴露的最小 API
  lib/compiler.js    编译调度（纯 Node，可单测）：调用 minic.exe、回读产物、编码处理
  src/index.html     界面结构
  src/styles.css     设计令牌（S5 色值）+ 布局 + 组件
  src/renderer.js    编辑器高亮、Token 表、7 标签、诊断解析、菜单与快捷键
  test/compile-smoke.js  编译管线冒烟测试
  启动GUI.bat        启动器（部署 ASCII 运行时）
```

## 产物与标签对应

| 标签 | 数据来源 | 说明 |
|---|---|---|
| Token | `<src>.token.txt` | `行:列 类型 值`，按类型着色 |
| 语法分析 | `<src>.parse.txt` | LL(1) 预测分析过程（Markdown 表） |
| 语法树 | `<src>.ast.txt` | 缩进树 |
| 符号表 | `<src>.sym.txt` | 分节符号表 |
| 四元式 | `<src>.quad.txt` | 当前为 TODO 占位（IR 生成器未实现） |
| 汇编 | `<src>.asm` | 挑战版目标代码（当前 TODO 占位） |
| 诊断 | stderr | `minic: error/warning: 行:列: 消息`，点击跳转 |

## 编辑体验（类 VS Code）

| 能力 | 说明 |
|---|---|
| 自动配对 | 输入 `(` `{` `[` `"` `'` 自动补右半；有选中内容时自动包裹；右侧已有右括号时按它直接跳过 |
| 成对退格 | 空括号/空引号中间按退格一次删掉一对 |
| 智能缩进 | 回车保持当前缩进；`{` 后回车自动多缩一级；在 `{}` 中间回车自动展开成三行；行首输入 `}` 自动退一级 |
| 回车自动排版 | 光标在行尾按回车时，先把这一行规范化（`a=10;` → `a = 10;`），注释行不动 |
| 格式化文档 | `Shift+Alt+F` 或 编辑菜单：重算缩进、规范操作符/逗号两侧空格、去行尾空格、压缩空行（只改空白，不改语义） |
| 保存时自动格式化 | 默认开启，可在 **视图 → 保存时自动格式化** 关闭（会记住） |
| 当前行高亮 | 光标所在行淡色底纹 |

## 快捷键

`Ctrl+N` 新建 · `Ctrl+O` 打开 · `Ctrl+S` 保存 · `Ctrl+Shift+S` 另存为 · `Ctrl+Enter` 编译 ·
`Shift+Alt+F` 格式化文档 · `Ctrl+Shift+L` 切换主题 · `Ctrl+滚轮` 缩放编辑器字号 · `Ctrl+=` / `Ctrl+-` 同样可调字号

## 测试

```bat
cd /d MiniCCompiler\gui
node test\compile-smoke.js
```

覆盖：正常源码（7 个产物齐全、Token 格式、AST/符号表内容）与错误源码（退出码 1、诊断格式）。

## 截图/回归模式

```bat
"%LOCALAPPDATA%\minic-gui\runtime\electron.exe" "%CD%" --no-sandbox --disable-gpu ^
  --shot=out.png --compile --theme=light
```

可选：`--break`（错误示例）、`--tab=asm|sym|diag|...`、`--menu=file|build|view|...`。
