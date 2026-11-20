import { useEffect, useState } from 'react';
import { Code, CornersOut, Minus, Square, X } from '@phosphor-icons/react';

export default function TitleBar({ activeTab }) {
  const [maximized, setMaximized] = useState(false);
  const api = window.minicAPI;

  useEffect(() => {
    let alive = true;
    const sync = async () => {
      if (!api?.isMaximized) return;
      const v = await api.isMaximized();
      if (alive) setMaximized(Boolean(v));
    };
    sync();
    const timer = setInterval(sync, 1200);
    return () => { alive = false; clearInterval(timer); };
  }, [api]);

  const toggleMax = async () => {
    const v = await api?.maximize?.();
    setMaximized(Boolean(v));
  };

  return (
    <header className="titlebar" onDoubleClick={toggleMax}>
      <div className="titlebar-brand">
        <div className="brand-mark">
          <Code size={17} weight="bold" />
        </div>
        <div className="brand-text">
          <strong>MiniC <span>Studio</span></strong>
          <em>编译原理可视化工作台</em>
        </div>
      </div>

      <div className="titlebar-center">
        <span className="title-file">{activeTab ? activeTab.name : '未打开文件'}</span>
        {activeTab?.dirty ? <span className="title-dirty" title="有未保存的修改" /> : null}
      </div>

      <div className="titlebar-actions" onDoubleClick={(e) => e.stopPropagation()}>
        <button type="button" className="window-btn" title="最小化" onClick={() => api?.minimize?.()}>
          <Minus size={15} />
        </button>
        <button type="button" className="window-btn" title={maximized ? '还原' : '最大化'} onClick={toggleMax}>
          {maximized ? <CornersOut size={14} /> : <Square size={13} />}
        </button>
        <button type="button" className="window-btn window-btn-close" title="关闭" onClick={() => api?.close?.()}>
          <X size={16} />
        </button>
      </div>
    </header>
  );
}
