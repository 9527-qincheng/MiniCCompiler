/* expect: err */
// 整型字面量超出 int 表示范围：MiniC 没有 long，只能报错。
int main() {
    int a;
    a = 2147483649;
    print(a);
    return 0;
}
