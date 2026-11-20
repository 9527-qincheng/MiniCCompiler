import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  Cpu, ListBullets, Path, Stack, TerminalWindow, TextAa, TreeStructure, WarningCircle,
} from '@phosphor-icons/react';
import TitleBar from './components/TitleBar.jsx';
import Toolbar from './components/Toolbar.jsx';
import EditorPane from './components/EditorPane.jsx';
import StatusBar from './components/StatusBar.jsx';
import ToastStack from './components/Toast.jsx';
import SamplesModal from './components/SamplesModal.jsx';
import BookModal from './components/BookModal.jsx';
import TokenPanel from './panels/TokenPanel.jsx';
import AstPanel from './panels/AstPanel.jsx';
import SymbolPanel from './panels/SymbolPanel.jsx';
import QuadPanel from './panels/QuadPanel.jsx';
import AsmPanel from './panels/AsmPanel.jsx';
import ParsePanel from './panels/ParsePanel.jsx';
import RunPanel from './panels/RunPanel.jsx';
import DiagnosticsPanel from './panels/DiagnosticsPanel.jsx';
import { parseAllArtifacts } from './lib/parseArtifacts.js';
import { SAMPLES } from './data/samples.js';

const PANELS = [
  { id: 'token', label: '词法', en: 'Token', icon: TextAa },
  { id: 'ast', label: '语法树', en: 'AST', icon: TreeStructure },
  { id: 'sym', label: '符号表', en: 'Symbol', icon: Stack },
  { id: 'quad', label: '中间码', en: 'IR', icon: Path },
  { id: 'asm', label: '目标码', en: 'ASM', icon: Cpu },
  { id: 'parse', label: 'LL(1)', en: 'Parse', icon: ListBullets },
  { id: 'run', label: '运行', en: 'Run', icon: TerminalWindow },
  { id: 'diag', label: '诊断', en: 'Issues', icon: WarningCircle },
];

const PANEL_ALIAS = {
  token: 'token', tokens: 'token', lex: 'token', lexer: 'token',
  ast: 'ast', grammar: 'ast', tree: 'ast',
  sym: 'sym', symbols: 'sym', symbol: 'sym',
  quad: 'quad', quads: 'quad', ir: 'quad', four: 'quad',
  asm: 'asm', code: 'asm', target: 'asm',
  parse: 'parse', ll1: 'parse', trace: 'parse',
  run: 'run', output: 'run', console: 'run',
  diag: 'diag', diagnostics: 'diag', errors: 'diag',
};

const EMPTY_NEW = `int main() {
    print(1 + 2 * 3); print('\\n');
    return 0;
}
`;

const SAMPLE_MAP = new Map(SAMPLES.map((s) => [s.id, s]));
const INITIAL_SAMPLE = SAMPLE_MAP.get('overview') || SAMPLES[0];

function makeTab(seq, name, content, extra = {}) {
  return {
    id: `tab-${seq}`,
    name,
    content,
    filePath: null,
    dirty: false,
    ...extra,
  };
}

export default function App() {
  const seqRef = useRef(1);
  const initialTabRef = useRef(makeTab(1, INITIAL_SAMPLE.name, INITIAL_SAMPLE.text, { sampleId: INITIAL_SAMPLE.id }));
  const [tabs, setTabs] = useState([initialTabRef.current]);
  const [activeId, setActiveId] = useState(initialTabRef.current.id);
  const [theme, setTheme] = useState(() => localStorage.getItem('minic.theme') || 'dark');
  const [fontSize, setFontSize] = useState(() => Number(localStorage.getItem('minic.fontSize')) || 14);
  const [optimize, setOptimize] = useState(() => localStorage.getItem('minic.optimize') === '1');
  const [includeParse, setIncludeParse] = useState(false);
  const [activePanel, setActivePanel] = useState('token');
  const [compileResults, setCompileResults] = useState({});
  const [runResults, setRunResults] = useState({});
  const [compiling, setCompiling] = useState(false);
  const [running, setRunning] = useState(false);
  const [cursor, setCursor] = useState({ line: 1, column: 1 });
  const [jumpTarget, setJumpTarget] = useState(null);
  const [modal, setModal] = useState(null);
  const [toasts, setToasts] = useState([]);
  const [editorWidth, setEditorWidth] = useState(() => Number(localStorage.getItem('minic.split')) || 46);
  const [appInfo, setAppInfo] = useState(null);
  const [dragging, setDragging] = useState(false);

  const tabsRef = useRef(tabs);
  tabsRef.current = tabs;
  const activeIdRef = useRef(activeId);
  activeIdRef.current = activeId;
  const resultsRef = useRef(compileResults);
  resultsRef.current = compileResults;
  const compilingRef = useRef(false);
  const runningRef = useRef(false);

  const activeTab = tabs.find((t) => t.id === activeId) || tabs[0];
  const result = activeTab ? compileResults[activeTab.id] : null;
  const run = activeTab ? runResults[activeTab.id] : null;

  const notify = useCallback((message, kind = 'info') => {
    const id = `${Date.now()}-${Math.random().toString(36).slice(2, 7)}`;
    setToasts((prev) => [...prev.slice(-3), { id, message, kind }]);
    setTimeout(() => setToasts((prev) => prev.filter((t) => t.id !== id)), 3600);
  }, []);

  const dismissToast = (id) => setToasts((prev) => prev.filter((t) => t.id !== id));

  /* ------------------------------ 外观持久化 ------------------------------ */
  useEffect(() => {
    document.documentElement.dataset.theme = theme;
    localStorage.setItem('minic.theme', theme);
  }, [theme]);
  useEffect(() => { localStorage.setItem('minic.fontSize', String(fontSize)); }, [fontSize]);
  useEffect(() => { localStorage.setItem('minic.optimize', optimize ? '1' : '0'); }, [optimize]);
  useEffect(() => { localStorage.setItem('minic.split', String(Math.round(editorWidth))); }, [editorWidth]);
  useEffect(() => {
    let alive = true;
    window.minicAPI?.info?.().then((info) => { if (alive) setAppInfo(info); }).catch(() => {});
    return () => { alive = false; };
  }, []);

  /* ------------------------------ 标签页操作 ------------------------------ */
  const updateTab = useCallback((id, patch) => {
    setTabs((prev) => prev.map((t) => (t.id === id ? { ...t, ...patch } : t)));
  }, []);

  const handleChange = useCallback((id, content) => {
    setTabs((prev) => prev.map((t) => (t.id === id ? (t.content === content ? t : { ...t, content, dirty: true }) : t)));
  }, []);

  const newTab = useCallback((content = EMPTY_NEW, name, extra = {}) => {
    seqRef.current += 1;
    const tab = makeTab(seqRef.current, name || `untitled-${seqRef.current}.mc`, content, extra);
    setTabs((prev) => [...prev, tab]);
    setActiveId(tab.id);
    return tab;
  }, []);

  const closeTab = useCallback((id) => {
    setTabs((prev) => {
      const idx = prev.findIndex((t) => t.id === id);
      if (idx < 0) return prev;
      if (prev[idx].dirty && !window.confirm(`「${prev[idx].name}」有未保存的修改，仍要关闭吗？`)) return prev;
      const next = prev.filter((t) => t.id !== id);
      if (activeIdRef.current === id) {
        const neighbor = next[Math.max(0, idx - 1)] || next[0];
        setActiveId(neighbor ? neighbor.id : null);
      }
      return next;
    });
  }, []);

  const openFile = useCallback(async () => {
    try {
      const r = await window.minicAPI?.openFile?.();
      if (!r || r.canceled) return;
      const tab = newTab(r.content ?? '', r.name, { filePath: r.filePath });
      updateTab(tab.id, { dirty: false });
      notify(`已打开 ${r.name}`, 'success');
    } catch (e) {
      notify(`打开文件失败：${e.message || e}`, 'error');
    }
  }, [newTab, notify, updateTab]);

  const saveTab = useCallback(async (tab = activeTab) => {
    if (!tab) return false;
    try {
      if (tab.filePath) {
        const r = await window.minicAPI.saveFile(tab.filePath, tab.content);
        if (r?.ok) { updateTab(tab.id, { dirty: false }); notify(`已保存 ${tab.name}`, 'success'); return true; }
        notify(r?.message || '保存失败', 'error');
        return false;
      }
      const r = await window.minicAPI.saveAs(tab.content, tab.name);
      if (r?.canceled) return false;
      if (r?.ok) {
        updateTab(tab.id, { filePath: r.filePath, name: r.name || tab.name, dirty: false });
        notify(`已另存为 ${r.name || tab.name}`, 'success');
        return true;
      }
      notify(r?.message || '保存失败', 'error');
    } catch (e) {
      notify(`保存失败：${e.message || e}`, 'error');
    }
    return false;
  }, [activeTab, notify, updateTab]);

  /* ------------------------------ 编译 / 运行 ------------------------------ */
  const compileTab = useCallback(async (tab, options = {}) => {
    if (!tab) return null;
    if (compilingRef.current && !options.force) return null;
    compilingRef.current = true;
    setCompiling(true);
    const useOptimize = options.optimize ?? optimize;
    const useParse = options.includeParse ?? includeParse;
    const started = Date.now();
    try {
      const res = await window.minicAPI.compile({
        text: tab.content,
        name: tab.name,
        optimize: useOptimize,
        includeParse: useParse,
      });
      const parsed = parseAllArtifacts(res.artifacts, res.stderr, res.stdout, res.ok);
      const enriched = { ...res, parsed, stamp: Date.now(), optimize: useOptimize, includeParse: useParse };
      setCompileResults((prev) => ({ ...prev, [tab.id]: enriched }));
      if (!res.ok) {
        setActivePanel('diag');
        const n = parsed.diagnostics?.items?.length || 0;
        if (!options.silent) notify(n ? `编译失败：${n} 个问题` : (res.message || '编译失败'), 'error');
      } else if (!options.silent) {
        notify(`编译成功 · ${Date.now() - started} ms`, 'success');
      }
      return enriched;
    } catch (e) {
      const enriched = {
        ok: false,
        reason: 'exception',
        message: String(e?.message || e),
        parsed: parseAllArtifacts({}, String(e?.message || e), '', false),
        stamp: Date.now(),
      };
      setCompileResults((prev) => ({ ...prev, [tab.id]: enriched }));
      setActivePanel('diag');
      if (!options.silent) notify(`编译异常：${e.message || e}`, 'error');
      return enriched;
    } finally {
      compilingRef.current = false;
      setCompiling(false);
    }
  }, [includeParse, notify, optimize]);

  const doCompile = useCallback((options) => compileTab(activeTab, options), [activeTab, compileTab]);

  const doRun = useCallback(async () => {
    const tab = tabsRef.current.find((t) => t.id === activeIdRef.current);
    if (!tab || runningRef.current) return null;
    runningRef.current = true;
    setRunning(true);
    setRunResults((prev) => ({ ...prev, [tab.id]: { running: true } }));
    setActivePanel('run');
    try {
      const res = await window.minicAPI.run({ text: tab.content, name: tab.name, optimize });
      const enriched = { ...res, stamp: Date.now() };
      setRunResults((prev) => ({ ...prev, [tab.id]: enriched }));
      if (!res.ok) {
        const msg = res.timedOut ? '程序运行超时（5 秒）' : (res.message || `运行失败（${res.exitCode ?? '未知'}）`);
        notify(msg, 'error');
      }
      return enriched;
    } catch (e) {
      const enriched = { ok: false, message: String(e?.message || e), stage: 'run', stamp: Date.now() };
      setRunResults((prev) => ({ ...prev, [tab.id]: enriched }));
      notify(`运行异常：${e.message || e}`, 'error');
      return enriched;
    } finally {
      runningRef.current = false;
      setRunning(false);
    }
  }, [notify, optimize]);

  /* ------------------------------ 示例与手册 ------------------------------ */
  const loadSample = useCallback((sample) => {
    setModal(null);
    newTab(sample.text, sample.name, { sampleId: sample.id });
    notify(`已载入示例「${sample.label}」`, 'info');
  }, [newTab, notify]);

  const loadBookExample = useCallback((code, title) => {
    setModal(null);
    seqRef.current += 1;
    const tab = makeTab(seqRef.current, `book-${seqRef.current}.mc`, code);
    setTabs((prev) => [...prev, tab]);
    setActiveId(tab.id);
    notify(`已载入「${title}」示例`, 'info');
  }, [notify]);

  /* ------------------------------ 定位 / 导出 ------------------------------ */
  const jumpTo = useCallback((line, col = 1) => {
    setJumpTarget({ line, col, stamp: Date.now() });
  }, []);

  const exportArtifact = useCallback(async (kind) => {
    if (!result) { notify('还没有可导出的产物', 'info'); return; }
    const raw = result.parsed?.raw || {};
    const map = {
      token: [raw.token, 'tokens.txt'],
      ast: [raw.ast, 'ast.txt'],
      sym: [raw.sym, 'symbols.txt'],
      quad: [raw.quad, 'quads.txt'],
      asm: [raw.asm, 'program.s'],
      parse: [raw.parse, 'parse.txt'],
    };
    const [content, suffix] = map[kind] || [];
    if (!content) { notify('该产物为空，无法导出', 'info'); return; }
    try {
      const name = (activeTab?.name || 'program.mc').replace(/\.mc$/i, `-${suffix}`);
      const r = await window.minicAPI.saveAs(content, name);
      if (r?.ok) notify(`已导出 ${r.name || name}`, 'success');
      else if (!r?.canceled) notify(r?.message || '导出失败', 'error');
    } catch (e) {
      notify(`导出失败：${e.message || e}`, 'error');
    }
  }, [activeTab, notify, result]);

  /* ------------------------------ 启动自动编译 ------------------------------ */
  const bootCompiledRef = useRef(false);
  useEffect(() => {
    if (bootCompiledRef.current) return undefined;
    const timer = setTimeout(() => {
      if (bootCompiledRef.current) return;
      bootCompiledRef.current = true;
      const tab = tabsRef.current[0];
      if (tab) compileTab(tab, { silent: true, includeParse });
    }, 650);
    return () => clearTimeout(timer);
  }, [compileTab, includeParse]);

  /* ------------------------------ 快捷键 ------------------------------ */
  useEffect(() => {
    const onKey = (e) => {
      const mod = e.ctrlKey || e.metaKey;
      if (mod && e.key.toLowerCase() === 'n') { e.preventDefault(); newTab(); }
      else if (mod && e.key.toLowerCase() === 'o') { e.preventDefault(); openFile(); }
      else if (mod && e.key.toLowerCase() === 's') { e.preventDefault(); saveTab(); }
      else if (e.key === 'F7') { e.preventDefault(); doCompile(); }
      else if (e.key === 'F5') { e.preventDefault(); doRun(); }
      else if (e.key === 'F1') { e.preventDefault(); setModal('book'); }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [doCompile, doRun, newTab, openFile, saveTab]);

  /* ------------------------------ 分栏拖拽 ------------------------------ */
  useEffect(() => {
    if (!dragging) return undefined;
    const move = (e) => {
      const pct = (e.clientX / window.innerWidth) * 100;
      setEditorWidth(Math.min(70, Math.max(28, pct)));
    };
    const up = () => setDragging(false);
    window.addEventListener('mousemove', move);
    window.addEventListener('mouseup', up);
    return () => {
      window.removeEventListener('mousemove', move);
      window.removeEventListener('mouseup', up);
    };
  }, [dragging]);

  /* ------------------------------ 面板徽标 ------------------------------ */
  const badges = useMemo(() => {
    const p = result?.parsed;
    return {
      token: p?.tokens?.rows?.length,
      ast: p?.ast?.count,
      sym: p?.symbols?.scopes?.length ? Object.values(p.symbols.totals).reduce((a, b) => a + b, 0) : null,
      quad: p?.quads?.rows?.length,
      asm: p?.asm?.lines?.length,
      parse: p?.parseTrace?.steps?.length,
      run: run?.running ? '…' : (run?.ok ? '✓' : (run ? '!' : null)),
      diag: p?.diagnostics?.items?.length || null,
    };
  }, [result, run]);

  /* ------------------------------ 截图测试钩子 ------------------------------ */
  useEffect(() => {
    window.__minicTest = async (opts = {}) => {
      if (opts.theme) setTheme(opts.theme);
      if (opts.fontSize) setFontSize(Number(opts.fontSize));
      if (opts.includeParse) setIncludeParse(true);
      let testResult = null;
      let runResult = null;
      if (opts.break) {
        const s = SAMPLE_MAP.get('err-combined') || SAMPLE_MAP.get('err-mixed-all');
        if (s) {
          const tab = makeTab(++seqRef.current, s.name, s.text);
          setTabs((prev) => [...prev, tab]);
          setActiveId(tab.id);
          setActivePanel('diag');
          testResult = await compileTab(tab, { silent: true, includeParse: false });
        }
      } else if (opts.demo || opts.compile) {
        const tab = tabsRef.current.find((t) => t.id === activeIdRef.current) || tabsRef.current[0];
        testResult = await compileTab(tab, { silent: true, includeParse: opts.includeParse ?? includeParse });
      }
      if (opts.run) {
        runResult = await doRun();
      }
      if (opts.diagJump) {
        const s = SAMPLE_MAP.get(opts.diagJump.sample || 'err-mixed-all') || SAMPLE_MAP.get('err-combined');
        if (s) {
          const tab = makeTab(++seqRef.current, s.name, s.text);
          setTabs((prev) => [...prev, tab]);
          setActiveId(tab.id);
          const r = await compileTab(tab, { silent: true, includeParse: false });
          const item = r?.parsed?.diagnostics?.items?.[opts.diagJump.index || 0];
          if (item) jumpTo(item.line, item.col);
          setActivePanel('diag');
          testResult = r;
        }
      }
      if (opts.multifile) {
        for (const s of SAMPLES.slice(0, 3)) {
          const tab = makeTab(++seqRef.current, s.name, s.text);
          setTabs((prev) => [...prev, tab]);
        }
      }
      if (opts.tab) {
        const key = String(opts.tab).toLowerCase();
        setActivePanel(PANEL_ALIAS[key] || key);
      }
      if (opts.menu) setModal(opts.menu === 'book' ? 'book' : 'samples');
      await new Promise((r) => setTimeout(r, 450));
      return {
        theme: opts.theme || theme,
        tabs: tabsRef.current.length,
        panel: opts.tab || activePanel,
        ok: testResult?.ok ?? runResult?.ok ?? null,
        errors: testResult?.parsed?.diagnostics?.items?.length ?? null,
        tokens: testResult?.parsed?.tokens?.rows?.length ?? null,
        runOk: runResult?.ok ?? null,
        runOutput: runResult?.stdout ?? null,
        includeParse: testResult?.includeParse ?? null,
        parseGenerated: testResult?.parseGenerated ?? null,
        parseLen: testResult?.artifacts?.['.parse.txt']?.length ?? null,
      };
    };
    return () => { delete window.__minicTest; };
  });

  const panelProps = { result, theme, onJump: jumpTo, onExport: exportArtifact };

  return (
    <div className="app-shell">
      <TitleBar activeTab={activeTab} />
      <Toolbar
        onNew={() => newTab()}
        onOpen={openFile}
        onSave={() => saveTab()}
        onOpenSamples={() => setModal('samples')}
        onOpenBook={() => setModal('book')}
        onCompile={() => doCompile()}
        onRun={doRun}
        compiling={compiling}
        running={running}
        optimize={optimize}
        onToggleOptimize={() => setOptimize((v) => !v)}
        theme={theme}
        onToggleTheme={() => setTheme((t) => (t === 'dark' ? 'light' : 'dark'))}
        fontSize={fontSize}
        onFontSize={setFontSize}
      />

      <main
        className={`workspace ${dragging ? 'is-dragging' : ''}`}
        style={{ gridTemplateColumns: `${editorWidth}% 6px minmax(0, 1fr)` }}
      >
        <EditorPane
          tabs={tabs}
          activeTab={activeTab}
          onSelect={setActiveId}
          onClose={closeTab}
          onNew={() => newTab()}
          onChange={handleChange}
          diagnostics={result?.parsed?.diagnostics?.items || []}
          theme={theme}
          fontSize={fontSize}
          jumpTarget={jumpTarget}
          onCursor={setCursor}
          onCompile={() => doCompile()}
          onRun={doRun}
          onSave={() => saveTab()}
          onNotify={notify}
        />

        <div
          className="splitter"
          role="separator"
          aria-orientation="vertical"
          onMouseDown={() => setDragging(true)}
          onDoubleClick={() => setEditorWidth(46)}
          title="拖拽调整编辑器宽度，双击复位"
        >
          <span />
        </div>

        <section className="inspector">
          <nav className="inspector-tabs" role="tablist">
            {PANELS.map((p) => {
              const Icon = p.icon;
              const badge = badges[p.id];
              return (
                <button
                  key={p.id}
                  type="button"
                  role="tab"
                  aria-selected={activePanel === p.id}
                  className={`inspector-tab ${activePanel === p.id ? 'is-active' : ''} ${p.id === 'diag' && badge ? 'has-error' : ''}`}
                  onClick={() => setActivePanel(p.id)}
                  title={`${p.label} (${p.en})`}
                >
                  <Icon size={16} weight={activePanel === p.id ? 'fill' : 'regular'} />
                  <span>{p.label}</span>
                  {badge != null && badge !== 0 ? <em>{badge}</em> : null}
                </button>
              );
            })}
          </nav>

          <div className="inspector-body">
            {activePanel === 'token' ? <TokenPanel {...panelProps} /> : null}
            {activePanel === 'ast' ? <AstPanel {...panelProps} /> : null}
            {activePanel === 'sym' ? <SymbolPanel {...panelProps} /> : null}
            {activePanel === 'quad' ? <QuadPanel {...panelProps} /> : null}
            {activePanel === 'asm' ? <AsmPanel {...panelProps} /> : null}
            {activePanel === 'parse' ? (
              <ParsePanel
                result={result}
                enabled={includeParse}
                onEnable={() => { setIncludeParse(true); setActivePanel('parse'); doCompile({ includeParse: true, silent: false }); }}
              />
            ) : null}
            {activePanel === 'run' ? (
              <RunPanel
                run={run}
                running={running}
                onRun={doRun}
                onClear={() => setRunResults((prev) => ({ ...prev, [activeTab?.id]: null }))}
                onOpenDiagnostics={() => setActivePanel('diag')}
              />
            ) : null}
            {activePanel === 'diag' ? <DiagnosticsPanel result={result} onJump={jumpTo} /> : null}
          </div>
        </section>
      </main>

      <StatusBar
        result={result}
        activeTab={activeTab}
        cursor={cursor}
        compiling={compiling}
        running={running}
        optimize={optimize}
        theme={theme}
        exePath={result?.exe || appInfo?.exePath}
      />

      <SamplesModal open={modal === 'samples'} onClose={() => setModal(null)} onSelect={loadSample} />
      <BookModal open={modal === 'book'} onClose={() => setModal(null)} onUse={loadBookExample} />
      <ToastStack toasts={toasts} onDismiss={dismissToast} />
    </div>
  );
}
