int a;
int b;
int c;
int main()
{
    a = 2; b = 3; c = 5;
    a = a == b < c;
    a = a < b == c;
    a = a < b < c;
    return 0;
}