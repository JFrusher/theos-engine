// Browser smoke test for the assembled site.
// Usage: node tests/web/playground_smoke.cjs <site-dir>   (needs network for the CDN scripts)
const assert = require('node:assert/strict');
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const { chromium } = require('playwright');

const root = path.resolve(process.argv[2] ?? 'site');
const types = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.wasm': 'application/wasm', '.theo': 'text/plain' };

const server = http.createServer((req, res) => {
  const url = decodeURIComponent(req.url.split('?')[0]);
  let file = path.join(root, url.endsWith('/') ? url + 'index.html' : url);
  if (!file.startsWith(root) || !fs.existsSync(file)) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { 'content-type': types[path.extname(file)] ?? 'application/octet-stream' });
  fs.createReadStream(file).pipe(res);
});

const INVALID = `premise P1 { text: "If A then B" formula: A -> B }
premise P2 { text: "B" formula: B }
conclusion C from [P1, P2] { text: "A" formula: A }
`;

(async () => {
  await new Promise(resolve => server.listen(0, resolve));
  const base = `http://localhost:${server.address().port}`;
  const proxy = process.env.HTTPS_PROXY ? { server: process.env.HTTPS_PROXY, bypass: 'localhost,127.0.0.1' } : undefined;
  const browser = await chromium.launch({ proxy });
  try {
    const page = await browser.newPage();
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));

    await page.goto(`${base}/index.html`);
    await page.waitForSelector('#graph svg', { timeout: 30000 });
    assert.equal(await page.textContent('#verdict'), 'The argument is valid.');

    // Affirming the consequent: the checker must reject it with a counterexample
    await page.evaluate(src => document.querySelector('.CodeMirror').CodeMirror.setValue(src), INVALID);
    await page.waitForFunction(() => document.querySelector('#verdict').textContent.includes('not valid'), null, { timeout: 10000 });
    const note = await page.textContent('#notes li.error');
    assert.match(note, /counterexample A = false, B = true/);

    // Fixing the conclusion makes it hold again
    await page.evaluate(src => document.querySelector('.CodeMirror').CodeMirror.setValue(src), INVALID.replace('formula: A }\n', 'formula: B }\n').replace('premise P2 { text: "B" formula: B }', 'premise P2 { text: "A" formula: A }'));
    await page.waitForFunction(() => document.querySelector('#verdict').textContent === 'The argument is valid.', null, { timeout: 10000 });

    await page.goto(`${base}/gallery/index.html`);
    assert.ok((await page.locator('.entries li').count()) >= 2);
    await page.click('text=The First Way');
    await page.waitForSelector('.mermaid svg', { timeout: 30000 });

    assert.deepEqual(errors, []);
    console.log('playground smoke test passed');
  } finally {
    await browser.close();
    server.close();
  }
})().catch(err => { console.error(err); process.exit(1); });
