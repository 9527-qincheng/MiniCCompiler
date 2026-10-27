# build.bat 使用说明

> 位置：`MiniCCompiler/build.bat`
> 用途：Windows 下一键构建 MiniC 编译器

---

## 一、这是什么

**`build.bat` 是 Windows 批处理脚本**，把那条又长又容易出错的 `g++` 编译命令固化下来。

### 它解决的问题

原来每次编译要手敲（且**要手动列全部 8 个源文件**）：

```bash
g++ -std=c++17 -Wall -Wextra -Isrc -o build/minic.exe ^
  src/lexer/token.cpp src/lexer/lexer.cpp ^
  src/parser/ast.cpp src/parser/parser.cpp ^
  src/symbol/symbol.cpp src/sema/ir.cpp ^
  src/codegen/codegen.cpp src/main.cpp
```

**两个痛点**：
1. 挑战版**新增源文件**时，如果忘了加进命令 → 链接报 `undefined reference`
2. 报错信息被 g++ 的 `collect2` 包装吞掉，只留一句 `ld returned 1`，**看不出原因**

### 用了之后

```bash
build.bat
```

**自动扫描 `src/` 下所有 `.cpp`**，新增文件无需改脚本。

---

## 二、命令一览

| 命令 | 作用 |
|---|---|
| `build.bat` | **编译**，产出 `build\minic.exe` |
| `build.bat clean` | 清理 `build` 目录 |
| `build.bat rebuild` | 先清理再编译 |
| `build.bat list` | 列出参与编译的源文件 |
| `build.bat run tests\test.mc` | 编译后运行指定源文件 |
| `build.bat run tests\test.mc --dump-tokens` | 编译后运行并传参 |

### 使用示例

```bash
cd E:\计算机软件实训\MiniCCompiler

build.bat                              # 编译
build.bat run tests\test.mc            # 编译并运行
build.bat run tests\test.mc --dump-tokens --dump-ast
build.bat list                         # 看会编译哪些文件
build.bat clean                        # 清理
```

---

## 三、关键设计

### 1. 自动收集源文件（与 CMake 对齐）

```bat
for /r "%SRCDIR%" %%f in (*.cpp) do set "SRC=!SRC! "%%f""
```

**递归扫描 `src/` 下所有 `.cpp`** —— 与 `CMakeLists.txt` 里的 `GLOB_RECURSE` **行为一致**。

> **为什么要对齐？**
> 项目有两条构建路径（CMake 与直接 g++），若行为不一致，会出现
> "**我本地能编过、你那边编不过**"的扯皮。
> - CMake 的 `GLOB` → 自动收集，新文件**会**进链接
> - 手敲 g++ 命令 → 手动列举，新文件**不会**进链接
> - **本脚本** → 自动收集，与 CMake 一致 ✅

### 2. 失败时给出排查提示

脚本会识别两类最常见的链接错误：

| 报错 | 含义 | 提示 |
|---|---|---|
| `multiple definition of 'X'` | **撞名冲突**（多了） | 提示用 `grep -rn "class X" src` 排查，并附历史案例 |
| `undefined reference to 'X'` | **漏文件**（少了） | 提示确认文件在 `src/` 下 |

**为什么会顺手给提示**：因为 g++ 默认用 `collect2` 包装 `ld`，
真正的错误信息**可能被压缩成一句 `ld returned 1 exit status`**，
组员看到会完全不知道哪里错了。

脚本还会给出**查看详细错误的方法**：

```bash
g++ -std=c++17 -Isrc -c src\lexer\token.cpp -o token.o   # 逐个编译
ld -o build\minic.exe *.o                                # 直接调 ld，看真实报错
```

### 3. 防漏判：检查产物大小

```bat
for %%A in ("%OUT%") do set "SIZE=%%~zA"
if !SIZE! LEQ 0 ( ... 判定为链接失败 ... )
```

**为什么需要**：链接中途失败时，有时仍会生成一个 **0 字节的 exe**。
只看"文件是否存在"会误判为成功，所以**额外检查大小**。

### 4. 纯 ASCII 编码

脚本**不含任何中文字符**（已用字节检查确认：非 ASCII 字节数 = 0）。

**为什么**：`.bat` 文件在 cmd 中按 **GBK/本地代码页**解析，
若用 UTF-8 保存中文，会出现**命令被拆解成乱码**的严重错误（实测踩过）。
所以脚本内注释与提示**全部用英文**，中文说明放在本文档里。

---

## 四、与其他构建方式的关系

| 方式 | 状态 | 说明 |
|---|---|---|
| **`build.bat`** | ✅ 可用，已实测 | 本机无 CMake 时的推荐方式 |
| **`CMakeLists.txt`** | ⚠️ 未实测 | 本机**未安装 CMake**；语法正确但从未跑过 |
| 手敲 g++ 命令 | ✅ 可用 | README 里有；但要手动列文件 |

**如果将来装上 CMake**，可以直接用：

```bash
winget install Kitware.CMake
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

此时 `build.bat` 仍可作为"不想装 CMake"时的备选。

---

## 五、实测记录

脚本的 **7 项功能均已实际测试**：

| 编号 | 测试项 | 结果 |
|---|---|---|
| 1 | `build.bat list` | ✅ 正确列出 8 个源文件 |
| 2 | `build.bat`（编译） | ✅ 成功，产出 915 KB 的 `minic.exe` |
| 3 | `build.bat run tests\test.mc --dump-tokens` | ✅ 编译并运行，退出码 0 |
| 4 | `build.bat run tests\test.mc`（多参数） | ✅ 4 个参数正确传递 |
| 5 | **撞名冲突场景** | ✅ 失败并给出排查提示 |
| 6 | `build.bat --help`（未知参数） | ✅ 打印用法 |
| 7 | `build.bat rebuild` | ✅ 清理后重新编译成功 |

**环境**：g++ 15.1.0 (MSYS2)、Windows、cmd

---

## 六、注意事项

1. **必须在项目根目录运行**（与 `src/`、`CMakeLists.txt` 同级）
   - 脚本内部有 `cd /d "%~dp0"`，双击运行也能正确定位
2. **需要 g++ 在 PATH 中**（脚本会先检查，找不到会明确报错）
3. **编码约定**：若你要修改此脚本，**请保持纯 ASCII**
   （用记事本编辑后另存为时会加 BOM，可能出问题）
4. **`build/` 已被 `.gitignore` 排除**，产物不会误提交

---

## 七、一句话总结

**`build.bat` = 一条命令完成编译，自动收集源文件，失败时告诉你原因。**

它主要解决两个坑：
- 新增 `.cpp` 忘了加进编译命令（`undefined reference`）
- 撞名冲突时只看到 `ld returned 1` 而不知所措
