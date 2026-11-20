import {
  BookOpen, CaretDown, FilePlus, FloppyDisk, FolderOpen, Hammer, Moon,
  Play, Sparkle, Sun, TextAa,
} from '@phosphor-icons/react';

export default function Toolbar({
  onNew, onOpen, onSave, onOpenSamples, onOpenBook, onCompile, onRun,
  compiling, running, optimize, onToggleOptimize, theme, onToggleTheme,
  fontSize, onFontSize,
}) {
  return (
    <div className="toolbar">
      <div className="toolbar-group">
        <button type="button" className="icon-btn" onClick={onNew} title="新建文件 (Ctrl+N)">
          <FilePlus size={18} />
        </button>
        <button type="button" className="icon-btn" onClick={onOpen} title="打开文件 (Ctrl+O)">
          <FolderOpen size={18} />
        </button>
        <button type="button" className="icon-btn" onClick={onSave} title="保存 (Ctrl+S)">
          <FloppyDisk size={18} />
        </button>
      </div>

      <div className="toolbar-sep" />

      <div className="toolbar-group">
        <button type="button" className="tool-btn" onClick={onOpenSamples}>
          <Sparkle size={17} weight="duotone" />
          <span>示例库</span>
          <CaretDown size={11} className="tool-caret" />
        </button>
        <button type="button" className="tool-btn" onClick={onOpenBook}>
          <BookOpen size={17} weight="duotone" />
          <span>语法手册</span>
        </button>
      </div>

      <div className="toolbar-sep" />

      <div className="toolbar-group toolbar-actions-main">
        <button type="button" className="action-btn action-compile" onClick={onCompile} disabled={compiling}>
          {compiling ? <span className="spinner" /> : <Hammer size={17} weight="fill" />}
          <span>{compiling ? '编译中…' : '编译'}</span>
          <kbd>F7</kbd>
        </button>
        <button type="button" className="action-btn action-run" onClick={onRun} disabled={running || compiling}>
          {running ? <span className="spinner spinner-light" /> : <Play size={16} weight="fill" />}
          <span>{running ? '运行中…' : '运行'}</span>
          <kbd>F5</kbd>
        </button>
      </div>

      <div className="toolbar-spacer" />

      <div className="toolbar-group toolbar-options">
        <button
          type="button"
          className={`optimize-toggle ${optimize ? 'is-on' : ''}`}
          onClick={onToggleOptimize}
          title="启用四元式优化（minic -O）"
        >
          <span className="opt-dot" />
          <span>优化</span>
          <code>-O</code>
        </button>

        <label className="fontsize-control" title="编辑器字号">
          <TextAa size={16} />
          <select value={fontSize} onChange={(e) => onFontSize(Number(e.target.value))}>
            {[12, 13, 14, 15, 16, 18, 20].map((n) => <option key={n} value={n}>{n}</option>)}
          </select>
        </label>

        <button type="button" className="icon-btn" onClick={onToggleTheme} title="切换深色 / 浅色主题">
          {theme === 'dark' ? <Sun size={18} /> : <Moon size={18} />}
        </button>
      </div>
    </div>
  );
}
