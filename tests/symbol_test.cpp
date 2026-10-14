// ============================================================================
// MiniC 编译器 —— 符号表作用域链单元测试
// 覆盖：作用域栈出入、内层遮蔽外层、链式查找、当前作用域查重、
//       扁平下标稳定、数组登记、RecVarTbl 返回值、全局作用域不可弹出
// 编译运行：
//   g++ -std=c++17 -Isrc tests/symbol_test.cpp src/symbol/symbol.cpp \
//       src/lexer/token.cpp src/sema/ir.cpp -o build/symbol_test.exe && build/symbol_test.exe
// 期望：stdout 打印 ALL SCOPE-CHAIN ASSERTIONS PASSED，退出码 0
// ============================================================================
#include "symbol/symbol.h"
#include "sema/ir.h"   // 完整 Quad 定义（SymbolTable 析构需要，缺则报 incomplete type）

#include <cassert>
#include <iostream>

int main()
{
    SymbolTable sym;

    sym.RecVar("a", TokenType::TOKEN_INT, 1, 1);       // 全局 a (int, 行1)
    sym.RecVar("x", TokenType::TOKEN_INT, 2, 1);       // 全局 x

    sym.EnterScope(ScopeType::SCOPE_BLOCK);            // 进入块作用域
    sym.RecVar("a", TokenType::TOKEN_FLOAT, 3, 5);     // 块内 a (float) 遮蔽全局

    // ① 内层遮蔽：找到的是内层（float, 行3, scope 2）
    const VarEntry* inner = sym.SearchVar("a");
    assert(inner != nullptr);
    assert(inner->m_type == TokenType::TOKEN_FLOAT);
    assert(inner->m_iLine == 3);
    assert(inner->m_iScope == 2);

    // ② 链式查找：外层 x 在块内依然可见
    const VarEntry* xv = sym.SearchVar("x");
    assert(xv != nullptr && xv->m_iLine == 2);

    // ③ 当前作用域重名：报错但返回已存在的那个（不新建）
    VarEntry* dup = sym.RecVar("a", TokenType::TOKEN_INT, 4, 1);
    assert(dup != nullptr && dup->m_type == TokenType::TOKEN_FLOAT);

    // ④ 退出作用域后回到外层（int, 行1）
    sym.ExitScope();
    const VarEntry* outer = sym.SearchVar("a");
    assert(outer != nullptr);
    assert(outer->m_type == TokenType::TOKEN_INT);
    assert(outer->m_iLine == 1);

    // ⑤ 扁平下标必须稳定：全局 a 永远是 idx 0（ir.cpp 依赖这个）
    assert(sym.m_VarInfoTbl[0].m_szName == "a");
    assert(sym.m_VarInfoTbl[0].m_type == TokenType::TOKEN_INT);

    // ⑥ 数组登记
    VarEntry* arr = sym.RecArray("buf", TokenType::TOKEN_INT, 10, 9, 1);
    assert(arr && arr->m_bIsArray && arr->m_iArraySize == 10);

    // ⑦ RecVarTbl 返回值 == 扁平下标（IR 模块按此下标回查）
    int idx = sym.RecVarTbl("y", TokenType::TOKEN_INT, 11);
    assert(idx == (int)sym.m_VarInfoTbl.size() - 1);
    assert(sym.m_VarInfoTbl[idx].m_szName == "y");

    // ⑧ 全局作用域不可弹出：多余的 ExitScope 不影响全局表项
    sym.ExitScope(); sym.ExitScope(); sym.ExitScope();
    assert(sym.m_VarInfoTbl[0].m_szName == "a");

    std::cout << "ALL SCOPE-CHAIN ASSERTIONS PASSED\n";
    return 0;
}
