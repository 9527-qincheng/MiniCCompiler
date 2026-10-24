/* expect: err */
/* 错误：函数定义只允许在最外层 */
int main() {
    int f() { return 1; }
    return f();
}
