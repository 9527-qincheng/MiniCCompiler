/* 多个函数：每个函数一段 `// --- proc: 名字 ---`，按定义顺序 */
int add(int x, int y) {
    return x + y;
}
int twice(int x) {
    return add(x, x);
}
int main() {
    print(twice(5));
    return 0;
}
