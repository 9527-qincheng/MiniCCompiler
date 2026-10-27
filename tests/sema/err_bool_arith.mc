/* expect: err */
/* bool 仍然不参与算术与关系：char 放宽了，bool 没有 */
bool b;
int a;
int main() {
    b = true;
    a = b + 1;
    if (a == b) { print(a); }
    return 0;
}
