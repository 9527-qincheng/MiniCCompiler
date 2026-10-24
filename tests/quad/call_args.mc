/* 调用：实参先全部求值，再逐个发 PARA，最后 CALL */
int f(int x, int y) {
    return x - y;
}
int a;
int b;
int main() {
    a = 10;
    b = 3;
    print(f(a, b));
    return 0;
}
