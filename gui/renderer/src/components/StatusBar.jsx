import { CheckCircle, CircleNotch, Clock, Cpu, WarningCircle, XCircle } from '@phosphor-icons/react';
import { formatMs } from '../lib/format.js';

export default function StatusBar({ result, activeTab, cursor, compiling, running, optimize, theme, exePath }) {
  let state = { tone: 'idle', icon: CheckCircle, text: '就绪' };
  if (compiling) state = { tone: 'busy', icon: CircleNotch, text: '正在编译…' };
  else if (running) state = { tone: 'busy', icon: CircleNotch, text: '正在运行…' };
  else if (result && result.ok === false) state = { tone: 'error', icon: XCircle, text: '编译失败' };
  else if (result) state = { tone: 'ok', icon: CheckCircle, text: '编译成功' };
  const Icon = state.icon;
  const errors = result?.parsed?.diagnostics?.items?.length || 0;

  return (
    <footer className="statusbar">
      <div className="status-left">
        <span className={`status-item status-${state.tone}`}>
          <Icon size={13} className={state.tone === 'busy' ? 'spin' : ''} />
          {state.text}
        </span>
        {result?.durationMs != null ? <span className="status-item"><Clock size={12} /> {formatMs(result.durationMs)}</span> : null}
        {result?.parsed?.tokens?.rows?.length ? <span className="status-item">{result.parsed.tokens.rows.length} tokens</span> : null}
        {errors ? <span className="status-item status-error"><WarningCircle size={12} /> {errors} 诊断</span> : null}
        {optimize ? <span className="status-item status-chip">-O 优化开启</span> : null}
      </div>
      <div className="status-right">
        {exePath ? <span className="status-item status-path" title={exePath}><Cpu size={12} /> {exePath.split(/[\\/]/).pop()}</span> : null}
        <span className="status-item">{activeTab ? 'MiniC' : '—'}</span>
        <span className="status-item">行 {cursor?.line || 1}，列 {cursor?.column || 1}</span>
        <span className="status-item">UTF-8</span>
        <span className="status-item">{theme === 'light' ? '浅色主题' : '深色主题'}</span>
      </div>
    </footer>
  );
}
