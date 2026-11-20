import { useMemo, useState } from 'react';
import { DownloadSimple, MagnifyingGlass, Path, Table } from '@phosphor-icons/react';
import Chart from '../components/Chart.jsx';
import EmptyState from '../components/EmptyState.jsx';
import { OP_CATEGORY_COLOR, OP_CATEGORY_LABEL } from '../lib/colors.js';

const EDGE_COLOR = { true: '#34d399', false: '#f5b86b', jump: '#22d3ee', next: '#3f556b' };

export default function QuadPanel({ result, theme, onJump, onExport }) {
  const quads = result?.parsed?.quads;
  const [view, setView] = useState('table');
  const [query, setQuery] = useState('');
  const [cat, setCat] = useState('ALL');

  const rows = quads?.rows || [];
  const blocks = quads?.blocks || [];

  const filtered = useMemo(() => {
    const q = query.trim().toLowerCase();
    return rows.filter((r) => {
      if (cat !== 'ALL' && r.category !== cat) return false;
      if (!q) return true;
      return `${r.op} ${r.args[0]} ${r.args[1]} ${r.result}`.toLowerCase().includes(q);
    });
  }, [rows, query, cat]);

  if (!result) {
    return <EmptyState icon={Path} title="尚未生成中间代码" description="编译成功后，这里会显示四元式序列、基本块划分与控制流图。" />;
  }

  const isDark = theme !== 'light';
  const graphOption = {
    backgroundColor: 'transparent',
    tooltip: {
      backgroundColor: isDark ? '#121a23' : '#ffffff',
      borderColor: isDark ? '#243242' : '#d9e2ec',
      textStyle: { color: isDark ? '#dbe7f3' : '#1e293b', fontSize: 12 },
      formatter: (p) => (p.dataType === 'edge' ? `${p.data.source} → ${p.data.target}（${p.data.kind}）` : p.name),
    },
    series: [{
      type: 'graph',
      layout: 'circular',
      circular: { rotateLabel: false },
      roam: true,
      symbol: 'roundRect',
      symbolSize: [86, 42],
      top: 20,
      bottom: 20,
      left: 30,
      right: 30,
      itemStyle: {
        color: isDark ? '#111b25' : '#f8fbfe',
        borderColor: isDark ? '#2c4155' : '#c8d6e3',
        borderWidth: 1.4,
      },
      label: {
        show: true,
        color: isDark ? '#dbe7f3' : '#1e293b',
        fontFamily: 'JetBrains Mono',
        fontSize: 10,
        lineHeight: 14,
        formatter: (p) => `{b|${p.data.label}}\n{s|${p.data.range}}`,
        rich: {
          b: { fontSize: 11, fontWeight: 700, color: isDark ? '#67e8f9' : '#0e7490', lineHeight: 16 },
          s: { fontSize: 9, color: isDark ? '#7d92a8' : '#64748b' },
        },
      },
      edgeSymbol: ['none', 'arrow'],
      edgeSymbolSize: 7,
      lineStyle: { color: isDark ? '#3f556b' : '#b6c6d6', width: 1.4, curveness: 0.14 },
      emphasis: { focus: 'adjacency', lineStyle: { width: 2.4 } },
      data: blocks.map((b) => ({
        id: b.id,
        name: b.id,
        label: b.id,
        range: b.start === b.end ? `#${b.start + 1}` : `#${b.start + 1}–${b.end + 1}`,
      })),
      links: (quads?.edges || []).map((e) => ({
        source: e.from,
        target: e.to,
        kind: e.kind,
        lineStyle: { color: EDGE_COLOR[e.kind] || '#3f556b', width: e.kind === 'next' ? 1.2 : 1.8, type: e.kind === 'false' ? 'dashed' : 'solid' },
        label: { show: true, formatter: e.kind, color: isDark ? '#6d8298' : '#94a3b8', fontSize: 9 },
      })),
    }],
  };

  return (
    <div className="panel quad-panel">
      <div className="panel-toolbar">
        <div className="segmented">
          <button type="button" className={view === 'table' ? 'is-active' : ''} onClick={() => setView('table')}>
            <Table size={14} /> 四元式
          </button>
          <button type="button" className={view === 'cfg' ? 'is-active' : ''} onClick={() => setView('cfg')}>
            <Path size={14} /> 控制流图
          </button>
        </div>
        {view === 'table' ? (
          <>
            <label className="search-box">
              <MagnifyingGlass size={14} />
              <input value={query} onChange={(e) => setQuery(e.target.value)} placeholder="搜索操作或操作数…" />
            </label>
            <select className="select-box" value={cat} onChange={(e) => setCat(e.target.value)}>
              <option value="ALL">全部操作</option>
              {(quads?.opCounts || []).map((o) => (
                <option key={o.category} value={o.category}>{OP_CATEGORY_LABEL[o.category] || o.category} ({o.count})</option>
              ))}
            </select>
          </>
        ) : null}
        <div className="panel-toolbar-spacer" />
        <span className="panel-count">{rows.length} 条 · {blocks.length} 块</span>
        <button type="button" className="icon-btn" title="导出四元式" onClick={() => onExport('quad')}>
          <DownloadSimple size={17} />
        </button>
      </div>

      {view === 'table' ? (
        <div className="panel-scroll">
          <div className="quad-stats">
            {(quads?.opCounts || []).map((o) => (
              <div className="quad-stat" key={o.category}>
                <span className="quad-dot" style={{ background: OP_CATEGORY_COLOR[o.category] }} />
                <span>{OP_CATEGORY_LABEL[o.category] || o.category}</span>
                <b>{o.count}</b>
              </div>
            ))}
          </div>
          <div className="table-card">
            <div className="data-table-wrap">
              <table className="data-table quad-table">
                <thead>
                  <tr>
                    <th className="col-index">#</th>
                    <th className="col-loc">块</th>
                    <th className="col-type">操作</th>
                    <th>参数 1</th>
                    <th>参数 2</th>
                    <th>结果</th>
                    <th className="col-loc">源码</th>
                  </tr>
                </thead>
                <tbody>
                  {filtered.slice(0, 3000).map((r) => {
                    const block = quads.blockOfRow(r.id);
                    return (
                      <tr key={r.id} className="row-click" onClick={() => onJump(r.line, r.col)}>
                        <td className="col-index">{r.index}</td>
                        <td className="col-loc mono block-tag">{block ? block.id : '—'}</td>
                        <td className="col-type">
                          <span className="op-badge" style={{ color: OP_CATEGORY_COLOR[r.category], borderColor: `${OP_CATEGORY_COLOR[r.category]}55` }}>{r.op}</span>
                        </td>
                        <td className="mono">{r.args[0]}</td>
                        <td className="mono">{r.args[1]}</td>
                        <td className="mono result-cell">{r.result}</td>
                        <td className="col-loc mono">{r.line}:{r.col}</td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            </div>
          </div>
        </div>
      ) : (
        <div className="panel-scroll">
          <div className="chart-card cfg-card">
            <div className="card-title">
              <span>控制流图</span>
              <em>拖拽/滚轮可缩放平移，虚线表示条件为假的路径</em>
            </div>
            <Chart option={graphOption} height={Math.max(300, Math.min(480, blocks.length * 62))} />
          </div>
          <div className="block-grid">
            {blocks.map((b) => (
              <section className="block-card" key={b.id}>
                <header>
                  <span className="block-id">{b.id}</span>
                  <span className="block-range">#{b.start + 1}–#{b.end + 1}</span>
                  {b.label ? <span className="block-label mono">{b.label}:</span> : null}
                </header>
                <div className="block-lines">
                  {b.rows.map((r) => (
                    <div key={r.id} className="block-line" onClick={() => onJump(r.line, r.col)}>
                      <span className="block-line-op" style={{ color: OP_CATEGORY_COLOR[r.category] }}>{r.op}</span>
                      <span className="mono block-line-args">{r.args.filter((a) => a !== '-').join(', ')}{r.result !== '-' ? ` → ${r.result}` : ''}</span>
                    </div>
                  ))}
                </div>
              </section>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}
