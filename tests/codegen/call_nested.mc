int inc(int x) { return x + 1; }
int add(int x, int y) { return x + y; }
int add3(int x, int y, int z) { return x + y + z; }
int a;
a = add(inc(1), inc(10));
print(a);
a = add(inc(5), 100);
print(a);
a = add(100, inc(5));
print(a);
a = add3(inc(0), inc(10), inc(20));
print(a);
a = add3(inc(0), 10, inc(20));
print(a);
a = add3(1, 2, 3);
print(a);
