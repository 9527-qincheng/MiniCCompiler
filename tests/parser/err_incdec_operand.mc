/* ++/-- 的错误用法：非左值（字面量、表达式）、非数值（bool 变量） */
int main() {
    int a;
    int b;
    bool f;
    a = 1;
    b = ++5;
    b = a++;
    f = true;
    f++;
    return b;
}
