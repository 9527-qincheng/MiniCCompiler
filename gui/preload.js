'use strict';
/** 预加载脚本：只暴露必要的 IPC 能力给渲染进程（contextIsolation 开启）。 */
const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('minicAPI', {
  minimize: () => ipcRenderer.invoke('win:minimize'),
  maximize: () => ipcRenderer.invoke('win:maximize'),
  close: () => ipcRenderer.invoke('win:close'),
  isMaximized: () => ipcRenderer.invoke('win:isMaximized'),
  info: () => ipcRenderer.invoke('app:info'),
  openFile: () => ipcRenderer.invoke('dialog:open'),
  saveFile: (filePath, content) => ipcRenderer.invoke('file:save', { filePath, content }),
  saveAs: (content, defaultName) => ipcRenderer.invoke('file:saveas', { content, defaultName }),
  compile: (payload) => ipcRenderer.invoke('compile:run', payload),
});
