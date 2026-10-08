// ============================================================================
// MiniC 编译器 —— main 入口（第 6.7 节约定，Q7 四类产物落盘）
// 命令行：minic <源文件> [--dump-tokens] [--dump-ast] [--dump-symtab] [--dump-quads]
// 流水线：CLex.Run() → CParser.Parse() → SymbolTable 语义检查 →
//         IRGenerator.Generate() → （挑战版）CCodeGen.Generate()
// 退出码：0 成功 / 1 出错（错误汇总 g_ErrorList，含行列号 Q8）
// ============================================================================
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "symbol/symbol.h"
#include "sema/ir.h"
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
    os << "Usage: minic <source.mc> [--dump-tokens] [--dump-ast] "
          "[--dump-symtab] [--dump-quads]\n";
    os << "  --dump-tokens : write <src>.token.txt\n";
    os << "  --dump-ast    : write <src>.ast.txt\n";
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
    bool dumpSymtab = false;
    bool dumpQuads = false;

    for (int i = 2; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--dump-tokens")      { dumpTokens = true; }
        else if (arg == "--dump-ast")    { dumpAst = true; }
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

    // ---------- 3. 语法分析：CParser.Parse（骨架阶段 TODO） ----------
    CParser parser(lexer.GetTokenList());
    ASTNodePtr root = parser.Parse();

    if (dumpAst)
    {
        std::ostringstream oss;
        if (root)
        {
            root->Print(oss, 0);
        }
        else
        {
            oss << "// AST dump: parser core not implemented yet (TODO)\n";
        }
        WriteTextFile(base + ".ast.txt", oss.str());
    }

    // ---------- 4. 语义阶段：SymbolTable（骨架阶段基础登记/查重可用） ----------
    SymbolTable symtab;
    // TODO: 遍历 AST 执行语法制导语义动作（登记/查重/类型检查/常量回填）
    if (dumpSymtab)
    {
        std::ostringstream oss;
        symtab.DumpSymTab(oss);
        WriteTextFile(base + ".sym.txt", oss.str());
    }

    // ---------- 5. 中间代码：IRGenerator.Generate（骨架阶段 TODO） ----------
    IRGenerator ir(symtab);
    ir.Generate(root);
    if (dumpQuads)
    {
        std::ostringstream oss;
        oss << "// MiniC quads (TODO: generator core not implemented yet)\n";
        for (const auto& q : ir.GetCodes())
        {
            q.Print(oss);
        }
        WriteTextFile(base + ".quad.txt", oss.str());
    }

    // ---------- 6. 挑战版：CCodeGen.Generate 输出汇编文件 ----------
    CCodeGen cg(ir.GetCodes());
    AsmOutput asmOut;
    if (cg.Generate(asmOut))
    {
        std::string asmText;
        for (const auto& line : asmOut.m_FileHeader) { asmText += line + "\n"; }
        for (const auto& line : asmOut.m_DataSection) { asmText += line + "\n"; }
        for (const auto& line : asmOut.m_CodeSection) { asmText += line + "\n"; }
        WriteTextFile(base + ".asm", asmText);
    }

    // ---------- 7. 错误汇总：有错误按非零退出码结束 ----------
    if (!g_ErrorList.empty())
    {
        std::cerr << "minic: " << g_ErrorList.size()
                  << " error(s) found, compilation failed\n";
        return 1;
    }

    std::cout << "minic: compilation finished (skeleton demo)\n";
    return 0;
}
