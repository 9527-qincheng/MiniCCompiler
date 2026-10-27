// 覆盖目标代码阶段此前无回归的三类场景：
//   1) char / bool 作为函数返回类型（返回值从 %al 取，需保证高位干净）
//   2) 局部变量遮蔽同名全局变量；形参遮蔽同名全局变量
//   3) 大栈帧（>4KB）与数组元素宽度差异
// 期望输出见 scope_ret.expected.txt

int gx;
int garr[4];
char gca[6];
float gf[3];

char pick(int i) {
    char c;
    c = 'a';
    if (i == 1) { c = 'b'; }
    if (i == 2) { c = 'c'; }
    return c;
}

bool isPos(int n) {
    bool r;
    r = false;
    if (n > 0) { r = true; }
    return r;
}

// 形参 x 遮蔽全局 x
int shadowParam(int x) {
    return x * 2;
}

// 大栈帧：1000 个 int 局部数组 = 4000 字节
int bigFrame() {
    int big[1000];
    int i;
    int sum;
    sum = 0;
    i = 0;
    while (i < 1000) {
        big[i] = i;
        i = i + 1;
    }
    i = 0;
    while (i < 1000) {
        sum = sum + big[i];
        i = i + 1;
    }
    return sum;
}

int main() {
    int gx;
    int i;
    float fsum;
    char c;

    // 局部 gx 遮蔽全局 gx
    gx = 100;
    print(gx); print('\n');
    print(shadowParam(5)); print('\n');
    print(garr[0]); print('\n');

    // char 返回类型
    i = 0;
    while (i < 3) {
        print(pick(i)); print('\n');
        i = i + 1;
    }

    // bool 返回类型
    print(isPos(3)); print('\n');
    print(isPos(-3)); print('\n');
    print(isPos(0)); print('\n');

    // 全局 char 数组逐元素写读（元素宽度 1）
    gca[0] = 'x';
    gca[1] = 'y';
    gca[2] = 'z';
    i = 0;
    while (i < 3) {
        print(gca[i]); print('\n');
        i = i + 1;
    }

    // 全局 float 数组求和（元素宽度 4）
    gf[0] = 1.5;
    gf[1] = 2.5;
    gf[2] = 3.5;
    fsum = 0.0;
    i = 0;
    while (i < 3) {
        fsum = fsum + gf[i];
        i = i + 1;
    }
    print(fsum); print('\n');

    // 大栈帧
    print(bigFrame()); print('\n');

    // char 变量与赋值
    c = 'Z';
    print(c); print('\n');

    return 0;
}
