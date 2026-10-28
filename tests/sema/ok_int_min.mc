/* expect: ok */
// -2147483648 在 AST 里是 UnOp(-, Num(2147483648))，不能被误报为越界。
int main() {
    int a;
    a = -2147483648;
    print(a);
    return 0;
}
