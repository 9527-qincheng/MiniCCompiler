'use strict';
/**
 * 「示例源码库」冒烟测试：node test/sample-smoke.js
 *
 * samples.js 里每条示例都宣称实测过，这个脚本负责兑现这个宣称：
 *   成功示例  minic 退出码 0 → gcc 汇编链接 → 跑 exe → stdout 必须与 output 逐字一致
 *   错误示例  minic 退出码 1 → stderr 必须包含 err 字段里的原文
 *
 * 只依赖 node + gcc，不需要 Electron 窗口，所以受限沙箱里也能跑。
 * 缺少 build/minic.exe 或 gcc 时打印 SKIP 并以 0 退出，不误判为失败。
 *
 * 注意：受限沙箱会禁止 spawn（EPERM），此时同样按 SKIP 处理。
 */
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');

const GUI_DIR = path.join(__dirname, '..');
const REPO_ROOT = path.join(GUI_DIR, '..');
const MINIC = path.join(REPO_ROOT, 'build', 'minic.exe');

let fail = 0;
function check(ok, label) {
  if (ok) {
    console.log('  PASS  ' + label);
  } else {
    console.log('  FAIL  ' + label);
    fail++;
  }
}

function findGcc() {
  const explicit = process.env.MINIC_GCC;
  if (explicit && fs.existsSync(explicit)) return explicit;
  const dirs = String(process.env.PATH || '').split(path.delimiter).filter(Boolean);
  for (const d of dirs) {
    const p = path.join(d, 'gcc.exe');
    try { if (fs.existsSync(p)) return p; } catch { /* ignore */ }
  }
  for (const p of [
    'C:\\msys64\\ucrt64\\bin\\gcc.exe',
    'C:\\msys64\\mingw64\\bin\\gcc.exe',
    'C:\\MinGW\\bin\\gcc.exe',
    'C:\\mingw64\\bin\\gcc.exe',
  ]) {
    try { if (fs.existsSync(p)) return p; } catch { /* ignore */ }
  }
  return null;
}

/* 跑一个进程，返回 {status, stdout, stderr, blocked}；blocked 表示环境禁止 spawn */
function run(exe, args, opts = {}) {
  const r = spawnSync(exe, args, { encoding: 'utf8', timeout: 60000, ...opts });
  if (r.error) return { blocked: r.error.code || r.error.message };
  return { status: r.status, stdout: r.stdout || '', stderr: r.stderr || '' };
}

function norm(text) {
  return String(text).replace(/\r\n/g, '\n').replace(/\s+$/, '');
}

function main() {
  console.log('[1] samples.js 数据层');
  const { SAMPLES } = require(path.join(GUI_DIR, 'src', 'samples.js'));
  check(Array.isArray(SAMPLES) && SAMPLES.length >= 10, `示例条数 ${SAMPLES.length} >= 10`);

  const ids = new Set();
  const dup = [];
  const badField = [];
  for (const s of SAMPLES) {
    if (ids.has(s.id)) dup.push(s.id);
    ids.add(s.id);
    const need = ['id', 'group', 'label', 'name', 'text'];
    for (const k of need) if (!s[k]) badField.push(s.id + '.' + k);
    if (s.expect === 'error') {
      if (!s.err) badField.push(s.id + '.err');
    } else if (!s.output) {
      badField.push(s.id + '.output');
    }
    if (s.exitCode !== undefined && typeof s.exitCode !== 'number') {
      badField.push(s.id + '.exitCode(非数字)');
    }
  }
  check(dup.length === 0, `id 唯一（重复 ${dup.join(',') || 0}）`);
  check(badField.length === 0, `字段齐全（缺 ${badField.join(',') || 0}）`);

  const groups = [...new Set(SAMPLES.map((s) => s.group))];
  check(groups.length >= 3, `分组 ${groups.length} 个：${groups.join(' / ')}`);
  const okN = SAMPLES.filter((s) => s.expect !== 'error').length;
  console.log(`        共 ${SAMPLES.length} 条：成功 ${okN} / 错误 ${SAMPLES.length - okN}`);

  console.log('\n[2] 源码格式约定');
  const crlf = SAMPLES.filter((s) => s.text.includes('\r'));
  check(crlf.length === 0, `源码统一 LF（含 CR 的 ${crlf.length}）`);
  const noTail = SAMPLES.filter((s) => !s.text.endsWith('\n'));
  check(noTail.length === 0, `源码都以换行结尾（缺 ${noTail.map((s) => s.id).join(',') || 0}）`);
  const tabs = SAMPLES.filter((s) => s.text.includes('\t'));
  check(tabs.length === 0, `缩进用空格不用 Tab（含 Tab 的 ${tabs.length}）`);

  console.log('\n[3] 在真实工具链上实跑');
  if (!fs.existsSync(MINIC)) {
    console.log('        SKIP  未找到 build/minic.exe，先运行 build.bat');
    return finish();
  }
  const gcc = findGcc();
  if (!gcc) {
    console.log('        SKIP  未找到 gcc.exe，设置 MINIC_GCC 环境变量指向它');
    return finish();
  }

  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'minic-samples-'));
  let blocked = null;
  for (const s of SAMPLES) {
    const mc = path.join(tmp, s.id + '.mc');
    fs.writeFileSync(mc, s.text, 'utf8');

    const c = run(MINIC, [mc]);
    if (c.blocked) { blocked = c.blocked; break; }

    if (s.expect === 'error') {
      const firstErr = String(c.stderr).split(/\r?\n/).filter((l) => l.includes('error'))[0] || '';
      check(c.status === 1 && firstErr.trim() === s.err.trim(),
        `${s.id}  错误示例退出码 1 且报错原文一致`);
      if (c.status !== 1) console.log(`        exit=${c.status}`);
      else if (firstErr.trim() !== s.err.trim()) {
        console.log(`        want: ${s.err.trim()}`);
        console.log(`        got : ${firstErr.trim()}`);
      }
      continue;
    }

    if (c.status !== 0) {
      check(false, `${s.id}  minic 编译失败`);
      console.log('        ' + String(c.stderr).split(/\r?\n/)[0]);
      continue;
    }

    const asmFile = path.join(tmp, s.id + '.asm');
    const sFile = path.join(tmp, s.id + '.s');
    const exeFile = path.join(tmp, s.id + '.exe');
    if (!fs.existsSync(asmFile)) {
      check(false, `${s.id}  未产出 .asm`);
      continue;
    }
    fs.copyFileSync(asmFile, sFile);

    const l = run(gcc, ['-x', 'assembler', '-o', exeFile, sFile]);
    if (l.status !== 0) {
      check(false, `${s.id}  汇编链接失败`);
      console.log('        ' + String(l.stderr).split(/\r?\n/)[0]);
      continue;
    }

    const r = run(exeFile, []);
    // 示例可以声明 exitCode：main 的返回值就是进程退出码，gcd 示例 return t=12 时退出码是 12。
    // 这是 MiniC 返回值语义的正确体现，不是失败；未声明时默认要求 0。
    const wantCode = typeof s.exitCode === 'number' ? s.exitCode : 0;
    if (r.status !== wantCode) {
      check(false, `${s.id}  运行退出码 ${r.status}（期望 ${wantCode}）`);
      continue;
    }
    const want = norm(s.output);
    const got = norm(r.stdout);
    check(want === got, `${s.id}  运行输出与标注一致`);
    if (want !== got) {
      console.log(`        want: ${JSON.stringify(want)}`);
      console.log(`        got : ${JSON.stringify(got)}`);
    }
  }

  if (blocked) {
    console.log(`        SKIP  当前环境禁止 spawn（${blocked}），跳过实跑校验`);
  }
  fs.rmSync(tmp, { recursive: true, force: true });
  return finish();
}

function finish() {
  console.log('\n' + (fail === 0 ? 'ALL PASS' : `${fail} FAILURE(S)`));
  process.exit(fail === 0 ? 0 : 1);
}

main();
