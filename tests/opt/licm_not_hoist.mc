/* 循环不变量外提的反例，三条都不许被提出来：
   一是除法 —— 除数 gi 虽然在循环里没变，但循环可能一次都不进，
   提到外面会把「不进循环」变成「一进来就除零」；
   二是乘的是循环变量 i —— 不是不变量；
   三是循环里有函数调用 —— 被调函数可能改写全局量，gi 不再恒定。 */
int gi;

int bump(int x)
{
    gi = gi + 1;
    return x;
}

int main()
{
    int i;
    int n;
    int s;

    gi = 8;
    i = 0;
    n = 3;
    s = 0;
    while (i < n)
    {
        s = s + gi / 2 + i * 3;
        s = s + bump(i);
        i = i + 1;
    }
    print(s);
    print('\n');
    return 0;
}
