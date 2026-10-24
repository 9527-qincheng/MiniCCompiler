/* expect: ok */
/* 合法：void 函数裸 return */
void show() {
    print(7);
    return;
}
int main() {
    show();
    return 0;
}
