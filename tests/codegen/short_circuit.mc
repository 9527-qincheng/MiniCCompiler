bool side() { print(888); print('\n'); return true; }
bool a;
bool b;
a = false;
b = a && side();
print(b); print('\n');
a = true;
b = a && side();
print(b); print('\n');
a = true;
b = a || side();
print(b); print('\n');
a = false;
b = a || side();
print(b); print('\n');
return;
