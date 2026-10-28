/* expect: warn:1 */
// char 只能存 -128..127，300 会静默截断成 44，这里给一条警告。
int main() {
    char c;
    c = 300;
    print(c);
    return 0;
}
