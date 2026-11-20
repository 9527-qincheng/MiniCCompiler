import { useMemo, useState } from 'react';
import { DownloadSimple, Hash, MagnifyingGlass, Rows, Shapes, Timer } from '@phosphor-icons/react';
import Chart from '../components/Chart.jsx';
import EmptyState from '../components/EmptyState.jsx';
import { tokenKind } from '../lib/colors.js';
import { formatMs } from '../lib/format.js';

const PALETTE_DARK = ['#22d3ee', '#c4a7f7', '#f5b86b', '#60a5fa', '#9ae6a4', '#f472b6', '#7dd3fc', '#facc15'];
const PALETTE_LIGHT = ['#0891b2', '#7e22ce', '#b45309', '#2563eb', '#15803d', '#be185d', '#0369a1', '#a16207'];

export default function TokenPanel({ result, theme, onJump, onExport }) {
  const [query, setQuery] = useState('');
  const [typeFilter, setTypeFilter] = useState('ALL');
  const data = result?.parsed?.tokens;
  const rows = data?.rows || [];

  const filtered = useMemo(() => {
    const q = query.trim().toLowerCase();
    return rows.filter((r) => {
      if (typeFilter !== 'ALL' && r.type !== typeFilter) return false;
      if (!q) return true;
      return r.lexeme.toLowerCase().includes(q) || r.type.toLowerCase().includes(q);
    });
  }, [rows, query, typeFilter]);

  if (!result) {
    return <EmptyState icon={Hash} title="尚未生成 Token" description="编译当前文件后，这里会按行列展示词法分析结果。" />;
  }

  const palette = theme === 'light' ? PALETTE_LIGHT : PALETTE_DARK;
  const isDark = theme !== 'light';
  const top = (data.typeCounts || []).slice(0, 8);
  const rest = (data.typeCounts || []).slice(8).reduce((s, x) => s + x.count, 0);
  const pieData = top.map((t) => ({ name: t.type, value: t.count }));
  if (rest > 0) pieData.push({ name: '其他', value: rest });

  const pieOption = {
    backgroundColor: 'transparent',
    color: palette,
    tooltip: {
      trigger: 'item',
      backgroundColor: isDark ? '#121a23' : '#ffffff',
      borderColor: isDark ? '#243242' : '#d9e2ec',
      textStyle: { color: isDark ? '#dbe7f3' : '#1e293b', fontSize: 12 },
      formatter: '{b}<br/>{c} 个 · {d}%',
    },
    legend: {
      type: 'scroll',
      orient: 'vertical',
      right: 6,
      top: 8,
      bottom: 8,
      itemWidth: 8,
      itemHeight: 8,
      icon: 'circle',
      textStyle: { color: isDark ? '#93a4b8' : '#64748b', fontSize: 11, fontFamily: 'JetBrains Mono' },
    },
    series: [{
      type: 'pie',
      radius: ['52%', '76%'],
      center: ['30%', '50%'],
      avoidLabelOverlap: true,
      itemStyle: { borderColor: isDark ? '#0d131a' : '#ffffff', borderWidth: 3 },
      label: { show: false },
      labelLine: { show: false },
      emphasis: { scale: true, scaleSize: 4 },
      data: pieData,
    }],
    graphic: [{
      type: 'text',
      left: '22%',
      top: '43%',
      style: {
        text: String(rows.length),
        fill: isDark ? '#e6edf3' : '#1e293b',
        fontSize: 22,
        fontWeight: 600,
        fontFamily: 'JetBrains Mono',
        textAlign: 'center',
      },
    }, {
      type: 'text',
      left: '23%',
      top: '56%',
      style: {
        text: 'TOKENS',
        fill: isDark ? '#5c7085' : '#94a3b8',
        fontSize: 9,
        fontFamily: 'IBM Plex Sans',
        textAlign: 'center',
      },
    }],
  };

  return (
    <div className="panel token-panel">
      <div className="panel-toolbar">
        <label className="search-box">
          <MagnifyingGlass size={14} />
          <input value={query} onChange={(e) => setQuery(e.target.value)} placeholder="搜索词素或类型…" />
        </label>
        <select className="select-box" value={typeFilter} onChange={(e) => setTypeFilter(e.target.value)}>
          <option value="ALL">全部类型</option>
          {(data.typeCounts || []).map((t) => <option key={t.type} value={t.type}>{t.type}</option>)}
        </select>
        <span className="panel-count">{filtered.length} / {rows.length}</span>
        <div className="panel-toolbar-spacer" />
        <button type="button" className="icon-btn" title="导出 Token 表" onClick={() => onExport('token')}>
          <DownloadSimple size={17} />
        </button>
      </div>

      <div className="panel-scroll">
        <div className="stat-grid">
          <div className="stat-card">
            <div className="stat-icon tone-cyan"><Hash size={17} /></div>
            <div><strong>{rows.length}</strong><span>Token 总数</span></div>
          </div>
          <div className="stat-card">
            <div className="stat-icon tone-violet"><Shapes size={17} /></div>
            <div><strong>{data.typeCounts?.length || 0}</strong><span>记号类型</span></div>
          </div>
          <div className="stat-card">
            <div className="stat-icon tone-amber"><Rows size={17} /></div>
            <div><strong>{data.maxLine || 0}</strong><span>覆盖行数</span></div>
          </div>
          <div className="stat-card">
            <div className="stat-icon tone-green"><Timer size={17} /></div>
            <div><strong>{formatMs(result.durationMs)}</strong><span>编译耗时</span></div>
          </div>
        </div>

        <div className="chart-card">
          <div className="card-title">
            <span>Token 类型分布</span>
            <em>按出现次数排序</em>
          </div>
          <Chart option={pieOption} height={206} />
        </div>

        <div className="table-card">
          <div className="card-title">
            <span>记号流</span>
            <em>点击任意行跳转到源码位置</em>
          </div>
          <div className="data-table-wrap">
            <table className="data-table token-table">
              <thead>
                <tr>
                  <th className="col-index">#</th>
                  <th className="col-loc">位置</th>
                  <th className="col-type">类型</th>
                  <th>词素</th>
                </tr>
              </thead>
              <tbody>
                {filtered.slice(0, 2000).map((r) => (
                  <tr key={r.id} onClick={() => onJump(r.line, r.col)} className="row-click">
                    <td className="col-index">{r.id + 1}</td>
                    <td className="col-loc mono">{r.line}:{r.col}</td>
                    <td className="col-type">
                      <span className={`token-badge token-${tokenKind(r.type)}`}>{r.type}</span>
                    </td>
                    <td className="mono lexeme-cell">{r.lexeme || '␄'}</td>
                  </tr>
                ))}
              </tbody>
            </table>
            {filtered.length === 0 ? <div className="table-empty">没有匹配的记号</div> : null}
            {filtered.length > 2000 ? <div className="table-more">仅显示前 2000 条（共 {filtered.length} 条）</div> : null}
          </div>
        </div>
      </div>
    </div>
  );
}
