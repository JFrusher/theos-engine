// Loads the WebAssembly build and checks one valid and one invalid argument.
// Usage: node tests/web/wasm_smoke.cjs   (after building web/dist with Emscripten)
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const createTheos = require(path.join(__dirname, '../../web/dist/theos.js'));

(async () => {
  const dist = path.join(__dirname, '../../web/dist');
  const theos = await createTheos({ locateFile: file => path.join(dist, file) });

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

  // The standard library is bundled, so imports resolve in the browser too
  const imported = JSON.parse(theos.check('argument "Imports"\nimport "std/principles"\n' +
    'premise P1 grounded_by [PSR] { text: "c" formula: Contingent(u) }\n' +
    'conclusion C1 from [P1, PSRPremise] { text: "e" formula: exists y. Explains(y, u)\n' +
    ' proof { 1. Contingent(u) premise P1\n 2. forall x. Contingent(x) -> exists y. Explains(y, x) premise PSRPremise\n' +
    ' 3. Contingent(u) -> exists y. Explains(y, u) forall_e 2\n 4. exists y. Explains(y, u) imp_e 3, 1 } }\n'));
  assert.equal(imported.ok, true, imported.rendered);

  const contested = JSON.parse(theos.check(fs.readFileSync(path.join(__dirname, '../../library/arguments/kalam.theo'), 'utf8')
    .replace(/reply R2 to O2 \{[^}]*\}/, '')));
  assert.deepEqual(contested.standing, [{ objection: 'O2', target: 'P2' }]);
  assert.equal(contested.nodes.find(n => n.id === 'C1').status, 'defeated');

  console.log('wasm smoke test passed');
})();
