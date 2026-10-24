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
    width: 1280,
    height: 824,
    minWidth: 1000,
    minHeight: 640,
    frame: false,
    show: false,
    backgroundColor: '#202020',
    title: 'MiniC 编译器',
    icon: path.join(__dirname, 'assets', 'icon.png'),
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      spellcheck: false,
    },
  });

  win.loadFile(path.join(__dirname, 'src', 'index.html'));
  win.once('ready-to-show', () => win.show());
  // 禁用页面级缩放（Ctrl+滚轮只用于编辑器字号，不应缩放整个窗口）
  try {
    win.webContents.setVisualZoomLevelLimits(1, 1);
  } catch { /* 旧版本无此 API */ }

  // 截图模式（验收/回归用）：--shot=<png> [--theme=light] [--compile] [--break]
  if (isShot) {
    win.webContents.once('did-finish-load', async () => {
      const arg = (name, def) => {
        const hit = process.argv.find((a) => a.startsWith(`--${name}=`));
        return hit ? hit.split('=')[1] : def;
      };
      const outPath = arg('shot', path.join(os.tmpdir(), 'minic-shot.png'));
      const theme = arg('theme', 'dark');
      const tab = arg('tab', '');
      const menu = arg('menu', '');
      const autoCompile = process.argv.includes('--compile');
      const breakIt = process.argv.includes('--break');
      const twice = process.argv.includes('--twice');
      const multifile = process.argv.includes('--multifile');
      const fmt = process.argv.includes('--fmt');
      const editTest = process.argv.includes('--edittest');
      const dupTest = process.argv.includes('--duptest');
      const demo = process.argv.includes('--demo');
      await new Promise((r) => setTimeout(r, 900));

      // Ctrl+滚轮缩放回归测试
      if (process.argv.includes('--wheeltest')) {
        const r = await win.webContents.executeJavaScript(`(() => {
          const wrap = document.getElementById('code-wrap');
          const src = document.getElementById('source');
          const before = getComputedStyle(src).fontSize;
          for (let i = 0; i < 3; i++) wrap.dispatchEvent(new WheelEvent('wheel', { deltaY: -120, ctrlKey: true, bubbles: true, cancelable: true }));
          const after = getComputedStyle(src).fontSize;
          for (let i = 0; i < 8; i++) wrap.dispatchEvent(new WheelEvent('wheel', { deltaY: 120, ctrlKey: true, bubbles: true, cancelable: true }));
          const after2 = getComputedStyle(src).fontSize;
          return { before, after, after2, stored: localStorage.getItem('minic.fontSize') };
        })()`);
        console.log('WHEELTEST ' + JSON.stringify(r));
      }

      if (autoCompile || breakIt || tab || menu || twice || multifile || fmt || editTest || dupTest || demo) {
        try {
          const r = await win.webContents.executeJavaScript(
            `window.__minicTest && window.__minicTest(${JSON.stringify({ theme, break: breakIt, tab, menu, twice, multifile, fmt, editTest, dupTest, demo })})`,
          );
          if (twice && r) console.log('TWICETEST ' + JSON.stringify(r));
          if (multifile && r) console.log('MULTITEST ' + JSON.stringify(r));
          if (fmt && r) console.log('FMTTEST ' + JSON.stringify(r));
          if (editTest && r) console.log('EDITTEST ' + JSON.stringify(r));
          if (dupTest && r) console.log('DUPTEST ' + JSON.stringify(r));
          if (demo && r) console.log('DEMO ' + JSON.stringify(r));
        } catch (e) {
          console.error('auto test failed:', e);
        }
        await new Promise((r) => setTimeout(r, 1800));
      }
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
