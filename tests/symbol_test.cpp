#include "symbol/symbol.h"
#include <cassert>

void test_symbol_table()
{
    SymbolTable st;
    // 进入全局作用域
    st.EnterScope(ScopeType::SCOPE_GLOBAL);
    auto pG_a = st.RecVar("a", TokenType::TOKEN_INT, 1, 1);
    assert(pG_a != nullptr);

    // 进入块作用域
    st.EnterScope(ScopeType::SCOPE_BLOCK);
    auto pB_a = st.RecVar("a", TokenType::TOKEN_INT, 2, 2);
    assert(pB_a != nullptr);

    // 查找应当命中内层a
    auto findA = st.SearchVar("a");
    assert(findA->m_scopeLevel == 1);

    st.ExitScope();
    // 退出块，应当找到全局a
    findA = st.SearchVar("a");
    assert(findA->m_scopeLevel == 0);

    st.ExitScope();
    assert(st.SearchVar("a") == nullptr);
}
