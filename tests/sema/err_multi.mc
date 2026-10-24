/* expect: err */
/* 错误：多个语义错误应全部报出，而不是停在第一个 */
int a;
int a;
int main() {
    b = 1;
    c = 2;
    return 0;
}
