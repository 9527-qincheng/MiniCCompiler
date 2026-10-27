/* expect: err */
void show(int a) {
    return a;
}
int main() {
    show(1);
    return 0;
}
