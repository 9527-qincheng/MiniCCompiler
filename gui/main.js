'use strict';
/**
 * MiniC 编译器 GUI — Electron 主进程
 * 职责：无边框窗口、窗口控制、文件对话框、调用 minic.exe 并回读 7 类产物。
 * 编译调度在 lib/compiler.js（纯 Node，可单测）。
 */
const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const path = require('path');
const fs = require('fs');
const os = require('os');
const { findMinic, decodeBuffer, compileSource, runSource } = require('./lib/compiler');

const isShot = process.argv.some((a) => a.startsWith('--shot'));
let win = null;

// 本机（虚拟机/远程桌面）GPU 进程会崩溃，强制软件渲染保证稳定启动
app.disableHardwareAcceleration();
app.commandLine.appendSwitch('disable-gpu');

/* ------------------------------------------------------------------ */
/* 窗口                                                               */
/* ------------------------------------------------------------------ */
function createWindow() {
  win = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 1120,
    minHeight: 700,
    frame: false,
    show: false,
    backgroundColor: '#070b10',
    title: 'MiniC Studio',
    icon: path.join(__dirname, 'assets', 'icon.png'),
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      spellcheck: false,
    },
  });

  win.loadFile(path.join(__dirname, 'renderer', 'dist', 'index.html'));
  win.once('ready-to-show', () => win.show());
  // 禁用页面级缩放（Ctrl+滚轮只用于编辑器字号，不应缩放整个窗口）
  try {
    win.webContents.setVisualZoomLevelLimits(1, 1);
  } catch { /* 旧版本无此 API */ }

  // 截图/验收模式：--shot=<png> [--theme=light] [--compile] [--tab=ast] [--menu=samples]
  if (isShot) {
    win.webContents.once('did-finish-load', async () => {
      const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
      const arg = (name, def) => {
        const hit = process.argv.find((a) => a.startsWith(`--${name}=`));
        return hit ? hit.split('=')[1] : def;
      };
      const outPath = arg('shot', path.join(os.tmpdir(), 'minic-shot.png'));
      const theme = arg('theme', 'dark');
      const tab = arg('tab', '');
      const menu = arg('menu', '');
      const autoCompile = process.argv.includes('--compile')
        || process.argv.includes('--demo')
        || process.argv.includes('--break')
        || process.argv.includes('--parse')
        || process.argv.includes('--run')
        || process.argv.includes('--diagjump');
      const breakIt = process.argv.includes('--break');
      const runIt = process.argv.includes('--run');
      const multifile = process.argv.includes('--multifile');
      const diagJump = process.argv.includes('--diagjump');
      const includeParse = process.argv.includes('--parse');
      const fontSize = +arg('fontsize', 0);

      // 等 React 挂载、字体就绪与首屏自动编译完成
      for (let i = 0; i < 60; i++) {
        try {
          const ready = await win.webContents.executeJavaScript('typeof window.__minicTest === "function"');
          if (ready) break;
        } catch { /* 页面仍在装载 */ }
        await sleep(150);
      }
      await sleep(1200);

      try {
        const payload = {
          theme,
          compile: autoCompile && !breakIt && !diagJump && !runIt,
          break: breakIt,
          run: runIt,
          includeParse,
          tab,
          menu,
          multifile,
          diagJump: diagJump ? { sample: arg('sample', 'err-mixed-all'), index: +arg('diagidx', 0) } : null,
          fontSize,
        };
        const r = await win.webContents.executeJavaScript(
          `window.__minicTest && window.__minicTest(${JSON.stringify(payload)})`,
        );
        console.log('TESTRESULT ' + JSON.stringify(r));
      } catch (e) {
        console.error('auto test failed:', e);
      }
      await sleep(1800);
      const image = await win.webContents.capturePage();
      fs.writeFileSync(outPath, image.toPNG());
      console.log('SHOT_SAVED ' + outPath);
      setTimeout(() => app.quit(), 200);
    });
  }
}

/* ------------------------------------------------------------------ */
/* IPC                                                                */
/* ------------------------------------------------------------------ */
ipcMain.handle('win:minimize', () => win && win.minimize());
ipcMain.handle('win:maximize', () => {
  if (!win) return false;
  if (win.isMaximized()) win.unmaximize();
  else win.maximize();
  return win.isMaximized();
});
ipcMain.handle('win:close', () => win && win.close());
ipcMain.handle('win:isMaximized', () => (win ? win.isMaximized() : false));

ipcMain.handle('app:info', () => {
  const exe = findMinic(__dirname);
  return {
    exePath: exe,
    exeFound: Boolean(exe),
    exeMissingHint: exe ? null : 'MiniCCompiler/build/minic.exe',
    electron: process.versions.electron,
    node: process.versions.node,
    chrome: process.versions.chrome,
    workDir: path.join(os.tmpdir(), 'minic-gui'),
  };
});

ipcMain.handle('dialog:open', async () => {
  const r = await dialog.showOpenDialog(win, {
    title: '打开 MiniC 源文件',
    filters: [
      { name: 'MiniC 源文件', extensions: ['mc', 'c', 'txt'] },
      { name: '全部文件', extensions: ['*'] },
    ],
    properties: ['openFile'],
  });
  if (r.canceled || !r.filePaths.length) return { canceled: true };
  const filePath = r.filePaths[0];
  return {
    canceled: false,
    filePath,
    name: path.basename(filePath),
    content: decodeBuffer(fs.readFileSync(filePath)),
  };
});

ipcMain.handle('file:save', async (_e, { filePath, content }) => {
  try {
    fs.writeFileSync(filePath, String(content == null ? '' : content), 'utf8');
    return { ok: true, filePath };
  } catch (e) {
    return { ok: false, message: String(e) };
  }
});

ipcMain.handle('file:saveas', async (_e, { content, defaultName }) => {
  const r = await dialog.showSaveDialog(win, {
    title: '另存为',
    defaultPath: defaultName || 'work.mc',
    filters: [{ name: 'MiniC 源文件', extensions: ['mc'] }],
  });
  if (r.canceled || !r.filePath) return { canceled: true };
  try {
    fs.writeFileSync(r.filePath, String(content == null ? '' : content), 'utf8');
    return { canceled: false, ok: true, filePath: r.filePath, name: path.basename(r.filePath) };
  } catch (e) {
    return { canceled: false, ok: false, message: String(e) };
  }
});

ipcMain.handle('compile:run', async (_e, payload) => {
  try {
    return await compileSource({ ...payload, guiDir: __dirname });
  } catch (e) {
    return { ok: false, reason: 'exception', message: String(e) };
  }
});

ipcMain.handle('run:run', async (_e, payload) => {
  try {
    return await runSource({ ...payload, guiDir: __dirname });
  } catch (e) {
    return { ok: false, reason: 'exception', message: String(e) };
  }
});

/* ------------------------------------------------------------------ */
/* 生命周期                                                            */
/* ------------------------------------------------------------------ */
app.whenReady().then(() => {
  createWindow();
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});
