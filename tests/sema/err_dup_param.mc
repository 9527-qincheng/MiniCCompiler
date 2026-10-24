/* expect: err */
/* 错误：形参与函数体内局部变量重名 */
int f(int x) {
    int x;
    return x;
}
int main() {
    return f(1);
}
