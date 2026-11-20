import { useMemo, useState } from 'react';
import { Cpu, DownloadSimple, MagnifyingGlass } from '@phosphor-icons/react';
import EmptyState from '../components/EmptyState.jsx';

function tokenize(line) {
  const out = [];
  const re = /(%[a-z0-9]+)|(^\s*[A-Za-z_.$][\w.$]*:)|(\.[A-Za-z][\w.]*)|(^[ \t]*[a-z][\w.]*)|(-?(?:0x[0-9a-fA-F]+|\d+))|(#.*$)/gi;
  let last = 0;
  let m;
  while ((m = re.exec(line)) !== null) {
    if (m.index > last) out.push({ text: line.slice(last, m.index), kind: 'plain' });
    let kind = 'plain';
    if (m[1]) kind = 'reg';
    else if (m[2]) kind = 'label';
    else if (m[3]) kind = 'directive';
    else if (m[4]) kind = 'mnemonic';
    else if (m[5]) kind = 'number';
    else if (m[6]) kind = 'comment';
    out.push({ text: m[0], kind });
    last = m.index + m[0].length;
  }
  if (last < line.length) out.push({ text: line.slice(last), kind: 'plain' });
  return out;
}

export default function AsmPanel({ result, onExport }) {
  const asm = result?.parsed?.asm;
  const [query, setQuery] = useState('');

  const lines = asm?.lines || [];
  const filtered = useMemo(() => {
    const q = query.trim().toLowerCase();
    if (!q) return lines;
    return lines.filter((l) => l.raw.toLowerCase().includes(q));
  }, [lines, query]);

  if (!result) {
    return <EmptyState icon={Cpu} title="尚未生成目标代码" description="编译成功后，这里会展示 AT&T 语法的 x86-64 汇编与寄存器使用情况。" />;
  }

  const stats = asm?.stats || {};
  const regs = asm?.registerCounts || [];
  const maxReg = Math.max(1, ...regs.map((r) => r.count));

  return (
    <div className="panel asm-panel">
      <div className="panel-toolbar">
        <label className="search-box">
          <MagnifyingGlass size={14} />
          <input value={query} onChange={(e) => setQuery(e.target.value)} placeholder="搜索指令或寄存器…" />
        </label>
        <span className="panel-count">{lines.length} 行</span>
        <div className="panel-toolbar-spacer" />
        <button type="button" className="icon-btn" title="导出汇编" onClick={() => onExport('asm')}>
          <DownloadSimple size={17} />
        </button>
      </div>

      <div className="asm-stats">
        <div><span>指令</span><b>{stats.instructions}</b></div>
        <div><span>标号</span><b>{stats.labels}</b></div>
        <div><span>伪指令</span><b>{stats.directives}</b></div>
        <div><span>寄存器</span><b>{stats.registers}</b></div>
      </div>

      <div className="asm-layout">
        <div className="asm-code-scroll">
          <div className="asm-code">
            {filtered.map((l) => (
              <div className={`asm-line asm-${l.kind}`} key={l.id}>
                <span className="asm-line-no">{l.number}</span>
                <span className="asm-line-text mono">
                  {tokenize(l.raw).map((t, i) => <span key={i} className={`asm-tok-${t.kind}`}>{t.text}</span>)}
                </span>
              </div>
            ))}
            {filtered.length === 0 ? <div className="table-empty">没有匹配的行</div> : null}
          </div>
        </div>

        <aside className="asm-side">
          <div className="asm-side-title">寄存器使用频次</div>
          {regs.length ? (
            <div className="reg-list">
              {regs.slice(0, 16).map((r) => (
                <div className="reg-row" key={r.name}>
                  <span className="reg-name mono">%{r.name}</span>
                  <div className="reg-bar"><i style={{ width: `${Math.max(6, (r.count / maxReg) * 100)}%` }} /></div>
                  <span className="reg-count">{r.count}</span>
                </div>
              ))}
            </div>
          ) : <div className="blank-hint">无寄存器引用</div>}
          <div className="asm-note">
            <strong>GNU as</strong>
            <p>代码由 MiniC 直接生成 AT&amp;T 语法汇编，可经 gcc 汇编链接后运行。</p>
          </div>
        </aside>
      </div>
    </div>
  );
}
