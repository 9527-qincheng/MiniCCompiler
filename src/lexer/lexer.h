// ============================================================================
// MiniC 编译器 —— 词法分析模块 CLex（第 4.2 节 + 第 6.2 节接口契约）
// 依据文档定稿：Q11 手写简化版转换表 m_szLexTbl[50][130]、Q12 常量词法登记
// ============================================================================
#ifndef MINIC_LEXER_LEXER_H
#define MINIC_LEXER_LEXER_H

#include "lexer/token.h"

#include <map>
#include <string>

// ----------------------------------------------------------------------------
// 4.2.1 词法分析器 CLex（参考书声明 2-2 / 程序 2-2）
// 接口按第 6 章契约：OpenSource / Run / GetTokenList
// ----------------------------------------------------------------------------
class CLex
{
public:
    CLex();
    ~CLex() = default;

    // --- 第 6 章接口契约 ---
    bool OpenSource(const std::string& szFileName);  // 打开源文件（读入 m_szSource）
    bool Run();                                     // 扫描源文件，产出 Token 序列
    const TokenList& GetTokenList() const;           // 获取 Token 序列（供 CParser）

    // --- 第 4 章扩展接口（参考书风格，供调试/测试） ---
    void        SetFileName(const std::string& szFileName) { m_szFileName = szFileName; }
    const std::string& GetFileName() const { return m_szFileName; }
    const std::string& GetSource()   const { return m_szSource; }

private:
    // 转换表：m_szLexTbl[状态][字符类别] -> 下一状态 / 动作（Q11 手写简化版）
    static const int m_szLexTbl[50][130];
    // 关键字表：map<string, TokenType>（参考书声明 2-3）
    std::map<std::string, TokenType> m_KeywordTbl;

    std::string m_szSource;     // 源文件全文
    std::string m_szFileName;   // 源文件名
    size_t      m_iPos = 0;     // 当前扫描位置
    int         m_iRow = 1;     // 当前行号
    int         m_iCol = 1;     // 当前列号

    TokenList   m_TokenList;    // Token 序列（m_pTokenList 的成员化）

    // 常量登记表：Q12 词法登记 m_ConstTexts（Token::m_iConstIdx 指向下标）
    std::vector<std::string> m_ConstTexts;

    // 私有扫描子程序
    void InitKeywordTable();                            // 初始化关键字表
    void SkipSpace();                                   // 跳过空白/换行
    void ScanIdent();                                   // 扫描标识符/关键字
    void ScanNumber();                                  // 扫描整数/浮点常量
    void ScanOperator();                                // 扫描运算符/界符
    int  LookupKeyword(const std::string& word) const;  // 查关键字表（-1 表示非关键字）
};

#endif // MINIC_LEXER_LEXER_H
