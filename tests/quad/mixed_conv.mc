/* 混合类型：int 参与 float 运算前插 ITOF，结果赋给 int 时插 FTOI */
int a;
float b;
int main() {
    b = 1.5;
    a = b + 1;
    return a;
}
