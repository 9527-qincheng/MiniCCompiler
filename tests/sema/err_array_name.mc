/* expect: err */
/* 错误：数组名不带下标就参与求值（MiniC 无指针，不存在"数组退化为首地址"） */
int main() {
    int a[3];
    int b;
    a[0] = 7;
    b = a;
    print(a);
    b = a + 1;
    return b;
}
