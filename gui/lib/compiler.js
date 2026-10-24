'use strict';
/**
 * minic.exe 调度层（纯 Node，不依赖 Electron，便于单测）
 *
 * 关键约束：minic.exe 由 MinGW 构建，工作目录含中文时 ifstream 会失败。
 * 因此统一把源码复制到 %TEMP%\minic-gui（ASCII 路径）下执行。
 */
const path = require('path');
const fs = require('fs');
const os = require('os');
const { spawn } = require('child_process');

const ARTIFACT_EXTS = ['.token.txt', '.parse.txt', '.ast.txt', '.sym.txt', '.quad.txt', '.asm'];

function findMinic(guiDir) {
  const root = guiDir || path.join(__dirname, '..');
  const candidates = [
    process.env.MINIC_EXE,
    path.join(root, '..', 'build', 'minic.exe'),
    path.join(root, 'minic.exe'),
  ].filter(Boolean);
  for (const p of candidates) {
    try {
      if (fs.statSync(p).isFile()) return p;
    } catch { /* 不存在则跳过 */ }
  }
  return null;
}

/** minic 输出可能是 GBK，优先严格 UTF-8 解码，失败再按 GBK */
function decodeBuffer(buf) {
  try {
    return new TextDecoder('utf-8', { fatal: true }).decode(buf);
  } catch {
    try {
      return new TextDecoder('gbk').decode(buf);
    } catch {
      return buf.toString('utf8');
    }
  }
}

/**
 * 若 minic.exe 自身路径含非 ASCII 字符（如 E:\计算机软件实训\...），
 * 该进程的文件 I/O 会整体失效（已实测：同一二进制复制到 ASCII 路径后一切正常）。
 * 因此统一复制到 %TEMP%\minic-gui\minic.exe 再运行；源 exe 变更时按 mtime/size 刷新。
 */
function ensureRunnableMinic(exe) {
  if (!exe || /^[\x00-\x7F]*$/.test(exe)) return exe;
  const dir = path.join(os.tmpdir(), 'minic-gui');
  fs.mkdirSync(dir, { recursive: true });
  const dst = path.join(dir, 'minic.exe');
  try {
    const src = fs.statSync(exe);
    let needCopy = true;
    try {
      const cur = fs.statSync(dst);
      needCopy = cur.size !== src.size || cur.mtimeMs < src.mtimeMs;
    } catch { /* 目标不存在 */ }
    if (needCopy) fs.copyFileSync(exe, dst);
    return dst;
  } catch {
    return exe; // 复制失败则退回原路径，仍可能可用（取决于环境）
  }
}

function runProcess(exe, args, cwd, opts = {}) {
  return new Promise((resolve) => {
    const out = [];
    const err = [];
    let timer = null;
    let timedOut = false;
    let settled = false;
    let child = null;

    const finish = (code, spawnError) => {
      if (settled) return;
      settled = true;
      if (timer) clearTimeout(timer);
      resolve({
        code,
        out: Buffer.concat(out),
        err: Buffer.concat(err),
        spawnError: spawnError || null,
        timedOut,
      });
    };

    // spawn 在极端情况下会同步抛（如受限沙箱里 EPERM 直接抛出而不是走 error 事件），
    // 必须兜住，否则整个 IPC handler 崩掉、界面只显示一个无头无尾的异常。
    try {
      child = spawn(exe, args, { cwd, windowsHide: true, env: { ...process.env } });
    } catch (e) {
      finish(-1, String(e));
      return;
    }

    // 用户程序可能死循环（while(1)），必须能中断，否则 GUI 会卡死
    if (opts.timeoutMs) {
      timer = setTimeout(() => {
        timedOut = true;
        try { child.kill(); } catch { /* 已退出 */ }
      }, opts.timeoutMs);
    }

    child.stdout.on('data', (d) => out.push(d));
    child.stderr.on('data', (d) => err.push(d));
    child.on('error', (e) => finish(-1, String(e)));
    child.on('close', (code) => finish(code));
  });
}

/** 把源码写进 %TEMP%\minic-gui，返回 { exe, workDir, fileName, srcPath, base } */
function prepareWork({ text, name, guiDir }) {
  const found = findMinic(guiDir);
  const exe = ensureRunnableMinic(found);
  if (!exe) return null;
  const safeName = String(name || 'work.mc').replace(/[\\/:*?"<>|]/g, '_');
  const fileName = /\.mc$/i.test(safeName) ? safeName : safeName + '.mc';

  const workDir = path.join(os.tmpdir(), 'minic-gui');
  fs.mkdirSync(workDir, { recursive: true });
  const srcPath = path.join(workDir, fileName);
  fs.writeFileSync(srcPath, String(text == null ? '' : text), 'utf8');
  return { exe, workDir, fileName, srcPath, base: srcPath.replace(/\.mc$/i, '') };
}

async function compileSource({ text, name, guiDir }) {
  const work = prepareWork({ text, name, guiDir });
  if (!work) {
    return { ok: false, reason: 'minic-not-found', message: '未找到 minic.exe（期望位于 MiniCCompiler/build/minic.exe）' };
  }
  const { exe, workDir, srcPath, base } = work;

  for (const ext of ARTIFACT_EXTS) {
    try { fs.unlinkSync(base + ext); } catch { /* 旧产物不存在 */ }
  }

  const t0 = Date.now();
  const res = await runProcess(
    exe,
    [srcPath, '--dump-tokens', '--dump-parse', '--dump-ast', '--dump-symtab', '--dump-quads'],
    workDir,
  );
  const durationMs = Date.now() - t0;

  const artifacts = {};
  for (const ext of ARTIFACT_EXTS) {
    try {
      artifacts[ext] = decodeBuffer(fs.readFileSync(base + ext));
    } catch {
      artifacts[ext] = null;
    }
  }

  // spawn 本身失败（找不到 exe、权限不足、被安全软件拦截）：这不是编译错误，
  // 不能只报「编译失败」，否则用户会去改源码，永远改不好。
  if (res.spawnError) {
    return {
      ok: false,
      reason: 'spawn-failed',
      exitCode: res.code,
      stdout: decodeBuffer(res.out),
      stderr: decodeBuffer(res.err),
      spawnError: res.spawnError,
      message: `无法启动 minic.exe：${res.spawnError}`,
      artifacts,
      durationMs,
      exe,
      workDir,
    };
  }

  return {
    ok: res.code === 0,
    exitCode: res.code,
    stdout: decodeBuffer(res.out),
    stderr: decodeBuffer(res.err),
    spawnError: null,
    artifacts,
    durationMs,
    exe,
    workDir,
  };
}

/**
 * 编译并运行用户程序：minic.exe 生成 .asm → gcc 汇编链接成 .exe → 执行 → 抓 stdout。
 *
 * 这是 GUI 里能看到 print 输出的唯一途径：minic.exe 只做编译，print 是运行期语句，
 * 必须真的把程序跑起来才有输出。汇编链接全程在 %TEMP%\minic-gui（ASCII 路径）下完成，
 * 因为 MinGW ld 不接受含中文的 -o 绝对路径。
 */
async function runSource({ text, name, guiDir, timeoutMs = 5000 }) {
  const work = prepareWork({ text, name, guiDir });
  if (!work) {
    return { ok: false, reason: 'minic-not-found', message: '未找到 minic.exe（期望位于 MiniCCompiler/build/minic.exe）' };
  }
  const { exe, workDir, srcPath, base } = work;

  // 1) 编译出 .asm
  const asmPath = base + '.asm';
  try { fs.unlinkSync(asmPath); } catch { /* 旧产物不存在 */ }
  const t0 = Date.now();
  const compileRes = await runProcess(exe, [srcPath], workDir);
  const compileMs = Date.now() - t0;

  if (compileRes.spawnError) {
    return {
      ok: false,
      stage: 'compile',
      reason: 'spawn-failed',
      exitCode: compileRes.code,
      stdout: decodeBuffer(compileRes.out),
      stderr: decodeBuffer(compileRes.err),
      message: `无法启动 minic.exe：${compileRes.spawnError}`,
      compileMs,
    };
  }

  if (compileRes.code !== 0) {
    return {
      ok: false,
      stage: 'compile',
      exitCode: compileRes.code,
      stdout: decodeBuffer(compileRes.out),
      stderr: decodeBuffer(compileRes.err),
      compileMs,
    };
  }
  let asmText;
  try {
    asmText = decodeBuffer(fs.readFileSync(asmPath));
  } catch {
    return { ok: false, stage: 'compile', reason: 'no-asm', message: '编译成功但没有生成 .asm 产物', compileMs };
  }

  // 2) gcc 汇编 + 链接（.asm 后缀 gcc 不认，必须复制成 .s 或加 -x assembler）
  const gcc = findGcc();
  if (!gcc) {
    return {
      ok: false,
      stage: 'gcc',
      reason: 'gcc-not-found',
      message: '未找到 gcc（汇编链接需要 MinGW gcc）。请把 gcc 所在目录加入 PATH，或设置环境变量 MINIC_GCC。',
      asm: asmText,
      compileMs,
    };
  }
  const sPath = base + '.s';
  const exePath = base + '.exe';
  fs.writeFileSync(sPath, asmText, 'utf8');
  try { fs.unlinkSync(exePath); } catch { /* 旧产物不存在 */ }
  const t1 = Date.now();
  const linkRes = await runProcess(gcc, ['-x', 'assembler', '-o', exePath, sPath], workDir);
  const linkMs = Date.now() - t1;

  if (linkRes.spawnError) {
    return {
      ok: false,
      stage: 'link',
      reason: 'spawn-failed',
      exitCode: linkRes.code,
      message: `无法启动 gcc：${linkRes.spawnError}`,
      asm: asmText,
      compileMs,
      linkMs,
    };
  }

  if (linkRes.code !== 0) {
    return {
      ok: false,
      stage: 'link',
      reason: 'link-failed',
      exitCode: linkRes.code,
      message: decodeBuffer(linkRes.err) || decodeBuffer(linkRes.out) || 'gcc 汇编/链接失败',
      asm: asmText,
      compileMs,
      linkMs,
    };
  }

  // 3) 运行
  const t2 = Date.now();
  const runRes = await runProcess(exePath, [], workDir, { timeoutMs });
  const runMs = Date.now() - t2;

  if (runRes.spawnError) {
    return {
      ok: false,
      stage: 'run',
      reason: 'spawn-failed',
      exitCode: runRes.code,
      message: `无法启动生成的可执行文件：${runRes.spawnError}`,
      asm: asmText,
      compileMs,
      linkMs,
      runMs,
      gcc,
    };
  }

  // Windows 上崩溃会得到 0xC0000000 段的负数退出码（如 0xC0000094 = 整数除零）
  const crashed = runRes.code != null && runRes.code < 0;

  return {
    ok: !runRes.timedOut && !crashed,
    stage: 'run',
    reason: runRes.timedOut ? 'timeout' : (crashed ? 'crash' : null),
    exitCode: runRes.code,
    timedOut: Boolean(runRes.timedOut),
    crashed,
    stdout: decodeBuffer(runRes.out),
    stderr: decodeBuffer(runRes.err),
    spawnError: runRes.spawnError || null,
    asm: asmText,
    compileMs,
    linkMs,
    runMs,
    gcc,
  };
}

/** 找 gcc：MINIC_GCC 环境变量 → PATH → 常见 MinGW 安装位置 */
function findGcc() {
  const isFile = (p) => {
    try { return fs.statSync(p).isFile(); } catch { return false; }
  };
  const exeName = process.platform === 'win32' ? 'gcc.exe' : 'gcc';
  if (process.env.MINIC_GCC && isFile(process.env.MINIC_GCC)) return process.env.MINIC_GCC;
  for (const dir of String(process.env.PATH || '').split(path.delimiter)) {
    if (!dir) continue;
    const p = path.join(dir, exeName);
    if (isFile(p)) return p;
  }
  const guesses = [
    'C:\\msys64\\ucrt64\\bin\\gcc.exe',
    'C:\\msys64\\mingw64\\bin\\gcc.exe',
    'C:\\MinGW\\bin\\gcc.exe',
    'C:\\mingw64\\bin\\gcc.exe',
  ];
  for (const p of guesses) if (isFile(p)) return p;
  return null;
}

module.exports = {
  findMinic, findGcc, ensureRunnableMinic, decodeBuffer, runProcess,
  compileSource, runSource, ARTIFACT_EXTS,
};
