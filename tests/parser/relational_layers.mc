int a;
int b;
int c;
int r;
int main()
{
    r = a == b < c;
    r = a < b == c;
    r = a < b < c;
    r = (a < b) == (c < a);
    return 0;
}
