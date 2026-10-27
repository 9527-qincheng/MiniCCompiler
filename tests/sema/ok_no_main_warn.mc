/* expect: warn:1 */
/* 告警：有函数定义、却没有 main，也没有任何顶层语句 —— 生成的入口什么也不做 */
int helper(int x) {
    return x + 1;
}

void show(int v) {
    print(v);
}
