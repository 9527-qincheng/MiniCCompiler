'use strict';
/**
 * 「运行」管线冒烟测试：node test/run-smoke.js
 *
 * 验证 lib/compiler.js 的 runSource()：minic 编译 → gcc 汇编链接 → 执行 → 抓 stdout。
 * 这是 GUI 里能看到 print 输出的唯一途径，必须确认三阶段与各失败分支都按预期返回。
 *
 * 依赖真实 gcc（MinGW）。若环境无 gcc，只跑「找不到 gcc」分支的断言并跳过端到端用例。
 */
const path = require('path');
const fs = require('fs');
const os = require('os');
const { spawnSync } = require('child_process');
const { runSource, findGcc, findMinic } = require('../lib/compiler');

const GUI_DIR = path.join(__dirname, '..');

const PRINT_INT = `int main() {
    int a;
    a = 1 + 2;
    print(a);
    print(a * 10);
    return 0;
}
`;

const COMPILE_ERROR = `int main() {
    int a;
    a = ;
    return 0;
}
`;

const CRASH = `int main() {
    int a;
    int b;
    a = 10;
    b = 0;
    print(a / b);
    return 0;
}
`;

let fail = 0;
function check(cond, label) {
  console.log((cond ? '  PASS  ' : '  FAIL  ') + label);
  if (!cond) fail++;
}

(async () => {
  const gcc = findGcc();
  const minic = findMinic(GUI_DIR);
  console.log('minic.exe =', minic);
  console.log('gcc       =', gcc);

  check(Boolean(minic), 'minic.exe 可定位');
  check(Boolean(gcc), 'gcc 可定位（可用环境变量 MINIC_GCC 指定）');

  if (!minic) {
    console.log('\nminic.exe 缺失，后续用例跳过。');
    console.log(`\n${fail} FAILURE(S)`);
    process.exit(1);
  }

  // ---- 用例 1：正常程序，print 输出应被完整捕获 ----
  const r1 = await runSource({ text: PRINT_INT, name: 'run-smoke-print.mc', guiDir: GUI_DIR });
  console.log('\n[print] ok =', r1.ok, '| stage =', r1.stage, '| exit =', r1.exitCode);
  console.log('[print] stdout =', JSON.stringify(r1.stdout));
  check(r1.stage === 'run', '正常程序走到 run 阶段');
  check(r1.ok === true, '正常程序 ok = true');
  check(r1.exitCode === 0, '正常程序退出码 0');
  check(String(r1.stdout || '').replace(/\s+$/, '') === '3\n30', 'print 输出为 "3" 与 "30"');
  check(Boolean(r1.asm), '返回了 .asm 文本');
  check(typeof r1.compileMs === 'number' && typeof r1.linkMs === 'number' && typeof r1.runMs === 'number',
    '返回了编译/链接/运行三段耗时');

  // ---- 用例 2：编译失败，不应进入 run 阶段 ----
  const r2 = await runSource({ text: COMPILE_ERROR, name: 'run-smoke-bad.mc', guiDir: GUI_DIR });
  console.log('\n[compile-error] ok =', r2.ok, '| stage =', r2.stage, '| exit =', r2.exitCode);
  console.log('[compile-error] stderr =', String(r2.stderr || '').trim().split(/\r?\n/)[0]);
  check(r2.stage === 'compile', '语法错误停在 compile 阶段');
  check(r2.ok === false, '语法错误 ok = false');
  check(r2.exitCode === 1, '语法错误退出码 1');
  check(/minic:\s*error:\s*\d+:\d+:/.test(String(r2.stderr || '')), 'stderr 含 minic error L:C 格式');

  // ---- 用例 3：运行期崩溃（整数除零），必须被识别为 crashed 而不是 ok ----
  if (gcc) {
    const r3 = await runSource({ text: CRASH, name: 'run-smoke-crash.mc', guiDir: GUI_DIR });
    console.log('\n[crash] ok =', r3.ok, '| stage =', r3.stage, '| reason =', r3.reason, '| exit =', r3.exitCode);
    check(r3.stage === 'run', '除零程序编译链接成功，进入 run 阶段');
    check(r3.ok === false, '崩溃时 ok = false');
    check(r3.crashed === true, '崩溃时 crashed = true');
    check(r3.reason === 'crash', '崩溃时 reason = "crash"');
    check(typeof r3.exitCode === 'number' && r3.exitCode < 0, `崩溃退出码为负数（实测 ${r3.exitCode}）`);
  } else {
    console.log('\n[crash] 无 gcc，跳过');
  }

  // ---- 用例 4：超时机制（死循环应被强制结束）----
  if (gcc) {
    const LOOP = `int main() {
    int i;
    i = 0;
    while (true) {
        i = i + 1;
    }
    return 0;
}
`;
    const r4 = await runSource({ text: LOOP, name: 'run-smoke-loop.mc', guiDir: GUI_DIR, timeoutMs: 1500 });
    console.log('\n[timeout] ok =', r4.ok, '| timedOut =', r4.timedOut, '| runMs =', r4.runMs);
    check(r4.timedOut === true, '死循环被判定超时');
    check(r4.ok === false, '超时时 ok = false');
    check(r4.reason === 'timeout', '超时时 reason = "timeout"');
    check(r4.runMs >= 1400, `运行时长达到超时阈值（实测 ${r4.runMs} ms）`);
  } else {
    console.log('\n[timeout] 无 gcc，跳过');
  }

  // ---- 清理临时产物 ----
  const workDir = path.join(os.tmpdir(), 'minic-gui');
  for (const name of ['run-smoke-print', 'run-smoke-bad', 'run-smoke-crash', 'run-smoke-loop']) {
    for (const ext of ['.mc', '.asm', '.s', '.exe']) {
      try { fs.unlinkSync(path.join(workDir, name + ext)); } catch { /* 不存在 */ }
    }
  }

  console.log('\n' + (fail === 0 ? 'ALL PASS' : `${fail} FAILURE(S)`));
  process.exit(fail === 0 ? 0 : 1);
})();
