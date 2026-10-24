/* expect: err */
/* 错误：同一作用域重复声明 */
int a;
int a;
int main() {
    a = 1;
    return a;
}
