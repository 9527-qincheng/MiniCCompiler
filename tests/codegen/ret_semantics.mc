// 返回值语义回归：裸 return / 无 return 落空出口 / main 的退出码。
// 回归点（修复前实测）：
//   1) `int f() { }` 无 return，返回 %eax 里的残值（曾输出 2043900272）；
//   2) 出口标号之后无条件 xorl %eax,%eax 把 `return 5;` 的返回值清零，
//      `int main() { return 5; }` 的进程退出码是 0 而不是 5；
//   3) 裸 `return;`（操作数 OP_NONE）什么都不发，同样带出残值。
int no_ret() {
}

int ret_val() {
    return 42;
}

float f_ret() {
    return 2.5;
}

float f_no_ret() {
}

int early(int n) {
    if (n > 0) {
        return 1;
    }
    return 0;
}

int main() {
    print(no_ret()); print('\n');
    print(ret_val()); print('\n');
    print(f_ret()); print('\n');
    print(f_no_ret()); print('\n');
    print(early(5)); print('\n');
    print(early(-5)); print('\n');
    return 0;
}
