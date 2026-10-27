/* expect: err */
int add(int a, int b) {
    return a + b;
}
int main() {
    int r;
    r = add(1, 2, 3);
    return 0;
}
