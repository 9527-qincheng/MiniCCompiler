/* 常量折叠（挑战版 C4）：编译期能算的表达式不生成四元式 */
int a;
float f;
int main() {
    a = 1 + 2 * 3;
    print(a);
    print(7 / 2);
    print(1.5 * 2.0);
    print(2 < 3);
    print(10 - 3 - 2);
    print(2 + 3 * 4 - 6 / 2);
    f = 1.5 * 2.0;
    print(f);
    a = 1;
    print(a + 2 * 3);
    return 0;
}
