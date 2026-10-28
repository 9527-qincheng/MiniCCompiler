int arr[4];

int main()
{
    int i;
    int s;
    i = 0;
    s = 0;
    while (i < 4)
    {
        arr[i] = i * 3 + 1;
        s = s + arr[i];
        i = i + 1;
    }
    print(s);
    print('\n');
    return 0;
}
