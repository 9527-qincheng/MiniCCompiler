/* expect: err */
/* bool 仍不参与算术与逻辑：比较运算放宽了（见 ok_bool_relational），算术没有 */
bool b;
int a;
int main() {
    b = true;
    a = b + 1;
    b = a && 1;
    return 0;
}
