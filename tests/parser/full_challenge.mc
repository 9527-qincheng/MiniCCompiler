int g[10];

int fill(int n) {
    int i;
    i = 0;
    while (i < n) {
        g[i] = i * 2;
        if (i > 3) {
            print(g[i]);
        } else {
            print(i);
        }
        i = i + 1;
    }
    return i;
}

int r;
r = fill(5);
print(r);
