/* while：入口标号 + JZ 出口 + 体 + JMP 回入口 + 出口标号 */
int a;
int main() {
    a = 0;
    while (a < 5) {
        a = a + 1;
    }
    return a;
}
