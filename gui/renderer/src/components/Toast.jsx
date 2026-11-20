import { CheckCircle, Info, WarningCircle, X } from '@phosphor-icons/react';

const ICONS = { success: CheckCircle, error: WarningCircle, info: Info };

export default function ToastStack({ toasts, onDismiss }) {
  return (
    <div className="toast-stack">
      {toasts.map((t) => {
        const Icon = ICONS[t.kind] || Info;
        return (
          <div key={t.id} className={`toast toast-${t.kind || 'info'}`}>
            <Icon size={18} weight="fill" />
            <span>{t.message}</span>
            <button type="button" className="toast-close" onClick={() => onDismiss(t.id)} aria-label="关闭提示">
              <X size={13} />
            </button>
          </div>
        );
      })}
    </div>
  );
}
