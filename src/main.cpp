// ============================================================================
// MiniC 编译器 —— main 入口（第 6.7 节约定，Q7 四类产物落盘）
// 命令行：minic <源文件> [--dump-tokens] [--dump-ast] [--dump-symtab] [--dump-quads]
// 流水线：CLex.Run() → CParser.Parse() → SymbolTable 语义检查 →
//         IRGenerator.Generate() → （挑战版）CCodeGen.Generate()
// 退出码：0 成功 / 1 出错（错误汇总 g_ErrorList，含行列号 Q8）
// ============================================================================
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "parser/grammar.h"
#include "symbol/symbol.h"
#include "sema/ir.h"
#include "sema/semantic.h"
#include "codegen/codegen.h"
#include "sema/optimizer.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

// 版本信息：--version / -V 输出。语义版本号手工维护，与 docs/ 里的
// 「挑战版 v4.0」对齐；BUILD 日期不写死，避免每次改代码都要动这里。
const char* const kVersion = "MiniC Compiler 4.0 (challenge, x86-64)";

void PrintUsage(std::ostream& os)
{
    os << "Usage: minic <source.mc> [options]\n";
    os << "\nOptions:\n";
    os << "  --dump-tokens : write <src>.token.txt\n";
    os << "  --dump-ast    : write <src>.ast.txt\n";
    os << "  --dump-parse  : write <src>.parse.txt (LL(1) 预测分析过程, §4.11)\n";
    os << "  --dump-symtab : write <src>.sym.txt\n";
    os << "  --dump-quads  : write <src>.quad.txt\n";
    os << "  -O, --optimize: run peephole optimization on the quads before\n";
    os << "                  code generation (constant-branch folding, dead\n";
    os << "                  code / temp / label elimination)\n";
    os << "  -h, --help    : print this help and exit\n";
    os << "  -V, --version : print version and exit\n";
    os << "\nThe target .asm is always written when compilation succeeds.\n";
    os << "Exit code: 0 on success, 1 on any diagnostic error.\n";
}

// 源文件去扩展名（如 tests/test.mc -> tests/test）
std::string StripExtension(const std::string& path)
{
    size_t dot = path.find_last_of('.');
    size_t sep = path.find_last_of("/\\");
    if (dot != std::string::npos && (sep == std::string::npos || dot > sep))
    {
        return path.substr(0, dot);
    }
    return path;
}

// 仅保留路径中的文件名部分（如 tests/x.mc -> x.mc），用于解析轨迹头部，
// 使 --dump-parse 的产物内容与源文件所在目录无关（便于与黄金基线逐字比对）。
std::string BaseName(const std::string& path)
{
    size_t sep = path.find_last_of("/\\");
    return (sep == std::string::npos) ? path : path.substr(sep + 1);
}

// 四元式序列文本（Q6：操作数按索引转原文，供 --dump-quads 落盘）
// 操作码文本统一由 ir.h 的 QuadOpName 提供：返回表 5-2 的助记符
// （大类名 + 宽度后缀，如 "ADD_4" / "ASSIGN_4F" / "LS_4F"），此处不再另设一份表，
// 避免与 Quad::Print 各写一份导致不一致。
std::string DumpQuadsText(const IRGenerator& ir, const CodeList& codes)
{
    std::string out;
    for (const auto& q : codes)
    {
        out += "(" + ::QuadOpName(q.m_eOp) + ", " +
               ir.OperandToString(q.m_Op1) + ", " +
               ir.OperandToString(q.m_Op2) + ", " +
               ir.OperandToString(q.m_Rslt) + ")  @" +
               std::to_string(q.m_loc.m_iRow) + ":" +
               std::to_string(q.m_loc.m_iCol) + "\n";
    }
    return out;
}

bool WriteTextFile(const std::string& path, const std::string& content)
{
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs)
    {
        EmitError("cannot write output file: " + path, LineInfo(0, 0));
        return false;
    }
    ofs << content;
    return true;
}

std::string DumpTokensText(const TokenList& tokens)
{
    std::string out;
    for (const auto& t : tokens)
    {
        out += std::to_string(t.m_line.m_iRow) + ":" +
               std::to_string(t.m_line.m_iCol) + "  " +
               TokenTypeToString(t.m_type) + "  " + t.m_szContent + "\n";
    }
    return out;
}

// LL(1) 预测分析过程（§4.11 表格形式）：<步骤> | <符号栈> | <剩余输入串> | <动作>
std::string DumpParseText(const CParser& parser, const std::string& szSrcFile)
{
    const std::vector<ParseStep>& trace = parser.GetTrace();

    std::string out = "# MiniC LL(1) 预测分析过程（设计文档 v4.0 §4.10 / §4.11）\n";
    out += "# 源文件: " + BaseName(szSrcFile) + "\n";
    out += "# 产生式: " + std::to_string(PROD_COUNT) + " 条；分析步骤: " +
           std::to_string(trace.size()) + "；结果: " +
           (parser.HasError() ? "有语法错误" : "接受") + "\n";
    out += "| 步骤 | 符号栈 | 剩余输入串 | 动作 |\n";
    out += "|---|---|---|---|\n";
    for (const auto& s : trace)
    {
        out += "| " + std::to_string(s.m_iStep) + " | `" + s.m_szStack + "` | `" +
               s.m_szInput + "` | " + s.m_szAction + " |\n";
    }
    if (parser.HasError())
    {
        out += "\n# 语法错误:\n";
        for (const auto& e : parser.GetErrors())
        {
            out += "#   " + std::to_string(e.m_loc.m_iRow) + ":" +
                   std::to_string(e.m_loc.m_iCol) + ": " + e.m_szMsg + "\n";
        }
    }
    return out;
}

} // namespace

int main(int argc, char* argv[])
{
    // ---------- 1. 命令行解析（未知选项报错并打印用法） ----------
    if (argc < 2)
    {
        PrintUsage(std::cerr);
        return 1;
    }

    // -h / --help / -V / --version 不需要源文件，单独先处理
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            PrintUsage(std::cout);
            return 0;
        }
        if (arg == "-V" || arg == "--version")
        {
            std::cout << kVersion << "\n";
            return 0;
        }
    }

    const std::string srcFile = argv[1];
    bool dumpTokens = false;
    bool dumpAst = false;
    bool dumpParse = false;
    bool dumpSymtab = false;
    bool dumpQuads = false;
    bool optimize = false;

    for (int i = 2; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--dump-tokens")      { dumpTokens = true; }
        else if (arg == "--dump-ast")    { dumpAst = true; }
        else if (arg == "--dump-parse")  { dumpParse = true; }
        else if (arg == "--dump-symtab") { dumpSymtab = true; }
        else if (arg == "--dump-quads")  { dumpQuads = true; }
        else if (arg == "-O" || arg == "--optimize") { optimize = true; }
        else
        {
            std::cerr << "minic: error: unknown option '" << arg << "'\n";
            PrintUsage(std::cerr);
            return 1;
        }
    }

    const std::string base = StripExtension(srcFile);

    // ---------- 2. 词法分析：CLex.OpenSource + Run ----------
    CLex lexer;
    if (!lexer.OpenSource(srcFile))
    {
        // 打开失败：错误已进 g_ErrorList，直接以非零退出
        return 1;
    }
    // 诊断上下文：让 EmitError/EmitWarning 能打印出错的源码行与 ^ 定位符。
    // 必须在 OpenSource 成功之后、Run 之前设置 —— 词法错误就是在 Run 里报的。
    g_pSourceText = &lexer.GetSource();
    lexer.Run();

    if (dumpTokens)
    {
        WriteTextFile(base + ".token.txt", DumpTokensText(lexer.GetTokenList()));
    }

    // ---------- 3. 语法分析：CParser.Parse（§4.10 LL(1) 预测分析表驱动） ----------
    CParser parser(lexer.GetTokenList());
    // 轨迹记录是 O(步数 × 输入长度) 的开销，只有真的要写 .parse.txt 时才打开。
    parser.SetTraceEnabled(dumpParse);
    ASTNodePtr root = parser.Parse();

    if (dumpParse)
    {
        WriteTextFile(base + ".parse.txt", DumpParseText(parser, srcFile));
    }

    if (dumpAst)
    {
        std::ostringstream oss;
        if (root)
        {
            root->Print(oss, 0);
        }
        WriteTextFile(base + ".ast.txt", oss.str());
    }

    // ---------- 4. 语义阶段：符号表登记 + 类型/作用域检查 ----------
    SymbolTable symtab;
    // 语义阶段无条件执行：一个语法错不应该把语义阶段该报的问题全部掩盖掉
    // （用户要求「把代码里的所有问题都报出来」）。
    // 代价：语法错会让 AST 残缺，语义可能报出次生错误（例如漏一个分号，
    // 后面的声明没建成节点，语义就把已声明的名字报成 undefined）。
    // 因此进入语义前记下「之前已有错」，置位 g_bErrorsMayBeSecondary，
    // 让这一段产生的诊断在打印时带上来源提示，和被真正写错的代码区分开。
    const bool bHadEarlierErrors = !g_ErrorList.empty();
    g_bErrorsMayBeSecondary = bHadEarlierErrors;
    {
        SemanticAnalyzer sema(symtab);
        sema.Analyze(root);
    }
    g_bErrorsMayBeSecondary = false;

    // ---------- 5. 中间代码：IRGenerator.Generate（阶段 07 已实现） ----------
    IRGenerator ir(symtab);
    ir.Generate(root);

    // ---------- 5.5 可选优化（-O / --optimize） ----------
    // 默认关闭：不加 -O 时 .quad.txt / .asm / 运行 stdout 与优化前逐字一致，
    // 全部既有基线不需要动。打开后只做「删掉不改变行为的东西」的窥孔优化：
    // 常量条件跳转、不可达代码、空跳、死临时量、死标号。
    minic_opt::OptStats optStats;
    if (optimize && g_ErrorList.empty())
    {
        // 全局（顶层语句）序列与每个过程的序列分别优化。
        // 注意是原地改 m_Codes：CCodeGen 构造时绑的就是这个成员的 const 引用，
        // 若换成一个新 vector，代码生成会读到优化前的旧内容。
        minic_opt::Optimize(ir.GetCodesMutable(), symtab, optStats);
        for (auto& p : symtab.m_ProcInfoTbl)
        {
            if (p.m_Codes.empty())
            {
                continue;
            }
            minic_opt::Optimize(p.m_Codes, symtab, optStats);
        }
        std::cerr << "minic: -O removed " << optStats.Removed()
              << " quad(s), rewrote " << optStats.Rewritten()
                  << " operand(s) [branch " << optStats.iFoldedBranch
                  << ", unreachable " << optStats.iUnreachable
                  << ", jumps " << optStats.iRedundantJump
                  << ", temps " << optStats.iDeadTemp
                  << ", labels " << optStats.iDeadLabel
              << ", const-prop " << optStats.iConstProp
              << ", cse " << optStats.iCse
              << ", dead-store " << optStats.iDeadStore
              << ", hoisted " << optStats.iHoisted << "]\n";
    }

    if (dumpQuads)
    {
        std::ostringstream oss;
        // 全局（main 过程）四元式
        oss << DumpQuadsText(ir, ir.GetCodes());
        // 函数定义的四元式（GenFuncDef → FlushToProc 挂入过程表项）
        for (const auto& p : symtab.m_ProcInfoTbl)
        {
            if (p.m_Codes.empty())
            {
                continue;
            }
            oss << "\n// --- proc: " << p.m_szName << " ---\n";
            oss << DumpQuadsText(ir, p.m_Codes);
        }
        WriteTextFile(base + ".quad.txt", oss.str());
    }

    // 符号表 dump 放在中间代码之后：标号表（-- Labels --）由 IR 阶段的
    // LABEL 四元式登记，早于 IR 写出会永远是空节。其余各节在语义阶段就已定型。
    if (dumpSymtab)
    {
        std::ostringstream oss;
        symtab.DumpSymTab(oss);
        WriteTextFile(base + ".sym.txt", oss.str());
    }

    // ---------- 6. 挑战版：CCodeGen.Generate 输出汇编文件 ----------
    // 只在前面各阶段全部无错时才生成汇编。
    // 否则语法/语义有错的输入（如 `print(1); int late(int x){...}`）会让
    // 残缺 AST 也走完 IR 与代码生成，落下一份只有空 main 的 .asm —— 那份文件
    // 能被 gcc 汇编、也能跑起来，但和用户写的程序毫无关系，属于误导性产物。
    if (g_ErrorList.empty())
    {
        CCodeGen cg(ir.GetCodes());
        cg.BindSymTab(symtab);          // 阶段 08：函数四元式在过程表里，需绑定符号表
        AsmOutput asmOut;
        if (cg.Generate(asmOut))
        {
            std::string asmText;
            for (const auto& line : asmOut.m_FileHeader) { asmText += line + "\n"; }
            for (const auto& line : asmOut.m_DataSection) { asmText += line + "\n"; }
            for (const auto& line : asmOut.m_CodeSection) { asmText += line + "\n"; }
            WriteTextFile(base + ".asm", asmText);
        }
    }

    // ---------- 7. 错误汇总：有错误按非零退出码结束 ----------
    if (!g_ErrorList.empty())
    {
        if (g_iSuppressedErrors > 0)
        {
            std::cerr << "minic: ("
                      << g_iSuppressedErrors
                      << " more error(s) suppressed; limit is "
                      << g_iMaxErrors << ")\n";
        }
        std::cerr << "minic: " << g_ErrorList.size()
                  << " error(s) found, compilation failed\n";
        return 1;
    }

    std::cout << "minic: compilation finished\n";
    return 0;
}
