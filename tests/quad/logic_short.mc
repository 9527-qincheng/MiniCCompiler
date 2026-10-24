/* 短路求值：&& 与 || 必须展开成条件跳转，不能求值两侧再与/或 */
bool a;
bool b;
bool c;
int main() {
    a = true;
    b = false;
    c = a && b;
    c = a || b;
    return 0;
}
