int x;
int acc;
int i;
int s;
int a[4];
float f;

int main()
{
    x = 5;
    acc = 0;
    if (x == 5) { acc = acc + 1; }
    if (x != 5) { acc = acc + 2; }
    if (x < 6) { acc = acc + 4; }
    if (x <= 5) { acc = acc + 8; }
    if (x > 4) { acc = acc + 16; }
    if (x >= 5) { acc = acc + 32; }
    print(acc); print('\n');

    i = 0;
    s = 0;
    while (i < 5) { s = s + i; i = i + 1; }
    print(s); print('\n');

    i = 0;
    while (i < 4) { a[i] = i + 1; i = i + 1; }
    s = 0;
    i = 0;
    while (i < 4) { s = s + a[i]; i = i + 1; }
    print(s); print('\n');

    f = 2.5;
    if (f < 3.0) { print(7); } else { print(8); }
    print('\n');
    return 0;
}
