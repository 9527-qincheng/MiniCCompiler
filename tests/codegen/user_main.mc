// 用户自己定义 int main() —— 文档《MiniC语法支持清单》7.1~7.3 的推荐写法。
// 回归点：入口 main 与用户定义的 main 不能生成两个 `main:` 标号（as 报
//         Error: symbol `main' is already defined）。
int fact(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * fact(n - 1);
}

int main() {
    int a[4];
    int i;
    i = 0;
    while (i < 4) {
        a[i] = i * i;
        i = i + 1;
    }
    print(a[3]);
    print(fact(5));
    print(1 + 2 * 3);
    return 0;
}
