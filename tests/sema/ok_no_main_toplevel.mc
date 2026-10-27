/* expect: ok */
/* 正例：没有 main，但顶层有语句 —— 由顶层语句合成入口，不应报警告 */
int helper(int x) {
    return x + 1;
}

int r;
r = helper(41);
print(r);
print('\n');
