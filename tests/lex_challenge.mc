// lex_challenge.mc —— v4.0 挑战版词法全覆盖
bool flag = true;
char c = 'a';
int a[10];

if (a == 10) {
    print a;
} else if (a != 10) {
    while (a <= 20) {
        a = a + 1;
    }
}

float x = 3.14;
if (x >= 1.5 && x <= 2.5 || x == 0.0) {
    x = x * 2.0;
}

if (a > b && !flag) {
    return false;
}

return 0;
