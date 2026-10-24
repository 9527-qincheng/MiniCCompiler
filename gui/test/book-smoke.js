'use strict';
/**
 * 「语法手册」冒烟测试：node test/book-smoke.js
 *
 * 手册是纯前端功能，在无显示器 / 无 Electron 的环境（CI、受限沙箱）里也能验证：
 *   1. 数据层：syntax-book.js 的章/条目结构、状态取值、交叉引用、id 唯一性
 *   2. 渲染层：把 renderer.js 载入一个最小 DOM stub，跑渲染、搜索、开关等真实代码路径
 *
 * 手册里所有示例都宣称「实测过」，所以这里还顺带跑一遍：把每条 ok/limit 示例交给
 * build/minic.exe 编译，要求退出码与 status 相符。没有 minic.exe 时跳过该段。
 *
 * 之所以用 vm + DOM stub 而不是真开窗口：Electron 在受限环境下起不来（mojo 拒绝访问），
 * 而手册逻辑全在 renderer.js 里，用 stub 跑能覆盖到同样的函数。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');
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

/* ------------------------------------------------------------------ */
/* 最小 DOM stub：够 renderer.js 在 init() 里跑完不出错，也够手册模块渲染 */
/* ------------------------------------------------------------------ */
function makeEl(id) {
  const classes = new Set();
  return {
    id,
    tagName: 'DIV',
    className: '',
    innerHTML: '',
    textContent: '',
    value: '',
    checked: false,
    disabled: false,
    hidden: false,
    scrollTop: 0,
    scrollLeft: 0,
    scrollHeight: 600,
    clientHeight: 600,
    clientWidth: 800,
    offsetTop: 0,
    offsetHeight: 20,
    dataset: {},
    style: {},
    children: [],
    parentNode: null,
    classList: {
      add(...c) { c.forEach((x) => classes.add(x)); },
      remove(...c) { c.forEach((x) => classes.delete(x)); },
      toggle(c, on) {
        if (on === undefined) { classes.has(c) ? classes.delete(c) : classes.add(c); }
        else if (on) { classes.add(c); } else { classes.delete(c); }
      },
      contains(c) { return classes.has(c); },
    },
    addEventListener() {},
    removeEventListener() {},
    dispatchEvent() { return true; },
    appendChild(c) { this.children.push(c); c.parentNode = this; return c; },
    insertBefore(c) { this.children.push(c); c.parentNode = this; return c; },
    removeChild(c) { return c; },
    remove() {},
    setAttribute() {},
    getAttribute() { return null; },
    hasAttribute() { return false; },
    querySelector() { return makeEl('q'); },
    querySelectorAll() { return []; },
    getBoundingClientRect() { return { top: 0, left: 0, width: 800, height: 600, bottom: 600, right: 800 }; },
    focus() {}, blur() {}, select() {}, scrollIntoView() {}, click() {},
    setSelectionRange() {},
    getContext() { return null; },
  };
}

function buildContext() {
  const store = new Map();
  const document = {
    getElementById(id) {
      if (!store.has(id)) store.set(id, makeEl(id));
      return store.get(id);
    },
    querySelector(s) { return makeEl('q:' + s); },
    querySelectorAll() { return []; },
    createElement(tag) { const e = makeEl('new:' + tag); e.tagName = String(tag).toUpperCase(); return e; },
    addEventListener() {},
    removeEventListener() {},
    body: makeEl('body'),
    documentElement: makeEl('html'),
    execCommand() { return true; },
  };

  const sandbox = {
    console,
    document,
    window: {
      minicAPI: null,
      matchMedia: () => ({ matches: false, addEventListener() {} }),
      addEventListener() {},
    },
    navigator: { clipboard: { writeText: async () => {} }, userAgent: 'node' },
    localStorage: { getItem() { return null; }, setItem() {}, removeItem() {} },
    setTimeout, clearTimeout, setInterval, clearInterval,
    requestAnimationFrame: (f) => setTimeout(f, 0),
    Event: class { constructor(t) { this.type = t; } },
    getComputedStyle: () => ({ getPropertyValue: () => '' }),
    SYNTAX_BOOK: require(path.join(GUI_DIR, 'src', 'syntax-book.js')).SYNTAX_BOOK,
    SAMPLES: require(path.join(GUI_DIR, 'src', 'samples.js')).SAMPLES,
  };
  sandbox.globalThis = sandbox;
  sandbox.self = sandbox;

  const ctx = vm.createContext(sandbox);
  const src = fs.readFileSync(path.join(GUI_DIR, 'src', 'renderer.js'), 'utf8');
  // renderer.js 是脚本不是模块，末尾追加一行把内部符号挂到 globalThis 供断言使用
  const EXPORTS = '\n;globalThis.__T = { BOOK, BOOK_INDEX, BOOK_BADGE, bookState, el,'
    + ' bookEntryHtml, bookMatch, renderBook, openBook, closeBook, toggleBook, bookFindEntry,'
    + ' bookInlineCode, SAMPLE_LIST, SAMPLE_GCD, SAMPLE_LEX, SAMPLE_BAD, sampleById, openMenu };\n';
  vm.runInContext(src + EXPORTS, ctx, { filename: 'renderer.js' });
  return ctx.__T;
}

async function main() {
  /* ---------------- 1. 数据层 ---------------- */
  console.log('[1] syntax-book.js 数据层');
  const { SYNTAX_BOOK } = require(path.join(GUI_DIR, 'src', 'syntax-book.js'));
  check(Array.isArray(SYNTAX_BOOK), 'SYNTAX_BOOK 是数组');
  check(SYNTAX_BOOK.length >= 8, `章数 ${SYNTAX_BOOK.length} >= 8`);

  const ids = new Set();
  const dup = [];
  const badStatus = [];
  const noTitle = [];
  const all = [];
  for (const ch of SYNTAX_BOOK) {
    check(!!ch.id && !!ch.title && Array.isArray(ch.entries), `章 ${ch.id} 结构完整`);
    for (const e of ch.entries) {
      all.push(e);
      if (ids.has(e.id)) dup.push(e.id);
      ids.add(e.id);
      if (!['ok', 'no', 'limit', 'info'].includes(e.status)) badStatus.push(e.id + ':' + e.status);
      if (!e.title) noTitle.push(e.id);
    }
  }
  check(dup.length === 0, `条目 id 唯一（重复 ${dup.length}）`);
  check(badStatus.length === 0, `status 取值合法（非法 ${badStatus.join(',') || 0}）`);
  check(noTitle.length === 0, `每条都有标题（缺 ${noTitle.join(',') || 0}）`);
  console.log(`        共 ${all.length} 条：ok ${all.filter((e) => e.status === 'ok').length}`
    + ` / no ${all.filter((e) => e.status === 'no').length}`
    + ` / limit ${all.filter((e) => e.status === 'limit').length}`
    + ` / info ${all.filter((e) => e.status === 'info').length}`);

  const badRel = [];
  for (const e of all) for (const r of (e.related || [])) if (!ids.has(r)) badRel.push(e.id + '->' + r);
  check(badRel.length === 0, `related 交叉引用全部有效（失效 ${badRel.join(',') || 0}）`);

  const noEx = all.filter((e) => (e.status === 'ok' || e.status === 'limit') && !e.example);
  check(noEx.length === 0, `ok/limit 条目都带示例（缺 ${noEx.map((e) => e.id).join(',') || 0}）`);
  const noErr = all.filter((e) => e.status === 'no' && !e.err);
  check(noErr.length === 0, `no 条目都带报错原文（缺 ${noErr.map((e) => e.id).join(',') || 0}）`);

  /* ---------------- 2. 渲染层（DOM stub） ---------------- */
  console.log('\n[2] renderer.js 渲染与交互（DOM stub）');
  const g = buildContext();
  check(g.BOOK.length === SYNTAX_BOOK.length, `BOOK 已注入 ${g.BOOK.length} 章`);
  check(g.BOOK_INDEX.length === all.length, `BOOK_INDEX 摊平 ${g.BOOK_INDEX.length} 条`);
  check(g.bookState.open === false, 'bookState 初始关闭');

  const item = (id) => g.BOOK_INDEX.find((x) => x.entry.id === id);
  const helloItem = item('hello');
  const helloHtml = g.bookEntryHtml(helloItem);
  check(helloHtml.includes('id="entry-hello"'), '条目带锚点 id');
  check(helloHtml.includes('✓ 支持'), '支持条目显示 ✓ 徽标');
  check(helloHtml.includes('class="book-code"') && helloHtml.includes('tok-kw'), '示例块带语法高亮');
  check(helloHtml.includes('class="book-out"'), '示例块带运行输出');
  check(['insert', 'load', 'copy'].every((a) => helloHtml.includes(`data-book-act="${a}"`)),
    '示例块三个操作按钮齐全');

  const noItem = g.BOOK_INDEX.find((x) => x.entry.status === 'no');
  const noHtml = g.bookEntryHtml(noItem);
  check(noHtml.includes('✗ 不支持'), '不支持条目显示 ✗ 徽标');
  check(noHtml.includes('class="book-err"'), '不支持条目显示报错原文块');

  check(g.bookMatch(helloItem, '') === true, '空查询匹配全部');
  check(g.bookMatch(helloItem, '最小') === true, '命中标题');
  check(g.bookMatch(helloItem, '最小 程序') === true, '多关键词全命中');
  check(g.bookMatch(helloItem, '最小 zzz') === false, '多关键词部分命中即不匹配');
  check(g.bookMatch(item('func-def'), 'INT X') === true, '搜索大小写不敏感');

  g.renderBook();
  const tocCh = (g.el.bookToc.innerHTML.match(/book-toc-ch/g) || []).length;
  const tocItem = (g.el.bookToc.innerHTML.match(/book-toc-item/g) || []).length;
  const secs = (g.el.bookContent.innerHTML.match(/class="book-entry"/g) || []).length;
  check(tocCh === SYNTAX_BOOK.length, `目录渲染 ${tocCh} 个章按钮`);
  check(tocItem === all.length, `目录渲染 ${tocItem} 个条目按钮`);
  check(secs === all.length, `正文渲染 ${secs} 个条目`);
  check(g.el.bookFoot.innerHTML.includes(String(all.length)), '页脚显示总条数');

  g.bookState.query = '递归';
  g.renderBook();
  const hit = (g.el.bookContent.innerHTML.match(/class="book-entry"/g) || []).length;
  check(hit >= 1 && hit < all.length, `搜索「递归」过滤到 ${hit} 条`);
  check(g.el.bookFoot.innerHTML.includes('匹配'), '页脚显示匹配数');

  g.bookState.query = 'zzzznotfound';
  g.renderBook();
  check(g.el.bookContent.innerHTML.includes('book-empty'), '无匹配时正文显示空态');
  check(g.el.bookToc.innerHTML.includes('book-toc-empty'), '无匹配时目录显示空态');
  g.bookState.query = '';

  g.openBook();
  check(g.bookState.open === true, 'openBook 打开');
  check(!g.el.bookOverlay.classList.contains('hidden'), 'openBook 移除 hidden');
  g.closeBook();
  check(g.bookState.open === false, 'closeBook 关闭');
  check(g.el.bookOverlay.classList.contains('hidden'), 'closeBook 加回 hidden');
  g.toggleBook();
  check(g.bookState.open === true, 'toggleBook 打开');
  g.toggleBook();
  check(g.bookState.open === false, 'toggleBook 关闭');

  g.openBook('recursion');
  check(g.bookState.current === 'recursion' && g.bookState.open === true, 'openBook(id) 打开并定位');
  g.closeBook();

  /* ---------------- 3. 示例库注入 ---------------- */
  console.log('\n[3] 示例库注入（samples.js → renderer.js）');
  const { SAMPLES } = require(path.join(GUI_DIR, 'src', 'samples.js'));
  check(g.SAMPLE_LIST.length === SAMPLES.length, `SAMPLE_LIST 注入 ${g.SAMPLE_LIST.length} 条`);
  check(g.SAMPLE_GCD.length > 0 && g.SAMPLE_GCD.includes('while'), 'SAMPLE_GCD 兼容别名可用');
  check(g.SAMPLE_LEX.length > 0 && g.SAMPLE_LEX.includes('float'), 'SAMPLE_LEX 兼容别名可用');
  check(g.SAMPLE_BAD.length > 0, 'SAMPLE_BAD 兼容别名可用（指向 err-combined）');
  check(g.sampleById('gcd').text === g.SAMPLE_GCD, 'sampleById 与别名一致');
  check(g.sampleById('__nope__').text === '', 'sampleById 未知 id 返回空对象');

  /* ---------------- 4. 示例可编译性 ---------------- */
  console.log('\n[4] 示例在 minic.exe 上的可编译性');
  if (!fs.existsSync(MINIC)) {
    console.log('        未找到 build/minic.exe，跳过（先运行 build.bat）');
  } else {
    const tmp = fs.mkdtempSync(path.join(REPO_ROOT, 'build', '_booksmoke-'));
    let mismatched = 0;
    let spawned = 0;
    let blocked = null;
    for (const e of all) {
      if (!e.example) continue;
      const file = path.join(tmp, e.id + '.mc');
      fs.writeFileSync(file, e.example, 'utf8');
      const r = spawnSync(MINIC, [file], { encoding: 'utf8' });
      // 受限环境（部分沙箱）禁止 spawn，r.error 会是 EPERM —— 跳过而不是误判为不一致
      if (r.error) { blocked = r.error.code || r.error.message; break; }
      spawned++;
      const want = e.status === 'no' ? 1 : 0;
      if (r.status !== want) {
        mismatched++;
        const firstErr = String(r.stderr || '').trim().split(/\r?\n/)[0];
        console.log(`        MISMATCH ${e.id}  status=${e.status}  exit=${r.status}  ${firstErr}`);
      }
    }
    if (blocked) {
      console.log(`        当前环境禁止 spawn（${blocked}），跳过示例编译校验`);
    } else {
      check(mismatched === 0, `${spawned} 条示例退出码与 status 一致`);
    }
    fs.rmSync(tmp, { recursive: true, force: true });
  }

  console.log('\n' + (fail === 0 ? 'ALL PASS' : `${fail} FAILURE(S)`));
  process.exit(fail === 0 ? 0 : 1);
}

main().catch((e) => {
  console.error('UNEXPECTED ERROR:', e && e.stack || e);
  process.exit(1);
});
