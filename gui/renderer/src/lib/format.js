export function formatMs(ms) {
  if (ms == null || Number.isNaN(Number(ms))) return '—';
  const n = Number(ms);
  if (n < 1) return '<1 ms';
  if (n < 1000) return `${Math.round(n)} ms`;
  return `${(n / 1000).toFixed(2)} s`;
}

export function formatBytes(n) {
  if (n == null) return '—';
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / 1024 / 1024).toFixed(2)} MB`;
}

export function cx(...parts) {
  return parts.filter(Boolean).join(' ');
}

export function lineCount(text) {
  if (!text) return 0;
  return String(text).split('\n').length;
}

export function truncate(text, n = 80) {
  const s = String(text == null ? '' : text);
  return s.length > n ? `${s.slice(0, n - 1)}…` : s;
}

export function escapeRegExp(s) {
  return String(s).replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

export function downloadName(base, ext) {
  return `${String(base || 'artifact').replace(/\.mc$/i, '')}${ext}`;
}
