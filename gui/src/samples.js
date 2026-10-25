'use strict';
/* ============================================================
   MiniC 编译器 GUI — 示例源码库
   ------------------------------------------------------------
   每个示例都在真实工具链上实测过：
       build/minic.exe 编译  →  gcc -x assembler 汇编链接  →  运行
   成功示例的 stdout 与 output 字段逐字一致；
   错误示例要求 minic 退出码为 1，且 stderr 首行与 err 字段一致。

   gui/test/sample-smoke.js 会把这里每一条重跑一遍。改完示例请跑：
       node gui/test/sample-smoke.js
   ============================================================ */

const SAMPLES = [
  /* --------------------------- 基础 --------------------------- */
  {
    id: 'minimal',
    group: '基础',
    label: '最小程序',
    name: 'minimal.mc',
    output: '1',
    text: `/* 最小程序：一条 print + 一个 return */
int main() {
    print(1);
    return 0;
}
`,
  },
  {
    id: 'gcd',
    group: '基础',
    label: '最大公约数',
    name: 'gcd-demo.mc',
    output: '12',
    // return t; → gcd(36,24)=12，进程退出码也是 12（返回值语义的直接体现）
    exitCode: 12,
    text: `/* 最大公约数：辗转相减法 */
int a;
int b;

int main() {
    int t;
    a = 36;
    b = 24;
    while (a != b) {
        if (a > b) a = a - b;
        else b = b - a;
    }
    t = a;
    print(t);
    return t;
}
`,
  },
  {
    id: 'lex',
    group: '基础',
    label: '词法演示',
    name: 'lex-demo.mc',
    output: '42\n3.14\n1\nx\n40',
    text: `/* 词法演示：四种基本类型 + 各类型字面量 */
int count;
float ratio;
bool flag;
char mark;

int main() {
    count = 42;
    ratio = 3.14;
    flag = true;
    mark = 'x';

    print(count);
    print(ratio);
    print(flag);
    print(mark);

    while (count > 40) {
        count = count - 1;
    }
    print(count);
    return 0;
}
`,
  },

  /* --------------------------- 类型 --------------------------- */
  {
    id: 'float',
    group: '类型',
    label: '浮点运算',
    name: 'float-demo.mc',
    output: '19.6349\n5.5\n3\n3.5\n0.333333',
    text: `/* 浮点运算：面积、平均值、整除与浮点除的差别 */
float pi;
float r;
float area;
float sum;
float avg;
int count;

int main() {
    pi = 3.14159;
    r = 2.5;
    area = pi * r * r;
    print(area);

    sum = 0.0;
    count = 1;
    while (count <= 10) {
        sum = sum + count;
        count = count + 1;
    }
    avg = sum / 10.0;
    print(avg);

    print(7 / 2);
    print(7.0 / 2.0);
    print(1.0 / 3.0);
    return 0;
}
`,
  },
  {
    id: 'char-bool',
    group: '类型',
    label: '字符与布尔',
    name: 'char-bool-demo.mc',
    output: 'M\ni\nn\nC\n!\n1\n3\n0',
    text: `/* 字符与布尔：char 数组逐字输出、&& || ! 求值 */
char msg[5];
bool ok;

int main() {
    int i;
    msg[0] = 'M';
    msg[1] = 'i';
    msg[2] = 'n';
    msg[3] = 'C';
    msg[4] = '!';

    i = 0;
    while (i < 5) {
        print(msg[i]);
        i = i + 1;
    }

    ok = true;
    if (ok && true) print(1);
    if (!ok || false) print(2);
    ok = false;
    if (!ok) print(3);
    print(ok);
    return 0;
}
`,
  },

  /* --------------------------- 函数 --------------------------- */
  {
    id: 'nested-call',
    group: '函数',
    label: '多实参与嵌套调用',
    name: 'nested-call-demo.mc',
    output: '7\n6\n13\n33',
    text: `/* 函数调用：多实参 + 实参本身又是函数调用 */
int add(int x, int y) {
    return x + y;
}

int inc(int x) {
    return x + 1;
}

int add3(int x, int y, int z) {
    return x + y + z;
}

int main() {
    print(add(3, 4));
    print(add3(1, 2, 3));
    print(add(inc(1), inc(10)));
    print(add3(inc(0), inc(10), inc(20)));
    return 0;
}
`,
  },
  {
    id: 'recursion',
    group: '函数',
    label: '递归',
    name: 'recursion-demo.mc',
    output: '1\n2\n6\n24\n120\n720\n0\n1\n1\n2\n3\n5\n8\n13\n21\n34\n55',
    text: `/* 递归：阶乘 1!~6! 与斐波那契 F(0)~F(10) */
int fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}

int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    int i;

    i = 1;
    while (i <= 6) {
        print(fact(i));
        i = i + 1;
    }

    i = 0;
    while (i <= 10) {
        print(fib(i));
        i = i + 1;
    }
    return 0;
}
`,
  },
  {
    id: 'primes',
    group: '函数',
    label: '素数筛选',
    name: 'primes-demo.mc',
    output: '2\n3\n5\n7\n11\n13\n17\n19\n23\n29\n31\n37\n41\n43\n47\n15',
    text: `/* 素数筛选：MiniC 没有 % 运算符，用 x - (x/i)*i 判断整除 */
int isPrime(int x) {
    int i;
    if (x < 2) return 0;
    i = 2;
    while (i * i <= x) {
        if (x - (x / i) * i == 0) return 0;
        i = i + 1;
    }
    return 1;
}

int main() {
    int n;
    int c;
    c = 0;
    n = 2;
    while (n <= 50) {
        if (isPrime(n)) {
            print(n);
            c = c + 1;
        }
        n = n + 1;
    }
    print(c);
    return 0;
}
`,
  },

  /* --------------------------- 数组与控制流 --------------------------- */
  {
    id: 'bubble',
    group: '数组与控制流',
    label: '冒泡排序',
    name: 'bubble-sort.mc',
    output: '3\n7\n15\n19\n24\n42\n61\n88',
    text: `/* 冒泡排序：8 个整数升序 */
int a[8];

int main() {
    int i;
    int j;
    int t;
    int n;

    n = 8;
    a[0] = 42;
    a[1] = 7;
    a[2] = 19;
    a[3] = 3;
    a[4] = 88;
    a[5] = 15;
    a[6] = 61;
    a[7] = 24;

    i = 0;
    while (i < n - 1) {
        j = 0;
        while (j < n - 1 - i) {
            if (a[j] > a[j + 1]) {
                t = a[j];
                a[j] = a[j + 1];
                a[j + 1] = t;
            }
            j = j + 1;
        }
        i = i + 1;
    }

    i = 0;
    while (i < n) {
        print(a[i]);
        i = i + 1;
    }
    return 0;
}
`,
  },
  {
    id: 'stats',
    group: '数组与控制流',
    label: '数组统计',
    name: 'array-stats.mc',
    output: '91\n1\n415\n41',
    text: `/* 数组统计：最大值、最小值、总和、平均值 */
int data[10];

int main() {
    int i;
    int max;
    int min;
    int sum;

    data[0] = 23;
    data[1] = 5;
    data[2] = 91;
    data[3] = 47;
    data[4] = 12;
    data[5] = 68;
    data[6] = 33;
    data[7] = 79;
    data[8] = 1;
    data[9] = 56;

    max = data[0];
    min = data[0];
    sum = 0;
    i = 0;
    while (i < 10) {
        if (data[i] > max) max = data[i];
        if (data[i] < min) min = data[i];
        sum = sum + data[i];
        i = i + 1;
    }

    print(max);
    print(min);
    print(sum);
    print(sum / 10);
    return 0;
}
`,
  },
  {
    id: 'nest-loop',
    group: '数组与控制流',
    label: '嵌套循环与多分支',
    name: 'nested-loop.mc',
    output: '0\n1\n1\n1\n2\n0\n1\n1\n2\n2\n0\n1\n2\n2\n2\n0\n1683',
    text: `/* 嵌套循环 + 多分支 if：打印 4x4 比较矩阵，再求 1~100 内 3 的倍数之和 */
int main() {
    int i;
    int j;
    int total;

    i = 1;
    while (i <= 4) {
        j = 1;
        while (j <= 4) {
            if (i == j) print(0);
            else if (i < j) print(1);
            else print(2);
            j = j + 1;
        }
        i = i + 1;
    }

    total = 0;
    i = 1;
    while (i <= 100) {
        if (i - (i / 3) * 3 == 0) total = total + i;
        i = i + 1;
    }
    print(total);
    return 0;
}
`,
  },

  /* --------------------------- 综合 --------------------------- */
  {
    id: 'overview',
    group: '综合',
    label: '全特性综合演示',
    name: 'overview-demo.mc',
    output: '1\n4\n9\n16\n25\n55\n55\n1\nO\nK\n!',
    text: `/* 综合演示：函数 + 数组 + 字符 + 浮点 + 布尔 + 全局变量 */
int gCount;
float gSum;

int square(int x) {
    return x * x;
}

int sumTo(int n) {
    int s;
    int i;
    s = 0;
    i = 1;
    while (i <= n) {
        s = s + i;
        i = i + 1;
    }
    return s;
}

int main() {
    int i;
    int data[5];
    char tag[3];
    bool flag;

    tag[0] = 'O';
    tag[1] = 'K';
    tag[2] = '!';

    i = 0;
    while (i < 5) {
        data[i] = square(i + 1);
        i = i + 1;
    }

    i = 0;
    gSum = 0.0;
    while (i < 5) {
        print(data[i]);
        gSum = gSum + data[i];
        i = i + 1;
    }

    gCount = sumTo(10);
    print(gCount);
    print(gSum);

    flag = gCount == 55;
    if (flag) print(1);
    else print(0);

    i = 0;
    while (i < 3) {
        print(tag[i]);
        i = i + 1;
    }
    return 0;
}
`,
  },

  /* --------------------------- 错误演示 --------------------------- */
  {
    id: 'err-combined',
    group: '错误演示',
    label: '缺分号 + 未声明标识符',
    name: 'error-demo.mc',
    expect: 'error',
    err: "minic: error: 8:5: unexpected 'y' (IDENT); expected one of: ( [ (in 后缀表达式1)",
    text: `/* 错误演示：第 7 行 x = 36 少了分号，第 8 行的 y 被当成表达式续接 */
int a;
float b;

int main() {
    int x;
    x = 36
    y = 24;
    return 0;
}
`,
  },
  {
    id: 'err-undeclared',
    group: '错误演示',
    label: '使用了未声明的变量',
    name: 'error-undeclared.mc',
    expect: 'error',
    err: 'minic: error: 7:13: undefined identifier y',
    text: `/* 错误演示：y 从未声明 */
int a;

int main() {
    int x;
    x = 10;
    a = x + y;
    return 0;
}
`,
  },
  {
    id: 'err-decl-order',
    group: '错误演示',
    label: '顶层声明写在语句之后',
    name: 'error-decl-order.mc',
    expect: 'error',
    err: "minic: error: 4:1: unexpected 'int' (INT); expected one of: if while print return { ; 标识符 (in 语句部分)",
    text: `/* 错误演示：顶层声明必须全部排在语句之前，int b 出现在赋值之后 */
int a;
a = 1;
int b;
b = 2;
print(a + b);
`,
  },
];

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { SAMPLES };
}
