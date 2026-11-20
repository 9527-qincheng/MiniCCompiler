import { useMemo, useState } from 'react';
import { MagnifyingGlass, Sparkle, WarningCircle } from '@phosphor-icons/react';
import Modal from './Modal.jsx';
import { SAMPLES } from '../data/samples.js';

export default function SamplesModal({ open, onClose, onSelect }) {
  const [query, setQuery] = useState('');
  const [group, setGroup] = useState('全部');
  const groups = useMemo(() => ['全部', ...new Set(SAMPLES.map((s) => s.group))], []);
  const list = useMemo(() => {
    const q = query.trim().toLowerCase();
    return SAMPLES.filter((s) => (group === '全部' || s.group === group)
      && (!q || `${s.label} ${s.name} ${s.group}`.toLowerCase().includes(q)));
  }, [query, group]);

  return (
    <Modal
      open={open}
      onClose={onClose}
      title="示例程序库"
      subtitle={`${SAMPLES.length} 个经过真实工具链验证的 MiniC 程序，点击即可载入编辑器`}
      className="samples-modal"
      width={980}
    >
      <div className="modal-filterbar">
        <label className="search-box">
          <MagnifyingGlass size={14} />
          <input value={query} onChange={(e) => setQuery(e.target.value)} placeholder="搜索示例…" />
        </label>
        <div className="filter-chips">
          {groups.map((g) => (
            <button key={g} type="button" className={`chip ${group === g ? 'is-active' : ''}`} onClick={() => setGroup(g)}>
              {g}
            </button>
          ))}
        </div>
      </div>

      <div className="sample-grid">
        {list.map((s) => (
          <button type="button" className="sample-card" key={s.id} onClick={() => onSelect(s)}>
            <div className="sample-card-top">
              <span className="sample-group">{s.group}</span>
              {s.expect === 'error' ? <span className="sample-err"><WarningCircle size={12} /> 预期报错</span> : null}
            </div>
            <strong>{s.label}</strong>
            <span className="sample-file mono">{s.name}</span>
            <div className="sample-foot">
              {s.expect === 'error'
                ? <span className="sample-output err">{(s.err || '').split('\n')[0].slice(0, 54)}…</span>
                : <span className="sample-output">输出：{(s.output || '').replace(/\n/g, '⏎').slice(0, 42)}</span>}
              <Sparkle size={14} />
            </div>
          </button>
        ))}
      </div>
      {list.length === 0 ? <div className="table-empty">没有匹配的示例</div> : null}
    </Modal>
  );
}
