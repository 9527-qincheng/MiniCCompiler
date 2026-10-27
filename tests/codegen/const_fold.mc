/* 常量折叠的运行结果：与不折叠时完全一致 */
int main() {
    print(1 + 2 * 3); print('\n');
    print(7 / 2); print('\n');
    print(1.5 * 2.0); print('\n');
    print(2 < 3); print('\n');
    print(10 - 3 - 2); print('\n');
    print(2 + 3 * 4 - 6 / 2); print('\n');
    print(-5 + 2); print('\n');
    return 0;
}
