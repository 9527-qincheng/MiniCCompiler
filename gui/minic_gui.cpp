// ============================================================================
//  MiniC 编译器图形界面（Win32 原生，无第三方依赖）
//
//  左侧写代码，右侧看结果。点「运行」即调用 build\minic.exe 编译，
//  并把 输出 / Token / 语法树 / 符号表 / 四元式 / 汇编 / 分析过程
//  七个页签填好。
//
//  构建：gui\build_gui.bat    产物：gui\minic_gui.exe
// ============================================================================
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <cwctype>

// ---------------------------------------------------------------- 控件 ID
#define ID_EDIT      1001
#define ID_TAB       1002
#define ID_OUT       1003
#define ID_STATUS    1004

#define ID_BTN_NEW   1101
#define ID_BTN_OPEN  1102
#define ID_BTN_SAVE  1103
#define ID_BTN_SAVEAS 1104
#define ID_BTN_RUN   1105
#define ID_BTN_SAMPLE 1106
#define ID_BTN_CLEAR 1107

#define ID_MENU_EXIT     2001
#define ID_MENU_ABOUT    2002
#define ID_MENU_RUN      2003
#define ID_MENU_OPEN     2004
#define ID_MENU_SAVE     2005
#define ID_MENU_SAVEAS   2006
#define ID_MENU_NEW      2007
#define ID_MENU_FINDC    2008

// ---------------------------------------------------------------- 全局状态
static HWND g_hMain = nullptr;
static HWND g_hEdit = nullptr;
static HWND g_hTab = nullptr;
static HWND g_hOut = nullptr;
static HWND g_hStatus = nullptr;
static HWND g_hBtns[7] = {nullptr};
static HFONT g_hFontUI = nullptr;
static HFONT g_hFontMono = nullptr;

static std::wstring g_srcPath;     // 当前源文件路径（空 = 未保存过）
static std::wstring g_minicPath;   // minic.exe 的绝对路径
static std::wstring g_exeDir;      // gui\minic_gui.exe 所在目录

static std::wstring g_tabText[7];  // 七个页签的内容
static int g_curTab = 0;
static bool g_dirty = false;
static bool g_loading = false;      // 正在程序化设置编辑框内容，忽略 EN_CHANGE

static const wchar_t* TAB_NAMES[7] = {
    L"输出", L"Token", L"语法树", L"符号表", L"四元式", L"汇编", L"分析过程"
};

// 七个页签对应的产物后缀；第 0 个（输出）不读文件
static const wchar_t* TAB_SUFFIX[7] = {
    nullptr, L".token.txt", L".ast.txt", L".sym.txt", L".quad.txt", L".asm", L".parse.txt"
};

// ---------------------------------------------------------------- 小工具
static std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

static std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

static bool ReadFileUtf8(const std::wstring& path, std::wstring& out)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    GetFileSizeEx(h, &sz);
    std::string buf;
    buf.resize((size_t)sz.QuadPart);
    DWORD got = 0;
    if (sz.QuadPart > 0 && !ReadFile(h, &buf[0], (DWORD)buf.size(), &got, nullptr))
    {
        CloseHandle(h);
        return false;
    }
    CloseHandle(h);
    buf.resize(got);
    // 去 UTF-8 BOM
    if (buf.size() >= 3 && (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF)
        buf.erase(0, 3);
    // 归一化换行
    std::string norm;
    norm.reserve(buf.size());
    for (size_t i = 0; i < buf.size(); ++i)
    {
        if (buf[i] == '\r')
        {
            if (i + 1 < buf.size() && buf[i + 1] == '\n') continue;
            norm.push_back('\n');
        }
        else norm.push_back(buf[i]);
    }
    out = Utf8ToWide(norm);
    return true;
}

static bool WriteFileUtf8(const std::wstring& path, const std::wstring& text)
{
    std::string s = WideToUtf8(text);
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    // 统一写 LF
    std::string conv;
    conv.reserve(s.size() + 16);
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\r')
        {
            if (i + 1 < s.size() && s[i + 1] == '\n') continue;
            conv.push_back('\n');
        }
        else conv.push_back(s[i]);
    }
    DWORD wrote = 0;
    bool ok = true;
    if (!conv.empty()) ok = WriteFile(h, conv.data(), (DWORD)conv.size(), &wrote, nullptr) != 0;
    CloseHandle(h);
    return ok && wrote == conv.size();
}

static std::wstring GetText(HWND h)
{
    int n = GetWindowTextLengthW(h);
    std::wstring s((size_t)n + 1, L'\0');
    if (n > 0) GetWindowTextW(h, &s[0], n + 1);
    s.resize((size_t)n);
    return s;
}

static void SetText(HWND h, const std::wstring& s)
{
    SetWindowTextW(h, s.c_str());
}

static std::wstring DirName(const std::wstring& p)
{
    size_t i = p.find_last_of(L"\\/");
    return (i == std::wstring::npos) ? L"" : p.substr(0, i);
}

static bool FileExists(const std::wstring& p)
{
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring StripMcExt(const std::wstring& p)
{
    if (p.size() > 3)
    {
        std::wstring e = p.substr(p.size() - 3);
        for (auto& c : e) c = (wchar_t)towlower(c);
        if (e == L".mc") return p.substr(0, p.size() - 3);
    }
    return p;
}

// ---------------------------------------------------------------- minic 定位
static std::wstring FindMinic()
{
    // 依次尝试：exe 同目录 / exe 上一级 build / exe 同目录 build / 再上一级
    std::vector<std::wstring> cands;
    cands.push_back(g_exeDir + L"minic.exe");
    cands.push_back(g_exeDir + L"..\\build\\minic.exe");
    cands.push_back(g_exeDir + L"build\\minic.exe");
    cands.push_back(g_exeDir + L"..\\..\\build\\minic.exe");
    for (auto& c : cands)
    {
        std::wstring full = c;
        wchar_t buf[MAX_PATH * 2];
        DWORD n = GetFullPathNameW(c.c_str(), MAX_PATH * 2, buf, nullptr);
        if (n > 0 && n < MAX_PATH * 2) full = buf;
        if (FileExists(full)) return full;
    }
    return L"";
}

// ---------------------------------------------------------------- 设置状态栏
static void SetStatus(const std::wstring& s)
{
    if (g_hStatus) SetText(g_hStatus, s);
}

// ---------------------------------------------------------------- 换行归一化
// Win32 的多行 EDIT 控件只认 CRLF 作为换行：只写 LF 的话，它会显示成一个方框，
// 整份产物挤在同一行里（这是「Token 页签看起来全挤在 1 行」的真正原因）。
// 产物文件本身用的是 LF，所以在送进控件之前统一转成 CRLF。
static std::wstring ToCrLf(const std::wstring& s)
{
    std::wstring r;
    r.reserve(s.size() + s.size() / 8 + 8);
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == L'\n' && (i == 0 || s[i - 1] != L'\r'))
            r.push_back(L'\r');
        r.push_back(s[i]);
    }
    return r;
}

// ---------------------------------------------------------------- 页签刷新
static void ShowTab(int idx)
{
    if (idx < 0 || idx >= 7) return;
    g_curTab = idx;
    const std::wstring& t = g_tabText[idx];
    SetText(g_hOut, t.empty() ? L"" : ToCrLf(t));
    // 滚回顶部
    SendMessageW(g_hOut, EM_SETSEL, 0, 0);
    SendMessageW(g_hOut, EM_SCROLLCARET, 0, 0);
}

// ---------------------------------------------------------------- 读产物填空页签
static void LoadArtifacts(const std::wstring& base)
{
    for (int i = 1; i < 7; ++i)
    {
        std::wstring p = base + TAB_SUFFIX[i];
        std::wstring content;
        if (ReadFileUtf8(p, content))
            g_tabText[i] = content;
        else
            g_tabText[i] = std::wstring(L"（未生成：") + p + L"）";
    }
}

// ---------------------------------------------------------------- 运行编译
static bool RunCompiler(std::wstring& consoleOut, int& exitCode)
{
    if (g_minicPath.empty()) g_minicPath = FindMinic();
    if (g_minicPath.empty())
    {
        MessageBoxW(g_hMain,
                    L"找不到 minic.exe。\n\n请先在本项目根目录运行 build.bat 生成 build\\minic.exe，"
                    L"并确认 minic_gui.exe 与它处在合理的相对位置。",
                    L"缺少编译器", MB_ICONERROR);
        return false;
    }
    if (g_srcPath.empty())
    {
        // 还没保存过：不要让用户先「保存」才能编译。
        // 自动落到 gui 目录下的 scratch 里，产物也生成在那里。
        std::wstring dir = g_exeDir + L"scratch\\";
        CreateDirectoryW(dir.c_str(), nullptr);
        int n = 1;
        std::wstring p;
        do
        {
            p = dir + L"untitled" + std::to_wstring(n) + L".mc";
            ++n;
        } while (FileExists(p));
        g_srcPath = p;
        SetStatus(L"自动保存到 " + g_srcPath);
    }

    // 把编辑框内容落盘
    if (!WriteFileUtf8(g_srcPath, GetText(g_hEdit)))
    {
        MessageBoxW(g_hMain, L"写入源文件失败。", L"错误", MB_ICONERROR);
        return false;
    }
    g_dirty = false;

    // 控制台输出重定向到临时文件。
    // 注意：不要用 GetTempPathW()。在受限/沙箱环境里系统 %TEMP% 可能不可写，
    // CreateFileW 会失败并弹出「无法创建输出文件」的模态框，看起来像程序卡死。
    // 统一放到 exe 同级的 scratch\ 下，由程序自己保证存在。
    std::wstring scratchDir = g_exeDir + L"scratch\\";
    CreateDirectoryW(scratchDir.c_str(), nullptr);
    std::wstring logPath = scratchDir + L"_console.txt";

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hLog = CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hLog == INVALID_HANDLE_VALUE)
    {
        MessageBoxW(g_hMain, L"无法创建输出文件。", L"错误", MB_ICONERROR);
        return false;
    }

    std::wstring cmd = L"\"" + g_minicPath + L"\" \"" + g_srcPath + L"\"";
    cmd += L" --dump-tokens --dump-ast --dump-parse --dump-symtab --dump-quads";

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hLog;
    si.hStdError = hLog;
    si.hStdInput = nullptr;

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    BOOL ok = CreateProcessW(g_minicPath.c_str(), cmdBuf.data(), nullptr, nullptr,
                             TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok)
    {
        CloseHandle(hLog);
        std::wstring msg = L"启动 minic.exe 失败（错误码 " + std::to_wstring(GetLastError()) + L"）";
        MessageBoxW(g_hMain, msg.c_str(), L"错误", MB_ICONERROR);
        return false;
    }

    DWORD wr = WaitForSingleObject(pi.hProcess, 60000);
    DWORD code = 0;
    if (wr == WAIT_TIMEOUT)
    {
        TerminateProcess(pi.hProcess, 1);
        consoleOut = L"[GUI] 编译超时（60 秒），已终止。\n";
        exitCode = -1;
    }
    else
    {
        GetExitCodeProcess(pi.hProcess, &code);
        exitCode = (int)code;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hLog);

    std::wstring log;
    ReadFileUtf8(logPath, log);
    DeleteFileW(logPath.c_str());
    if (wr != WAIT_TIMEOUT) consoleOut = log;
    return true;
}

// ---------------------------------------------------------------- 运行动作
static void DoRun()
{
    SetStatus(L"正在编译 …");
    EnableWindow(g_hBtns[4], FALSE);   // 运行按钮

    std::wstring console;
    int code = 0;
    bool started = RunCompiler(console, code);

    if (!started)
    {
        SetStatus(L"未执行");
        EnableWindow(g_hBtns[4], TRUE);
        return;
    }

    std::wstring base = StripMcExt(g_srcPath);
    LoadArtifacts(base);

    // 第 0 页：控制台输出 + 结论
    std::wstring head;
    if (code == 0)
        head = L"═══ 编译成功（退出码 0）═══\r\n\r\n";
    else
        head = L"═══ 编译失败（退出码 " + std::to_wstring(code) + L"）═══\r\n\r\n";
    if (console.empty()) console = L"（minic 没有输出任何文字）\r\n";
    g_tabText[0] = head + console +
                   L"\r\n──── 产物 ────\r\n" + base + L".token.txt\r\n" +
                   base + L".ast.txt\r\n" + base + L".sym.txt\r\n" +
                   base + L".quad.txt\r\n" + base + L".asm\r\n" +
                   base + L".parse.txt";

    // 切到输出页（有错时让用户第一眼看到错误）
    SendMessageW(g_hTab, TCM_SETCURSEL, 0, 0);
    ShowTab(0);

    SetStatus(code == 0 ? L"编译成功" : L"编译失败，请查看「输出」页签");
    EnableWindow(g_hBtns[4], TRUE);
}

// ---------------------------------------------------------------- 文件操作
static bool SaveTo(const std::wstring& path)
{
    if (!WriteFileUtf8(path, GetText(g_hEdit))) return false;
    g_srcPath = path;
    g_dirty = false;
    SetStatus(L"已保存：" + path);
    return true;
}

static bool SaveAsDialog()
{
    wchar_t buf[MAX_PATH] = {0};
    if (!g_srcPath.empty())
        wcsncpy(buf, g_srcPath.c_str(), MAX_PATH - 1);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMain;
    ofn.lpstrFilter = L"MiniC 源文件 (*.mc)\0*.mc\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"mc";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return false;
    return SaveTo(buf);
}

static bool OpenDialog()
{
    wchar_t buf[MAX_PATH] = {0};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMain;
    ofn.lpstrFilter = L"MiniC 源文件 (*.mc)\0*.mc\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return false;

    std::wstring text;
    if (!ReadFileUtf8(buf, text))
    {
        MessageBoxW(g_hMain, L"读取文件失败。", L"错误", MB_ICONERROR);
        return false;
    }
    g_loading = true;
    SetText(g_hEdit, text);
    g_loading = false;
    g_srcPath = buf;
    g_dirty = false;
    // 清空旧结果
    for (int i = 0; i < 7; ++i) g_tabText[i].clear();
    SendMessageW(g_hTab, TCM_SETCURSEL, 0, 0);
    ShowTab(0);
    SetStatus(L"已打开：" + g_srcPath);
    return true;
}

static void DoNew()
{
    if (g_dirty && !GetText(g_hEdit).empty())
    {
        int r = MessageBoxW(g_hMain, L"当前内容尚未保存，确定新建？", L"新建",
                            MB_YESNO | MB_ICONQUESTION);
        if (r != IDYES) return;
    }
    g_loading = true;
    SetText(g_hEdit, L"");
    g_loading = false;
    g_srcPath.clear();
    g_dirty = false;
    for (int i = 0; i < 7; ++i) g_tabText[i].clear();
    SendMessageW(g_hTab, TCM_SETCURSEL, 0, 0);
    ShowTab(0);
    SetStatus(L"新建文件");
    SetFocus(g_hEdit);
}

static const wchar_t* SAMPLE =
    L"// MiniC 示例：挑战版特性演示\r\n"
    L"int a;\r\n"
    L"float b;\r\n"
    L"int arr[10];\r\n"
    L"bool flag;\r\n"
    L"char c;\r\n"
    L"\r\n"
    L"a = 1 + 2 * 3;\r\n"
    L"b = a + 1.5;\r\n"
    L"flag = a < 10 && a != 0;\r\n"
    L"c = 'x';\r\n"
    L"\r\n"
    L"if (a < 10) {\r\n"
    L"    a = a + 1;\r\n"
    L"} else {\r\n"
    L"    a = a - 1;\r\n"
    L"}\r\n"
    L"\r\n"
    L"while (a < 20) {\r\n"
    L"    a = a + 1;\r\n"
    L"}\r\n"
    L"\r\n"
    L"print(a);\r\n";

static void InsertSample()
{
    g_loading = true;
    SetText(g_hEdit, SAMPLE);
    g_loading = false;
    g_dirty = true;
    SetStatus(L"已插入示例程序，按 F5 或点「运行」");
    SetFocus(g_hEdit);
}

// ---------------------------------------------------------------- 布局
static void Layout()
{
    RECT rc;
    GetClientRect(g_hMain, &rc);
    int W = rc.right, H = rc.bottom;

    const int tbH = 46;      // 工具条高
    const int sbH = 26;      // 状态栏高
    const int pad = 8;

    // 工具条各按钮位置（x, w）
    struct { int x, w; } pos[7] = {
        {8, 62}, {76, 62}, {144, 62}, {212, 80},     // 新建 打开 保存 另存为
        {306, 96}, {410, 70}, {488, 92}              // 运行 示例 清空结果
    };
    for (int i = 0; i < 7; ++i)
        if (g_hBtns[i])
            MoveWindow(g_hBtns[i], pos[i].x, 9, pos[i].w, 28, TRUE);

    int top = tbH + pad;
    int bottom = H - sbH - pad;
    int contentH = bottom - top;
    if (contentH < 40) contentH = 40;

    int clientW = W - pad * 2;
    int leftW = (int)(clientW * 0.46);
    int rightW = clientW - leftW - pad;
    if (rightW < 260) { rightW = 260; leftW = clientW - rightW - pad; }
    if (leftW < 200) leftW = 200;

    MoveWindow(g_hEdit, pad, top, leftW, contentH, TRUE);

    int rx = pad + leftW + pad;
    MoveWindow(g_hTab, rx, top, rightW, contentH, TRUE);
    MoveWindow(g_hOut, rx, top, rightW, contentH, TRUE);

    // 页签用自适应宽度 + TCS_MULTILINE：7 个页签放不下时自动折成两行，
    // 不用手算宽度，也就不会出现「右侧页签被面板截断」。
    // （宽度由文字决定，只在窗口尺寸变化时重排一次即可。）

    // 页签高度随折行变化，所以输出框的位置要按页签控件实际需要的高度算，
    // 不能写死 26px。TCM_ADJUSTRECT 给出页签的显示区矩形，用它夹掉页签头最稳。
    RECT trc;
    GetWindowRect(g_hTab, &trc);
    MapWindowPoints(nullptr, g_hMain, (POINT*)&trc, 2);
    RECT dis = {0, 0, rightW, contentH};
    TabCtrl_AdjustRect(g_hTab, FALSE, &dis);
    int headH = dis.top;
    if (headH < 24) headH = 24;

    MoveWindow(g_hOut, rx + 2, trc.top + headH, rightW - 6, contentH - headH - 2, TRUE);

    MoveWindow(g_hStatus, pad, H - sbH, W - pad * 2, sbH, TRUE);
}

// ---------------------------------------------------------------- 换字体
static void ApplyFonts()
{
    int dpi = GetDeviceCaps(GetDC(g_hMain), LOGPIXELSY);
    int ptUI = MulDiv(10, dpi, 72);
    int ptMono = MulDiv(11, dpi, 72);

    g_hFontUI = CreateFontW(-ptUI, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");
    g_hFontMono = CreateFontW(-ptMono, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                              FIXED_PITCH | FF_MODERN, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    for (int i = 0; i < 7; ++i)
        if (g_hBtns[i]) SendMessageW(g_hBtns[i], WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
    SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
    SendMessageW(g_hEdit, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
    SendMessageW(g_hOut, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
}

// ---------------------------------------------------------------- 菜单
static HMENU BuildMenu()
{
    HMENU file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, ID_MENU_NEW, L"新建(&N)\tCtrl+N");
    AppendMenuW(file, MF_STRING, ID_MENU_OPEN, L"打开(&O)…\tCtrl+O");
    AppendMenuW(file, MF_STRING, ID_MENU_SAVE, L"保存(&S)\tCtrl+S");
    AppendMenuW(file, MF_STRING, ID_MENU_SAVEAS, L"另存为(&A)…");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, ID_MENU_EXIT, L"退出(&X)\tAlt+F4");

    HMENU build = CreatePopupMenu();
    AppendMenuW(build, MF_STRING, ID_MENU_RUN, L"编译并运行(&R)\tF5");
    AppendMenuW(build, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(build, MF_STRING, ID_MENU_FINDC, L"定位 minic.exe(&F)");

    HMENU help = CreatePopupMenu();
    AppendMenuW(help, MF_STRING, ID_MENU_ABOUT, L"关于(&A)");

    HMENU bar = CreateMenu();
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)file, L"文件(&F)");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)build, L"编译(&B)");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)help, L"帮助(&H)");
    return bar;
}

// ---------------------------------------------------------------- 窗口过程
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        g_hMain = hWnd;

        // 工具条按钮
        struct { const wchar_t* text; int id; } btns[7] = {
            {L"新建", ID_BTN_NEW}, {L"打开", ID_BTN_OPEN}, {L"保存", ID_BTN_SAVE},
            {L"另存为", ID_BTN_SAVEAS}, {L"▶ 运行 (F5)", ID_BTN_RUN},
            {L"示例", ID_BTN_SAMPLE}, {L"清空结果", ID_BTN_CLEAR}
        };
        for (int i = 0; i < 7; ++i)
        {
            g_hBtns[i] = CreateWindowW(L"BUTTON", btns[i].text,
                                       WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)btns[i].id,
                                       (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE), nullptr);
        }

        // 编辑框
        g_hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                  WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                                  ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
                                  ES_WANTRETURN | ES_NOHIDESEL,
                                  0, 0, 10, 10, hWnd, (HMENU)ID_EDIT,
                                  (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE), nullptr);

        // 页签
        // 不用 TCS_FIXEDWIDTH：固定宽度模式必须事先确定每页签的 cx，稍有偏差就会
        // 让 7 个页签超出右侧面板被截断。默认的「按文字自适应」最稳，宽度不够时
        // 系统会自动折行（配合 TCS_MULTILINE 观察效果更好）。
        g_hTab = CreateWindowExW(0, WC_TABCONTROLW, L"",
                                 WS_CHILD | WS_VISIBLE | TCS_MULTILINE,
                                 0, 0, 10, 10, hWnd, (HMENU)ID_TAB,
                                 (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE), nullptr);
        SendMessageW(g_hTab, TCM_SETPADDING, 0, MAKELPARAM(10, 4));
        for (int i = 0; i < 7; ++i)
        {
            TCITEMW it{};
            it.mask = TCIF_TEXT;
            it.pszText = (LPWSTR)TAB_NAMES[i];
            SendMessageW(g_hTab, TCM_INSERTITEMW, (WPARAM)i, (LPARAM)&it);
        }

        // 输出框
        g_hOut = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                                 ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
                                 0, 0, 10, 10, hWnd, (HMENU)ID_OUT,
                                 (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE), nullptr);

        // 状态栏
        g_hStatus = CreateWindowExW(0, L"STATIC", L"就绪",
                                    WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                                    0, 0, 10, 10, hWnd, (HMENU)ID_STATUS,
                                    (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE), nullptr);

        ApplyFonts();
        Layout();
        SetStatus(L"就绪　|　左侧写代码，Ctrl+S 保存后按 F5 编译");
        return 0;
    }

    case WM_SIZE:
        if (g_hMain) Layout();
        return 0;

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* mm = (MINMAXINFO*)lParam;
        mm->ptMinTrackSize.x = 760;
        mm->ptMinTrackSize.y = 520;
        return 0;
    }

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        switch (id)
        {
        case ID_BTN_NEW:    case ID_MENU_NEW:    DoNew(); break;
        case ID_BTN_OPEN:   case ID_MENU_OPEN:   OpenDialog(); break;
        case ID_BTN_SAVE:
        case ID_MENU_SAVE:
            if (g_srcPath.empty()) SaveAsDialog(); else SaveTo(g_srcPath);
            break;
        case ID_BTN_SAVEAS: case ID_MENU_SAVEAS: SaveAsDialog(); break;
        case ID_BTN_RUN:    case ID_MENU_RUN:    DoRun(); break;
        case ID_BTN_SAMPLE: InsertSample(); break;
        case ID_BTN_CLEAR:
            for (int i = 0; i < 7; ++i) g_tabText[i].clear();
            ShowTab(g_curTab);
            SetStatus(L"已清空结果");
            break;
        case ID_MENU_EXIT: PostMessageW(hWnd, WM_CLOSE, 0, 0); break;
        case ID_MENU_FINDC:
        {
            std::wstring p = FindMinic();
            if (p.empty())
                MessageBoxW(hWnd, L"没有找到 minic.exe。\n请先在仓库根目录运行 build.bat。",
                            L"未找到", MB_ICONWARNING);
            else
            {
                g_minicPath = p;
                std::wstring m = L"已定位编译器：\n" + p;
                MessageBoxW(hWnd, m.c_str(), L"minic.exe", MB_ICONINFORMATION);
            }
            break;
        }
        case ID_MENU_ABOUT:
            MessageBoxW(hWnd,
                        L"MiniC 编译器图形界面\n\n"
                        L"左侧编写 MiniC 源码，Ctrl+S 保存，按 F5 调用编译，\n"
                        L"右侧可查看 Token、语法树、符号表、四元式与汇编。\n\n"
                        L"第 5 组 · 挑战版 · LL(1) 预测分析",
                        L"关于", MB_ICONINFORMATION);
            break;
        default: break;
        }
        return 0;
    }

    case WM_NOTIFY:
    {
        LPNMHDR nh = (LPNMHDR)lParam;
        if (nh->idFrom == ID_TAB && nh->code == TCN_SELCHANGE)
        {
            int sel = (int)SendMessageW(g_hTab, TCM_GETCURSEL, 0, 0);
            ShowTab(sel);
        }
        return 0;
    }

    case WM_CTLCOLORSTATIC:
        if ((HWND)lParam == g_hStatus)
        {
            SetBkMode((HDC)wParam, TRANSPARENT);
            SetTextColor((HDC)wParam, RGB(60, 60, 60));
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------- 入口
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    // 高 DPI 下字体不至于糊
    SetProcessDPIAware();

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TAB_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    // 取 exe 所在目录
    wchar_t buf[MAX_PATH * 2] = {0};
    GetModuleFileNameW(nullptr, buf, MAX_PATH * 2);
    g_exeDir = DirName(buf) + L"\\";
    g_minicPath = FindMinic();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"MiniCGuiWnd";
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    if (!RegisterClassExW(&wc)) return 1;

    HMENU menu = BuildMenu();

    // 居中显示
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int w = 1180, h = 760;
    if (w > sw - 80) w = sw - 80;
    if (h > sh - 80) h = sh - 80;
    int x = (sw - w) / 2, y = (sh - h) / 2;

    HWND hWnd = CreateWindowExW(0, L"MiniCGuiWnd", L"MiniC 编译器 · 图形界面",
                                WS_OVERLAPPEDWINDOW,
                                x, y, w, h, nullptr, menu, hInst, nullptr);
    if (!hWnd) return 1;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 加速键
    ACCEL acc[4];
    acc[0].fVirt = FVIRTKEY; acc[0].key = VK_F5; acc[0].cmd = ID_MENU_RUN;
    acc[1].fVirt = FVIRTKEY | FCONTROL; acc[1].key = 'S'; acc[1].cmd = ID_MENU_SAVE;
    acc[2].fVirt = FVIRTKEY | FCONTROL; acc[2].key = 'O'; acc[2].cmd = ID_MENU_OPEN;
    acc[3].fVirt = FVIRTKEY | FCONTROL; acc[3].key = 'N'; acc[3].cmd = ID_MENU_NEW;
    HACCEL hAccel = CreateAcceleratorTableW(acc, 4);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        if (!TranslateAcceleratorW(hWnd, hAccel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}
