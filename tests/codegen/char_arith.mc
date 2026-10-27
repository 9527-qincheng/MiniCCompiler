/* char 参与算术与关系运算：按整型提升（C 语义） */
char c;
char arr[2];
int n;
int main() {
    c = 'A';
    n = c + 1;
    print(n); print('\n');
    print(c < 'a'); print('\n');
    print(c == 65); print('\n');
    arr[0] = 'x';
    arr[0] = arr[0] + 1;
    print(arr[0]); print('\n');
    c++;
    print(c); print('\n');
    return 0;
}
