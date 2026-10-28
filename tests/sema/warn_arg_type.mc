/* expect: warn:1 */
// bool 实参传给 int 形参：不是 C 的常规算术转换，基本只会是写错。
int f(int a) { return a; }
int main() {
    bool t;
    t = true;
    print(f(t));
    return 0;
}
