/* expect: err */
/* 错误：整型常量下标越界（编译期可判），两处：上界与负数 */
int main() {
    int a[3];
    int b;
    a[0] = 1;
    a[5] = 2;
    b = a[-1];
    return b;
}
