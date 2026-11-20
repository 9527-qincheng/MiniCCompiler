import { useMemo, useState } from 'react';
import { BookOpen, CheckCircle, Info, MagnifyingGlass, Play, WarningCircle, XCircle } from '@phosphor-icons/react';
import Modal from './Modal.jsx';
import { SYNTAX_BOOK } from '../data/syntax-book.js';

const STATUS = {
  ok: { label: '支持', icon: CheckCircle, tone: 'ok' },
  no: { label: '不支持', icon: XCircle, tone: 'no' },
  limit: { label: '有限制', icon: WarningCircle, tone: 'limit' },
  info: { label: '说明', icon: Info, tone: 'info' },
};

function EntryCard({ entry, onUse }) {
  const meta = STATUS[entry.status] || STATUS.info;
  const Icon = meta.icon;
  return (
    <article className={`book-entry book-entry--${meta.tone}`}>
      <header className="book-entry-head">
        <div>
          <span className={`book-status ${meta.tone}`}><Icon size={12} /> {meta.label}</span>
          <h4>{entry.title}</h4>
        </div>
        {entry.example ? (
          <button type="button" className="tool-btn book-try" onClick={() => onUse(entry.example, entry.title)}>
            <Play size={13} weight="fill" /> 试用
          </button>
        ) : null}
      </header>
      {entry.syntax ? (
        <div className="book-syntax">
          <span>语法</span>
          <code className="mono">{entry.syntax}</code>
        </div>
      ) : null}
      {entry.example ? (
        <pre className="book-code mono">{entry.example}</pre>
      ) : null}
      {entry.output ? (
        <div className="book-output">
          <span>运行输出</span>
          <pre className="mono">{entry.output}</pre>
        </div>
      ) : null}
      {entry.err ? <div className="book-err mono">{entry.err}</div> : null}
      {entry.notes && entry.notes.length ? (
        <ul className="book-notes">
          {entry.notes.map((n, i) => <li key={i}>{n}</li>)}
        </ul>
      ) : null}
    </article>
  );
}

export default function BookModal({ open, onClose, onUse }) {
  const [query, setQuery] = useState('');
  const [status, setStatus] = useState('all');
  const [chapterId, setChapterId] = useState(SYNTAX_BOOK[0]?.id);

  const chapter = SYNTAX_BOOK.find((c) => c.id === chapterId) || SYNTAX_BOOK[0];
  const total = useMemo(() => SYNTAX_BOOK.reduce((s, c) => s + c.entries.length, 0), []);

  const searching = query.trim().length > 0;
  const results = useMemo(() => {
    const q = query.trim().toLowerCase();
    const out = [];
    for (const c of SYNTAX_BOOK) {
      for (const e of c.entries) {
        if (status !== 'all' && e.status !== status) continue;
        if (q && !`${e.title} ${e.syntax || ''} ${(e.notes || []).join(' ')}`.toLowerCase().includes(q)) continue;
        out.push({ ...e, chapterTitle: c.title });
      }
    }
    return out;
  }, [query, status]);

  const entries = searching ? results : (chapter?.entries || []).filter((e) => status === 'all' || e.status === status);

  return (
    <Modal
      open={open}
      onClose={onClose}
      title="MiniC 语法手册"
      subtitle={`${SYNTAX_BOOK.length} 章 · ${total} 个条目，全部在真实编译器上实测过`}
      className="book-modal"
      width={1120}
    >
      <div className="book-layout">
        <aside className="book-side">
          <label className="search-box">
            <MagnifyingGlass size={14} />
            <input value={query} onChange={(e) => setQuery(e.target.value)} placeholder="搜索语法…" />
          </label>
          <div className="book-status-filter">
            {[['all', '全部'], ['ok', '支持'], ['no', '不支持'], ['limit', '限制'], ['info', '说明']].map(([k, l]) => (
              <button key={k} type="button" className={`chip chip-sm ${status === k ? 'is-active' : ''}`} onClick={() => setStatus(k)}>{l}</button>
            ))}
          </div>
          <div className="book-chapters">
            <div className="book-side-title"><BookOpen size={13} /> 章节</div>
            {SYNTAX_BOOK.map((c) => (
              <button
                key={c.id}
                type="button"
                className={`book-chapter ${!searching && c.id === chapter?.id ? 'is-active' : ''}`}
                onClick={() => { setQuery(''); setChapterId(c.id); }}
              >
                <span>{c.title}</span>
                <em>{c.entries.length}</em>
              </button>
            ))}
          </div>
        </aside>

        <div className="book-content">
          <div className="book-content-head">
            <h3>{searching ? `搜索结果` : chapter?.title}</h3>
            <span>{entries.length} 个条目</span>
          </div>
          <div className="book-entries">
            {entries.map((e) => <EntryCard key={`${e.chapterTitle || ''}-${e.id}`} entry={e} onUse={onUse} />)}
            {entries.length === 0 ? <div className="table-empty">没有匹配的条目</div> : null}
          </div>
        </div>
      </div>
    </Modal>
  );
}
