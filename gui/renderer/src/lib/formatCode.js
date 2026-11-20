/**
 * MiniC 轻量格式化：只重排空白与缩进，不改变任何记号、字符串和注释内容。
 * 规则来自编译器项目原有 GUI，经过回归测试，保持幂等。
 */

const BINARY_OPS = new Set(['=', '==', '!=', '<=', '>=', '<', '>', '&&', '||']);
const KEYWORD_BEFORE_PAREN = new Set(['if', 'while', 'return']);

function tokenizeLine(line, markBlockComment) {
  const toks = [];
  let i = 0;
  while (i < line.length) {
    const c = line[i];
    if (/\s/.test(c)) { i++; continue; }
    if (c === '/' && line[i + 1] === '/') { toks.push({ t: 'comment', v: line.slice(i) }); break; }
    if (c === '/' && line[i + 1] === '*') {
      const end = line.indexOf('*/', i + 2);
      if (end === -1) { toks.push({ t: 'comment', v: line.slice(i) }); markBlockComment(true); break; }
      toks.push({ t: 'comment', v: line.slice(i, end + 2) });
      i = end + 2;
      continue;
    }
    if (c === '"' || c === "'") {
      let j = i + 1;
      while (j < line.length && line[j] !== c) { if (line[j] === '\\') j++; j++; }
      j = Math.min(line.length, j + 1);
      toks.push({ t: 'lit', v: line.slice(i, j) });
      i = j;
      continue;
    }
    if (/[A-Za-z_]/.test(c)) {
      const m = line.slice(i).match(/^[A-Za-z_][A-Za-z0-9_]*/);
      toks.push({ t: 'id', v: m[0] });
      i += m[0].length;
      continue;
    }
    if (/[0-9]/.test(c)) {
      const m = line.slice(i).match(/^[0-9]+(\.[0-9]+)?/);
      toks.push({ t: 'num', v: m[0] });
      i += m[0].length;
      continue;
    }
    const two = line.slice(i, i + 2);
    if (['==', '!=', '<=', '>=', '&&', '||'].includes(two)) {
      toks.push({ t: 'op', v: two });
      i += 2;
      continue;
    }
    if ('+-*/=<>!'.includes(c)) { toks.push({ t: 'op', v: c }); i++; continue; }
    if ('(){}[],;'.includes(c)) { toks.push({ t: 'p', v: c }); i++; continue; }
    toks.push({ t: 'other', v: c });
    i++;
  }
  return toks;
}

function joinTokens(toks) {
  let out = '';
  let pendingSpace = false;
  const isBinary = (tok, prev) => {
    if (BINARY_OPS.has(tok.v)) return true;
    if (['+', '-', '*', '/'].includes(tok.v)) {
      if (!prev) return false;
      return prev.t === 'id' || prev.t === 'num' || prev.t === 'lit' || (prev.t === 'p' && (prev.v === ')' || prev.v === ']'));
    }
    return false;
  };
  toks.forEach((tok, idx) => {
    const prev = idx ? toks[idx - 1] : null;
    if (tok.t === 'comment') {
      if (out && !out.endsWith(' ')) out += ' ';
      out += tok.v;
      pendingSpace = false;
      return;
    }
    if (tok.t === 'p') {
      if (tok.v === '(') {
        if (prev && prev.t === 'id' && KEYWORD_BEFORE_PAREN.has(prev.v) && !out.endsWith(' ')) out += ' ';
        out += '(';
        pendingSpace = false;
        return;
      }
      if (tok.v === '[') { out += '['; pendingSpace = false; return; }
      if (tok.v === ')' || tok.v === ']' || tok.v === ',' || tok.v === ';') {
        if (out.endsWith(' ')) out = out.replace(/ $/, '');
        out += tok.v;
        pendingSpace = tok.v === ',';
        return;
      }
      if (tok.v === '{') {
        if (out && !out.endsWith(' ') && !out.endsWith('(') && !out.endsWith('[')) out += ' ';
        out += '{';
        pendingSpace = true;
        return;
      }
      if (tok.v === '}') {
        if (out.endsWith(' ')) out = out.replace(/ $/, '');
        if (out.endsWith(';')) out += ' ';
        out += '}';
        pendingSpace = true;
        return;
      }
    }
    if (tok.t === 'op') {
      if (isBinary(tok, prev)) {
        if (out && !out.endsWith(' ')) out += ' ';
        out += tok.v;
        pendingSpace = true;
        return;
      }
      out += tok.v;
      pendingSpace = false;
      return;
    }
    const wordLike = (t) => t && (t.t === 'id' || t.t === 'num' || t.t === 'lit');
    const needSpace = wordLike(prev) && wordLike(tok);
    if ((pendingSpace || needSpace) && out && !out.endsWith(' ')) out += ' ';
    out += tok.v;
    pendingSpace = false;
  });
  return out.replace(/\s+$/, '');
}

export function formatMiniC(src) {
  const lines = String(src || '').replace(/\r\n?/g, '\n').split('\n');
  const out = [];
  let depth = 0;
  let inBlock = false;
  let blanks = 0;

  for (const raw of lines) {
    const line = raw.replace(/[ \t]+$/, '');
    if (inBlock) {
      out.push('    '.repeat(depth) + line.trim());
      if (line.includes('*/')) inBlock = false;
      continue;
    }
    if (!line.trim()) {
      blanks++;
      if (blanks <= 1 && out.length && out[out.length - 1] !== '') out.push('');
      continue;
    }
    blanks = 0;
    const toks = tokenizeLine(line.trim(), (v) => { inBlock = v; });
    const content = joinTokens(toks);
    const dedent = content.startsWith('}') ? 1 : 0;
    out.push('    '.repeat(Math.max(0, depth - dedent)) + content);
    let opens = 0;
    let closes = 0;
    for (const t of toks) {
      if (t.t === 'p' && t.v === '{') opens++;
      if (t.t === 'p' && t.v === '}') closes++;
    }
    depth = Math.max(0, depth + opens - closes);
  }
  while (out.length && out[out.length - 1] === '') out.pop();
  return out.join('\n') + '\n';
}

export function normalizeLine(lineText) {
  const indent = (lineText.match(/^[ \t]*/) || [''])[0];
  const body = lineText.slice(indent.length);
  if (!body.trim()) return lineText;
  const t = body.trim();
  if (t.startsWith('//') || t.startsWith('/*') || t.startsWith('*')) return lineText;
  let openedBlock = false;
  const toks = tokenizeLine(t, (v) => { openedBlock = v; });
  if (openedBlock) return lineText;
  return indent + joinTokens(toks);
}
