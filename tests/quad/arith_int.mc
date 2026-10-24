/* 整型四则与优先级：* / 先于 + -，左侧结合 */
int a;
int b;
int c;
int main() {
    a = 1;
    b = 2;
    c = a + b * 3 - 4 / 2;
    return c;
}
