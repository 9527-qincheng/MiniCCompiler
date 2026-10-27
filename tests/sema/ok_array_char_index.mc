/* expect: ok */
int main()
{
    char buf[200];
    buf['a'] = 65;
    print(buf['a']);
    return 0;
}