/**
 * MiniC 语言在 Monaco 中的静态配置：语法着色、括号、补全、悬停说明。
 * 这里只做编辑器体验，不参与真正的词法/语法分析（那由 minic.exe 完成）。
 */

const KEYWORDS = [
  'int', 'float', 'bool', 'char', 'void',
  'if', 'else', 'while', 'return', 'print',
  'true', 'false', 'const',
];

const TYPE_WORDS = ['int', 'float', 'bool', 'char', 'void'];

const KEYWORD_DOCS = {
  int: '32 位有符号整型。声明：`int a;`，数组：`int a[10];`',
  float: '双精度浮点型（MiniC 内部按 double 处理）。',
  bool: '布尔类型，字面量只有 `true` / `false`。',
  char: '字符类型，字面量写作单引号，例如 `\'a\'`。',
  void: '只能作为函数返回类型；不能声明 void 变量。',
  if: '条件语句：`if (条件) 语句 else 语句`',
  else: '与最近的 `if` 配对。',
  while: '当型循环：`while (条件) 语句`。',
  return: '函数返回：`return;` 或 `return 表达式;`',
  print: '内建输出语句：`print(表达式);`，可输出 int / float / bool / char。',
  true: '布尔真值，等价于 1。',
  false: '布尔假值，等价于 0。',
  const: '常量修饰符：`const int N = 10;`',
};

function buildMonarch() {
  return {
    defaultToken: '',
    tokenPostfix: '.minic',
    keywords: KEYWORDS,
    typeKeywords: TYPE_WORDS,
    operators: [
      '=', '>', '<', '!', '~', '?', ':', '==', '<=', '>=', '!=',
      '&&', '||', '++', '--', '+', '-', '*', '/', '&', '|', '^', '%',
    ],
    symbols: /[=><!~?:&|+\-*/^%]+/,
    escapes: /\\(?:[abfnrtv\\"']|x[0-9A-Fa-f]{1,4}|u[0-9A-Fa-f]{4})/,
    tokenizer: {
      root: [
        [/[a-zA-Z_]\w*/, {
          cases: {
            '@typeKeywords': 'type',
            '@keywords': 'keyword',
            '@default': 'identifier',
          },
        }],
        { include: '@whitespace' },
        [/[{}()[\]]/, '@brackets'],
        [/[<>](?!@symbols)/, '@brackets'],
        [/@symbols/, {
          cases: {
            '@operators': 'operator',
            '@default': '',
          },
        }],
        [/\d*\.\d+([eE][-+]?\d+)?/, 'number.float'],
        [/0[xX][0-9a-fA-F]+/, 'number.hex'],
        [/\d+/, 'number'],
        [/[;,.]/, 'delimiter'],
        [/"([^"\\]|\\.)*$/, 'string.invalid'],
        [/"/, 'string', '@string'],
        [/'[^\\']'/, 'string'],
        [/'/, 'string.invalid'],
      ],
      whitespace: [
        [/[ \t\r\n]+/, ''],
        [/\/\*/, 'comment', '@comment'],
        [/\/\/.*$/, 'comment'],
      ],
      comment: [
        [/[^/*]+/, 'comment'],
        [/\*\//, 'comment', '@pop'],
        [/[/*]/, 'comment'],
      ],
      string: [
        [/[^\\"]+/, 'string'],
        [/@escapes/, 'string.escape'],
        [/\\./, 'string.escape.invalid'],
        [/"/, 'string', '@pop'],
      ],
    },
  };
}

function defineThemes(monaco) {
  monaco.editor.defineTheme('minic-dark', {
    base: 'vs-dark',
    inherit: true,
    rules: [
      { token: 'comment', foreground: '5c7085', fontStyle: 'italic' },
      { token: 'keyword', foreground: '22d3ee' },
      { token: 'type', foreground: '7dd3fc' },
      { token: 'identifier', foreground: 'dbe7f3' },
      { token: 'number', foreground: 'f5b86b' },
      { token: 'number.float', foreground: 'f5b86b' },
      { token: 'string', foreground: '9ae6a4' },
      { token: 'string.escape', foreground: 'f2d27d' },
      { token: 'operator', foreground: 'c4a7f7' },
      { token: 'delimiter', foreground: '8fa3b8' },
      { token: 'delimiter.bracket', foreground: '8fa3b8' },
    ],
    colors: {
      'editor.background': '#0b1016',
      'editor.foreground': '#dbe7f3',
      'editorLineNumber.foreground': '#3f5064',
      'editorLineNumber.activeForeground': '#22d3ee',
      'editor.lineHighlightBackground': '#ffffff08',
      'editor.lineHighlightBorder': '#22d3ee12',
      'editor.selectionBackground': '#22d3ee2e',
      'editor.inactiveSelectionBackground': '#22d3ee18',
      'editorCursor.foreground': '#22d3ee',
      'editorIndentGuide.background1': '#1a2430',
      'editorIndentGuide.activeBackground1': '#2d4054',
      'editorBracketHighlight.foreground1': '#22d3ee',
      'editorBracketHighlight.foreground2': '#c4a7f7',
      'editorBracketHighlight.foreground3': '#f5b86b',
      'editorWidget.background': '#121a23',
      'editorWidget.border': '#243242',
      'editorSuggestWidget.background': '#121a23',
      'editorSuggestWidget.border': '#243242',
      'editorSuggestWidget.selectedBackground': '#1d2c3c',
      'editorHoverWidget.background': '#121a23',
      'editorHoverWidget.border': '#243242',
      'scrollbarSlider.background': '#33475b55',
      'scrollbarSlider.hoverBackground': '#44607a77',
      'scrollbarSlider.activeBackground': '#5b7b9a88',
      'minimap.background': '#0b1016',
    },
  });

  monaco.editor.defineTheme('minic-light', {
    base: 'vs',
    inherit: true,
    rules: [
      { token: 'comment', foreground: '8494a8', fontStyle: 'italic' },
      { token: 'keyword', foreground: '0e7490' },
      { token: 'type', foreground: '0369a1' },
      { token: 'identifier', foreground: '1e293b' },
      { token: 'number', foreground: 'b45309' },
      { token: 'number.float', foreground: 'b45309' },
      { token: 'string', foreground: '15803d' },
      { token: 'string.escape', foreground: 'a16207' },
      { token: 'operator', foreground: '7e22ce' },
      { token: 'delimiter', foreground: '64748b' },
    ],
    colors: {
      'editor.background': '#fbfdff',
      'editor.foreground': '#1e293b',
      'editorLineNumber.foreground': '#b2c0d0',
      'editorLineNumber.activeForeground': '#0891b2',
      'editor.lineHighlightBackground': '#0e749009',
      'editor.selectionBackground': '#06b6d42a',
      'editor.inactiveSelectionBackground': '#06b6d416',
      'editorCursor.foreground': '#0891b2',
      'editorIndentGuide.background1': '#e4ebf2',
      'editorIndentGuide.activeBackground1': '#c2d3e2',
      'editorWidget.background': '#ffffff',
      'editorWidget.border': '#d9e2ec',
      'editorSuggestWidget.background': '#ffffff',
      'editorSuggestWidget.border': '#d9e2ec',
      'editorSuggestWidget.selectedBackground': '#e8f7fa',
      'editorHoverWidget.background': '#ffffff',
      'editorHoverWidget.border': '#d9e2ec',
      'scrollbarSlider.background': '#94a3b833',
      'scrollbarSlider.hoverBackground': '#64748b55',
      'minimap.background': '#fbfdff',
    },
  });
}

export function registerMiniCLanguage(monaco) {
  if (monaco.languages.getLanguages().some((l) => l.id === 'minic')) {
    defineThemes(monaco);
    return;
  }

  monaco.languages.register({ id: 'minic', extensions: ['.mc'], aliases: ['MiniC', 'minic'] });
  monaco.languages.setMonarchTokensProvider('minic', buildMonarch());
  monaco.languages.setLanguageConfiguration('minic', {
    comments: { lineComment: '//', blockComment: ['/*', '*/'] },
    brackets: [['{', '}'], ['[', ']'], ['(', ')']],
    autoClosingPairs: [
      { open: '{', close: '}' },
      { open: '[', close: ']' },
      { open: '(', close: ')' },
      { open: '"', close: '"' },
      { open: "'", close: "'" },
    ],
    surroundingPairs: [
      { open: '{', close: '}' },
      { open: '[', close: ']' },
      { open: '(', close: ')' },
      { open: '"', close: '"' },
      { open: "'", close: "'" },
    ],
    indentationRules: {
      increaseIndentPattern: /\{[^}"']*$/,
      decreaseIndentPattern: /^\s*\}/,
    },
  });

  monaco.languages.registerCompletionItemProvider('minic', {
    triggerCharacters: ['.', ' ', '(', '='],
    provideCompletionItems(model, position) {
      const word = model.getWordUntilPosition(position);
      const range = {
        startLineNumber: position.lineNumber,
        endLineNumber: position.lineNumber,
        startColumn: word.startColumn,
        endColumn: word.endColumn,
      };
      const kind = monaco.languages.CompletionItemKind;
      const items = KEYWORDS.map((k) => ({
        label: k,
        kind: TYPE_WORDS.includes(k) ? kind.Keyword : kind.Keyword,
        insertText: k,
        range,
        detail: TYPE_WORDS.includes(k) ? '类型' : '关键字',
        documentation: KEYWORD_DOCS[k] || '',
      }));
      const snippets = [
        {
          label: 'main',
          detail: 'int main() 程序入口',
          insertText: 'int main() {\n    ${1:/* 代码 */}\n    return 0;\n}',
          insertTextRules: monaco.languages.InsertTextRules.InsertAsSnippet,
          kind: kind.Snippet,
          range,
        },
        {
          label: 'while',
          detail: 'while 循环',
          insertText: 'while (${1:条件}) {\n    ${2}\n}',
          insertTextRules: monaco.languages.InsertTextRules.InsertAsSnippet,
          kind: kind.Snippet,
          range,
        },
        {
          label: 'if',
          detail: 'if / else 条件',
          insertText: 'if (${1:条件}) {\n    ${2}\n} else {\n    ${3}\n}',
          insertTextRules: monaco.languages.InsertTextRules.InsertAsSnippet,
          kind: kind.Snippet,
          range,
        },
        {
          label: 'print',
          detail: '输出表达式',
          insertText: "print(${1:值}); print('\\n');",
          insertTextRules: monaco.languages.InsertTextRules.InsertAsSnippet,
          kind: kind.Snippet,
          range,
        },
        {
          label: 'for',
          detail: 'MiniC 不支持 for（仅提示）',
          insertText: '/* MiniC 不支持 for，请改用 while */\n${1:int i;}\ni = 0;\nwhile (i < ${2:n}) {\n    ${3}\n    i = i + 1;\n}',
          insertTextRules: monaco.languages.InsertTextRules.InsertAsSnippet,
          kind: kind.Snippet,
          range,
        },
      ];
      return { suggestions: [...items, ...snippets] };
    },
  });

  monaco.languages.registerHoverProvider('minic', {
    provideHover(model, position) {
      const word = model.getWordAtPosition(position);
      if (!word) return null;
      const doc = KEYWORD_DOCS[word.word];
      if (!doc) return null;
      return {
        range: new monaco.Range(position.lineNumber, word.startColumn, position.lineNumber, word.endColumn),
        contents: [{ value: `**${word.word}**` }, { value: doc }],
      };
    },
  });

  defineThemes(monaco);
}
