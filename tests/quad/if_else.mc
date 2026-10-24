/* if-else：JZ 跳假出口，then 末尾 JMP 跳过 else */
int a;
int b;
int main() {
    a = 3;
    b = 4;
    if (a < b) { a = 10; } else { a = 20; }
    return a;
}
