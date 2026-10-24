/* expect: warn:1 */
/* 告警：float 赋给 int 会截断（Q9），但编译通过、退出码 0 */
int a;
float b;
int main() {
    b = 1.5;
    a = b;
    return a;
}
