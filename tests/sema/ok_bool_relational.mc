/* expect: ok */
/* bool 参与比较：C++ 式分层之后 a == b < c 这类链式比较可用 */
int a;
int b;
int c;
int r;
int main() {
    a = 2; b = 3; c = 5;
    r = a == b < c;
    r = a < b == c;
    r = a < b < c;
    r = (a < b) < c;
    r = a == (b < c);
    if (a < b == b < c) { print(r); }
    return 0;
}
