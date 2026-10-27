/* expect: err */
int give() {
    return;
}
int main() {
    int r;
    r = give();
    return 0;
}
