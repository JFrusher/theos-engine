// Loads the WebAssembly build and checks one valid and one invalid argument.
// Usage: node tests/web/wasm_smoke.cjs   (after building web/dist with Emscripten)
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const createTheos = require(path.join(__dirname, '../../web/dist/theos.js'));

(async () => {
  const theos = await createTheos();

  const good = JSON.parse(theos.check(fs.readFileSync(path.join(__dirname, '../../library/arguments/aquinas_first_way.theo'), 'utf8')));
  assert.equal(good.ok, true, good.rendered);
  assert.ok(good.mermaid.startsWith('flowchart TD'));
  assert.equal(good.nodes.length, 7);

  const bad = JSON.parse(theos.check('premise P1 { text: "a" formula: A -> B }\npremise P2 { text: "b" formula: B }\n' +
                                     'conclusion C from [P1, P2] { text: "c" formula: A }\n'));
  assert.equal(bad.ok, false);
  assert.match(bad.rendered, /counterexample A = false, B = true/);
  assert.equal(bad.nodes.find(n => n.id === 'C').error, true);
  assert.match(bad.mermaid, /class C invalid/);

  const syntax = JSON.parse(theos.check('premise {'));
  assert.equal(syntax.ok, false);
  assert.equal(syntax.diagnostics[0].line, 1);
  assert.equal(syntax.mermaid, undefined);

  console.log('wasm smoke test passed');
})();
