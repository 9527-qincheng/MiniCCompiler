/* expect: ok */
/* 合法：内层块可以遮蔽外层同名变量，退出码 0、无诊断 */
int main() {
    int a;
    a = 1;
    {
        int a;
        a = 2;
        print(a);
    }
    print(a);
    return 0;
}
