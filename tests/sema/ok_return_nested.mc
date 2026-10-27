/* expect: ok */
/* 正例：return 藏在 if / while 里也算有 return，不应误报警告 */
int pick() {
    int n;
    n = 3;
    if (n > 2) {
        return n;
    }
    while (n < 5) {
        n = n + 1;
    }
    return 0;
}

void nothing() {
    int a;
    a = 1;
}

int main() {
    int r;
    r = pick();
    nothing();
    print(r);
    return 0;
}
