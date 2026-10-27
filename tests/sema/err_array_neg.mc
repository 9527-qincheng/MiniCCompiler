/* expect: err */
int main()
{
    int a[3];
    a[-1] = 1;
    return 0;
}