bool side() { print(888); return true; }
bool a;
bool b;
a = false;
b = a && side();
print(b);
a = true;
b = a && side();
print(b);
a = true;
b = a || side();
print(b);
a = false;
b = a || side();
print(b);
return;
