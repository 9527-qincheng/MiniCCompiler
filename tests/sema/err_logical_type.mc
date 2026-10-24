/* expect: err */
/* 错误：&& 要求 bool 操作数 */
int a;
bool c;
int main() {
    a = 1;
    c = a && true;
    return 0;
}
