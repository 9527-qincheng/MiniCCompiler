import { useEffect, useRef } from 'react';
import * as monaco from 'monaco-editor';
import {
  FileCode, FilePlus, MagicWand, X, FolderSimple, DotsThreeOutline,
} from '@phosphor-icons/react';
import { formatMiniC } from '../lib/formatCode.js';

const EMPTY_SOURCE = `// 新建一个 MiniC 程序，或从示例库载入模板
int main() {
    print(1 + 2 * 3); print('\\n');
    return 0;
}
`;

export default function EditorPane({
  tabs, activeTab, onSelect, onClose, onNew, onChange, diagnostics,
  theme, fontSize, jumpTarget, onCursor, onCompile, onRun, onSave, onNotify,
}) {
  const hostRef = useRef(null);
  const editorRef = useRef(null);
  const modelsRef = useRef(new Map());
  const viewStatesRef = useRef(new Map());
  const activeIdRef = useRef(null);
  const callbacksRef = useRef({});
  const decorationsRef = useRef([]);
  const jumpStampRef = useRef(0);

  callbacksRef.current = { onCompile, onRun, onSave };

  /* 创建编辑器（只创建一次） */
  useEffect(() => {
    if (!hostRef.current) return undefined;
    const editor = monaco.editor.create(hostRef.current, {
      value: '',
      language: 'minic',
      theme: 'minic-dark',
      automaticLayout: true,
      fontFamily: "'JetBrains Mono', Consolas, 'Microsoft YaHei', monospace",
      fontSize: 14,
      lineHeight: 23,
      letterSpacing: 0,
      fontLigatures: true,
      padding: { top: 14, bottom: 30 },
      minimap: { enabled: true, renderCharacters: false, maxColumn: 90, scale: 1, showSlider: 'mouseover' },
      smoothScrolling: true,
      cursorBlinking: 'smooth',
      cursorSmoothCaretAnimation: 'on',
      renderLineHighlight: 'all',
      renderWhitespace: 'selection',
      scrollBeyondLastLine: false,
      tabSize: 4,
      insertSpaces: true,
      bracketPairColorization: { enabled: true },
      guides: { bracketPairs: 'active', indentation: true },
      stickyScroll: { enabled: true, maxLineCount: 3 },
      scrollbar: { verticalScrollbarSize: 10, horizontalScrollbarSize: 10, useShadows: false },
      overviewRulerBorder: false,
      hideCursorInOverviewRuler: true,
      wordBasedSuggestions: 'currentDocument',
      suggest: { showWords: true, preview: true },
    });
    editorRef.current = editor;

    const sub = editor.onDidChangeCursorPosition((e) => {
      onCursor?.({ line: e.position.lineNumber, column: e.position.column });
    });

    const changeSub = editor.onDidChangeModelContent(() => {
      const id = activeIdRef.current;
      if (!id) return;
      onChange?.(id, editor.getValue());
    });

    editor.addCommand(monaco.KeyCode.F7, () => callbacksRef.current.onCompile?.());
    editor.addCommand(monaco.KeyCode.F5, () => callbacksRef.current.onRun?.());
    editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS, () => callbacksRef.current.onSave?.());
    editor.addCommand(monaco.KeyMod.Shift | monaco.KeyMod.Alt | monaco.KeyCode.KeyF, () => {
      const model = editor.getModel();
      if (!model) return;
      const src = model.getValue();
      const out = formatMiniC(src);
      if (out === src) return;
      editor.pushUndoStop();
      editor.executeEdits('minic-format', [{ range: model.getFullModelRange(), text: out }]);
      editor.pushUndoStop();
    });

    return () => {
      sub.dispose();
      changeSub.dispose();
      editor.dispose();
      for (const model of modelsRef.current.values()) model.dispose();
      modelsRef.current.clear();
      editorRef.current = null;
    };
  }, []);

  /* 管理每个标签页的 model */
  useEffect(() => {
    const editor = editorRef.current;
    if (!editor) return;
    const alive = new Set(tabs.map((t) => t.id));
    for (const [id, model] of modelsRef.current.entries()) {
      if (!alive.has(id)) {
        model.dispose();
        modelsRef.current.delete(id);
        viewStatesRef.current.delete(id);
      }
    }
    for (const tab of tabs) {
      if (!modelsRef.current.has(tab.id)) {
        const uri = monaco.Uri.parse(`inmemory://minic/${tab.id}.mc`);
        const model = monaco.editor.createModel(tab.content ?? '', 'minic', uri);
        monaco.editor.setModelLanguage(model, 'minic');
        modelsRef.current.set(tab.id, model);
      }
    }
  }, [tabs]);

  /* 切换活动标签 */
  useEffect(() => {
    const editor = editorRef.current;
    if (!editor) return;
    const prev = activeIdRef.current;
    if (prev && prev !== activeTab?.id) {
      viewStatesRef.current.set(prev, editor.saveViewState());
    }
    if (!activeTab) {
      editor.setModel(null);
      activeIdRef.current = null;
      return;
    }
    const model = modelsRef.current.get(activeTab.id);
    if (model) {
      editor.setModel(model);
      activeIdRef.current = activeTab.id;
      const state = viewStatesRef.current.get(activeTab.id);
      if (state) editor.restoreViewState(state);
      editor.focus();
    }
  }, [activeTab?.id, tabs.length]);

  /* 外部内容变化（例如格式化由外部触发时）同步到 model */
  useEffect(() => {
    if (!activeTab) return;
    const model = modelsRef.current.get(activeTab.id);
    if (model && model.getValue() !== activeTab.content) {
      model.setValue(activeTab.content ?? '');
    }
  }, [activeTab?.content, activeTab?.id]);

  /* 主题 */
  useEffect(() => {
    monaco.editor.setTheme(theme === 'light' ? 'minic-light' : 'minic-dark');
  }, [theme]);

  /* 字号 */
  useEffect(() => {
    editorRef.current?.updateOptions({ fontSize });
  }, [fontSize]);

  /* 诊断标记 */
  useEffect(() => {
    if (!activeTab) return;
    const model = modelsRef.current.get(activeTab.id);
    if (!model) return;
    const markers = (diagnostics || [])
      .filter((d) => d.line > 0)
      .map((d) => ({
        startLineNumber: d.line,
        startColumn: Math.max(1, d.col || 1),
        endLineNumber: d.line,
        endColumn: Math.max((d.col || 1) + 1, 2),
        message: d.message,
        severity: d.severity === 'warning' ? monaco.MarkerSeverity.Warning : monaco.MarkerSeverity.Error,
        source: 'minic',
      }));
    monaco.editor.setModelMarkers(model, 'minic', markers);
  }, [diagnostics, activeTab?.id]);

  /* 跳转 + 行高亮 */
  useEffect(() => {
    const editor = editorRef.current;
    if (!editor || !jumpTarget || jumpTarget.line <= 0) return;
    if (jumpTarget.stamp === jumpStampRef.current) return;
    jumpStampRef.current = jumpTarget.stamp;
    const line = jumpTarget.line;
    editor.revealLineInCenter(line);
    editor.setPosition({ lineNumber: line, column: Math.max(1, jumpTarget.col || 1) });
    editor.focus();
    decorationsRef.current = editor.deltaDecorations(decorationsRef.current, [{
      range: new monaco.Range(line, 1, line, 1),
      options: {
        isWholeLine: true,
        className: 'minic-jump-line',
        linesDecorationsClassName: 'minic-jump-gutter',
      },
    }]);
    const timer = setTimeout(() => {
      if (editorRef.current) {
        decorationsRef.current = editorRef.current.deltaDecorations(decorationsRef.current, []);
      }
    }, 1600);
    return () => clearTimeout(timer);
  }, [jumpTarget]);

  const doFormat = () => {
    const editor = editorRef.current;
    const model = editor?.getModel();
    if (!editor || !model) return;
    const src = model.getValue();
    const out = formatMiniC(src);
    if (out === src) {
      onNotify?.('格式已经很整齐了', 'info');
      return;
    }
    editor.pushUndoStop();
    editor.executeEdits('minic-format', [{ range: model.getFullModelRange(), text: out }]);
    editor.pushUndoStop();
    onNotify?.('已格式化当前文件', 'success');
  };

  return (
    <section className="editor-pane">
      <div className="editor-tabs">
        <div className="editor-tabs-scroll">
          {tabs.map((tab) => (
            <div
              key={tab.id}
              role="tab"
              tabIndex={0}
              className={`editor-tab ${tab.id === activeTab?.id ? 'is-active' : ''}`}
              onClick={() => onSelect(tab.id)}
              onKeyDown={(e) => { if (e.key === 'Enter') onSelect(tab.id); }}
              title={tab.filePath || tab.name}
            >
              <FileCode size={14} weight={tab.id === activeTab?.id ? 'fill' : 'regular'} />
              <span className="editor-tab-name">{tab.name}</span>
              {tab.dirty ? <span className="dirty-dot" /> : null}
              <button
                type="button"
                className="editor-tab-close"
                title="关闭"
                onClick={(e) => { e.stopPropagation(); onClose(tab.id); }}
              >
                <X size={11} weight="bold" />
              </button>
            </div>
          ))}
        </div>
        <button type="button" className="tab-new" onClick={onNew} title="新建文件">
          <FilePlus size={15} />
        </button>
        <button type="button" className="tab-more" title="更多操作">
          <DotsThreeOutline size={16} />
        </button>
      </div>

      <div className="editor-breadcrumb">
        <div className="breadcrumb-path">
          <FolderSimple size={14} weight="duotone" />
          <span className="crumb-root">workspace</span>
          <span className="crumb-sep">/</span>
          <span className="crumb-file">{activeTab?.name || '未命名'}</span>
          {activeTab?.filePath ? <em title={activeTab.filePath}>已保存到磁盘</em> : <em>未保存</em>}
        </div>
        <div className="breadcrumb-actions">
          <button type="button" className="crumb-btn" onClick={doFormat} title="格式化文档 (Shift+Alt+F)">
            <MagicWand size={14} />
            <span>格式化</span>
          </button>
          <span className="lang-chip">MiniC</span>
        </div>
      </div>

      {activeTab ? (
        <div className="editor-host" ref={hostRef} />
      ) : (
        <div className="editor-welcome">
          <div className="welcome-glow" />
          <div className="welcome-logo">&lt;/&gt;</div>
          <h1>MiniC Studio</h1>
          <p>一个把词法、语法、语义、中间代码与目标代码放在同一张工作台上的编译器 IDE。</p>
          <div className="welcome-actions">
            <button type="button" className="action-btn action-compile" onClick={onNew}>
              <FilePlus size={17} weight="fill" /> 新建文件
            </button>
          </div>
          <div className="welcome-shortcuts">
            <span><kbd>F7</kbd> 编译</span>
            <span><kbd>F5</kbd> 运行</span>
            <span><kbd>Ctrl</kbd> + <kbd>S</kbd> 保存</span>
          </div>
          <pre className="welcome-code">{EMPTY_SOURCE}</pre>
        </div>
      )}
    </section>
  );
}
