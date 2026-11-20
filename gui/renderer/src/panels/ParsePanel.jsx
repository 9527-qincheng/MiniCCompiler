import { useEffect, useRef, useState } from 'react';
import {
  ArrowRight, CaretLeft, CaretRight, Pause, Play, Stack,
} from '@phosphor-icons/react';
import EmptyState from '../components/EmptyState.jsx';
import { formatBytes } from '../lib/format.js';

export default function ParsePanel({ result, enabled, onEnable }) {
  const trace = result?.parsed?.parseTrace;
  const steps = trace?.steps || [];
  const [index, setIndex] = useState(0);
  const [playing, setPlaying] = useState(false);
  const rowRef = useRef(null);

  useEffect(() => { setIndex(0); setPlaying(false); }, [result?.stamp]);
  useEffect(() => {
    if (!playing) return undefined;
    const timer = setInterval(() => {
      setIndex((i) => {
        if (i >= steps.length - 1) { setPlaying(false); return i; }
        return i + 1;
      });
    }, 420);
    return () => clearInterval(timer);
  }, [playing, steps.length]);
  useEffect(() => {
    rowRef.current?.scrollIntoView({ block: 'center', behavior: 'smooth' });
  }, [index]);

  if (!enabled) {
    return (
      <div className="panel parse-panel">
        <EmptyState
          icon={Stack}
          title="LL(1) 分析轨迹未开启"
          description="开启后会额外生成 .parse.txt（仅建议用于较小的文件，轨迹体积随语句数增长）。"
          action={<button type="button" className="tool-btn is-primary" onClick={onEnable}>开启并重新编译</button>}
        />
      </div>
    );
  }

  if (!result) {
    return <EmptyState icon={Stack} title="尚未生成分析轨迹" description="编译一次后，这里会逐步展示预测分析表的执行过程。" />;
  }

  if (!steps.length) {
    return <EmptyState icon={Stack} title="没有分析轨迹" description={trace?.rawLength ? '轨迹为空或格式无法识别。' : '当前编译未生成 .parse.txt，请开启分析轨迹后重试。'} />;
  }

  const cur = steps[Math.min(index, steps.length - 1)] || steps[0];

  return (
    <div className="panel parse-panel">
      <div className="panel-toolbar">
        <button type="button" className="icon-btn" onClick={() => setIndex((i) => Math.max(0, i - 1))} title="上一步">
          <CaretLeft size={16} />
        </button>
        <button type="button" className="icon-btn primary-soft" onClick={() => setPlaying((p) => !p)} title={playing ? '暂停' : '播放'}>
          {playing ? <Pause size={16} weight="fill" /> : <Play size={16} weight="fill" />}
        </button>
        <button type="button" className="icon-btn" onClick={() => setIndex((i) => Math.min(steps.length - 1, i + 1))} title="下一步">
          <CaretRight size={16} />
        </button>
        <span className="step-counter mono">{index + 1} / {steps.length}</span>
        <input
          className="step-slider"
          type="range"
          min={0}
          max={Math.max(0, steps.length - 1)}
          value={index}
          onChange={(e) => { setPlaying(false); setIndex(Number(e.target.value)); }}
        />
        <div className="panel-toolbar-spacer" />
        <span className="panel-count">{trace.productions || 0} 条产生式 · {formatBytes(trace.rawLength)}</span>
      </div>

      <div className="parse-state">
        <div className="parse-state-card">
          <span className="state-label">符号栈</span>
          <code title={cur.stack || ''}>{cur.stack || '—'}</code>
        </div>
        <div className="parse-state-card">
          <span className="state-label">剩余输入串</span>
          <code title={cur.input || ''}>{cur.input || '—'}</code>
        </div>
        <div className="parse-state-card action-card">
          <span className="state-label">动作</span>
          <code title={cur.action || ''}>{cur.action || '—'}</code>
        </div>
      </div>

      <div className="parse-table-wrap">
        <table className="data-table parse-table">
          <thead>
            <tr><th className="col-index">步骤</th><th>符号栈</th><th>剩余输入串</th><th>动作</th></tr>
          </thead>
          <tbody>
            {steps.map((s, i) => (
              <tr
                key={s.id}
                ref={i === index ? rowRef : undefined}
                className={i === index ? 'is-current' : ''}
                onClick={() => { setPlaying(false); setIndex(i); }}
              >
                <td className="col-index">{s.step}</td>
                <td className="mono" title={s.stack}>{s.stack}</td>
                <td className="mono" title={s.input}>{s.input}</td>
                <td title={s.action}>{s.action}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <div className="parse-footer">
        <ArrowRight size={14} /> 点击任意行可跳到该步骤；轨迹展示 LL(1) 预测分析表的栈、输入与动作。
        {trace.result ? <b>{trace.result}</b> : null}
      </div>
    </div>
  );
}
