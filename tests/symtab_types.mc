// 类型表按需登记：本程序只用了 int（变量 a、函数返回类型、字面量都是 int），
// 所以 --dump-symtab 的 Types 节只应出现 int 一行。
int main() {
    int a;
    a = 1;
    return 0;
}
