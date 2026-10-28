/* 循环不变量外提的正例：
   gi 在循环体内没有被写过，gi * 2 每轮都算同一个值，应当被提到前导块；
   而 i * 4 依赖循环变量 i，必须留在循环体里。 */
int gi;

int main()
{
    int i;
    int n;
    int s;
    int a[4];

    gi = 5;
    i = 0;
    n = 4;
    s = 0;
    while (i < n)
    {
        a[i] = i * 4 + 1;
        s = s + a[i] + gi * 2;
        i = i + 1;
    }
    print(s);
    print('\n');
    return 0;
}
