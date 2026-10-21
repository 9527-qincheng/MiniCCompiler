'use strict';
/** 编译管线冒烟测试：node test/compile-smoke.js */
const { compileSource, findMinic } = require('../lib/compiler');

const GOOD = `/* smoke: gcd */
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
    return t;
}
`;

const BAD = `int a;
float b;

int main() {
    int x;
    x = 36
    y = 24;
    return 0;
}
`;

(async () => {
  let fail = 0;
  const check = (cond, label) => {
    console.log((cond ? '  PASS  ' : '  FAIL  ') + label);
    if (!cond) fail++;
  };

  console.log('minic.exe =', findMinic(require('path').join(__dirname, '..')));

  const r1 = await compileSource({ text: GOOD, name: 'smoke-good.mc', guiDir: require('path').join(__dirname, '..') });
  console.log('\n[good] exitCode =', r1.exitCode, '| duration =', r1.durationMs + 'ms');
  check(r1.exitCode === 0, 'good source exits 0');
  check(Boolean(r1.artifacts['.token.txt']), 'token artifact written');
  check(Boolean(r1.artifacts['.parse.txt']), 'parse artifact written');
  check(Boolean(r1.artifacts['.ast.txt']), 'ast artifact written');
  check(Boolean(r1.artifacts['.sym.txt']), 'symtab artifact written');
  check(Boolean(r1.artifacts['.quad.txt']), 'quad artifact written');
  check(Boolean(r1.artifacts['.asm']), 'asm artifact written');
  const tokenLines = (r1.artifacts['.token.txt'] || '').trim().split(/\r?\n/);
  check(tokenLines.length > 40, `token count = ${tokenLines.length}`);
  check(/^\d+:\d+\s+[A-Z_]+/m.test(r1.artifacts['.token.txt'] || ''), 'token format matches L:C TYPE value');
  check(/Program/.test(r1.artifacts['.ast.txt'] || ''), 'ast contains Program node');
  check(/Symbol Table/.test(r1.artifacts['.sym.txt'] || ''), 'symtab contains table header');

  const r2 = await compileSource({ text: BAD, name: 'smoke-bad.mc', guiDir: require('path').join(__dirname, '..') });
  console.log('\n[bad] exitCode =', r2.exitCode);
  console.log('[bad] stderr =', (r2.stderr || '').trim().split(/\r?\n/)[0]);
  check(r2.exitCode === 1, 'bad source exits 1');
  check(/minic:\s*error:\s*\d+:\d+:/.test(r2.stderr || ''), 'stderr has minic error L:C format');

  console.log('\n' + (fail === 0 ? 'ALL PASS' : `${fail} FAILURE(S)`));
  process.exit(fail === 0 ? 0 : 1);
})();
