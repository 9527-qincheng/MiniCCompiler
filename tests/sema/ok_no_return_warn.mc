/* expect: warn:1 */
/* 告警：非 void 函数没有 return 语句，调用方会拿到未定义值 */
int no_ret() {
    int a;
    a = 42;
}

int main() {
    int r;
    r = no_ret();
    print(r);
    return 0;
}
