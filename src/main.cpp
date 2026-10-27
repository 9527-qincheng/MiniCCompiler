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

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

void PrintUsage(std::ostream& os)
{
    os << "Usage: minic <source.mc> [--dump-tokens] [--dump-ast] [--dump-parse] "
          "[--dump-symtab] [--dump-quads]\n";
    os << "  --dump-tokens : write <src>.token.txt\n";
    os << "  --dump-ast    : write <src>.ast.txt\n";
    os << "  --dump-parse  : write <src>.parse.txt (LL(1) 预测分析过程, §4.11)\n";
    os << "  --dump-symtab : write <src>.sym.txt\n";
    os << "  --dump-quads  : write <src>.quad.txt\n";
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

// 四元式操作码文本名（--dump-quads；与 Quad::Print 的 opName 表保持一致）
const char* QuadOpName(QuadOp op)
{
    static const char* opName[] = {
        "NONE", "=", "+", "-", "*", "/",
        "JMP", "JZ", "JNZ", "LABEL", "RETURN", "CALL", "PRINT",
        "PARA", "INDEX", "ITOF", "FTOI",
        "==", "!=", "<", "<=", ">", ">="
    };
    const int i = static_cast<int>(op);
    return (i >= 0 && i < 23) ? opName[i] : "??";
}

// 四元式序列文本（Q6：操作数按索引转原文，供 --dump-quads 落盘）
std::string DumpQuadsText(const IRGenerator& ir, const CodeList& codes)
{
    std::string out;
    for (const auto& q : codes)
    {
        out += "(" + std::string(QuadOpName(q.m_eOp)) + ", " +
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

    const std::string srcFile = argv[1];
    bool dumpTokens = false;
    bool dumpAst = false;
    bool dumpParse = false;
    bool dumpSymtab = false;
    bool dumpQuads = false;

    for (int i = 2; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--dump-tokens")      { dumpTokens = true; }
        else if (arg == "--dump-ast")    { dumpAst = true; }
        else if (arg == "--dump-parse")  { dumpParse = true; }
        else if (arg == "--dump-symtab") { dumpSymtab = true; }
        else if (arg == "--dump-quads")  { dumpQuads = true; }
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
    lexer.Run();

    if (dumpTokens)
    {
        WriteTextFile(base + ".token.txt", DumpTokensText(lexer.GetTokenList()));
    }

    // ---------- 3. 语法分析：CParser.Parse（§4.10 LL(1) 预测分析表驱动） ----------
    CParser parser(lexer.GetTokenList());
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
    if (dumpSymtab)
    {
        std::ostringstream oss;
        symtab.DumpSymTab(oss);
        WriteTextFile(base + ".sym.txt", oss.str());
    }

    // ---------- 5. 中间代码：IRGenerator.Generate（阶段 07 已实现） ----------
    IRGenerator ir(symtab);
    ir.Generate(root);
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
        std::cerr << "minic: " << g_ErrorList.size()
                  << " error(s) found, compilation failed\n";
        return 1;
    }

    std::cout << "minic: compilation finished\n";
    return 0;
}
