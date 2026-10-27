/* 自增自减（挑战版 ++/--）：前缀取新值、后缀取旧值 */
int a;
int b;

int main() {
    a = 5;
    b = a++;
    print(b);
    print(a);
    b = ++a;
    print(b);
    print(a);
    return 0;
}
