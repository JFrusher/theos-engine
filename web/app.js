// Theos playground: live checking in the browser through the WebAssembly build.
'use strict';

const EXAMPLES = [
  ['Affirming the consequent (invalid)', 'examples/affirming_the_consequent.theo'],
  ['The First Way (Aquinas)', 'library/arguments/aquinas_first_way.theo'],
  ['The kalam cosmological argument', 'library/arguments/kalam.theo'],
  ['Resurrection (minimal facts)', 'library/arguments/resurrection.theo'],
];

const STARTER = `argument "Modus Ponens"

axiom Obs = source("Field notes", "p. 4")

premise P1 grounded_by [Obs] {
    text:    "If it rains, the street is wet."
    formula: Rain -> Wet
}

premise P2 grounded_by [Obs] {
    text:    "It rains."
    formula: Rain
}

conclusion C1 from [P1, P2] by imp_e {
    text:    "The street is wet."
    formula: Wet
}
`;

CodeMirror.defineSimpleMode('theo', {
  start: [
    { regex: /\/\/.*/, token: 'comment' },
    { regex: /\/\*/, token: 'comment', next: 'comment' },
    { regex: /"(?:[^\\"]|\\.)*"/, token: 'string' },
    { regex: /\b(?:argument|by|import|axiom|scripture|source|principle|define|premise|deduction|conclusion|from|grounded_by|text|formula|proof|box|objection|reply|against|to|forall|exists|lambda|bot|ind|prop)\b/, token: 'keyword' },
    { regex: /<->|->|<>|\[\]|:=|!=|[~&|∧∨¬→↔□◇∀∃λ⊥≠\\]/, token: 'operator' },
    { regex: /\b\d+\b/, token: 'number' },
  ],
  comment: [
    { regex: /.*?\*\//, token: 'comment', next: 'start' },
    { regex: /.*/, token: 'comment' },
  ],
});

const editor = CodeMirror.fromTextArea(document.getElementById('source'), {
  mode: 'theo',
  lineNumbers: true,
  indentUnit: 4,
  viewportMargin: Infinity,
});

const verdict = document.getElementById('verdict');
const detail = document.getElementById('verdict-detail');
const graph = document.getElementById('graph');
const notes = document.getElementById('notes');
const picker = document.getElementById('examples');

mermaid.initialize({ startOnLoad: false, securityLevel: 'strict', theme: 'base', flowchart: { useMaxWidth: false },
  themeVariables: { fontFamily: 'Alegreya Sans, sans-serif', lineColor: '#565463' } });

let theos = null;
let marked = [];
let renderCount = 0;

function encodeSource(text) {
  return btoa(String.fromCharCode(...new TextEncoder().encode(text)));
}

function decodeSource(b64) {
  return new TextDecoder().decode(Uint8Array.from(atob(b64), c => c.charCodeAt(0)));
}

function plural(n, word) {
  return `${n} ${word}${n === 1 ? '' : 's'}`;
}

function dialectic(report) {
  if (!report.objections) return '';
  if (!report.standing.length) return `${report.objections === 1 ? 'The objection is' : `All ${report.objections} objections are`} answered.`;
  const contested = report.nodes.filter(n => n.kind === 'conclusion' && n.status !== 'accepted').map(n => n.id);
  return [...report.standing.map(s => `Objection ${s.objection} stands against ${s.target}.`),
          contested.length ? `So ${contested.join(', ')} ${contested.length === 1 ? 'is' : 'are'} contested.` : ''].join(' ').trim();
}

function showVerdict(report) {
  const errors = report.diagnostics.filter(d => d.severity === 'error');
  const warnings = report.diagnostics.length - errors.length;
  verdict.classList.toggle('fails', !report.ok);
  if (report.ok) {
    verdict.textContent = 'The argument is valid.';
    detail.textContent = `Every inference is valid. ${dialectic(report)}${warnings ? ` ${plural(warnings, 'warning')} below.` : ''}`;
  } else if (!report.mermaid) {
    verdict.textContent = 'The text could not be read.';
    detail.textContent = errors[0].message;
  } else {
    verdict.textContent = 'The argument is not valid yet.';
    detail.textContent = `${plural(errors.length, 'problem')}${warnings ? `, ${plural(warnings, 'warning')}` : ''}. Select a note to jump to its line.`;
  }
}

function showNotes(report) {
  marked.forEach(line => editor.removeLineClass(line, 'background'));
  marked = [];
  notes.replaceChildren(...report.diagnostics.map(d => {
    const li = document.createElement('li');
    li.className = d.severity;
    li.tabIndex = 0;
    const where = document.createElement('span');
    where.className = 'where';
    where.textContent = d.line ? `line ${d.line}` : 'file';
    const message = document.createElement('span');
    message.textContent = d.message;
    li.append(where, message);
    const jump = () => {
      if (!d.line) return;
      editor.focus();
      editor.setCursor({ line: d.line - 1, ch: Math.max(d.col - 1, 0) });
    };
    li.addEventListener('click', jump);
    li.addEventListener('keydown', e => { if (e.key === 'Enter') jump(); });
    if (d.line) {
      const handle = editor.addLineClass(d.line - 1, 'background', d.severity === 'error' ? 'line-error' : 'line-warning');
      marked.push(handle);
    }
    return li;
  }));
}

async function showGraph(report) {
  if (!report.mermaid) {
    graph.replaceChildren();
    return;
  }
  const id = `graph-${++renderCount}`;
  try {
    const { svg } = await mermaid.render(id, report.mermaid);
    if (id === `graph-${renderCount}`) graph.innerHTML = svg;
  } catch (err) {
    graph.textContent = `The graph could not be drawn: ${err.message ?? err}`;
  }
}

function check() {
  if (!theos) return;
  const source = editor.getValue();
  const report = JSON.parse(theos.check(source));
  showVerdict(report);
  showNotes(report);
  showGraph(report);
  history.replaceState(null, '', `#src=${encodeSource(source)}`);
}

let timer = null;
editor.on('change', () => {
  clearTimeout(timer);
  timer = setTimeout(check, 250);
});

async function loadExample(path) {
  const response = await fetch(path);
  if (!response.ok) throw new Error(`${path}: ${response.status}`);
  editor.setValue(await response.text());
}

picker.append(new Option('Modus ponens', ''), ...EXAMPLES.map(([name, path]) => new Option(name, path)));
picker.addEventListener('change', () => {
  if (picker.value) loadExample(picker.value).catch(err => { detail.textContent = `Could not load the example: ${err.message}`; });
  else editor.setValue(STARTER);
});

const shared = location.hash.startsWith('#src=') ? location.hash.slice(5) : null;
editor.setValue(shared ? decodeSource(shared) : STARTER);

// The bundled library (theos.data) sits next to the wasm in dist/
createTheos({ locateFile: file => `dist/${file}` }).then(module => {
  theos = module;
  check();
});
