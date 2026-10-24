/* expect: ok */
/* 合法：bool 用作条件 */
bool flag;
int main() {
    flag = true;
    if (flag) { print(1); }
    while (flag) { flag = false; }
    return 0;
}
