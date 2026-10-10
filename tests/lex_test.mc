// lex_test.mc —— 词法分析模块测试用例
// 覆盖：int/float 常量、运算符、界符、关键字、注释、字符串、错误恢复

int a;
float b;

/* 这是多行注释
   应被词法分析跳过 */
int x = 10 + 20;  // 这是单行注释
float y = 3.14 * 2.5;

if (x) {
    b = y - 1.0;
}

string s = "hello world";

return 0;
