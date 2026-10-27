/* expect: ok */
/* char 参与算术与关系运算：按整型提升，不报错 */
char c;
int n;
int main() {
    c = 'A';
    n = c + 1;
    if (c == 'A') { print(n); }
    return 0;
}
