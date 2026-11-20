import { useEffect, useMemo, useState } from 'react';
import { DownloadSimple, MagnifyingGlass, Stack, Tag } from '@phosphor-icons/react';
import EmptyState from '../components/EmptyState.jsx';

const CATEGORIES = [
  { key: 'variables', label: '变量', tone: 'cyan' },
  { key: 'constants', label: '常量', tone: 'amber' },
  { key: 'types', label: '类型', tone: 'violet' },
  { key: 'procedures', label: '过程', tone: 'green', challenge: true },
  { key: 'params', label: '参数', tone: 'blue', challenge: true },
  { key: 'labels', label: '标签', tone: 'pink', challenge: true },
];

export default function SymbolPanel({ result, onJump, onExport }) {
  const symbols = result?.parsed?.symbols;
  const [query, setQuery] = useState('');
  const [activeScope, setActiveScope] = useState(0);

  useEffect(() => { setActiveScope(0); }, [result?.stamp]);

  const scopes = symbols?.scopes || [];
  const scope = scopes[Math.min(activeScope, Math.max(0, scopes.length - 1))];

  const filtered = useMemo(() => {
    if (!scope) return {};
    const q = query.trim().toLowerCase();
    const out = {};
    for (const c of CATEGORIES) {
      const list = scope.categories[c.key] || [];
      out[c.key] = q ? list.filter((e) => `${e.name} ${e.type} ${e.detail}`.toLowerCase().includes(q)) : list;
    }
    return out;
  }, [scope, query]);

  if (!result) {
    return <EmptyState icon={Stack} title="尚未生成符号表" description="编译成功后，这里会按作用域展示变量、常量、类型、过程与标号。" />;
  }

  const totals = symbols?.totals || {};
  const totalCount = Object.values(totals).reduce((s, n) => s + n, 0);

  return (
    <div className="panel symbol-panel">
      <div className="panel-toolbar">
        <label className="search-box">
          <MagnifyingGlass size={14} />
          <input value={query} onChange={(e) => setQuery(e.target.value)} placeholder="搜索符号…" />
        </label>
        <div className="panel-toolbar-spacer" />
        <span className="panel-count">{totalCount} 个符号 · {scopes.length} 个作用域</span>
        <button type="button" className="icon-btn" title="导出符号表" onClick={() => onExport('sym')}>
          <DownloadSimple size={17} />
        </button>
      </div>

      {scopes.length === 0 ? (
        <div className="panel-scroll"><div className="blank-hint">{symbols?.raw || '符号表为空'}</div></div>
      ) : (
        <div className="symbol-layout">
          <aside className="scope-rail">
            <div className="scope-rail-title">作用域链</div>
            {scopes.map((s, i) => {
              const count = Object.values(s.categories).reduce((n, arr) => n + arr.length, 0);
              return (
                <button
                  key={s.scope}
                  type="button"
                  className={`scope-item ${i === activeScope ? 'is-active' : ''}`}
                  onClick={() => setActiveScope(i)}
                >
                  <span className="scope-index">S{s.scope}</span>
                  <span className="scope-name">{s.scope === 1 ? '全局作用域' : `作用域 #${s.scope}`}</span>
                  <span className="scope-count">{count}</span>
                </button>
              );
            })}
          </aside>

          <div className="scope-content">
            <div className="scope-head">
              <div>
                <h3>{scope.scope === 1 ? '全局作用域' : `作用域 #${scope.scope}`}</h3>
                <p>符号按类别归档，点击行可跳转到定义位置。</p>
              </div>
              <span className="scope-total">{Object.values(scope.categories).reduce((n, arr) => n + arr.length, 0)} 项</span>
            </div>

            <div className="symbol-cards">
              {CATEGORIES.map((c) => {
                const list = filtered[c.key] || [];
                const all = scope.categories[c.key] || [];
                if (!all.length && !query) return null;
                return (
                  <section className="symbol-card" key={c.key}>
                    <header>
                      <span className={`cat-dot tone-${c.tone}`} />
                      <strong>{c.label}</strong>
                      {c.challenge ? <em>挑战</em> : null}
                      <span className="cat-count">{list.length}</span>
                    </header>
                    {list.length ? (
                      <div className="symbol-rows">
                        {list.map((e, i) => (
                          <div
                            key={`${e.name}-${i}`}
                            className={`symbol-row ${e.line > 0 ? 'row-click' : ''}`}
                            onClick={() => { if (e.line > 0) onJump(e.line, 1); }}
                          >
                            <span className="sym-name mono">{e.name}</span>
                            <span className="sym-type">{e.type || '—'}</span>
                            <span className="sym-detail">{e.detail}</span>
                            <span className="sym-line mono">{e.line > 0 ? `L${e.line}` : ''}</span>
                          </div>
                        ))}
                      </div>
                    ) : <div className="symbol-empty">无匹配项</div>}
                  </section>
                );
              })}
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
