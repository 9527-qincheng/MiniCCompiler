/**
 * 把 minic.exe 的文本产物解析成界面可直接渲染的结构。
 * 所有函数都容错：遇到不认识的格式保留原文，绝不因为一行异常导致整个面板白屏。
 */

export const EMPTY_RESULT = Object.freeze({
  tokens: { rows: [], typeCounts: [], maxLine: 0 },
  ast: { roots: [], flat: [], count: 0, maxDepth: 0 },
  symbols: { scopes: [], totals: { variables: 0, constants: 0, types: 0, labels: 0, procedures: 0, params: 0 } },
  quads: { rows: [], blocks: [], edges: [], opCounts: [] },
  asm: { lines: [], stats: { instructions: 0, labels: 0, directives: 0, registers: 0 }, registerCounts: [] },
  diagnostics: [],
});

const textOf = (v) => (typeof v === 'string' ? v : v == null ? '' : String(v));
const linesOf = (v) => textOf(v).replace(/\r\n/g, '\n').split('\n');

/* ------------------------------------------------------------------ */
/* 词法                                                               */
/* ------------------------------------------------------------------ */
export function parseTokens(text) {
  const rows = [];
  const counts = new Map();
  let maxLine = 0;
  linesOf(text).forEach((line) => {
    if (!line.trim()) return;
    const m = line.match(/^(\d+):(\d+)\s+(\S+)\s*(.*)$/);
    if (!m) return;
    const row = {
      id: rows.length,
      line: Number(m[1]),
      col: Number(m[2]),
      type: m[3],
      lexeme: m[4] === undefined ? '' : m[4],
    };
    maxLine = Math.max(maxLine, row.line);
    counts.set(row.type, (counts.get(row.type) || 0) + 1);
    rows.push(row);
  });
  const typeCounts = [...counts.entries()]
    .map(([type, count]) => ({ type, count }))
    .sort((a, b) => b.count - a.count || a.type.localeCompare(b.type));
  return { rows, typeCounts, maxLine };
}

/* ------------------------------------------------------------------ */
/* 语法（缩进树）                                                     */
/* ------------------------------------------------------------------ */
const AST_KIND_ZH = {
  Program: '程序',
  Decl: '变量声明',
  ArrayDecl: '数组声明',
  FuncDef: '函数定义',
  Param: '形参',
  Block: '语句块',
  Assign: '赋值',
  If: '条件',
  While: '循环',
  Print: '输出',
  Return: '返回',
  Empty: '空语句',
  BinOp: '二元运算',
  UnOp: '一元运算',
  Num: '常量',
  Ident: '标识符',
  Index: '下标',
  Call: '函数调用',
};

export function astKindOf(label) {
  const raw = String(label || '');
  const name = raw.split(/[ (]/)[0];
  return { name, zh: AST_KIND_ZH[name] || name };
}

export function parseAst(text) {
  const roots = [];
  const flat = [];
  const stack = [];
  let maxDepth = 0;

  linesOf(text).forEach((line) => {
    if (!line.trim()) return;
    const m = line.match(/^(\s*)(.*?)\s+@(\d+):(\d+)\s*$/);
    if (!m) return;
    const depth = Math.floor(m[1].length / 2);
    const label = m[2].trim();
    const node = {
      id: flat.length,
      label,
      ...astKindOf(label),
      line: Number(m[3]),
      col: Number(m[4]),
      depth,
      children: [],
      parentId: null,
    };
    while (stack.length > depth) stack.pop();
    const parent = depth > 0 ? stack[depth - 1] : null;
    if (parent) {
      node.parentId = parent.id;
      parent.children.push(node);
    } else {
      roots.push(node);
    }
    stack[depth] = node;
    flat.push(node);
    maxDepth = Math.max(maxDepth, depth);
  });

  return { roots, flat, count: flat.length, maxDepth };
}

export function flattenAst(ast) {
  return ast ? ast.flat : [];
}

/* ------------------------------------------------------------------ */
/* 符号表                                                             */
/* ------------------------------------------------------------------ */
const SYM_SECTIONS = {
  'Variables': 'variables',
  'Constants': 'constants',
  'Types': 'types',
  'Procedures (challenge)': 'procedures',
  'Params (challenge)': 'params',
  'Labels (challenge)': 'labels',
};

export function parseSymbols(text) {
  const scopes = [];
  const totals = { variables: 0, constants: 0, types: 0, labels: 0, procedures: 0, params: 0 };
  let scope = null;
  let section = null;

  const pushScope = (n) => {
    scope = {
      scope: n,
      label: n === 1 ? '全局作用域' : `作用域 #${n}`,
      categories: { variables: [], constants: [], types: [], procedures: [], params: [], labels: [] },
    };
    scopes.push(scope);
    section = null;
  };

  linesOf(text).forEach((line) => {
    if (!line.trim()) return;
    const sm = line.match(/^===\s*Symbol Table \(scope\s+(\d+)\)\s*===$/);
    if (sm) {
      pushScope(Number(sm[1]));
      return;
    }
    const cm = line.match(/^--\s*(.+?)\s*--\s*$/);
    if (cm) {
      section = SYM_SECTIONS[cm[1].trim()] || null;
      return;
    }
    if (!scope || !section) return;
    const body = line.replace(/^\s+/, '').trim();
    if (!body) return;

    let entry = { raw: body, name: body, type: '', detail: '', line: 0 };
    if (section === 'variables') {
      const m = body.match(/^(.+?)\s*:\s*(.+?)\s+@line\s+(\d+)(?:\s+scope\s+(\d+))?$/);
      if (m) entry = { raw: body, name: m[1].trim(), type: m[2].trim(), detail: m[4] ? `scope ${m[4]}` : '', line: Number(m[3]) };
    } else if (section === 'constants') {
      const m = body.match(/^(.+?)\s*:\s*(.+?)\s*=\s*(.*?)\s+@line\s+(\d+)$/);
      if (m) entry = { raw: body, name: m[1].trim(), type: m[2].trim(), detail: `= ${m[3].trim()}`, line: Number(m[4]) };
    } else if (section === 'types') {
      const m = body.match(/^(.+?)\s*:\s*(.+?)\s+size\s+(\d+)$/);
      if (m) entry = { raw: body, name: m[1].trim(), type: m[2].trim(), detail: `${m[3]} 字节`, line: 0 };
    } else if (section === 'labels') {
      const m = body.match(/^(\S+)\s+@line\s+(\d+)$/);
      if (m) entry = { raw: body, name: m[1], type: 'LABEL', detail: '跳转标签', line: Number(m[2]) };
    } else {
      const m = body.match(/^(.*?)\s+@line\s+(\d+)$/);
      if (m) entry = { raw: body, name: m[1], type: section === 'procedures' ? 'FUNC' : 'PARAM', detail: '', line: Number(m[2]) };
    }
    if (entry) {
      scope.categories[section].push(entry);
      if (section in totals) totals[section]++;
    }
  });

  if (!scopes.length && text && text.trim()) {
    return { scopes: [], totals, raw: text };
  }
  return { scopes, totals };
}

/* ------------------------------------------------------------------ */
/* 四元式 & 控制流图                                                  */
/* ------------------------------------------------------------------ */
const JUMP_OPS = new Set(['JMP', 'JNT', 'JF', 'JNZ', 'JZ', 'GOTO']);
// 「条件为假时跳转」的指令：JNT = Jump if Not True，JF = Jump if False，JZ = Jump if Zero。
// MiniC 只产生 JNT，但这里一并列出，避免以后换指令集时把 true/false 又标反。
const FALSE_JUMP_OPS = new Set(['JNT', 'JF', 'JZ']);
const TERMINAL_OPS = new Set(['RET', 'RETV', 'HALT', 'EXIT']);

export function opCategory(op) {
  const o = String(op || '').toUpperCase().replace(/_\d+$/, '');
  if (o.startsWith('ADD') || o.startsWith('SUB') || o.startsWith('MUL') || o.startsWith('DIV') || o.startsWith('MOD') || o === 'NEG') return 'arith';
  if (o.startsWith('ASSIGN')) return 'assign';
  if (JUMP_OPS.has(o) || o === 'LABEL') return 'control';
  if (TERMINAL_OPS.has(o)) return 'terminal';
  if (o.startsWith('CALL') || o === 'PARAM' || o === 'ARG') return 'call';
  if (o.startsWith('PRINT') || o.startsWith('READ')) return 'io';
  if (o.includes('CMP') || o === 'GT' || o === 'LT' || o === 'GE' || o === 'LE' || o === 'EQ' || o === 'NE') return 'compare';
  return 'other';
}

export function parseQuads(text) {
  const rows = [];
  linesOf(text).forEach((line) => {
    if (!line.trim()) return;
    const m = line.match(/^\(([^)]*)\)\s+@(\d+):(\d+)\s*$/);
    if (!m) return;
    const parts = m[1].split(',').map((s) => s.trim());
    const op = parts[0] || '';
    rows.push({
      id: rows.length,
      index: rows.length + 1,
      op,
      category: opCategory(op),
      args: [parts[1] || '-', parts[2] || '-'],
      result: parts[3] || '-',
      line: Number(m[2]),
      col: Number(m[3]),
    });
  });
  return buildCfg(rows);
}

export function buildCfg(rows) {
  const labelIndex = new Map();
  rows.forEach((r, i) => {
    if (r.op === 'LABEL' && r.args[0] && r.args[0] !== '-') labelIndex.set(r.args[0], i);
  });

  const leaders = new Set([0]);
  rows.forEach((r, i) => {
    if (r.op === 'LABEL') leaders.add(i);
    if ((JUMP_OPS.has(r.op) || TERMINAL_OPS.has(r.op)) && i + 1 < rows.length) leaders.add(i + 1);
  });
  const sorted = [...leaders].filter((i) => i >= 0 && i < rows.length).sort((a, b) => a - b);

  const blocks = sorted.map((start, idx) => {
    const end = idx + 1 < sorted.length ? sorted[idx + 1] - 1 : rows.length - 1;
    const blockRows = rows.slice(start, end + 1);
    const labelRow = blockRows.find((r) => r.op === 'LABEL');
    return {
      id: `B${idx + 1}`,
      index: idx,
      label: labelRow ? labelRow.args[0] : '',
      start,
      end,
      rows: blockRows,
      line: blockRows[0] ? blockRows[0].line : 0,
    };
  });

  const blockOfRow = (i) => blocks.find((b) => i >= b.start && i <= b.end);
  const edges = [];
  blocks.forEach((block, idx) => {
    const last = block.rows[block.rows.length - 1];
    if (!last) return;
    const next = blocks[idx + 1];
    // 标号的位置随指令而变：
    //   LABEL 的标号在 args[0]（(LABEL, L1, -, -)）
    //   JMP / JNT 的跳转目标在 result（(JNT, cond, -, L1)），args[0] 是判断用的条件变量
    // 早前版本一律取 args[0]，于是 JNT 把条件变量当成标号去找，永远找不到目标块，
    // 所有「条件不成立时跳出循环」的边都丢了（nested-loop 少 9 条边）。
    const targetName = last.op === 'LABEL' ? last.args[0] : last.result;
    const target = targetName && targetName !== '-' ? blocks.find((b) => b.label === targetName) : null;

    if (JUMP_OPS.has(last.op)) {
      // 条件跳转的两条边：跳目标那条对应「条件成立/不成立」中的一支，
      // 落空到下一块那条是另一支。JNT 是「不成立才跳」，所以：
      //   目标边 = false，落空边 = true
      // 早前版本把这两支写反了，导致图上「条件成立」指到循环出口。
      const jumpsOnFalse = FALSE_JUMP_OPS.has(last.op);
      if (target) edges.push({ from: block.id, to: target.id, kind: last.op === 'JMP' ? 'jump' : (jumpsOnFalse ? 'false' : 'true') });
      if (last.op !== 'JMP' && next) edges.push({ from: block.id, to: next.id, kind: jumpsOnFalse ? 'true' : 'false' });
    } else if (!TERMINAL_OPS.has(last.op) && next) {
      edges.push({ from: block.id, to: next.id, kind: 'next' });
    }
  });

  const opCounts = [...rows.reduce((m, r) => m.set(r.category, (m.get(r.category) || 0) + 1), new Map())]
    .map(([category, count]) => ({ category, count }));

  return { rows, blocks, edges, opCounts, blockOfRow };
}

/* ------------------------------------------------------------------ */
/* 汇编                                                               */
/* ------------------------------------------------------------------ */
export function parseAsm(text) {
  const lines = [];
  const registerCounts = new Map();
  const stats = { instructions: 0, labels: 0, directives: 0, registers: 0 };
  const REG_RE = /%([a-z0-9]+)/gi;

  linesOf(text).forEach((raw, i) => {
    const trimmed = raw.trim();
    let kind = 'empty';
    let mnemonic = '';
    let label = '';
    if (!trimmed) kind = 'empty';
    else if (trimmed.startsWith('#')) kind = 'comment';
    else if (trimmed.startsWith('.')) { kind = 'directive'; stats.directives++; }
    else if (/^[A-Za-z_.$][\w.$]*:\s*(?:#.*)?$/.test(trimmed)) { kind = 'label'; label = trimmed.replace(/:.*$/, ''); stats.labels++; }
    else {
      const m = trimmed.match(/^([A-Za-z][\w.]*)\b/);
      if (m) { kind = 'instruction'; mnemonic = m[1]; stats.instructions++; }
      else kind = 'other';
    }
    if (kind === 'instruction' || kind === 'directive') {
      for (const reg of trimmed.matchAll(REG_RE)) {
        registerCounts.set(reg[1], (registerCounts.get(reg[1]) || 0) + 1);
      }
    }
    lines.push({ id: i, number: i + 1, raw, kind, mnemonic, label, indent: raw.match(/^\s*/)?.[0].length || 0 });
  });

  stats.registers = registerCounts.size;
  const registerCountsArr = [...registerCounts.entries()]
    .map(([name, count]) => ({ name, count }))
    .sort((a, b) => b.count - a.count || a.name.localeCompare(b.name));

  return { lines, stats, registerCounts: registerCountsArr };
}

/* ------------------------------------------------------------------ */
/* 诊断                                                               */
/* ------------------------------------------------------------------ */
export function parseDiagnostics(stderr, stdout, ok = true) {
  const source = [textOf(stderr), textOf(stdout)].filter(Boolean).join('\n');
  const lines = linesOf(source);
  const items = [];
  let summary = '';
  const RE = /^(?:.*?minic:\s*)?(error|warning|note|fatal error):\s*(\d+):(\d+):\s*(.*)$/i;
  const SUMMARY_RE = /^.*minic:\s*(\d+)\s+error\(s\)\s+found.*$/i;

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const m = line.match(RE);
    if (m) {
      const diag = {
        id: items.length,
        severity: m[1].toLowerCase().includes('warning') ? 'warning' : 'error',
        line: Number(m[2]),
        col: Number(m[3]),
        message: m[4].trim(),
        context: [],
      };
      for (let j = i + 1; j < Math.min(lines.length, i + 5); j++) {
        const cm = lines[j].match(/^\s*(\d+)\s*\|\s?(.*)$/);
        if (cm) {
          diag.context.push({ line: Number(cm[1]), text: cm[2] });
          continue;
        }
        if (/^\s*\|.*\^/.test(lines[j])) break;
        if (!lines[j].trim()) continue;
        break;
      }
      items.push(diag);
      continue;
    }
    const sm = line.match(SUMMARY_RE);
    if (sm) summary = line.trim();
  }

  if (!items.length && !ok && source.trim()) {
    const first = lines.find((l) => l.trim());
    if (first) {
      items.push({
        id: 0,
        severity: 'error',
        line: 0,
        col: 0,
        message: first.trim(),
        context: [],
        general: true,
      });
    }
  }

  return { items, summary, raw: source };
}

/* ------------------------------------------------------------------ */
/* LL(1) 分析轨迹                                                     */
/* ------------------------------------------------------------------ */
export function parseParseTrace(text) {
  const lines = linesOf(text);
  const steps = [];
  let productions = 0;
  let result = '';
  for (const line of lines) {
    if (line.startsWith('# 产生式')) {
      const m = line.match(/(\d+)\s*条/);
      if (m) productions = Number(m[1]);
    }
    if (line.startsWith('#') && line.includes('结果')) {
      const m = line.match(/结果:\s*(.*)$/);
      if (m) result = m[1].trim();
    }
  }
  for (const line of lines) {
    if (!line.trim().startsWith('|')) continue;
    const cells = line.split('|').slice(1, -1).map((s) => s.trim());
    if (cells.length < 4) continue;
    if (/^[-:\s]+$/.test(cells[0])) continue;
    if (cells[0] === '步骤') continue;
    const strip = (s) => s.replace(/^`|`$/g, '').trim();
    steps.push({
      id: steps.length,
      step: Number(cells[0]) || steps.length + 1,
      stack: strip(cells[1]),
      input: strip(cells[2]),
      action: strip(cells[3]),
    });
  }
  return { steps, productions, result, rawLength: textOf(text).length };
}

/* ------------------------------------------------------------------ */
/* 汇总                                                               */
/* ------------------------------------------------------------------ */
export function parseAllArtifacts(artifacts, stderr, stdout, ok = true) {
  const a = artifacts || {};
  return {
    tokens: parseTokens(a['.token.txt']),
    ast: parseAst(a['.ast.txt']),
    symbols: parseSymbols(a['.sym.txt']),
    quads: parseQuads(a['.quad.txt']),
    asm: parseAsm(a['.asm']),
    diagnostics: parseDiagnostics(stderr, stdout, ok),
    parseTrace: parseParseTrace(a['.parse.txt']),
    raw: {
      token: a['.token.txt'] || '',
      ast: a['.ast.txt'] || '',
      sym: a['.sym.txt'] || '',
      quad: a['.quad.txt'] || '',
      asm: a['.asm'] || '',
      parse: a['.parse.txt'] || '',
    },
  };
}
