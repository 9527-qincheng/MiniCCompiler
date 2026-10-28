/* expect: ok */
// int 实参传给 float 形参属于常规算术转换，不应有诊断。
float f(float a) { return a; }
int main() {
    int i;
    i = 3;
    print(f(i));
    return 0;
}
