const TYPE_WORDS = new Set(['INT', 'FLOAT', 'BOOL', 'CHAR', 'VOID']);
const LITERALS = new Set(['NUM_INT', 'NUM_FLOAT', 'CHAR_LIT', 'STRING_LIT', 'TRUE', 'FALSE']);
const KEYWORDS = new Set(['IF', 'ELSE', 'WHILE', 'RETURN', 'PRINT', 'CONST']);
const OPERATORS = new Set([
  'PLUS', 'MINUS', 'STAR', 'SLASH', 'PERCENT', 'ASSIGN', 'EQ', 'NE', 'LT', 'LE', 'GT', 'GE',
  'AND', 'OR', 'NOT', 'INC', 'DEC', 'BITAND', 'BITOR', 'BITXOR', 'SHL', 'SHR',
]);

export function tokenKind(type) {
  const t = String(type || '').toUpperCase();
  if (TYPE_WORDS.has(t)) return 'type';
  if (LITERALS.has(t)) return 'literal';
  if (KEYWORDS.has(t)) return 'keyword';
  if (OPERATORS.has(t)) return 'operator';
  if (['SEMI', 'COMMA', 'LPAREN', 'RPAREN', 'LBRACE', 'RBRACE', 'LBRACKET', 'RBRACKET', 'COLON', 'QUESTION'].includes(t)) return 'punct';
  return 'other';
}

export const OP_CATEGORY_COLOR = {
  arith: '#22d3ee',
  assign: '#60a5fa',
  control: '#c4a7f7',
  terminal: '#f472b6',
  call: '#f5b86b',
  io: '#9ae6a4',
  compare: '#7dd3fc',
  other: '#8fa3b8',
};

export const OP_CATEGORY_LABEL = {
  arith: '算术',
  assign: '赋值',
  control: '控制',
  terminal: '返回',
  call: '调用',
  io: '输入输出',
  compare: '比较',
  other: '其他',
};
