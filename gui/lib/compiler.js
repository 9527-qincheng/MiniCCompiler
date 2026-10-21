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

function runProcess(exe, args, cwd) {  return new Promise((resolve) => {
    const child = spawn(exe, args, { cwd, windowsHide: true, env: { ...process.env } });
    const out = [];
    const err = [];
    child.stdout.on('data', (d) => out.push(d));
    child.stderr.on('data', (d) => err.push(d));
    child.on('error', (e) => resolve({ code: -1, out: Buffer.concat(out), err: Buffer.concat(err), spawnError: String(e) }));
    child.on('close', (code) => resolve({ code, out: Buffer.concat(out), err: Buffer.concat(err) }));
  });
}

async function compileSource({ text, name, guiDir }) {
  const found = findMinic(guiDir);
  const exe = ensureRunnableMinic(found);
  if (!exe) {
    return { ok: false, reason: 'minic-not-found', message: '未找到 minic.exe（期望位于 MiniCCompiler/build/minic.exe）' };
  }
  const safeName = String(name || 'work.mc').replace(/[\\/:*?"<>|]/g, '_');
  const fileName = /\.mc$/i.test(safeName) ? safeName : safeName + '.mc';

  const workDir = path.join(os.tmpdir(), 'minic-gui');
  fs.mkdirSync(workDir, { recursive: true });
  const srcPath = path.join(workDir, fileName);
  fs.writeFileSync(srcPath, String(text == null ? '' : text), 'utf8');

  const base = srcPath.replace(/\.mc$/i, '');
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

  return {
    ok: res.code === 0,
    exitCode: res.code,
    stdout: decodeBuffer(res.out),
    stderr: decodeBuffer(res.err),
    spawnError: res.spawnError || null,
    artifacts,
    durationMs,
    exe,
    workDir,
  };
}

module.exports = { findMinic, ensureRunnableMinic, decodeBuffer, runProcess, compileSource, ARTIFACT_EXTS };
