import { CheckCircle, Crosshair, WarningCircle, XCircle } from '@phosphor-icons/react';
import EmptyState from '../components/EmptyState.jsx';

export default function DiagnosticsPanel({ result, onJump }) {
  if (!result) {
    return <EmptyState icon={WarningCircle} title="尚无诊断信息" description="编译一次后，这里的错误与警告可以点击定位到源码。" />;
  }

  const diag = result.parsed?.diagnostics;
  const items = diag?.items || [];
  const errors = items.filter((d) => d.severity !== 'warning').length;
  const warnings = items.length - errors;

  if (items.length === 0) {
    return (
      <div className="panel diagnostics-panel">
        <div className="diag-ok">
          <CheckCircle size={44} weight="duotone" />
          <h3>编译通过，没有错误或警告</h3>
          <p>当前文件已成功生成 Token、语法树、符号表、四元式与汇编。</p>
        </div>
      </div>
    );
  }

  return (
    <div className="panel diagnostics-panel">
      <div className="panel-toolbar">
        <div className="diag-summary">
          <span className="diag-badge error"><XCircle size={14} /> {errors} 个错误</span>
          {warnings ? <span className="diag-badge warning"><WarningCircle size={14} /> {warnings} 个警告</span> : null}
        </div>
        <div className="panel-toolbar-spacer" />
        <span className="panel-count">{diag.summary || '编译失败'}</span>
      </div>

      <div className="panel-scroll">
        <div className="diag-list">
          {items.map((d) => (
            <article className="diag-item" key={d.id}>
              <div className="diag-item-head">
                <span className={`diag-severity ${d.severity}`}>
                  {d.severity === 'warning' ? <WarningCircle size={14} /> : <XCircle size={14} />}
                  {d.severity === 'warning' ? '警告' : '错误'}
                </span>
                {d.line > 0 ? (
                  <button type="button" className="diag-loc mono" onClick={() => onJump(d.line, d.col || 1)}>
                    {d.line}:{d.col} <Crosshair size={12} />
                  </button>
                ) : <span className="diag-loc">全局</span>}
                <span className="diag-message">{d.message}</span>
              </div>
              {d.context && d.context.length ? (
                <div className="diag-context">
                  {d.context.map((c) => (
                    <div key={c.line} className={`diag-code-line ${c.line === d.line ? 'is-target' : ''}`}>
                      <span className="diag-code-no">{c.line}</span>
                      <code>{c.text}</code>
                    </div>
                  ))}
                  {d.line > 0 ? (
                    <div className="diag-caret-line">
                      <span className="diag-code-no" />
                      <code>{' '.repeat(Math.max(0, (d.col || 1) - 1))}^</code>
                    </div>
                  ) : null}
                </div>
              ) : null}
            </article>
          ))}
        </div>
      </div>
    </div>
  );
}
