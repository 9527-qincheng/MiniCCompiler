/* 自增自减（挑战版 ++/--）：前缀取新值、后缀取旧值 */
int a;
int b;
int arr[3];

int main() {
    a = 5;
    b = a++;
    b = ++a;
    a++;
    ++a;
    arr[0] = 7;
    arr[0]++;
    ++arr[1];
    return a;
}
