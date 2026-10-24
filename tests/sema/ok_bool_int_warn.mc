/* expect: warn:1 */
/* 告警：INT 与 BOOL 之间的可疑赋值，编译通过 */
bool flag;
int n;
int main() {
    flag = true;
    n = flag;
    return n;
}
