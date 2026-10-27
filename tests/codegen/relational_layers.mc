int a;
int b;
int c;
int main()
{
    a = 2; b = 3; c = 5;
    print(a == b < c); print('\n');
    print(a < b == c); print('\n');
    print(a < b < c); print('\n');
    print(a < b == b < c); print('\n');
    return 0;
}