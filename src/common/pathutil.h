// ============================================================================
// MiniC 编译器 —— UTF-8 路径的文件读写（Windows 窄字符路径的补丁层）
//
// 为什么需要这一层：
//   MinGW 的 std::ifstream / std::ofstream 在 Windows 上把 std::string 当作
//   「当前 ANSI 代码页」来解释（中文系统是 GBK/936）。只要路径里出现该代码页
//   表示不了的字（生僻字、emoji，或系统开了「Beta: 使用 Unicode UTF-8」），
//   转换就会失败，文件打不开。更麻烦的是 argv 在进入 main 之前已经被 CRT 按
//   ANSI 转过一次，那时就已经变成 "??" 了，事后再转也救不回来。
//   所以这里同时做两件事：
//     1) 用 GetCommandLineW()/CommandLineToArgvW() 重新取一遍命令行（宽字符），
//        绕开 CRT 的 ANSI 转换（仅 main 用）；
//     2) 用 _wfopen() 按宽字符路径打开文件。
//
// 注意：实现全部放在 pathutil.cpp 里。
//   本头文件**绝不能** include <windows.h>——Windows SDK 的 winnt.h 里
//   `_TOKEN_INFORMATION_CLASS` 枚举有一个枚举值就叫 `TokenType`，
//   （非 scoped enum 的枚举值会注入全局命名空间），会把本项目的
//   `enum class TokenType` 挡住，编译报 "expected ';' before 'type'"。
// ============================================================================
#ifndef MINIC_COMMON_PATHUTIL_H
#define MINIC_COMMON_PATHUTIL_H

#include <cstddef>
#include <string>

namespace minic_path
{
#ifdef _WIN32
// 重新从 GetCommandLineW() 取第 index 个参数（0 是程序名），返回 UTF-8。
// 取不到时返回空串，调用方回退到 argv。
std::string WideArgv(std::size_t index);
#endif

// 把整个文件读进 out；成功返回 true。
bool ReadFileUtf8(const std::string& path, std::string& out);

// 把 text 写进 path；成功返回 true。
bool WriteFileUtf8(const std::string& path, const std::string& text);
}

#endif // MINIC_COMMON_PATHUTIL_H
