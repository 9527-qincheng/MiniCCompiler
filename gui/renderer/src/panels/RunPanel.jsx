import { useEffect, useRef } from 'react';
import {
  CheckCircle, CircleNotch, Play, TerminalWindow, Trash, WarningCircle, XCircle,
} from '@phosphor-icons/react';
import { formatMs } from '../lib/format.js';

function statusOf(run) {
  if (!run) return { tone: 'idle', icon: TerminalWindow, title: '尚未运行', desc: '编译后会调用 gcc 汇编链接，并在沙箱中运行生成的可执行文件。' };
  if (run.running) return { tone: 'running', icon: CircleNotch, title: '正在运行…', desc: '等待程序结束（最多 5 秒）。' };
  if (run.stage && run.stage !== 'run') {
    return { tone: 'error', icon: XCircle, title: run.stage === 'compile' ? '编译失败' : '汇编链接失败', desc: run.message || '' };
  }
  if (run.timedOut) return { tone: 'warn', icon: WarningCircle, title: '运行超时', desc: '程序超过 5 秒仍未退出，已被终止。' };
  if (run.crashed) return { tone: 'error', icon: XCircle, title: `程序崩溃（${run.exitCode}）`, desc: '请检查除零、越界或非法内存访问。' };
  if (run.ok) return { tone: 'ok', icon: CheckCircle, title: '运行结束', desc: `进程正常退出，退出码 ${run.exitCode ?? 0}。` };
  return { tone: 'error', icon: XCircle, title: '运行失败', desc: run.message || '' };
}

export default function RunPanel({ run, running, onRun, onClear, onOpenDiagnostics }) {
  const outputRef = useRef(null);
  const state = statusOf(run || (running ? { running: true } : null));
  const Icon = state.icon;
  const stdout = run?.stdout || '';
  const stderr = run?.stderr || '';

  useEffect(() => {
    if (outputRef.current) outputRef.current.scrollTop = outputRef.current.scrollHeight;
  }, [stdout, stderr, running]);

  return (
    <div className="panel run-panel">
      <div className="panel-toolbar">
        <button type="button" className="tool-btn is-primary" onClick={onRun} disabled={running}>
          {running ? <CircleNotch size={15} className="spin" /> : <Play size={15} weight="fill" />}
          {running ? '运行中…' : '运行程序'}
        </button>
        <div className="panel-toolbar-spacer" />
        {run ? (
          <button type="button" className="tool-btn" onClick={onClear} disabled={running}>
            <Trash size={15} /> 清空
          </button>
        ) : null}
      </div>

      <div className="run-status">
        <div className={`run-status-icon tone-${state.tone}`}><Icon size={22} className={state.tone === 'running' ? 'spin' : ''} /></div>
        <div className="run-status-text">
          <strong>{state.title}</strong>
          <span>{state.desc}</span>
        </div>
        {run && !run.running ? (
          <div className="run-metrics">
            {run.compileMs != null ? <span>编译 <b>{formatMs(run.compileMs)}</b></span> : null}
            {run.linkMs != null ? <span>链接 <b>{formatMs(run.linkMs)}</b></span> : null}
            {run.runMs != null ? <span>运行 <b>{formatMs(run.runMs)}</b></span> : null}
          </div>
        ) : null}
        {state.tone === 'error' && run?.stage === 'compile' ? (
          <button type="button" className="tool-btn" onClick={onOpenDiagnostics}>查看诊断</button>
        ) : null}
      </div>

      <div className="terminal" ref={outputRef}>
        <div className="terminal-bar">
          <span className="dot dot-red" /><span className="dot dot-amber" /><span className="dot dot-green" />
          <span className="terminal-title">program output</span>
        </div>
        <pre className="terminal-body">
          {!stdout && !stderr && !run ? <span className="terminal-muted">$ 等待运行…</span> : null}
          {stdout ? <span className="term-out">{stdout}</span> : null}
          {stderr ? <span className="term-err">{stderr}</span> : null}
          {run && !run.running && !stdout && !stderr && run.ok ? <span className="terminal-muted">（程序没有输出）</span> : null}
          {run?.message ? <span className="term-err">{run.message}</span> : null}
        </pre>
      </div>

      <div className="run-hint">
        <TerminalWindow size={14} />
        标准输入暂未接入；程序中的 <code>scan</code> 会读到空输入。运行超时上限 5000 ms。
      </div>
    </div>
  );
}
