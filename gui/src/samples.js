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
    output: "1",
    text: "/* 最小程序：一条 print + 一个 return */\nint main() {\n    print(1); print('\\n');\n    return 0;\n}\n",
  },
  {
    id: 'gcd',
    group: '基础',
    label: '最大公约数',
    name: 'gcd-demo.mc',
    output: "12",
    // return t; → gcd(36,24)=12，进程退出码也是 12（返回值语义的直接体现）
    exitCode: 12,
    text: "/* 最大公约数：辗转相减法 */\nint a;\nint b;\n\nint main() {\n    int t;\n    a = 36;\n    b = 24;\n    while (a != b) {\n        if (a > b) a = a - b;\n        else b = b - a;\n    }\n    t = a;\n    print(t); print('\\n');\n    return t;\n}\n",
  },
  {
    id: 'lex',
    group: '基础',
    label: '词法演示',
    name: 'lex-demo.mc',
    output: "42\n3.14\n1\nx\n40",
    text: "/* 词法演示：四种基本类型 + 各类型字面量 */\nint count;\nfloat ratio;\nbool flag;\nchar mark;\n\nint main() {\n    count = 42;\n    ratio = 3.14;\n    flag = true;\n    mark = 'x';\n\n    print(count); print('\\n');\n    print(ratio); print('\\n');\n    print(flag); print('\\n');\n    print(mark); print('\\n');\n\n    while (count > 40) {\n        count = count - 1;\n    }\n    print(count); print('\\n');\n    return 0;\n}\n",
  },

  /* --------------------------- 类型 --------------------------- */
  {
    id: 'float',
    group: '类型',
    label: '浮点运算',
    name: 'float-demo.mc',
    output: "19.6349\n5.5\n3\n3.5\n0.333333",
    text: "/* 浮点运算：面积、平均值、整除与浮点除的差别 */\nfloat pi;\nfloat r;\nfloat area;\nfloat sum;\nfloat avg;\nint count;\n\nint main() {\n    pi = 3.14159;\n    r = 2.5;\n    area = pi * r * r;\n    print(area); print('\\n');\n\n    sum = 0.0;\n    count = 1;\n    while (count <= 10) {\n        sum = sum + count;\n        count = count + 1;\n    }\n    avg = sum / 10.0;\n    print(avg); print('\\n');\n\n    print(7 / 2); print('\\n');\n    print(7.0 / 2.0); print('\\n');\n    print(1.0 / 3.0); print('\\n');\n    return 0;\n}\n",
  },
  {
    id: 'char-bool',
    group: '类型',
    label: '字符与布尔',
    name: 'char-bool-demo.mc',
    output: "M\ni\nn\nC\n!\n1\n3\n0",
    text: "/* 字符与布尔：char 数组逐字输出、&& || ! 求值 */\nchar msg[5];\nbool ok;\n\nint main() {\n    int i;\n    msg[0] = 'M';\n    msg[1] = 'i';\n    msg[2] = 'n';\n    msg[3] = 'C';\n    msg[4] = '!';\n\n    i = 0;\n    while (i < 5) {\n        print(msg[i]); print('\\n');\n        i = i + 1;\n    }\n\n    ok = true;\n    if (ok && true) { print(1); print('\\n'); }\n    if (!ok || false) { print(2); print('\\n'); }\n    ok = false;\n    if (!ok) { print(3); print('\\n'); }\n    print(ok); print('\\n');\n    return 0;\n}\n",
  },

  /* --------------------------- 函数 --------------------------- */
  {
    id: 'nested-call',
    group: '函数',
    label: '多实参与嵌套调用',
    name: 'nested-call-demo.mc',
    output: "7\n6\n13\n33",
    text: "/* 函数调用：多实参 + 实参本身又是函数调用 */\nint add(int x, int y) {\n    return x + y;\n}\n\nint inc(int x) {\n    return x + 1;\n}\n\nint add3(int x, int y, int z) {\n    return x + y + z;\n}\n\nint main() {\n    print(add(3, 4)); print('\\n');\n    print(add3(1, 2, 3)); print('\\n');\n    print(add(inc(1), inc(10))); print('\\n');\n    print(add3(inc(0), inc(10), inc(20))); print('\\n');\n    return 0;\n}\n",
  },
  {
    id: 'recursion',
    group: '函数',
    label: '递归',
    name: 'recursion-demo.mc',
    output: "1\n2\n6\n24\n120\n720\n0\n1\n1\n2\n3\n5\n8\n13\n21\n34\n55",
    text: "/* 递归：阶乘 1!~6! 与斐波那契 F(0)~F(10) */\nint fact(int n) {\n    if (n <= 1) return 1;\n    return n * fact(n - 1);\n}\n\nint fib(int n) {\n    if (n < 2) return n;\n    return fib(n - 1) + fib(n - 2);\n}\n\nint main() {\n    int i;\n\n    i = 1;\n    while (i <= 6) {\n        print(fact(i)); print('\\n');\n        i = i + 1;\n    }\n\n    i = 0;\n    while (i <= 10) {\n        print(fib(i)); print('\\n');\n        i = i + 1;\n    }\n    return 0;\n}\n",
  },
  {
    id: 'primes',
    group: '函数',
    label: '素数筛选',
    name: 'primes-demo.mc',
    output: "2\n3\n5\n7\n11\n13\n17\n19\n23\n29\n31\n37\n41\n43\n47\n15",
    text: "/* 素数筛选：MiniC 没有 % 运算符，用 x - (x/i)*i 判断整除 */\nint isPrime(int x) {\n    int i;\n    if (x < 2) return 0;\n    i = 2;\n    while (i * i <= x) {\n        if (x - (x / i) * i == 0) return 0;\n        i = i + 1;\n    }\n    return 1;\n}\n\nint main() {\n    int n;\n    int c;\n    c = 0;\n    n = 2;\n    while (n <= 50) {\n        if (isPrime(n)) {\n            print(n); print('\\n');\n            c = c + 1;\n        }\n        n = n + 1;\n    }\n    print(c); print('\\n');\n    return 0;\n}\n",
  },

  /* --------------------------- 数组与控制流 --------------------------- */
  {
    id: 'bubble',
    group: '数组与控制流',
    label: '冒泡排序',
    name: 'bubble-sort.mc',
    output: "3\n7\n15\n19\n24\n42\n61\n88",
    text: "/* 冒泡排序：8 个整数升序 */\nint a[8];\n\nint main() {\n    int i;\n    int j;\n    int t;\n    int n;\n\n    n = 8;\n    a[0] = 42;\n    a[1] = 7;\n    a[2] = 19;\n    a[3] = 3;\n    a[4] = 88;\n    a[5] = 15;\n    a[6] = 61;\n    a[7] = 24;\n\n    i = 0;\n    while (i < n - 1) {\n        j = 0;\n        while (j < n - 1 - i) {\n            if (a[j] > a[j + 1]) {\n                t = a[j];\n                a[j] = a[j + 1];\n                a[j + 1] = t;\n            }\n            j = j + 1;\n        }\n        i = i + 1;\n    }\n\n    i = 0;\n    while (i < n) {\n        print(a[i]); print('\\n');\n        i = i + 1;\n    }\n    return 0;\n}\n",
  },
  {
    id: 'stats',
    group: '数组与控制流',
    label: '数组统计',
    name: 'array-stats.mc',
    output: "91\n1\n415\n41",
    text: "/* 数组统计：最大值、最小值、总和、平均值 */\nint data[10];\n\nint main() {\n    int i;\n    int max;\n    int min;\n    int sum;\n\n    data[0] = 23;\n    data[1] = 5;\n    data[2] = 91;\n    data[3] = 47;\n    data[4] = 12;\n    data[5] = 68;\n    data[6] = 33;\n    data[7] = 79;\n    data[8] = 1;\n    data[9] = 56;\n\n    max = data[0];\n    min = data[0];\n    sum = 0;\n    i = 0;\n    while (i < 10) {\n        if (data[i] > max) max = data[i];\n        if (data[i] < min) min = data[i];\n        sum = sum + data[i];\n        i = i + 1;\n    }\n\n    print(max); print('\\n');\n    print(min); print('\\n');\n    print(sum); print('\\n');\n    print(sum / 10); print('\\n');\n    return 0;\n}\n",
  },
  {
    id: 'nest-loop',
    group: '数组与控制流',
    label: '嵌套循环与多分支',
    name: 'nested-loop.mc',
    output: "0\n1\n1\n1\n2\n0\n1\n1\n2\n2\n0\n1\n2\n2\n2\n0\n1683",
    text: "/* 嵌套循环 + 多分支 if：打印 4x4 比较矩阵，再求 1~100 内 3 的倍数之和 */\nint main() {\n    int i;\n    int j;\n    int total;\n\n    i = 1;\n    while (i <= 4) {\n        j = 1;\n        while (j <= 4) {\n            if (i == j) { print(0); print('\\n'); }\n            else if (i < j) { print(1); print('\\n'); }\n            else { print(2); print('\\n'); }\n            j = j + 1;\n        }\n        i = i + 1;\n    }\n\n    total = 0;\n    i = 1;\n    while (i <= 100) {\n        if (i - (i / 3) * 3 == 0) total = total + i;\n        i = i + 1;\n    }\n    print(total); print('\\n');\n    return 0;\n}\n",
  },

  /* --------------------------- 综合 --------------------------- */
  {
    id: 'overview',
    group: '综合',
    label: '全特性综合演示',
    name: 'overview-demo.mc',
    output: "1\n4\n9\n16\n25\n55\n55\n1\nO\nK\n!",
    text: "/* 综合演示：函数 + 数组 + 字符 + 浮点 + 布尔 + 全局变量 */\nint gCount;\nfloat gSum;\n\nint square(int x) {\n    return x * x;\n}\n\nint sumTo(int n) {\n    int s;\n    int i;\n    s = 0;\n    i = 1;\n    while (i <= n) {\n        s = s + i;\n        i = i + 1;\n    }\n    return s;\n}\n\nint main() {\n    int i;\n    int data[5];\n    char tag[3];\n    bool flag;\n\n    tag[0] = 'O';\n    tag[1] = 'K';\n    tag[2] = '!';\n\n    i = 0;\n    while (i < 5) {\n        data[i] = square(i + 1);\n        i = i + 1;\n    }\n\n    i = 0;\n    gSum = 0.0;\n    while (i < 5) {\n        print(data[i]); print('\\n');\n        gSum = gSum + data[i];\n        i = i + 1;\n    }\n\n    gCount = sumTo(10);\n    print(gCount); print('\\n');\n    print(gSum); print('\\n');\n\n    flag = gCount == 55;\n    if (flag) { print(1); print('\\n'); }\n    else { print(0); print('\\n'); }\n\n    i = 0;\n    while (i < 3) {\n        print(tag[i]); print('\\n');\n        i = i + 1;\n    }\n    return 0;\n}\n",
  },

  /* --------------------------- 错误演示 --------------------------- */
  {
    id: 'err-combined',
    group: '错误演示',
    label: '缺分号 + 未声明标识符',
    name: 'error-demo.mc',
    expect: 'error',
    err: "minic: error: 8:5: unexpected 'y' (IDENT); expected one of: ( [ ++ -- (in 后缀表达式1)",
    text: "/* 错误演示：第 7 行 x = 36 少了分号，第 8 行的 y 被当成表达式续接 */\nint a;\nfloat b;\n\nint main() {\n    int x;\n    x = 36\n    y = 24;\n    return 0;\n}\n",
  },
  {
    id: 'err-undeclared',
    group: '错误演示',
    label: '使用了未声明的变量',
    name: 'error-undeclared.mc',
    expect: 'error',
    err: 'minic: error: 7:13: undefined identifier y',
    text: "/* 错误演示：y 从未声明 */\nint a;\n\nint main() {\n    int x;\n    x = 10;\n    a = x + y;\n    return 0;\n}\n",
  },
  {
    id: 'err-decl-order',
    group: '错误演示',
    label: '顶层声明写在语句之后',
    name: 'error-decl-order.mc',
    expect: 'error',
    err: "minic: error: 4:1: unexpected 'int' (INT); expected one of: if while print return { ; ++ -- 标识符 (in 语句部分)",
    text: "/* 错误演示：顶层声明必须全部排在语句之前，int b 出现在赋值之后 */\nint a;\na = 1;\nint b;\nb = 2;\nprint(a + b); print('\\n');\n",
  },
  {
    id: "err-mixed-all",
    group: '错误演示',
    label: "三类错误混合（词法 + 语法 + 语义）",
    name: "error-mixed-all.mc",
    expect: 'error',
    err: "minic: error: 12:11: unexpected character '@'",
    text: "/* 混合错误演示：词法 / 语法 / 语义三类错误同时出现\n   非法字符 @            词法\n   二元表达式缺右操作数   语法\n   未声明标识符 z         语义\n   常量下标越界 a[5]      语义\n   数组名不带下标 s = 1   语义\n   语法错误恢复后，后面的函数体仍被完整分析，语义错误照常报出。 */\nint g;\n\nint bad_lex() {\n    int c;\n    c = 1 @ 2;\n    return c;\n}\n\nint bad_syntax() {\n    int x;\n    x = 1 + ;\n    return x;\n}\n\nint bad_semantic() {\n    int a[3];\n    int y;\n    y = z + 1;\n    a[5] = 1;\n    return 0;\n}\n\nint main() {\n    int s[2];\n    s = 1;\n    return 0;\n}\n",
  },
  {
    id: "err-lex-cluster",
    group: '错误演示',
    label: "词法错误集中（非法字符 / 多字符 / 转义）",
    name: "error-lex-cluster.mc",
    expect: 'error',
    err: "minic: error: 9:11: unexpected character '@'",
    text: "/* 词法错误集中演示：一处写错，词法阶段记一条，语法阶段再记一条次生错误\n   @      非法字符\n   &      非法字符（MiniC 不支持位运算）\n   'ab'   多字符字符字面量\n   '\\q'    未知转义序列 */\nint main() {\n    int c;\n    char s;\n    c = 1 @ 2;\n    c = c & 3;\n    s = 'ab';\n    s = '\\q';\n    return 0;\n}\n",
  },
  {
    id: "err-semantic-cluster",
    group: '错误演示',
    label: "语义错误集中（未声明 / 越界 / 实参 / 隐式转换）",
    name: "error-semantic-cluster.mc",
    expect: 'error',
    err: "minic: error: 14:9: undefined identifier notDeclared",
    text: "/* 语义错误集中演示：6 个错误 + 2 个警告，语法完全正确\n   未声明标识符 / 常量下标越界 / 负下标 / 数组名不带下标\n   实参偏少 / 实参偏多\n   float→int 隐式转换（警告） / bool←int 可疑赋值（警告） */\nint f(int a, int b) {\n    return a + b;\n}\n\nint main() {\n    int arr[3];\n    float fl;\n    bool bb;\n    int u;\n    u = notDeclared;\n    arr[5] = 1;\n    arr[-1] = 2;\n    u = arr;\n    u = f(1);\n    u = f(1, 2, 3);\n    u = 1.9;\n    bb = u;\n    fl = u;\n    return 0;\n}\n",
  },
  {
    id: "err-recovery",
    group: '错误演示',
    label: "错误恢复：一个函数出错，后面继续分析",
    name: "error-recovery.mc",
    expect: 'error',
    err: "minic: error: 7:13: unexpected ';' (SEMI); expected one of: true false ( - ! ++ -- 标识符 整型常量 实型常量 字符常量 (in 乘法表达式)",
    text: "/* 错误恢复演示：第一个函数里有语法错误，编译器跳过它继续分析后面两个函数\n   后面的 a[9] 与 unknown 仍被报出，说明函数体没有被整体丢弃。\n   报错末尾的「possibly caused by earlier lexical/syntax errors」\n   是编译器在标注「这条是前序错误引发的次生诊断」。 */\nint broken() {\n    int x;\n    x = 1 + ;\n    return x;\n}\n\nint still_checked() {\n    int a[3];\n    a[9] = 1;\n    return 0;\n}\n\nint main() {\n    int y;\n    y = unknown + 1;\n    return broken();\n}\n",
  },
  {
    id: "err-void-misuse",
    group: '错误演示',
    label: "void 与类型误用",
    name: "error-void-misuse.mc",
    expect: 'error',
    err: "minic: error: 3:5: variable 'v' cannot have type void",
    text: "/* void 只能作函数返回类型：变量、数组、形参三种误用各报一条 */\nvoid bad_var() {\n    void v;\n    return;\n}\n\nvoid bad_arr() {\n    void a[3];\n    return;\n}\n\nint bad_param(void x) {\n    return 0;\n}\n\nint main() {\n    int n;\n    n = bad_param();\n    return n;\n}\n",
  },
];

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { SAMPLES };
}
