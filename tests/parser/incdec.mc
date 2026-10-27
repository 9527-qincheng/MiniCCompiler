/* 自增自减的四种写法：前缀/后缀在表达式中，以及单独成句 */
int a;
int arr[3];

int main() {
    a = 1;
    a++;
    ++a;
    a--;
    --a;
    arr[0] = 2;
    arr[0]++;
    ++arr[1];
    return a++;
}
