/* expect: ok */
int add(int a, int b) {
    return a + b;
}
void show(int v) {
    print(v);
    print('\n');
    return;
}
int main() {
    int r;
    r = add(1, 2);
    print(r);
    print('\n');
    show(r);
    return 0;
}
