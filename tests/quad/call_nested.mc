/* 嵌套调用：内层调用的实参不能与外层 PARA 交错 */
int g(int x) {
    return x + 1;
}
int f(int x, int y) {
    return x * y;
}
int a;
int main() {
    a = 2;
    print(f(g(a), g(a + 1)));
    return 0;
}
