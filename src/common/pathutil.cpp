// ============================================================================
// MiniC 编译器 —— UTF-8 路径文件读写的实现（唯一允许 include <windows.h> 的地方）
// 头文件那边不 include windows.h 的原因见 pathutil.h 顶部注释。
// ============================================================================
#include "common/pathutil.h"

#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace minic_path
{
#ifdef _WIN32
namespace
{
std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return std::wstring();
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                        static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

// 系统 ANSI 代码页 -> 宽字符。用于兜底：有些调用方（独立测试程序）传进来的
// 路径是 CRT 按 ANSI 转换过的 std::string，而不是 UTF-8。
std::wstring AnsiToWide(const std::string& s)
{
    if (s.empty()) return std::wstring();
    const int n = ::MultiByteToWideChar(CP_ACP, 0, s.c_str(),
                                        static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_ACP, 0, s.c_str(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty()) return std::string();
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                                        static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string s(static_cast<std::size_t>(n), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}
}

std::string WideArgv(std::size_t index)
{
    int wargc = 0;
    LPWSTR* wargv = ::CommandLineToArgvW(::GetCommandLineW(), &wargc);
    if (!wargv) return std::string();
    std::string s;
    if (index < static_cast<std::size_t>(wargc)) s = WideToUtf8(wargv[index]);
    ::LocalFree(wargv);
    return s;
}
#endif

bool ReadFileUtf8(const std::string& path, std::string& out)
{
#ifdef _WIN32
    // 先按 UTF-8 解释（编译器内部统一用 UTF-8 传路径），失败再按系统 ANSI
    // 代码页解释（独立测试程序仍把 argv 原样传进来，那是 ANSI 字节）。
    std::wstring wpath = Utf8ToWide(path);
    std::FILE* f = wpath.empty() ? nullptr : ::_wfopen(wpath.c_str(), L"rb");
    if (!f)
    {
        wpath = AnsiToWide(path);
        if (!wpath.empty()) f = ::_wfopen(wpath.c_str(), L"rb");
    }
#else
    std::FILE* f = std::fopen(path.c_str(), "rb");
#endif
    if (!f) return false;
    out.clear();
    char buf[65536];
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    const bool ok = (std::ferror(f) == 0);
    std::fclose(f);
    return ok;
}

bool WriteFileUtf8(const std::string& path, const std::string& text)
{
#ifdef _WIN32
    const std::wstring wpath = Utf8ToWide(path);
    if (wpath.empty()) return false;
    std::FILE* f = ::_wfopen(wpath.c_str(), L"wb");
#else
    std::FILE* f = std::fopen(path.c_str(), "wb");
#endif
    if (!f) return false;
    const std::size_t n = text.empty() ? 0 : std::fwrite(text.data(), 1, text.size(), f);
    const bool ok = (n == text.size()) && (std::ferror(f) == 0);
    std::fclose(f);
    return ok;
}
}
