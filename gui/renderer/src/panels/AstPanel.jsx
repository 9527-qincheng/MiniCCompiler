import { useMemo, useState } from 'react';
import {
  ArrowsInSimple, ArrowsOutSimple, CaretRight, Crosshair, DownloadSimple,
  MagnifyingGlass, TreeStructure,
} from '@phosphor-icons/react';
import EmptyState from '../components/EmptyState.jsx';

function collectIds(nodes, acc = []) {
  for (const n of nodes) {
    acc.push(n.id);
    collectIds(n.children, acc);
  }
  return acc;
}

function filterTree(nodes, q) {
  return nodes.map((n) => {
    const kids = filterTree(n.children || [], q);
    if (n.label.toLowerCase().includes(q) || kids.length) {
      return { ...n, children: kids, forced: true };
    }
    return null;
  }).filter(Boolean);
}

function AstNode({ node, depth, collapsed, toggle, selectedId, onSelect, onJump, query }) {
  const hasChildren = node.children && node.children.length > 0;
  const open = query ? true : !collapsed.has(node.id);
  const isSelected = selectedId === node.id;
  return (
    <div className="ast-node">
      <div
        className={`ast-row ${isSelected ? 'is-selected' : ''}`}
        style={{ paddingLeft: 8 + depth * 16 }}
        onClick={() => onSelect(node)}
        onDoubleClick={() => onJump(node.line, node.col)}
      >
        <button
          type="button"
          className={`ast-caret ${hasChildren ? '' : 'is-empty'} ${open ? 'is-open' : ''}`}
          onClick={(e) => { e.stopPropagation(); if (hasChildren) toggle(node.id); }}
        >
          <CaretRight size={11} weight="bold" />
        </button>
        <span className={`ast-kind ast-kind--${node.name?.toLowerCase() || 'other'}`}>{node.name}</span>
        <span className="ast-label" title={node.label}>{node.label}</span>
        <span className="ast-loc">{node.line}:{node.col}</span>
      </div>
      {hasChildren && open ? (
        <div className="ast-children">
          {node.children.map((child) => (
            <AstNode
              key={child.id}
              node={child}
              depth={depth + 1}
              collapsed={collapsed}
              toggle={toggle}
              selectedId={selectedId}
              onSelect={onSelect}
              onJump={onJump}
              query={query}
            />
          ))}
        </div>
      ) : null}
    </div>
  );
}

export default function AstPanel({ result, onJump, onExport }) {
  const ast = result?.parsed?.ast;
  const [query, setQuery] = useState('');
  const [collapsed, setCollapsed] = useState(() => new Set());
  const [selected, setSelected] = useState(null);

  const viewRoots = useMemo(() => {
    if (!ast) return [];
    const q = query.trim().toLowerCase();
    return q ? filterTree(ast.roots, q) : ast.roots;
  }, [ast, query]);

  if (!result) {
    return <EmptyState icon={TreeStructure} title="尚未生成语法树" description="编译成功后，这里会显示可折叠的抽象语法树，双击节点可跳回源码。" />;
  }

  if (!ast || ast.count === 0) {
    return <EmptyState icon={TreeStructure} title="没有生成 AST" description="当前编译可能在中途失败，请先修复词法或语法错误。" />;
  }

  const expandAll = () => setCollapsed(new Set());
  const collapseAll = () => setCollapsed(new Set(collectIds(ast.roots)));
  const toggle = (id) => setCollapsed((prev) => {
    const next = new Set(prev);
    if (next.has(id)) next.delete(id); else next.add(id);
    return next;
  });

  const node = selected || viewRoots[0] || null;
  const parent = node?.parentId != null ? ast.flat[node.parentId] : null;
  const matched = query ? ast.flat.filter((n) => n.label.toLowerCase().includes(query.trim().toLowerCase())).length : 0;

  return (
    <div className="panel ast-panel">
      <div className="panel-toolbar">
        <label className="search-box">
          <MagnifyingGlass size={14} />
          <input value={query} onChange={(e) => setQuery(e.target.value)} placeholder="搜索节点…" />
        </label>
        {query ? <span className="panel-count">{matched} 个匹配</span> : null}
        <div className="panel-toolbar-spacer" />
        <button type="button" className="tool-btn" onClick={expandAll} title="全部展开">
          <ArrowsOutSimple size={15} /> 展开
        </button>
        <button type="button" className="tool-btn" onClick={collapseAll} title="全部折叠">
          <ArrowsInSimple size={15} /> 折叠
        </button>
        <button type="button" className="icon-btn" title="导出 AST" onClick={() => onExport('ast')}>
          <DownloadSimple size={17} />
        </button>
      </div>

      <div className="ast-summary">
        <div className="summary-pill"><TreeStructure size={13} /> {ast.count} 个节点</div>
        <div className="summary-pill">最大深度 <b>{ast.maxDepth + 1}</b></div>
        <div className="summary-pill">根节点 <b>{ast.roots.length}</b></div>
        <span className="summary-hint">单击选中 · 双击跳转源码</span>
      </div>

      <div className="ast-layout">
        <div className="ast-tree-scroll">
          {viewRoots.map((root) => (
            <AstNode
              key={root.id}
              node={root}
              depth={0}
              collapsed={collapsed}
              toggle={toggle}
              selectedId={node?.id}
              onSelect={setSelected}
              onJump={onJump}
              query={query}
            />
          ))}
        </div>

        <aside className="ast-detail">
          {node ? (
            <>
              <div className="detail-head">
                <span className={`ast-kind ast-kind--${node.name?.toLowerCase() || 'other'}`}>{node.name}</span>
                <strong>{node.zh}</strong>
              </div>
              <div className="detail-label mono">{node.label}</div>
              <dl className="detail-list">
                <div><dt>源码位置</dt><dd className="mono">{node.line}:{node.col}</dd></div>
                <div><dt>层级</dt><dd>{node.depth + 1}</dd></div>
                <div><dt>子节点</dt><dd>{node.children.length}</dd></div>
                <div><dt>父节点</dt><dd>{parent ? parent.label : '—'}</dd></div>
              </dl>
              <button type="button" className="detail-jump" onClick={() => onJump(node.line, node.col)}>
                <Crosshair size={14} /> 在源码中定位
              </button>
            </>
          ) : (
            <div className="detail-placeholder">选择一个节点查看详情</div>
          )}
        </aside>
      </div>
    </div>
  );
}
