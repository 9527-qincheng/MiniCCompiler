int g1;
int ga[5];
char gc[3];
float gf2;

int add(int x, int y)
{
    return x + y;
}

int main()
{
    int i;
    int la[4];
    i = 0;
    while (i < 3) {
        if (i == 1) { ga[i] = 7; } else { ga[i] = i; }
        i = i + 1;
    }
    la[0] = add(g1, ga[2]);
    return 0;
}
