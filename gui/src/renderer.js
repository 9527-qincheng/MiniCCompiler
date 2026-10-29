'use strict';
/* ============================================================
   MiniC 编译器 GUI — 渲染进程
   编辑器（高亮叠加层 + 行号）、7 类产物标签、诊断跳转、双主题
   ============================================================ */

const $ = (id) => document.getElementById(id);
const app = $('app');

/* ------------------------- 示例源码 ------------------------- */
/* 示例库本体在 src/samples.js（20 条，逐条在真实工具链上实测过）。
   这里只负责取值，以及保留几个旧名字给截图/回归钩子用。 */
const SAMPLE_LIST =
  typeof SAMPLES !== 'undefined' && Array.isArray(SAMPLES) ? SAMPLES : [];

function sampleById(id) {
  return SAMPLE_LIST.find((s) => s.id === id) || { id: '', label: '', name: '', text: '' };
}

const SAMPLE_GCD = sampleById('gcd').text;
const SAMPLE_LEX = sampleById('lex').text;
const SAMPLE_BAD = sampleById('err-combined').text;

/** 专用于格式化回归测试的"乱排版"源码 */
const SAMPLE_MESSY = `/*格式化测试*/
int a ;
int b;
int main( ) {
int t ;
a=36 ;
     b =24;
while ( a != b ) {
if(a>b){a=a-b;}else{b=b-a;}
}
t=a;
return t ;
}
`;

/* ------------------------- 状态 ------------------------- */
const state = {
  docs: [],          // 打开的文件列表
  activeId: null,
  fileName: '未命名.mc',
  filePath: null,
  dirty: false,
  theme: 'dark',
  fontSize: 13,
  lineHeight: 19.5,
  artifacts: { tokens: null, parse: null, ast: null, sym: null, quad: null, asm: null },
  diagnostics: [],
  errorLines: [],
  warnLines: [],
  activeTab: 'token',
  compiled: false,
  lastResult: null,
  runResult: null,
  busy: false,
  autoFormatOnSave: true,
};

/* ------------------------- DOM 引用 ------------------------- */
const el = {
  source: $('source'),
  highlight: $('highlight'),
  gutter: $('gutter'),
  marks: $('line-marks'),
  placeholder: $('editor-placeholder'),
  currentLine: $('current-line'),
  tokenEmpty: $('token-empty'),
  fileName: $('file-name'),
  dirty: $('dirty-mark'),
  tokenRows: $('token-rows'),
  tokenFoot: $('token-foot'),
  rawAst: $('raw-ast'),
  rawSym: $('raw-sym'),
  rawQuad: $('raw-quad'),
  rawAsm: $('raw-asm'),
  rawRun: $('raw-run'),
  runEmpty: $('run-empty'),
  runFoot: $('run-foot'),
  cRun: $('c-run'),
  diagHead: $('diag-head'),
  diagList: $('diag-list'),
  diagFoot: $('diag-foot'),
  cToken: $('c-token'),
  cSym: $('c-sym'),
  cQuad: $('c-quad'),
  cDiag: $('c-diag'),
  statusCursor: $('sb-cursor'),
  statusExe: $('sb-exe'),
  statusTheme: $('sb-theme'),
  topDot: $('top-dot'),
  topStatus: $('top-status-text'),
  menuPop: $('menu-pop'),
  // 语法手册
  bookOverlay: $('book-overlay'),
  bookToc: $('book-toc'),
  bookContent: $('book-content'),
  bookSearch: $('book-search'),
  bookFoot: $('book-foot'),
  btnBook: $('btn-book'),
  bookClose: $('book-close'),
};

/* ------------------------- 工具 ------------------------- */
function escapeHtml(s) {
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function measureLineHeight() {
  state.lineHeight = parseFloat(getComputedStyle(el.source).lineHeight) || state.fontSize * 1.5;
}

/* ------------------------- 语法高亮 ------------------------- */
const KEYWORDS = new Set(['int', 'float', 'bool', 'char', 'void', 'if', 'else', 'while', 'print', 'return', 'true', 'false']);

function highlightMiniC(src) {
  let out = '';
  let i = 0;
  const n = src.length;
  while (i < n) {
    const c = src[i];
    const rest = src.slice(i);

    // 块注释
    if (c === '/' && src[i + 1] === '*') {
      const end = src.indexOf('*/', i + 2);
      const stop = end === -1 ? n : end + 2;
      out += `<span class="tok-comment">${escapeHtml(src.slice(i, stop))}</span>`;
      i = stop;
      continue;
    }
    // 行注释
    if (c === '/' && src[i + 1] === '/') {
      let end = src.indexOf('\n', i);
      if (end === -1) end = n;
      out += `<span class="tok-comment">${escapeHtml(src.slice(i, end))}</span>`;
      i = end;
      continue;
    }
    // 字符串
    if (c === '"') {
      let j = i + 1;
      while (j < n && src[j] !== '"') { if (src[j] === '\\') j++; j++; }
      const stop = Math.min(n, j + 1);
      out += `<span class="tok-str">${escapeHtml(src.slice(i, stop))}</span>`;
      i = stop;
      continue;
    }
    // 字符常量
    if (c === "'") {
      let j = i + 1;
      while (j < n && src[j] !== "'") { if (src[j] === '\\') j++; j++; }
      const stop = Math.min(n, j + 1);
      out += `<span class="tok-str">${escapeHtml(src.slice(i, stop))}</span>`;
      i = stop;
      continue;
    }
    // 数字
    if (/[0-9]/.test(c)) {
      const m = rest.match(/^[0-9]+(\.[0-9]+)?/);
      out += `<span class="tok-num">${m[0]}</span>`;
      i += m[0].length;
      continue;
    }
    // 标识符 / 关键字
    if (/[A-Za-z_]/.test(c)) {
      const m = rest.match(/^[A-Za-z_][A-Za-z0-9_]*/);
      const word = m[0];
      out += KEYWORDS.has(word)
        ? `<span class="tok-kw">${word}</span>`
        : `<span class="tok-id">${word}</span>`;
      i += word.length;
      continue;
    }
    // 运算符
    if (/[+\-*/=<>!&|]/.test(c)) {
      out += `<span class="tok-op">${escapeHtml(c)}</span>`;
      i++;
      continue;
    }
    // 标点
    if (/[(){}[\];,]/.test(c)) {
      out += `<span class="tok-punct">${escapeHtml(c)}</span>`;
      i++;
      continue;
    }
    // 空白与其他（含中文注释以外的字符）
    out += escapeHtml(c);
    i++;
  }
  return out;
}

/* ------------------------- 代码格式化（轻量美化） ------------------------- */
const BINARY_OPS = new Set(['=', '==', '!=', '<=', '>=', '<', '>', '&&', '||']);
const KEYWORD_BEFORE_PAREN = new Set(['if', 'while', 'return']);

/** 把一行切成记号（保留字符串/注释原样；块注释跨行由外层状态处理） */
function tokenizeLine(line, markBlockComment) {
  const toks = [];
  let i = 0;
  while (i < line.length) {
    const c = line[i];
    if (/\s/.test(c)) { i++; continue; }
    if (c === '/' && line[i + 1] === '/') { toks.push({ t: 'comment', v: line.slice(i) }); break; }
    if (c === '/' && line[i + 1] === '*') {
      const end = line.indexOf('*/', i + 2);
      if (end === -1) { toks.push({ t: 'comment', v: line.slice(i) }); markBlockComment(true); break; }
      toks.push({ t: 'comment', v: line.slice(i, end + 2) });
      i = end + 2;
      continue;
    }
    if (c === '"' || c === "'") {
      let j = i + 1;
      while (j < line.length && line[j] !== c) { if (line[j] === '\\') j++; j++; }
      j = Math.min(line.length, j + 1);
      toks.push({ t: 'lit', v: line.slice(i, j) });
      i = j;
      continue;
    }
    if (/[A-Za-z_]/.test(c)) {
      const m = line.slice(i).match(/^[A-Za-z_][A-Za-z0-9_]*/);
      toks.push({ t: 'id', v: m[0] });
      i += m[0].length;
      continue;
    }
    if (/[0-9]/.test(c)) {
      const m = line.slice(i).match(/^[0-9]+(\.[0-9]+)?/);
      toks.push({ t: 'num', v: m[0] });
      i += m[0].length;
      continue;
    }
    const two = line.slice(i, i + 2);
    if (['==', '!=', '<=', '>=', '&&', '||'].includes(two)) {
      toks.push({ t: 'op', v: two });
      i += 2;
      continue;
    }
    if ('+-*/=<>!'.includes(c)) { toks.push({ t: 'op', v: c }); i++; continue; }
    if ('(){}[],;'.includes(c)) { toks.push({ t: 'p', v: c }); i++; continue; }
    toks.push({ t: 'other', v: c });
    i++;
  }
  return toks;
}

/** 按 C 风格规则拼接记号（只调整空白，不改动记号和顺序） */
function joinTokens(toks) {
  let out = '';
  let pendingSpace = false;
  const isBinary = (tok, prev) => {
    if (BINARY_OPS.has(tok.v)) return true;
    if (['+', '-', '*', '/'].includes(tok.v)) {
      if (!prev) return false;
      return prev.t === 'id' || prev.t === 'num' || prev.t === 'lit' || (prev.t === 'p' && (prev.v === ')' || prev.v === ']'));
    }
    return false;
  };
  toks.forEach((tok, idx) => {
    const prev = idx ? toks[idx - 1] : null;
    if (tok.t === 'comment') {
      if (out && !out.endsWith(' ')) out += ' ';
      out += tok.v;
      pendingSpace = false;
      return;
    }
    if (tok.t === 'p') {
      if (tok.v === '(') {
        if (prev && prev.t === 'id' && KEYWORD_BEFORE_PAREN.has(prev.v) && !out.endsWith(' ')) out += ' ';
        out += '(';
        pendingSpace = false;
        return;
      }
      if (tok.v === '[') { out += '['; pendingSpace = false; return; }
      if (tok.v === ')' || tok.v === ']' || tok.v === ',' || tok.v === ';') {
        if (out.endsWith(' ')) out = out.replace(/ $/, '');
        out += tok.v;
        pendingSpace = tok.v === ',';
        return;
      }
      if (tok.v === '{') {
        if (out && !out.endsWith(' ') && !out.endsWith('(') && !out.endsWith('[')) out += ' ';
        out += '{';
        pendingSpace = true;
        return;
      }
      if (tok.v === '}') {
        if (out.endsWith(' ')) out = out.replace(/ $/, '');
        if (out.endsWith(';')) out += ' ';
        out += '}';
        pendingSpace = true;
        return;
      }
    }
    if (tok.t === 'op') {
      if (isBinary(tok, prev)) {
        if (out && !out.endsWith(' ')) out += ' ';
        out += tok.v;
        pendingSpace = true;
        return;
      }
      out += tok.v;
      pendingSpace = false;
      return;
    }
    // id / num / lit / other：与前一"词"之间必须有空格
    const wordLike = (t) => t && (t.t === 'id' || t.t === 'num' || t.t === 'lit');
    const needSpace = wordLike(prev) && wordLike(tok);
    if ((pendingSpace || needSpace) && out && !out.endsWith(' ')) out += ' ';
    out += tok.v;
    pendingSpace = false;
  });
  return out.replace(/\s+$/, '');
}

/** 行级格式化：重算缩进、规范空白、压缩空行（不改变换行结构） */
function formatMiniC(src) {
  const lines = src.replace(/\r\n?/g, '\n').split('\n');
  const out = [];
  let depth = 0;
  let inBlock = false;
  let blanks = 0;

  for (const raw of lines) {
    const line = raw.replace(/[ \t]+$/, '');
    if (inBlock) {
      out.push('    '.repeat(depth) + line.trim());
      if (line.includes('*/')) inBlock = false;
      continue;
    }
    if (!line.trim()) {
      blanks++;
      if (blanks <= 1 && out.length && out[out.length - 1] !== '') out.push('');
      continue;
    }
    blanks = 0;
    const toks = tokenizeLine(line.trim(), (v) => { inBlock = v; });
    const content = joinTokens(toks);
    const dedent = content.startsWith('}') ? 1 : 0;
    out.push('    '.repeat(Math.max(0, depth - dedent)) + content);
    let opens = 0;
    let closes = 0;
    for (const t of toks) {
      if (t.t === 'p' && t.v === '{') opens++;
      if (t.t === 'p' && t.v === '}') closes++;
    }
    depth = Math.max(0, depth + opens - closes);
  }
  while (out.length && out[out.length - 1] === '') out.pop();
  return out.join('\n') + '\n';
}

/** 规范化单行（保留缩进）：a=10; → a = 10; */
function normalizeLine(lineText) {
  const indent = (lineText.match(/^[ \t]*/) || [''])[0];
  const body = lineText.slice(indent.length);
  if (!body.trim()) return lineText;
  const t = body.trim();
  // 注释行 / 不完整的块注释：不动
  if (t.startsWith('//') || t.startsWith('/*') || t.startsWith('*')) return lineText;
  let openedBlock = false;
  const toks = tokenizeLine(t, (v) => { openedBlock = v; });
  if (openedBlock) return lineText;
  return indent + joinTokens(toks);
}

/** 应用格式化（保留撤销栈） */
function formatDocument({ silent = false } = {}) {
  const src = el.source.value;
  if (!src.trim()) return false;
  const out = formatMiniC(src);
  if (out === src) {
    if (!silent) statusNote('格式无变化');
    return false;
  }
  const caret = el.source.selectionStart;
  el.source.focus();
  el.source.setSelectionRange(0, src.length);
  // 优先用 execCommand（保留撤销栈）；未聚焦等情况下会失败，退回 setRangeText
  let ok = false;
  try { ok = document.execCommand('insertText', false, out); } catch { ok = false; }
  if (!ok || el.source.value !== out) {
    el.source.setRangeText(out, 0, src.length, 'end');
  }
  const pos = Math.min(caret, out.length);
  el.source.setSelectionRange(pos, pos);
  markDirty(true);
  renderEditor();
  updateCursor();
  if (!silent) statusNote('已格式化');
  return true;
}

/* ------------------------- 编辑器渲染 ------------------------- */
function renderEditor() {
  const text = el.source.value;
  el.highlight.innerHTML = highlightMiniC(text) + '\n';
  if (el.placeholder) el.placeholder.classList.toggle('hidden', text.length > 0);
  const lineCount = text.split('\n').length;
  let g = '';
  for (let i = 1; i <= lineCount; i++) g += i + '\n';
  el.gutter.textContent = g;
  renderMarks();
  syncScroll();
}

function syncScroll() {
  el.highlight.scrollTop = el.source.scrollTop;
  el.highlight.scrollLeft = el.source.scrollLeft;
  el.gutter.scrollTop = el.source.scrollTop;
  el.marks.style.transform = `translateY(${-el.source.scrollTop}px)`;
  updateCurrentLine();
}

/** 当前行高亮 */
function updateCurrentLine() {
  const cl = el.currentLine;
  if (!cl) return;
  const padTop = parseFloat(getComputedStyle(el.source).paddingTop) || 0;
  const pos = el.source.selectionStart;
  const before = el.source.value.slice(0, pos);
  const line = before.split('\n').length;
  cl.style.display = 'block';
  cl.style.height = state.lineHeight + 'px';
  cl.style.top = `${padTop + (line - 1) * state.lineHeight - el.source.scrollTop}px`;
}

function renderMarks() {
  const lh = state.lineHeight;
  const padTop = parseFloat(getComputedStyle(el.source).paddingTop) || 0;
  let html = '';
  for (const line of state.errorLines) {
    html += `<div class="line-mark" style="top:${padTop + (line - 1) * lh}px;height:${lh}px"></div>`;
  }
  for (const line of state.warnLines) {
    html += `<div class="line-mark warn" style="top:${padTop + (line - 1) * lh}px;height:${lh}px"></div>`;
  }
  el.marks.innerHTML = html;
}

function updateCursor() {
  const pos = el.source.selectionStart;
  const before = el.source.value.slice(0, pos);
  const line = before.split('\n').length;
  const col = pos - before.lastIndexOf('\n');
  el.statusCursor.textContent = `行 ${line} · 列 ${col}`;
  updateCurrentLine();
}

function setSource(text, { resetMarks = true } = {}) {
  el.source.value = text;
  if (resetMarks) {
    state.errorLines = [];
    state.warnLines = [];
  }
  renderEditor();
  updateCursor();
}

function markDirty(d) {
  state.dirty = d;
  el.dirty.classList.toggle('show', d);
  renderFileTabs();
}

function setFile(name, filePath) {
  state.fileName = name || '未命名.mc';
  state.filePath = filePath || null;
  el.fileName.textContent = state.fileName;
  el.fileName.title = filePath || state.fileName;
  const doc = activeDoc();
  if (doc) {
    doc.name = state.fileName;
    doc.path = state.filePath;
  }
  markDirty(false);
}

/* ------------------------- 多文件管理 ------------------------- */
function emptyArtifacts() {
  return { tokens: null, parse: null, ast: null, sym: null, quad: null, asm: null };
}

let docSeq = 0;

function newDoc({ name, content = '', filePath = null } = {}) {
  docSeq += 1;
  const doc = {
    id: 'doc' + docSeq,
    name: name || (docSeq === 1 ? '未命名.mc' : `未命名-${docSeq}.mc`),
    path: filePath,
    content,
    dirty: false,
    compiled: false,
    lastResult: null,
    runResult: null,
    artifacts: emptyArtifacts(),
    diagnostics: [],
    errorLines: [],
    warnLines: [],
  };
  state.docs.push(doc);
  return doc;
}

function activeDoc() {
  return state.docs.find((d) => d.id === state.activeId) || null;
}

function snapshotActive() {
  const doc = activeDoc();
  if (!doc) return;
  doc.content = el.source.value;
  doc.dirty = state.dirty;
  doc.compiled = state.compiled;
  doc.lastResult = state.lastResult;
  doc.runResult = state.runResult;
  doc.artifacts = state.artifacts;
  doc.diagnostics = state.diagnostics;
  doc.errorLines = state.errorLines;
  doc.warnLines = state.warnLines;
  doc.activeTab = state.activeTab;
  doc.path = state.filePath;
}

function renderFileTabs() {
  const list = $('file-tab-list');
  if (!list) return;
  list.innerHTML = state.docs
    .map((d) => {
      const isActive = d.id === state.activeId;
      const dirty = isActive ? state.dirty : d.dirty;
      return (
        `<div class="file-tab${isActive ? ' active' : ''}" data-id="${d.id}" title="${escapeHtml(d.path || d.name)}">` +
        `<span class="tab-name">${escapeHtml(d.name)}</span>` +
        (dirty ? '<span class="tab-dirty"></span>' : '') +
        '<span class="tab-close" data-close="' + d.id + '" title="关闭">' +
        '<svg width="10" height="10" viewBox="0 0 12 12"><path d="M2.4 2.4l7.2 7.2M9.6 2.4L2.4 9.6" stroke="currentColor" stroke-width="1.3"/></svg>' +
        '</span></div>'
      );
    })
    .join('');
}

function activateDoc(id, { skipSnapshot = false } = {}) {
  if (!skipSnapshot) snapshotActive();
  const doc = state.docs.find((d) => d.id === id);
  if (!doc) return;
  state.activeId = id;
  state.fileName = doc.name;
  state.filePath = doc.path;
  state.compiled = doc.compiled;
  state.lastResult = doc.lastResult;
  state.runResult = doc.runResult || null;
  state.artifacts = doc.artifacts || emptyArtifacts();
  state.diagnostics = doc.diagnostics || [];
  state.errorLines = doc.errorLines || [];
  state.warnLines = doc.warnLines || [];

  el.fileName.textContent = doc.name;
  el.fileName.title = doc.path || doc.name;
  el.dirty.classList.toggle('show', Boolean(doc.dirty));

  setSource(doc.content, { resetMarks: false });
  renderArtifactsView();
  renderRunResult(state.runResult);
  selectTab(doc.activeTab || 'token');
  renderFileTabs();

  if (!doc.compiled) {
    el.topDot.className = 'dot ok';
    el.topStatus.textContent = '就绪';
  } else if (doc.lastResult && doc.lastResult.exitCode === 0) {
    el.topDot.className = 'dot ok';
    const warns = (doc.diagnostics || []).filter((d) => d.severity === 'warning').length;
    el.topStatus.textContent = warns ? `编译完成 · ${warns} 个警告` : '就绪';
  } else {
    el.topDot.className = 'dot err';
    const errs = (doc.diagnostics || []).filter((d) => d.severity === 'error').length;
    el.topStatus.textContent = `编译失败 · ${errs} 个错误`;
  }
}

/** 路径归一化：Windows 下大小写不敏感、斜杠方向不一 */
function normPath(p) {
  if (!p) return '';
  return String(p).replace(/\//g, '\\').toLowerCase();
}

/**
 * 在已打开的文档里找一个"同一个文件"：
 * - 有真实路径 → 按路径匹配（优先）
 * - 无路径（示例文件）→ 按同名匹配
 * 找到就直接切过去，绝不再新建标签。
 */
function findOpenedDoc({ name, filePath = null }) {
  const target = normPath(filePath);
  if (target) {
    const byPath = state.docs.find((d) => d.path && normPath(d.path) === target);
    if (byPath) return byPath;
  } else {
    // 示例/无路径文档：同名且也无路径即视为同一个
    const byName = state.docs.find((d) => !d.path && d.name === name);
    if (byName) return byName;
  }
  return null;
}

/** 打开文件/载入示例时：如果当前是"空的、干净的、没编译过"的新文件，就直接复用，避免堆积空白标签 */
function reuseOrCreateDoc({ name, content, filePath = null }) {
  // ① 已经开过同一个文件 → 聚焦，不新建
  const opened = findOpenedDoc({ name, filePath });
  if (opened) {
    activateDoc(opened.id);
    statusNote(`已切换到已打开的 ${opened.name}`);
    return opened;
  }
  // ② 当前是空白未命名文件 → 就地复用
  const cur = activeDoc();
  if (cur && !cur.dirty && !cur.compiled && !cur.path && !el.source.value.trim()) {
    cur.name = name;
    cur.path = filePath;
    cur.content = content;
    cur.dirty = false;
    // 跳过快照：否则会用编辑器里的旧内容覆盖刚写入的新内容
    activateDoc(cur.id, { skipSnapshot: true });
    return cur;
  }
  snapshotActive();
  const doc = newDoc({ name, content, filePath });
  state.activeId = null;
  activateDoc(doc.id);
  return doc;
}

function newFile() {
  snapshotActive();
  const doc = newDoc({});
  state.activeId = null;
  activateDoc(doc.id);
}

function closeDoc(id) {
  const idx = state.docs.findIndex((d) => d.id === id);
  if (idx < 0) return;
  const doc = state.docs[idx];
  const dirty = id === state.activeId ? state.dirty : doc.dirty;
  if (dirty && !window.confirm(`「${doc.name}」有未保存的修改，确定关闭吗？`)) return;

  state.docs.splice(idx, 1);
  if (state.activeId === id) {
    state.activeId = null;
    const next = state.docs[Math.min(idx, state.docs.length - 1)];
    if (next) {
      activateDoc(next.id);
    } else {
      const d = newDoc({ name: '未命名.mc', content: '' });
      state.activeId = null;
      activateDoc(d.id);
    }
  } else {
    renderFileTabs();
  }
}

/** 按当前 state 重绘右侧产物视图（6 份编译产物 + 运行输出） */
function renderArtifactsView() {
  renderTokens();
  renderAst(el.rawAst, state.artifacts.ast);
  renderRaw(el.rawSym, state.artifacts.sym, 'sym');
  renderRaw(el.rawQuad, state.artifacts.quad, 'quad');
  renderRaw(el.rawAsm, state.artifacts.asm, 'asm');
  renderDiagnostics(state.lastResult);
  renderMarks();
  el.cSym.textContent = state.artifacts.sym ? String(countSymbols(state.artifacts.sym)) : '';
  el.cQuad.textContent = state.artifacts.quad
    ? String(Math.max(0, state.artifacts.quad.split(/\r?\n/).filter((l) => l.trim() && !l.trim().startsWith('//')).length))
    : '';
}

/* ------------------------- 主题 ------------------------- */
function applyTheme(theme) {
  state.theme = theme;
  app.dataset.theme = theme;
  el.statusTheme.textContent = theme === 'dark' ? '暗色主题' : '亮色主题';
  $('theme-label').textContent = theme === 'dark' ? '主题' : '主题';
  try { localStorage.setItem('minic.theme', theme); } catch { /* ignore */ }
}

function toggleTheme() {
  applyTheme(state.theme === 'dark' ? 'light' : 'dark');
}

/* ------------------------- 字号 ------------------------- */
function applyFontSize(size) {
  state.fontSize = Math.max(11, Math.min(20, size));
  const px = state.fontSize + 'px';
  el.source.style.fontSize = px;
  el.highlight.style.fontSize = px;
  el.gutter.style.fontSize = px;
  if (el.placeholder) el.placeholder.style.fontSize = px;
  measureLineHeight();
  renderEditor();
  try { localStorage.setItem('minic.fontSize', String(state.fontSize)); } catch { /* ignore */ }
}

/* ------------------------- Token 表 ------------------------- */
const TOKEN_CLASS = (() => {
  const kw = new Set(['INT', 'FLOAT', 'BOOL', 'CHAR', 'VOID', 'IF', 'ELSE', 'WHILE', 'PRINT', 'RETURN', 'TRUE', 'FALSE']);
  const num = new Set(['NUM_INT', 'NUM_FLOAT', 'CHAR_CONST', 'STRING']);
  const op = new Set(['PLUS', 'MINUS', 'MUL', 'DIV', 'ASSIGN', 'EQ', 'NE', 'LT', 'LE', 'GT', 'GE', 'AND', 'OR', 'NOT']);
  const punct = new Set(['LPAREN', 'RPAREN', 'LBRACE', 'RBRACE', 'LBRACKET', 'RBRACKET', 'SEMI', 'COMMA']);
  return (type) => {
    if (kw.has(type)) return 't-kw';
    if (type === 'IDENT') return 't-id';
    if (num.has(type)) return 't-num';
    if (op.has(type)) return 't-op';
    if (punct.has(type)) return 't-punct';
    return 't-mute';
  };
})();

function parseTokens(text) {
  if (!text) return [];
  const rows = [];
  for (const line of text.split(/\r?\n/)) {
    if (!line.trim()) continue;
    const m = line.match(/^(\d+):(\d+)\s+([A-Z_]+)\s*(.*)$/);
    if (m) rows.push({ line: +m[1], col: +m[2], type: m[3], value: m[4].replace(/\s+$/, '') });
  }
  return rows;
}

const TOKEN_LIMIT = 300;

function renderTokens() {
  const rows = parseTokens(state.artifacts.tokens);
  const slice = rows.slice(0, TOKEN_LIMIT);
  el.tokenRows.innerHTML = slice
    .map(
      (r) =>
        `<tr><td class="c-line">${r.line}</td><td class="c-col">${r.col}</td>` +
        `<td class="c-type"><span class="${TOKEN_CLASS(r.type)}">${escapeHtml(r.type)}</span></td>` +
        `<td class="c-val">${escapeHtml(r.value)}</td></tr>`,
    )
    .join('');
  el.cToken.textContent = rows.length ? String(rows.length) : '0';
  if (el.tokenEmpty) el.tokenEmpty.classList.toggle('show', rows.length === 0);
  el.tokenFoot.textContent = rows.length
    ? rows.length > TOKEN_LIMIT
      ? `共 ${rows.length} 个记号（节选前 ${TOKEN_LIMIT} 行）`
      : `共 ${rows.length} 个记号`
    : '暂无记号 — 点击「编译」生成';
}

/* ------------------------- 纯文本产物 ------------------------- */
function renderRaw(target, text, kind) {
  if (!text) {
    target.innerHTML = `<span class="raw-comment">暂无内容 — 点击「编译」生成</span>`;
    return;
  }
  const lines = text.replace(/\s+$/, '').split(/\r?\n/);
  target.innerHTML = lines
    .map((line) => {
      const esc = escapeHtml(line);
      if (/^\s*(#|\/\/)/.test(line)) return `<span class="raw-comment">${esc}</span>`;
      if (/^===/.test(line)) return `<span class="raw-head">${esc}</span>`;
      if (/^\s*--/.test(line)) return `<span class="raw-head">${esc}</span>`;
      if (kind === 'sym' && /^\s{2}\S/.test(line)) return esc;
      return esc;
    })
    .join('\n');
}

/* ------------------------- 语法树（框线树） ------------------------- */
// 叶子"值"节点：这类只做浅色强调，其余视为结构节点用主色
const AST_VALUE_HEAD = new Set(['Num', 'NumInt', 'NumFloat', 'Char', 'CharConst', 'String', 'Str', 'True', 'False']);

function astNodeClass(name) {
  const head = name.replace(/\([\s\S]*$/, '').trim();
  if (head === 'Ident' || head === 'Var') return 'a-id';
  if (AST_VALUE_HEAD.has(head)) return 'a-num';
  return 'a-struct';
}

/** 把缩进式 AST 文本解析成带层级/位置/末子标记的节点表 */
function parseAstNodes(text) {
  const raw = [];
  for (const line of text.replace(/\s+$/, '').split(/\r?\n/)) {
    if (!line.trim()) continue;
    const m = line.match(/^(\s*)(.+?)\s+@(\d+):(\d+)\s*$/);
    if (m) {
      raw.push({ indent: m[1].length, name: m[2], line: +m[3], col: +m[4] });
    } else {
      const m2 = line.match(/^(\s*)(\S[\s\S]*)$/);
      if (m2) raw.push({ indent: m2[1].length, name: m2[2].replace(/\s+$/, ''), line: 0, col: 0 });
    }
  }
  if (!raw.length) return [];
  // 缩进宽度 → 层级（不同缩进步长都能正确归层）
  const widths = [...new Set(raw.map((r) => r.indent))].sort((a, b) => a - b);
  const levelOf = new Map(widths.map((w, i) => [w, i]));
  for (const r of raw) r.level = levelOf.get(r.indent) || 0;

  // 判定「是否是同级最后一个」→ 决定 └── 还是 ├──
  const lastOfLevel = [];
  for (let i = 0; i < raw.length; i++) {
    const n = raw[i];
    let isLast = true;
    for (let j = i + 1; j < raw.length; j++) {
      if (raw[j].level < n.level) break;
      if (raw[j].level === n.level) { isLast = false; break; }
    }
    n.isLast = isLast;
    n.ancestorLast = lastOfLevel.slice(0, n.level);
    lastOfLevel[n.level] = isLast;
  }
  return raw;
}

/** 渲染为 tree 命令风格的框线树，位置注解右对齐成列 */
function renderAst(target, text) {
  if (!text) {
    target.innerHTML = '<span class="raw-comment">暂无内容 — 点击「编译」生成</span>';
    return;
  }
  const nodes = parseAstNodes(text);
  if (!nodes.length) { renderRaw(target, text, 'ast'); return; }

  // 位置列：默认第 40 列，节点名过长时整体右移，保证始终对齐
  let maxLen = 0;
  for (const n of nodes) maxLen = Math.max(maxLen, n.level * 4 + n.name.length);
  const col = Math.max(40, maxLen + 2);

  target.innerHTML = nodes
    .map((n) => {
      const pad = ' '.repeat(Math.max(2, col - (n.level * 4 + n.name.length)));
      const pos = n.line ? `@${n.line}:${n.col}` : '';
      const click = n.line ? ' clickable' : '';
      const attrs = n.line ? ` data-line="${n.line}" data-col="${n.col}"` : '';
      // 框线：祖先层竖线（该层已是末子则断掉）+ 本层 └/├ + 横线
      let gutter = '';
      for (let d = 1; d < n.level; d++) {
        gutter += `<span class="ast-cell${n.ancestorLast[d] ? ' pass' : ''}"></span>`;
      }
      if (n.level > 0) gutter += `<span class="ast-cell arm${n.isLast ? ' last' : ''}"></span>`;
      return (
        `<span class="ast-line${click}" style="padding-left:${n.level * 4}ch"${attrs}>` +
        (gutter ? `<span class="ast-gutter">${gutter}</span>` : '') +
        `<span class="ast-node ${astNodeClass(n.name)}">${escapeHtml(n.name)}</span>` +
        `<span class="ast-pos">${pad}${escapeHtml(pos)}</span>` +
        '</span>'
      );
    })
    .join('');
}

function countSymbols(symText) {
  if (!symText) return 0;
  let n = 0;
  let inSection = false;
  for (const line of symText.split(/\r?\n/)) {
    if (/^--\s/.test(line)) { inSection = /Variables|Constants/.test(line); continue; }
    if (inSection && /^\s{2}\S/.test(line)) n++;
  }
  return n;
}

/* ------------------------- 诊断 ------------------------- */
const DIAG_RE = /^minic:\s*(error|warning):\s*(\d+):(\d+):\s*(.*)$/;

function inferStage(msg) {
  const m = msg;
  if (/cannot open|unterminated|unexpected character|invalid character/i.test(m)) return '词法';
  if (/unexpected|expected one of|expected |syntax/i.test(m)) return '语法分析';
  if (/implicit conversion|suspicious assignment|undeclared|redeclared|type/i.test(m)) return '语义分析';
  return '语法分析';
}

function parseDiagnostics(stderr, exitCode) {
  const items = [];
  if (!stderr) return items;
  for (const line of stderr.split(/\r?\n/)) {
    const t = line.trim();
    if (!t) continue;
    const m = t.match(DIAG_RE);
    if (m) {
      items.push({
        severity: m[1],
        line: +m[2],
        col: +m[3],
        stage: inferStage(m[4]),
        msg: m[4].replace(/\s*\(in\s+[^)]*\)\s*$/, ''),
      });
      continue;
    }
    if (/error\(s\) found|compilation failed|compilation finished/i.test(t)) continue;
  }
  return items;
}

function renderDiagnostics(result) {
  const items = state.diagnostics;
  const errors = items.filter((d) => d.severity === 'error');
  const warns = items.filter((d) => d.severity === 'warning');

  el.cDiag.textContent = String(items.length);

  if (!state.compiled) {
    el.diagHead.className = 'diag-head';
    el.diagHead.textContent = '尚未编译';
    el.diagList.innerHTML = `<div class="diag-item info"><span class="sev">提示</span><span class="msg">在左侧写入代码（或用工具栏「示例▾」载入示例），然后点击「编译」或按 Ctrl+Enter。</span></div>`;
    el.diagFoot.textContent = '';
    return;
  }

  if (result && result.exitCode === 0) {
    el.diagHead.className = 'diag-head ok';
    el.diagHead.textContent = `编译成功 · 退出码 0${warns.length ? `（${warns.length} 个警告）` : ''}`;
  } else {
    el.diagHead.className = 'diag-head';
    el.diagHead.textContent = `编译失败 · 退出码 ${result ? result.exitCode : '?'}（${errors.length} 个错误${warns.length ? ` / ${warns.length} 个警告` : ''}）`;
  }

  const html = items
    .map((d) => {
      const sevText = d.severity === 'error' ? '错误' : '警告';
      const cls = d.severity === 'error' ? 'err' : 'warn';
      return (
        `<div class="diag-item ${cls} clickable" data-line="${d.line}" data-col="${d.col}">` +
        `<span class="sev">${sevText}</span>` +
        `<span class="loc">行 ${d.line}${d.col ? ` · 列 ${d.col}` : ''}</span>` +
        `<span class="stage">${escapeHtml(d.stage)}</span>` +
        `<span class="msg">${escapeHtml(d.msg)}</span></div>`
      );
    })
    .join('');

  const passed = [];
  const tokenCount = parseTokens(state.artifacts.tokens).length;
  if (state.artifacts.tokens) passed.push(`词法 ${tokenCount} 记号`);
  if (state.artifacts.parse) passed.push('LL(1) 语法分析');
  if (state.artifacts.ast) passed.push('AST 构建');

  el.diagList.innerHTML =
    html +
    (passed.length
      ? `<div class="diag-item info"><span class="sev">已通过</span><span class="msg">${escapeHtml(passed.join(' · '))}</span></div>`
      : '');

  el.diagFoot.textContent = errors.length
    ? '提示：修复错误后重新点击「编译」；点击错误行可跳转到源码。'
    : '提示：点击任意诊断行可跳转到对应源码位置。';
}

function jumpToLine(line, col) {
  const lines = el.source.value.split('\n');
  let pos = 0;
  for (let i = 0; i < line - 1 && i < lines.length; i++) pos += lines[i].length + 1;
  pos += Math.max(0, (col || 1) - 1);
  el.source.focus();
  el.source.setSelectionRange(pos, pos);
  // 滚动到可见（考虑内边距）
  const padTop = parseFloat(getComputedStyle(el.source).paddingTop) || 0;
  const top = padTop + (line - 2) * state.lineHeight;
  el.source.scrollTop = Math.max(0, top);
  syncScroll();
  updateCursor();
}

/* ------------------------- 标签切换 ------------------------- */
function selectTab(name) {
  state.activeTab = name;
  document.querySelectorAll('.tab').forEach((t) => t.classList.toggle('active', t.dataset.tab === name));
  document.querySelectorAll('.pane').forEach((p) => p.classList.toggle('active', p.id === 'pane-' + name));
}

/* ------------------------- 编译 ------------------------- */
function setBusy(busy, text) {
  state.busy = busy;
  el.topDot.className = 'dot ' + (busy ? 'busy' : 'ok');
  if (text) el.topStatus.textContent = text;
}

async function doCompile() {
  if (state.busy) return;
  if (!el.source.value.trim()) {
    state.compiled = false;
    selectTab('diag');
    el.diagHead.className = 'diag-head';
    el.diagHead.textContent = '源码为空';
    el.diagList.innerHTML =
      '<div class="diag-item info"><span class="sev">提示</span><span class="msg">编辑器里还没有代码 — 直接输入，或用工具栏「示例▾」载入一个示例。</span></div>';
    el.diagFoot.textContent = '';
    return;
  }
  setBusy(true, '编译中…');
  try {
    const t0 = performance.now();
    let res;
    try {
      res = await window.minicAPI.compile({ text: el.source.value, name: state.fileName });
    } catch (e) {
      res = { ok: false, reason: 'exception', message: String(e) };
    }
    state.compiled = true;

  if (res && res.artifacts) {
    state.artifacts = {
      tokens: res.artifacts['.token.txt'],
      parse: res.artifacts['.parse.txt'],
      ast: res.artifacts['.ast.txt'],
      sym: res.artifacts['.sym.txt'],
      quad: res.artifacts['.quad.txt'],
      asm: res.artifacts['.asm'],
    };
  }
  state.lastResult = res || null;
  // 源码变了，上一次的运行输出就过期了：留着会让用户以为那是新代码的结果
  state.runResult = null;
  const _doc = activeDoc();
  if (_doc) _doc.runResult = null;
  renderRunResult(null);
  state.diagnostics = parseDiagnostics(res && res.stderr, res && res.exitCode);

  const errs = state.diagnostics.filter((d) => d.severity === 'error');
  const warns = state.diagnostics.filter((d) => d.severity === 'warning');
  state.errorLines = [...new Set(errs.map((d) => d.line).filter((l) => l > 0))];
  state.warnLines = [...new Set(warns.map((d) => d.line).filter((l) => l > 0))];

  renderArtifactsView();

  const elapsed = res && res.durationMs != null ? res.durationMs : Math.round(performance.now() - t0);

  if (res && res.reason === 'minic-not-found') {
    setBusy(false, '未找到 minic.exe');
    el.topDot.className = 'dot err';
    selectTab('diag');
    el.diagHead.className = 'diag-head';
    el.diagHead.textContent = '无法编译：未找到 minic.exe';
    el.diagList.innerHTML = `<div class="diag-item err"><span class="sev">错误</span><span class="msg">${escapeHtml(res.message || '')}</span></div>`;
    el.diagFoot.textContent = '请先运行 build.bat 生成 MiniCCompiler/build/minic.exe。';
    return;
  }

  if (res && res.exitCode === 0) {
    el.topDot.className = 'dot ok';
    el.topStatus.textContent = warns.length ? `编译完成 · ${warns.length} 个警告` : '就绪';
    if (state.activeTab === 'diag') selectTab('token');
  } else {
    el.topDot.className = 'dot err';
    el.topStatus.textContent = `编译失败 · ${errs.length} 个错误${warns.length ? ` ${warns.length} 个警告` : ''}`;
    selectTab('diag');
  }
  statusNote(`编译耗时 ${elapsed} ms`);
  } finally {
    // 无论成功、失败还是异常，都必须复位忙碌标志，否则下一次点击会被直接吞掉
    state.busy = false;
  }
}

let noteTimer = null;
function statusNote(text) {
  el.statusExe.textContent = text;
  clearTimeout(noteTimer);
  noteTimer = setTimeout(() => {
    el.statusExe.textContent = 'minic v1.0 · C++17';
  }, 4000);
}

/* ------------------------- 运行 ------------------------- */
/** Windows 崩溃退出码是 0xC0000000 段的负数，转成常见的 0xC0000094 形式便于排查 */
function formatExitCode(code) {
  if (code == null) return '—';
  if (code < 0) {
    const hex = (code >>> 0).toString(16).toUpperCase().padStart(8, '0');
    const known = {
      C0000094: '整数除零',
      C0000095: '整数溢出',
      C0000005: '访问冲突（野指针/越界）',
      C00000FD: '栈溢出（递归过深）',
      C0000409: '栈缓冲区溢出（安全检查失败）',
    };
    return `0x${hex}${known[hex] ? '（' + known[hex] + '）' : ''}`;
  }
  return String(code);
}

/**
 * 编译 + 汇编链接 + 执行，把 print 的输出显示在「运行输出」页签。
 * 注意：minic.exe 本身只做编译，print 是运行期语句，必须真的把程序跑起来才有输出。
 */
async function doRun() {
  if (state.busy) return;
  if (!el.source.value.trim()) {
    selectTab('run');
    renderRunResult({ ok: false, stage: 'compile', reason: 'empty', message: '源码为空，先写点代码再运行。' });
    return;
  }
  setBusy(true, '运行中…');
  el.topDot.className = 'dot busy';
  try {
    let res;
    try {
      res = await window.minicAPI.run({ text: el.source.value, name: state.fileName });
    } catch (e) {
      res = { ok: false, reason: 'exception', message: String(e) };
    }
    state.runResult = res || null;
    const doc = activeDoc();
    if (doc) doc.runResult = state.runResult;

    renderRunResult(res);
    selectTab('run');

    if (res && res.ok) {
      const lines = String(res.stdout || '').replace(/\s+$/, '');
      const n = lines ? lines.split(/\r?\n/).length : 0;
      el.topDot.className = 'dot ok';
      el.topStatus.textContent = n ? `运行完成 · ${n} 行输出` : '运行完成 · 无输出';
    } else if (res && res.crashed) {
      el.topDot.className = 'dot err';
      el.topStatus.textContent = '运行崩溃 · 退出码 ' + formatExitCode(res.exitCode);
    } else {
      el.topDot.className = 'dot err';
      const stageText = { compile: '编译失败', link: '链接失败', gcc: '找不到 gcc', run: '运行失败' }[res && res.stage] || '运行失败';
      el.topStatus.textContent = res && res.timedOut ? '运行超时（已强制结束）' : stageText;
    }
  } finally {
    state.busy = false;
  }
}

/** 把运行结果画到「运行输出」页签 */
function renderRunResult(res) {
  if (!el.rawRun) return;
  if (!res) {
    el.rawRun.innerHTML = '';
    if (el.runEmpty) el.runEmpty.classList.add('show');
    if (el.runFoot) el.runFoot.textContent = '';
    if (el.cRun) el.cRun.textContent = '';
    return;
  }
  const parts = [];
  const addStdout = (text) => {
    if (!text) return;
    String(text).replace(/\s+$/, '').split(/\r?\n/).forEach((line) => {
      parts.push(escapeHtml(line));
    });
  };

  // 编译阶段就失败：没有可运行的程序
  if (res.stage === 'compile') {
    if (res.reason === 'minic-not-found' || res.reason === 'spawn-failed') {
      parts.push(`<span class="run-err">${escapeHtml(res.message || '未找到 minic.exe')}</span>`);
      parts.push('<span class="run-meta">提示：先运行 build.bat 生成 build\\minic.exe。</span>');
    } else if (res.reason === 'empty') {
      parts.push(`<span class="run-meta">${escapeHtml(res.message || '')}</span>`);
    } else {
      parts.push('<span class="run-meta">编译阶段就失败了，没有生成可运行的程序：</span>');
      const errText = String(res.stderr || res.stdout || res.message || '').replace(/\s+$/, '');
      errText.split(/\r?\n/).forEach((line) => {
        if (line.trim()) parts.push(`<span class="run-err">${escapeHtml(line)}</span>`);
      });
    }
    el.rawRun.innerHTML = parts.join('\n');
    if (el.runEmpty) el.runEmpty.classList.remove('show');
    if (el.runFoot) el.runFoot.textContent = '请到「诊断」页签查看完整错误列表。';
    if (el.cRun) el.cRun.textContent = '';
    return;
  }

  // gcc 缺失或链接失败
  if (res.stage === 'gcc' || res.stage === 'link') {
    parts.push(`<span class="run-err">${escapeHtml(res.message || '汇编链接失败')}</span>`);
    if (res.stage === 'gcc') {
      parts.push('<span class="run-meta">提示：设置环境变量 MINIC_GCC 指向 gcc.exe 的完整路径，或把 MinGW 的 bin 目录加入 PATH。</span>');
    }
    el.rawRun.innerHTML = parts.join('\n');
    if (el.runEmpty) el.runEmpty.classList.remove('show');
    if (el.runFoot) el.runFoot.textContent = '汇编产物已生成，可在「汇编」页签查看。';
    if (el.cRun) el.cRun.textContent = '';
    return;
  }

  // 程序启动失败（exe 被杀毒软件拦截、权限不足等）
  if (res.reason === 'spawn-failed') {
    parts.push(`<span class="run-err">${escapeHtml(res.message || '无法启动生成的可执行文件')}</span>`);
    parts.push('<span class="run-meta">常见原因：杀毒软件拦截了 %TEMP% 下新生成的 exe，或该目录被策略禁止执行。</span>');
    el.rawRun.innerHTML = parts.join('\n');
    if (el.runEmpty) el.runEmpty.classList.remove('show');
    if (el.runFoot) el.runFoot.textContent = '编译与汇编链接本身是成功的。';
    if (el.cRun) el.cRun.textContent = '';
    return;
  }

  // 正常运行结束
  const out = String(res.stdout || '').replace(/\s+$/, '');
  const err = String(res.stderr || '').replace(/\s+$/, '');
  if (out) {
    addStdout(out);
  } else if (!err && !res.crashed && !res.timedOut) {
    parts.push('<span class="run-meta">程序运行结束，但没有任何输出（源码里没有 print，或 print 的分支没被执行）。</span>');
  }
  if (err) {
    if (out) parts.push('<span class="run-sep">—— stderr ——</span>');
    err.split(/\r?\n/).forEach((line) => parts.push(`<span class="run-err">${escapeHtml(line)}</span>`));
  }
  if (res.timedOut) {
    parts.push('<span class="run-sep">—— 超时 ——</span>');
    parts.push('<span class="run-err">程序运行超过 5 秒被强制结束，可能存在死循环。</span>');
  } else if (res.crashed) {
    parts.push('<span class="run-sep">—— 崩溃 ——</span>');
    parts.push(`<span class="run-err">程序异常终止，退出码 ${escapeHtml(formatExitCode(res.exitCode))}。</span>`);
    if (!err) {
      parts.push('<span class="run-meta">常见原因：整数除零、数组下标越界、递归过深导致栈溢出。</span>');
    }
  }
  el.rawRun.innerHTML = parts.join('\n') || '<span class="run-meta">（无输出）</span>';
  if (el.runEmpty) el.runEmpty.classList.remove('show');

  const outLines = out ? out.split(/\r?\n/).length : 0;
  if (el.cRun) el.cRun.textContent = outLines ? String(outLines) : '';
  if (el.runFoot) {
    const bits = [];
    if (res.compileMs != null) bits.push(`编译 ${res.compileMs} ms`);
    if (res.linkMs != null) bits.push(`汇编链接 ${res.linkMs} ms`);
    if (res.runMs != null) bits.push(`运行 ${res.runMs} ms`);
    bits.push(`退出码 ${formatExitCode(res.exitCode)}`);
    el.runFoot.textContent = bits.join(' · ');
  }
}

function clearRunOutput() {
  state.runResult = null;
  const doc = activeDoc();
  if (doc) doc.runResult = null;
  renderRunResult(null);
}

/* ------------------------- 文件操作 ------------------------- */
function maybeAutoFormat() {
  if (!state.autoFormatOnSave) return;
  if (!el.source.value.trim()) return;
  formatDocument({ silent: true });
}

function toggleAutoFormat() {
  state.autoFormatOnSave = !state.autoFormatOnSave;
  try { localStorage.setItem('minic.autoFormat', state.autoFormatOnSave ? '1' : '0'); } catch { /* ignore */ }
  statusNote(state.autoFormatOnSave ? '已开启：保存时自动格式化' : '已关闭：保存时自动格式化');
}

async function openFile() {
  const r = await window.minicAPI.openFile();
  if (!r || r.canceled) return;
  reuseOrCreateDoc({ name: r.name, content: r.content, filePath: r.filePath });
  selectTab('token');
}

async function saveFile() {
  if (state.filePath) {
    maybeAutoFormat();
    const r = await window.minicAPI.saveFile(state.filePath, el.source.value);
    if (r && r.ok) {
      const doc = activeDoc();
      if (doc) { doc.content = el.source.value; doc.dirty = false; }
      markDirty(false);
      statusNote('已保存 ' + state.filePath);
    } else {
      statusNote('保存失败：' + (r && r.message));
    }
  } else {
    await saveAs();
  }
}

async function saveAs() {
  maybeAutoFormat();
  const r = await window.minicAPI.saveAs(el.source.value, state.fileName);
  if (!r || r.canceled) return;
  if (r.ok) {
    setFile(r.name, r.filePath);
    const doc = activeDoc();
    if (doc) { doc.content = el.source.value; doc.dirty = false; }
    renderFileTabs();
    statusNote('已另存为 ' + r.filePath);
  } else {
    statusNote('保存失败：' + (r.message || ''));
  }
}

/* ------------------------- 菜单 ------------------------- */
const MENUS = {
  file: [
    { label: '新建文件', shortcut: 'Ctrl+N', action: newFile },
    { label: '打开…', shortcut: 'Ctrl+O', action: openFile },
    { label: '保存', shortcut: 'Ctrl+S', action: saveFile },
    { label: '另存为…', shortcut: 'Ctrl+Shift+S', action: saveAs },
    { sep: true },
    { label: '退出', shortcut: 'Alt+F4', action: () => window.minicAPI.close() },
  ],
  edit: [
    { label: '撤销', shortcut: 'Ctrl+Z', action: () => document.execCommand('undo') },
    { label: '重做', shortcut: 'Ctrl+Y', action: () => document.execCommand('redo') },
    { sep: true },
    { label: '剪切', shortcut: 'Ctrl+X', action: () => document.execCommand('cut') },
    { label: '复制', shortcut: 'Ctrl+C', action: () => document.execCommand('copy') },
    { label: '粘贴', shortcut: 'Ctrl+V', action: () => document.execCommand('paste') },
    { label: '全选', shortcut: 'Ctrl+A', action: () => { el.source.focus(); el.source.select(); } },
    { sep: true },
    { label: '格式化文档', shortcut: 'Shift+Alt+F', action: () => formatDocument() },
  ],
  build: [
    { label: '编译', shortcut: 'Ctrl+Enter', action: doCompile },
    { label: '运行', shortcut: 'F5', action: doRun },
    { label: '清空产物', action: clearOutputs },
  ],
  view: [
    { label: '切换主题', shortcut: 'Ctrl+Shift+L', action: toggleTheme },
    { label: '放大字号', shortcut: 'Ctrl+=', action: () => applyFontSize(state.fontSize + 1) },
    { label: '缩小字号', shortcut: 'Ctrl+-', action: () => applyFontSize(state.fontSize - 1) },
    { label: '重置字号', action: () => applyFontSize(13) },
    { sep: true },
    { label: () => (state.autoFormatOnSave ? '✓ 保存时自动格式化' : '　保存时自动格式化'), action: toggleAutoFormat },
  ],
  help: [
    { label: '语法手册', shortcut: 'F1', action: () => openBook() },
    { sep: true },
    { label: '关于 MiniC 编译器 GUI', action: showAbout },
  ],
};

function openMenu(anchor, items) {
  const labelOf = (it) => (typeof it.label === 'function' ? it.label() : it.label);
  el.menuPop.innerHTML = items
    .map((it, idx) =>
      it.sep
        ? '<div class="menu-sep"></div>'
        : `<div class="menu-row" data-idx="${idx}">${escapeHtml(labelOf(it))}${
            it.shortcut ? `<span class="menu-shortcut">${escapeHtml(it.shortcut)}</span>` : ''
          }</div>`,
    )
    .join('');
  el.menuPop.classList.remove('hidden');
  const rect = anchor.getBoundingClientRect();
  el.menuPop.style.top = rect.bottom + 2 + 'px';
  el.menuPop.style.left = Math.min(rect.left, window.innerWidth - 260) + 'px';
  // 菜单比剩余空间高时向上翻，避免超出窗口底部
  const popH = el.menuPop.offsetHeight;
  if (popH > 0 && rect.bottom + 2 + popH > window.innerHeight - 8) {
    const up = rect.top - 2 - popH;
    el.menuPop.style.top = Math.max(8, up) + 'px';
  }
  el.menuPop.querySelectorAll('.menu-row').forEach((row) => {
    row.onclick = () => {
      closeMenu();
      const item = items[+row.dataset.idx];
      if (item && item.action) item.action();
    };
  });
  document.querySelectorAll('.menu-item').forEach((m) => m.classList.remove('open'));
  if (anchor.classList.contains('menu-item')) anchor.classList.add('open');
}

function closeMenu() {
  el.menuPop.classList.add('hidden');
  document.querySelectorAll('.menu-item').forEach((m) => m.classList.remove('open'));
}

function showAbout() {
  el.diagHead.className = 'diag-head ok';
  el.diagHead.textContent = '关于 MiniC 编译器 GUI';
  el.diagList.innerHTML =
    `<div class="diag-item info"><span class="sev">版本</span><span class="msg">GUI v1.0 · Electron 前端 + minic C++17 后端</span></div>` +
    `<div class="diag-item info"><span class="sev">产物</span><span class="msg">Token / 语法分析 / 语法树 / 符号表 / 四元式 / 汇编 / 诊断</span></div>` +
    `<div class="diag-item info"><span class="sev">说明</span><span class="msg">界面依据设计规范板 v1.0 实现，暗/亮双主题。</span></div>`;
  el.diagFoot.textContent = 'minic.exe 由 build.bat 构建；GUI 通过临时目录调用，规避中文路径限制。';
  selectTab('diag');
}

function clearOutputs() {
  state.artifacts = emptyArtifacts();
  state.diagnostics = [];
  state.errorLines = [];
  state.warnLines = [];
  state.compiled = false;
  state.lastResult = null;
  state.runResult = null;
  const doc = activeDoc();
  if (doc) {
    doc.artifacts = state.artifacts;
    doc.diagnostics = [];
    doc.errorLines = [];
    doc.warnLines = [];
    doc.compiled = false;
    doc.lastResult = null;
    doc.runResult = null;
  }
  renderArtifactsView();
  renderRunResult(null);
  el.topDot.className = 'dot ok';
  el.topStatus.textContent = '就绪';
}

/* ------------------------- 事件绑定 ------------------------- */
function bindEvents() {
  // 窗口控制
  $('btn-min').onclick = () => window.minicAPI.minimize();
  $('btn-max').onclick = () => window.minicAPI.maximize();
  $('btn-close').onclick = () => window.minicAPI.close();

  // 工具栏
  $('btn-new-file').onclick = newFile;
  $('btn-tab-new').onclick = newFile;
  $('btn-open').onclick = openFile;
  $('btn-save').onclick = saveFile;
  $('btn-compile').onclick = doCompile;
  $('btn-run').onclick = doRun;
  $('btn-clear').onclick = clearOutputs;
  $('btn-theme').onclick = toggleTheme;
  $('btn-font').onclick = () => applyFontSize(state.fontSize >= 18 ? 12 : state.fontSize + 1);

  // 语法手册
  bindBookEvents();

  // 示例下拉：按分组渲染，分组之间自动插分隔线
  $('btn-sample').onclick = (e) => {
    e.stopPropagation();
    const items = [];
    let lastGroup = null;
    for (const s of SAMPLE_LIST) {
      if (s.group !== lastGroup) {
        if (lastGroup !== null) items.push({ sep: true });
        lastGroup = s.group;
      }
      items.push({
        label: s.label,
        shortcut: s.name,
        action: () => loadSample(s.text, s.name),
      });
    }
    if (items.length === 0) items.push({ label: '（示例库未加载）', action: () => {} });
    openMenu($('btn-sample'), items);
  };

  // 文件标签栏：点击切换、点 × 关闭、中键关闭
  $('file-tab-list').addEventListener('click', (e) => {
    const closeBtn = e.target.closest('.tab-close');
    if (closeBtn) {
      e.stopPropagation();
      closeDoc(closeBtn.dataset.close);
      return;
    }
    const tab = e.target.closest('.file-tab');
    if (tab && tab.dataset.id !== state.activeId) activateDoc(tab.dataset.id);
  });
  $('file-tab-list').addEventListener('mousedown', (e) => {
    if (e.button === 1) {
      const tab = e.target.closest('.file-tab');
      if (tab) {
        e.preventDefault();
        closeDoc(tab.dataset.id);
      }
    }
  });

  // 菜单
  document.querySelectorAll('.menu-item').forEach((btn) => {
    btn.onclick = (e) => {
      e.stopPropagation();
      if (!el.menuPop.classList.contains('hidden') && btn.classList.contains('open')) {
        closeMenu();
        return;
      }
      openMenu(btn, MENUS[btn.dataset.menu] || []);
    };
  });
  document.addEventListener('click', (e) => {
    if (!el.menuPop.contains(e.target)) closeMenu();
  });

  // 标签页
  document.querySelectorAll('.tab').forEach((tab) => {
    tab.onclick = () => selectTab(tab.dataset.tab);
  });

  // 诊断跳转
  el.diagList.addEventListener('click', (e) => {
    const item = e.target.closest('.diag-item.clickable');
    if (item) jumpToLine(+item.dataset.line, +item.dataset.col);
  });

  // 语法树节点跳转（点节点 → 跳到源码对应位置）
  el.rawAst.addEventListener('click', (e) => {
    const node = e.target.closest('.ast-line.clickable');
    if (node) jumpToLine(+node.dataset.line, +node.dataset.col);
  });

  // 编辑器
  el.source.addEventListener('input', () => {
    markDirty(true);
    renderEditor();
    // 编译过之后又改动：提示需要重新编译
    if (state.compiled) {
      el.topDot.className = 'dot idle';
      el.topStatus.textContent = '已修改 · 待编译';
    }
  });
  el.source.addEventListener('scroll', syncScroll);
  el.source.addEventListener('click', updateCursor);
  el.source.addEventListener('keyup', updateCursor);
  el.source.addEventListener('select', updateCursor);
  el.source.addEventListener('keydown', (e) => {
    const val = el.source.value;
    const s = el.source.selectionStart;
    const en = el.source.selectionEnd;

    // Shift+Alt+F 格式化文档
    if (e.altKey && e.shiftKey && (e.key === 'F' || e.key === 'f')) {
      e.preventDefault();
      formatDocument();
      return;
    }

    // Tab 缩进
    if (e.key === 'Tab') {
      e.preventDefault();
      el.source.value = val.slice(0, s) + '    ' + val.slice(en);
      el.source.selectionStart = el.source.selectionEnd = s + 4;
      markDirty(true);
      renderEditor();
      updateCursor();
      return;
    }

    if (e.ctrlKey || e.metaKey || e.altKey) return;

    const PAIRS = { '(': ')', '[': ']', '{': '}', '"': '"', "'": "'" };

    // 成对退格：光标在两个相邻的括号/引号之间时一次删掉一对
    if (e.key === 'Backspace' && s === en && s > 0) {
      const closer = PAIRS[val[s - 1]];
      if (closer && val[s] === closer) {
        e.preventDefault();
        el.source.setSelectionRange(s - 1, s + 1);
        document.execCommand('delete');
        markDirty(true);
        renderEditor();
        updateCursor();
        return;
      }
    }

    // 左括号 / 引号自动配对
    if (PAIRS[e.key]) {
      e.preventDefault();
      const sel = val.slice(s, en);
      if (sel) {
        // 有选中内容 → 包裹
        document.execCommand('insertText', false, e.key + sel + PAIRS[e.key]);
        el.source.setSelectionRange(s + 1, s + 1 + sel.length);
      } else if ((e.key === '"' || e.key === "'") && val[s] === e.key) {
        // 右侧已是同一个引号 → 跳过，不重复插入
        el.source.setSelectionRange(s + 1, s + 1);
      } else {
        document.execCommand('insertText', false, e.key + PAIRS[e.key]);
        el.source.setSelectionRange(s + 1, s + 1);
      }
      markDirty(true);
      renderEditor();
      updateCursor();
      return;
    }

    // 右括号：右侧就是它 → 光标跳过
    if ((e.key === ')' || e.key === ']' || e.key === '}') && s === en && val[s] === e.key) {
      e.preventDefault();
      el.source.setSelectionRange(s + 1, s + 1);
      updateCursor();
      return;
    }

    // 行首输入 } 自动退一级
    if (e.key === '}') {
      const lineStart = val.lastIndexOf('\n', s - 1) + 1;
      const before = val.slice(lineStart, s);
      if (/^[ ]*$/.test(before) && before.length >= 4) {
        e.preventDefault();
        el.source.setSelectionRange(lineStart, s);
        document.execCommand('insertText', false, before.slice(0, before.length - 4) + '}');
        markDirty(true);
        renderEditor();
        updateCursor();
        return;
      }
    }

    // 回车：先排版当前行，再保持缩进换行；在 { } 之间自动展开并多缩进一级
    if (e.key === 'Enter') {
      e.preventDefault();
      let start = s;
      let end = en;
      const lineStart = val.lastIndexOf('\n', s - 1) + 1;
      const nl = val.indexOf('\n', s);
      const lineEnd = nl === -1 ? val.length : nl;

      // 光标在行尾时：把这一行先规范化（a=10; → a = 10;）
      if (!val.slice(s, lineEnd).trim()) {
        const lineText = val.slice(lineStart, lineEnd);
        const formatted = normalizeLine(lineText);
        if (formatted !== lineText) {
          el.source.setSelectionRange(lineStart, lineEnd);
          document.execCommand('insertText', false, formatted);
          start = lineStart + formatted.length;
          end = start;
        }
      }

      const cur = el.source.value;
      const ls = cur.lastIndexOf('\n', start - 1) + 1;
      const curLine = cur.slice(ls, start);
      const indent = (curLine.match(/^[ \t]*/) || [''])[0];
      const prevChar = curLine.replace(/[ \t]+$/, '').slice(-1);
      const after = cur.slice(end);
      const lead = (after.match(/^[ \t]*/) || [''])[0].length;
      const nextChar = after.slice(lead, lead + 1);

      if (prevChar === '{' && nextChar === '}') {
        const inner = indent + '    ';
        el.source.setSelectionRange(start, end + lead); // 吃掉 } 前的空格
        document.execCommand('insertText', false, `\n${inner}\n${indent}`);
        const caret = start + 1 + inner.length;
        el.source.setSelectionRange(caret, caret);
      } else {
        const extra = prevChar === '{' ? '    ' : '';
        document.execCommand('insertText', false, `\n${indent}${extra}`);
      }
      markDirty(true);
      renderEditor();
      updateCursor();
    }
  });

  // Ctrl + 滚轮缩放编辑器字号
  const wheelZoom = (e) => {
    if (!e.ctrlKey) return;
    e.preventDefault();
    applyFontSize(state.fontSize + (e.deltaY < 0 ? 1 : -1));
  };
  $('code-wrap').addEventListener('wheel', wheelZoom, { passive: false });
  el.gutter.addEventListener('wheel', wheelZoom, { passive: false });

  // 快捷键
  window.addEventListener('keydown', (e) => {
    const ctrl = e.ctrlKey || e.metaKey;
    if (ctrl && e.key.toLowerCase() === 'n') { e.preventDefault(); newFile(); }
    else if (ctrl && e.key.toLowerCase() === 'o') { e.preventDefault(); openFile(); }
    else if (ctrl && e.shiftKey && e.key.toLowerCase() === 's') { e.preventDefault(); saveAs(); }
    else if (ctrl && e.key.toLowerCase() === 's') { e.preventDefault(); saveFile(); }
    else if (ctrl && e.key === 'Enter') { e.preventDefault(); doCompile(); }
    else if (e.key === 'F5') { e.preventDefault(); doRun(); }
    else if (e.key === 'F1') { e.preventDefault(); toggleBook(); }
    else if (ctrl && e.key.toLowerCase() === 'f' && bookState.open) { e.preventDefault(); el.bookSearch.focus(); el.bookSearch.select(); }
    else if (ctrl && e.shiftKey && e.key.toLowerCase() === 'l') { e.preventDefault(); toggleTheme(); }
    else if (ctrl && (e.key === '=' || e.key === '+')) { e.preventDefault(); applyFontSize(state.fontSize + 1); }
    else if (ctrl && e.key === '-') { e.preventDefault(); applyFontSize(state.fontSize - 1); }
    else if (e.key === 'Escape') {
      closeMenu();
      if (bookState.open) closeBook();
    }
  });

  // 窗口尺寸变化
  window.addEventListener('resize', () => {
    renderMarks();
    syncScroll();
  });
}

/* ============================================================
   语法手册（语言参考书）
   ------------------------------------------------------------
   数据来自 syntax-book.js 的 SYNTAX_BOOK；本段只负责渲染与交互：
   目录、搜索、代码块高亮、插入编辑器、载入为新文件。
   ============================================================ */

const BOOK = (typeof SYNTAX_BOOK !== 'undefined' && Array.isArray(SYNTAX_BOOK)) ? SYNTAX_BOOK : [];

/* 把「章 → 条目」摊平，便于搜索与跳转 */
const BOOK_INDEX = (() => {
  const list = [];
  for (const ch of BOOK) {
    for (const entry of (ch.entries || [])) {
      list.push({ chId: ch.id, chTitle: ch.title, entry });
    }
  }
  return list;
})();

const BOOK_BADGE = {
  ok: '✓ 支持',
  no: '✗ 不支持',
  limit: '⚠ 有限制',
  info: '· 说明',
};

const bookState = { open: false, query: '', current: null };

/* 说明文字里的 `反引号` 转成行内代码 */
function bookInlineCode(text) {
  return escapeHtml(String(text)).replace(/`([^`]+)`/g, '<code>$1</code>');
}

function bookEntryHtml(item) {
  const e = item.entry;
  const parts = [];

  parts.push(`<section class="book-entry" id="entry-${e.id}" data-entry="${e.id}">`);
  parts.push('<div class="book-entry-head">');
  parts.push(`<h3 class="book-entry-title">${escapeHtml(e.title)}</h3>`);
  if (e.status && BOOK_BADGE[e.status]) {
    parts.push(`<span class="book-badge ${e.status}">${BOOK_BADGE[e.status]}</span>`);
  }
  parts.push('</div>');

  if (e.syntax) {
    parts.push(`<pre class="book-syntax">${escapeHtml(e.syntax)}</pre>`);
  }

  if (e.example) {
    parts.push('<div class="book-code">');
    parts.push('<div class="book-code-bar">');
    parts.push('<span class="book-code-label">示例</span><span class="spacer"></span>');
    parts.push(`<button class="book-code-btn" data-book-act="insert" data-entry="${e.id}">插入到编辑器</button>`);
    parts.push(`<button class="book-code-btn" data-book-act="load" data-entry="${e.id}">载入为新文件</button>`);
    parts.push(`<button class="book-code-btn" data-book-act="copy" data-entry="${e.id}">复制</button>`);
    parts.push('</div>');
    parts.push(`<pre>${highlightMiniC(e.example)}</pre>`);
    parts.push('</div>');
  }

  if (e.err) {
    parts.push(`<pre class="book-err">${escapeHtml(e.err)}</pre>`);
  }

  if (e.output) {
    parts.push('<pre class="book-out"><span class="book-out-label">运行输出</span>');
    parts.push(escapeHtml(e.output));
    parts.push('</pre>');
  }

  if (e.notes && e.notes.length) {
    parts.push('<ul class="book-notes">');
    for (const n of e.notes) parts.push(`<li>${bookInlineCode(n)}</li>`);
    parts.push('</ul>');
  }

  if (e.related && e.related.length) {
    const links = e.related
      .map((rid) => {
        const target = BOOK_INDEX.find((x) => x.entry.id === rid);
        if (!target) return '';
        return `<button class="book-rel-link" data-book-act="goto" data-target="${rid}">${escapeHtml(target.entry.title)}</button>`;
      })
      .join('');
    if (links) parts.push(`<div class="book-related"><span>相关：</span>${links}</div>`);
  }

  parts.push('</section>');
  return parts.join('');
}

function bookMatch(item, q) {
  if (!q) return true;
  const e = item.entry;
  const hay = [
    e.title,
    e.syntax || '',
    e.example || '',
    e.err || '',
    (e.notes || []).join(' '),
    item.chTitle,
  ].join('\n').toLowerCase();
  // 空格分词，全部命中才算匹配
  return q.toLowerCase().split(/\s+/).filter(Boolean).every((t) => hay.includes(t));
}

function renderBookToc(matched) {
  if (!matched.length) {
    el.bookToc.innerHTML = '<div class="book-toc-empty">没有匹配的条目。<br>换个关键词试试，比如 <code>for</code>、<code>数组</code>、<code>print</code>。</div>';
    return;
  }
  const byCh = new Map();
  for (const item of matched) {
    if (!byCh.has(item.chId)) byCh.set(item.chId, { title: item.chTitle, items: [] });
    byCh.get(item.chId).items.push(item);
  }
  const out = [];
  for (const [chId, group] of byCh) {
    out.push(`<button class="book-toc-ch" data-book-act="goto" data-target="ch-${chId}">${escapeHtml(group.title)}</button>`);
    for (const item of group.items) {
      out.push(`<button class="book-toc-item" data-book-act="goto" data-target="entry-${item.entry.id}">${escapeHtml(item.entry.title)}</button>`);
    }
  }
  el.bookToc.innerHTML = out.join('');
  syncBookTocActive();
}

function renderBookContent(matched) {
  if (!matched.length) {
    el.bookContent.innerHTML = '<div class="book-empty">没有匹配的条目。<br>试试 <code>for</code>、<code>取模</code>、<code>数组</code>、<code>print</code>、<code>报错</code>。</div>';
    return;
  }
  const byCh = new Map();
  for (const item of matched) {
    if (!byCh.has(item.chId)) byCh.set(item.chId, { title: item.chTitle, items: [] });
    byCh.get(item.chId).items.push(item);
  }
  const out = [];
  for (const [chId, group] of byCh) {
    out.push(`<div class="book-chapter" id="ch-${chId}">`);
    out.push(`<h2>${escapeHtml(group.title)}</h2>`);
    for (const item of group.items) out.push(bookEntryHtml(item));
    out.push('</div>');
  }
  el.bookContent.innerHTML = out.join('');
}

function renderBookFoot(count) {
  const total = BOOK_INDEX.length;
  const shown = count === total ? `共 ${total} 条` : `匹配 ${count} / ${total} 条`;
  el.bookFoot.innerHTML =
    `<span>${shown}</span>` +
    `<span>全部条目均在 <code>minic.exe</code> 上实测</span>` +
    '<span class="spacer"></span>' +
    '<span><kbd>F1</kbd> 开关手册　<kbd>Esc</kbd> 关闭　<kbd>Ctrl+F</kbd> 搜索</span>';
}

function renderBook() {
  if (!BOOK.length) {
    el.bookToc.innerHTML = '';
    el.bookContent.innerHTML = '<div class="book-empty">手册数据未加载（syntax-book.js 缺失）。</div>';
    el.bookFoot.innerHTML = '';
    return;
  }
  const q = bookState.query.trim();
  const matched = BOOK_INDEX.filter((item) => bookMatch(item, q));
  renderBookToc(matched);
  renderBookContent(matched);
  renderBookFoot(matched.length);
}

/* 高亮目录里当前可见的条目 */
function syncBookTocActive() {
  const nodes = el.bookContent.querySelectorAll('.book-entry');
  if (!nodes.length) return;
  const top = el.bookContent.scrollTop + 12;
  let activeId = nodes[0].id;
  for (const n of nodes) {
    if (n.offsetTop - el.bookContent.offsetTop <= top) activeId = n.id;
    else break;
  }
  for (const b of el.bookToc.querySelectorAll('.book-toc-item')) {
    b.classList.toggle('active', b.dataset.target === activeId);
  }
}

function bookScrollTo(targetId) {
  const target = document.getElementById(targetId);
  if (!target) return;
  target.scrollIntoView({ block: 'start', behavior: 'smooth' });
  if (targetId.startsWith('entry-')) {
    bookState.current = targetId.slice(6);
    for (const b of el.bookToc.querySelectorAll('.book-toc-item')) {
      b.classList.toggle('active', b.dataset.target === targetId);
    }
  }
}

function openBook(entryId) {
  if (!el.bookOverlay) return;
  bookState.open = true;
  el.bookOverlay.classList.remove('hidden');
  if (!el.bookToc.dataset.rendered) {
    renderBook();
    el.bookToc.dataset.rendered = '1';
  }
  if (entryId) {
    // 若被搜索过滤掉了，先清空搜索再跳
    if (!document.getElementById(`entry-${entryId}`)) {
      bookState.query = '';
      el.bookSearch.value = '';
      renderBook();
    }
    bookScrollTo(`entry-${entryId}`);
  } else if (el.bookContent) {
    el.bookContent.scrollTop = 0;
  }
  syncBookTocActive();
  setTimeout(() => el.bookSearch && el.bookSearch.focus(), 30);
}

function closeBook() {
  if (!el.bookOverlay) return;
  bookState.open = false;
  el.bookOverlay.classList.add('hidden');
}

function toggleBook() {
  if (bookState.open) closeBook();
  else openBook();
}

function bookFindEntry(id) {
  const item = BOOK_INDEX.find((x) => x.entry.id === id);
  return item ? item.entry : null;
}

/* 把示例插入当前编辑器光标处 */
function bookInsertExample(id) {
  const entry = bookFindEntry(id);
  if (!entry || !entry.example) return;
  const ta = el.source;
  const start = ta.selectionStart;
  const end = ta.selectionEnd;
  const cur = ta.value;
  const before = cur.slice(0, start);
  const after = cur.slice(end);
  const needNl = before.length && !before.endsWith('\n') ? '\n' : '';
  const tail = after.length && !after.startsWith('\n') ? '\n' : '';
  const text = before + needNl + entry.example + tail + after;
  ta.value = text;
  const caret = (before + needNl).length + entry.example.length;
  ta.selectionStart = ta.selectionEnd = caret;
  ta.focus();
  // 触发一次输入事件，让高亮/脏标记/行号刷新
  ta.dispatchEvent(new Event('input', { bubbles: true }));
  closeBook();
  statusNote(`已插入示例：${entry.title}`);
  el.topDot.className = 'dot ok';
  el.topStatus.textContent = '已插入示例';
}

/* 把示例载入为一个新文件（不动当前文件） */
function bookLoadExample(id) {
  const entry = bookFindEntry(id);
  if (!entry || !entry.example) return;
  const safeName = entry.id.replace(/[^A-Za-z0-9_-]/g, '_') + '.mc';
  reuseOrCreateDoc({ name: safeName, content: entry.example });
  closeBook();
  selectTab('token');
  statusNote(`已载入示例：${entry.title}`);
  el.topDot.className = 'dot ok';
  el.topStatus.textContent = '已载入示例';
}

async function bookCopyExample(id) {
  const entry = bookFindEntry(id);
  if (!entry || !entry.example) return;
  try {
    await navigator.clipboard.writeText(entry.example);
    statusNote('示例已复制到剪贴板');
  } catch {
    statusNote('复制失败：剪贴板不可用');
  }
}

function bindBookEvents() {
  if (!el.bookOverlay) return;

  el.btnBook && el.btnBook.addEventListener('click', () => toggleBook());
  el.bookClose && el.bookClose.addEventListener('click', closeBook);

  // 点遮罩空白处关闭
  el.bookOverlay.addEventListener('mousedown', (e) => {
    if (e.target === el.bookOverlay) closeBook();
  });

  // 正文区所有按钮走事件委托
  el.bookContent.addEventListener('click', (e) => {
    const btn = e.target.closest('[data-book-act]');
    if (!btn) return;
    const act = btn.dataset.bookAct;
    if (act === 'goto') bookScrollTo(btn.dataset.target);
    else if (act === 'insert') bookInsertExample(btn.dataset.entry);
    else if (act === 'load') bookLoadExample(btn.dataset.entry);
    else if (act === 'copy') bookCopyExample(btn.dataset.entry);
  });

  // 目录点击
  el.bookToc.addEventListener('click', (e) => {
    const btn = e.target.closest('[data-book-act]');
    if (btn && btn.dataset.bookAct === 'goto') bookScrollTo(btn.dataset.target);
  });

  // 搜索（防抖 120ms）
  let timer = null;
  el.bookSearch.addEventListener('input', () => {
    clearTimeout(timer);
    timer = setTimeout(() => {
      bookState.query = el.bookSearch.value;
      renderBook();
      el.bookContent.scrollTop = 0;
    }, 120);
  });
  el.bookSearch.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
      if (el.bookSearch.value) {
        el.bookSearch.value = '';
        bookState.query = '';
        renderBook();
      } else {
        closeBook();
      }
      e.stopPropagation();
    } else if (e.key === 'Enter') {
      const first = el.bookContent.querySelector('.book-entry');
      if (first) bookScrollTo(first.id);
    }
  });

  // 滚动时同步目录高亮（节流）
  let scrollTick = false;
  el.bookContent.addEventListener('scroll', () => {
    if (scrollTick) return;
    scrollTick = true;
    requestAnimationFrame(() => { scrollTick = false; syncBookTocActive(); });
  });
}

function loadSample(text, name) {
  reuseOrCreateDoc({ name, content: text });
  selectTab('token');
  el.topDot.className = 'dot ok';
  el.topStatus.textContent = '就绪';
}

/* ------------------------- 初始化 ------------------------- */
async function init() {
  // 主题 / 字号
  let savedTheme = 'dark';
  let savedFont = 13;
  try {
    savedTheme = localStorage.getItem('minic.theme') || 'dark';
    savedFont = +(localStorage.getItem('minic.fontSize') || 13);
    state.autoFormatOnSave = (localStorage.getItem('minic.autoFormat') || '1') === '1';
  } catch { /* ignore */ }
  applyTheme(savedTheme);
  applyFontSize(savedFont);

  bindEvents();
  // 启动时是空白新文件：不预载示例
  const first = newDoc({ name: '未命名.mc', content: '' });
  activateDoc(first.id);
  measureLineHeight();
  renderMarks();

  // 环境信息
  try {
    const info = await window.minicAPI.info();
    if (!info.exeFound) {
      el.topDot.className = 'dot err';
      el.topStatus.textContent = '未找到 minic.exe';
      statusNote('未找到 minic.exe，请先运行 build.bat');
    } else {
      el.statusExe.title = info.exePath;
    }
  } catch { /* ignore */ }
}

/* 截图/回归钩子：--compile / --break / --tab=xx / --menu=xx / --twice 模式使用 */
window.__minicTest = async ({ theme, break: breakIt, tab, menu, twice, multifile, fmt, editTest, dupTest, demo } = {}) => {
  if (theme) applyTheme(theme);
  if (demo) {
    // 效果图/演示：载入成功示例并编译，焦点停在指定产物标签
    loadSample(SAMPLE_GCD, 'gcd-demo.mc');
    await doCompile();
    if (tab) selectTab(tab);
    if (theme) applyTheme(theme);
    await new Promise((r) => setTimeout(r, 250));
    return { docs: state.docs.map((d) => d.name), activeTab: state.activeTab, tokens: parseTokens(state.artifacts.tokens).length };
  }
  if (dupTest) {
    // 回归：同一个示例连续打开 3 次 + 打开同一磁盘文件 2 次，都不应产生重复标签
    loadSample(SAMPLE_GCD, 'gcd-demo.mc');
    loadSample(SAMPLE_GCD, 'gcd-demo.mc');
    loadSample(SAMPLE_GCD, 'gcd-demo.mc');
    const afterSample = state.docs.length;
    const info = await window.minicAPI.info();
    // 换一个示例、再回来，仍应复用
    loadSample(SAMPLE_LEX, 'lex-demo.mc');
    loadSample(SAMPLE_GCD, 'gcd-demo.mc');
    await new Promise((r) => setTimeout(r, 150));
    return {
      afterSample,
      finalDocs: state.docs.map((d) => d.name),
      dupCount: state.docs.filter((d) => d.name === 'gcd-demo.mc').length,
      active: state.fileName,
      exeFound: info.exeFound,
    };
  }
  if (editTest) {
    const src = el.source;
    const press = (key) => src.dispatchEvent(new KeyboardEvent('keydown', { key, bubbles: true, cancelable: true }));
    const set = (v, caret) => { src.value = v; src.focus(); src.setSelectionRange(caret, caret); renderEditor(); };
    const r = {};

    // 1) 自动配对：输入 {
    set('', 0); press('{');
    r.pairValue = src.value;
    r.pairCaret = src.selectionStart;

    // 2) 回车展开：if (a){} 光标在 {} 之间
    set('if (a){}', 7); press('Enter');
    r.enterValue = src.value;
    r.enterCaret = src.selectionStart;

    // 3) 成对退格：() 之间按退格
    set('()', 1); press('Backspace');
    r.backspaceValue = src.value;

    // 4) 引号跳过："" 之间再输入 "
    set('""', 1); press('"');
    r.quoteValue = src.value;
    r.quoteCaret = src.selectionStart;

    // 5) 行首 } 自动退一级
    set('    ', 4); press('}');
    r.dedentValue = src.value;

    // 6) 当前行高亮
    set('int a;\nint b;', 8);
    r.currentLine = getComputedStyle(el.currentLine).display;

    // 7) 回车时自动排版当前行：a=10; → a = 10;
    set('a=10;', 5); press('Enter');
    r.enterFormatValue = src.value;
    r.enterFormatCaret = src.selectionStart;

    // 8) 带缩进的行同样保留缩进
    set('    b=24;', 9); press('Enter');
    r.enterFormatIndent = src.value;

    // 收尾：放一段正常代码并编译，便于截图
    loadSample(SAMPLE_GCD, 'gcd-demo.mc');
    await doCompile();
    await new Promise((res) => setTimeout(res, 200));
    return r;
  }
  if (fmt) {
    // 格式化回归：乱排版 → 格式化 → 再格式化（幂等）→ 编译
    el.source.value = SAMPLE_MESSY;
    renderEditor();
    const raw = el.source.value;
    formatDocument({ silent: true });
    const once = el.source.value;
    formatDocument({ silent: true });
    const twiceText = el.source.value;
    await doCompile();
    await new Promise((r) => setTimeout(r, 150));
    return {
      changed: raw !== once,
      idempotent: once === twiceText,
      exit: state.lastResult ? state.lastResult.exitCode : null,
      tokens: parseTokens(state.artifacts.tokens).length,
      text: once,
    };
  }
  if (multifile) {
    // 演示多文件：三个有产物的文件 + 一个空白新文件
    loadSample(SAMPLE_GCD, 'gcd-demo.mc');
    await doCompile();
    loadSample(SAMPLE_LEX, 'lex-demo.mc');
    await doCompile();
    loadSample(SAMPLE_BAD, 'error-demo.mc');
    await doCompile();
    newFile(); // 未命名.mc（空白）
    await new Promise((r) => setTimeout(r, 150));
    // 焦点停在有产物且编译成功的文件上，展示"每个文件独立保留产物"
    const gcd = state.docs.find((d) => d.name === 'gcd-demo.mc');
    if (gcd) activateDoc(gcd.id);
    // 演示"未保存"：给当前文件追加一行，标签上会出现蓝点
    if (!el.source.value.endsWith('\n')) el.source.value += '\n';
    el.source.value += '/* 未保存演示：每个文件的内容各自保留 */\n';
    el.source.dispatchEvent(new Event('input'));
    if (theme) applyTheme(theme);
    await new Promise((r) => setTimeout(r, 250));
    return {
      docs: state.docs.map((d) => ({ name: d.name, compiled: d.compiled, errs: (d.diagnostics || []).length })),
      active: state.fileName,
    };
  }
  if (twice) {
    // 回归：编译成功 → 改内容 → 再编译，第二次必须真正执行
    loadSample(SAMPLE_GCD, 'gcd-demo.mc');
    await doCompile();
    loadSample(SAMPLE_BAD, 'error-demo.mc');
    await doCompile();
    await new Promise((r) => setTimeout(r, 200));
    return {
      busy: state.busy,
      diagCount: state.diagnostics.length,
      status: el.topStatus.textContent,
      activeTab: state.activeTab,
    };
  }
  if (breakIt) {
    loadSample(SAMPLE_BAD, 'error-demo.mc');
  }
  if (theme) applyTheme(theme);
  await doCompile();
  if (tab) selectTab(tab);
  if (menu) {
    const anchor = document.querySelector(`.menu-item[data-menu="${menu}"]`) || $('btn-sample');
    openMenu(anchor, MENUS[menu] || []);
  }
  await new Promise((r) => setTimeout(r, 300));
};

init();
