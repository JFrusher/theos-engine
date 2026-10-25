// Renders the README images in docs/assets/ from the real tool: every terminal
// line is captured from build/theosc, the playground is the real WebAssembly site.
// Usage: node tools/readme/render.cjs [banner|format|terminal|playground ...]
// Needs: build/theosc, web/dist (WebAssembly build), ffmpeg, Playwright, network for fonts/CDN.
'use strict';
const { execFileSync, execSync } = require('node:child_process');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const { chromium } = require('playwright');

const ROOT = path.resolve(__dirname, '../..');
const THEOSC = path.join(ROOT, 'build/theosc');
const OUT = path.join(ROOT, 'docs/assets');
const TMP = fs.mkdtempSync(path.join(os.tmpdir(), 'theos-readme-'));

// Palette from web/theos.css
const THEMES = {
  light: { page: '#f3f4f1', ink: '#23212b', soft: '#565463', lapis: '#1f4e9c', rubric: '#b3261e', gold: '#a8842a', rule: '#c9ccc6', surface: '#ffffff' },
  dark: { page: '#1c1b22', ink: '#e7e6ea', soft: '#a9a7b4', lapis: '#8fb0ea', rubric: '#f08a80', gold: '#d9b95b', rule: '#3a3944', surface: '#24232b' },
};
const FONTS = '<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Alegreya+Sans:wght@400;500;700&family=IM+Fell+English:ital@0;1&family=JetBrains+Mono:wght@400;700&display=swap">';

const esc = s => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
const sleep = ms => new Promise(r => setTimeout(r, ms));

async function shoot(page, html, file, selector = '#frame') {
  await page.setContent(html, { waitUntil: 'networkidle' });
  await page.evaluate(() => document.fonts.ready);
  await page.locator(selector).screenshot({ path: path.join(OUT, file) });
  console.log('wrote', file);
}

// frames: [{ file, ms }] -> optimized GIF
function gif(frames, file, width) {
  const list = frames.map(f => `file '${f.file}'\nduration ${(f.ms / 1000).toFixed(3)}`).join('\n') + `\nfile '${frames.at(-1).file}'\n`;
  const listFile = path.join(TMP, `${file}.txt`);
  fs.writeFileSync(listFile, list);
  const scale = `scale=${width}:-1:flags=lanczos`;
  execFileSync('ffmpeg', ['-y', '-loglevel', 'error', '-f', 'concat', '-safe', '0', '-i', listFile,
    '-vf', `${scale},split[a][b];[a]palettegen=stats_mode=diff:max_colors=128[p];[b][p]paletteuse=dither=none:diff_mode=rectangle`,
    '-loop', '0', path.join(OUT, file)]);
  console.log('wrote', file, `${(fs.statSync(path.join(OUT, file)).size / 1e6).toFixed(2)} MB`);
}

// ---------------------------------------------------------------- banner

function bannerHtml(t) {
  return `<!doctype html><meta charset="utf-8">${FONTS}<style>
  body { margin: 0; background: transparent; }
  #frame { width: 1280px; height: 360px; box-sizing: border-box; padding: 18px; background: ${t.page}; position: relative; overflow: hidden; }
  .border { position: absolute; inset: 18px; border: 1.5px solid ${t.gold}; outline: 1px solid ${t.gold}; outline-offset: 5px; }
  .content { position: absolute; inset: 0; display: flex; align-items: center; padding: 0 96px; gap: 56px; }
  .initial { font: 220px/1 "IM Fell English", serif; color: ${t.rubric}; margin-top: -26px; }
  .words { flex: 1; }
  .name { font: 112px/1 "IM Fell English", serif; color: ${t.ink}; letter-spacing: 1px; margin-left: -132px; }
  .motto { font: 500 17px/1 "Alegreya Sans", sans-serif; letter-spacing: 6px; text-transform: uppercase; white-space: nowrap; color: ${t.gold}; margin: 22px 0 18px; }
  .tag { font: 26px/1.35 "Alegreya Sans", sans-serif; color: ${t.soft}; max-width: 600px; }
  .formula { font: italic 64px/1 "IM Fell English", serif; color: ${t.lapis}; white-space: nowrap; text-align: right; }
  .formula small { display: block; font: 15px/1.6 "JetBrains Mono", monospace; font-style: normal; color: ${t.soft}; margin-top: 14px; }
  </style>
  <div id="frame"><div class="border"></div><div class="content">
    <div class="initial">T</div>
    <div class="words">
      <div class="name"><span style="visibility:hidden">T</span>heos</div>
      <div class="motto">Fides quaerens intellectum</div>
      <div class="tag">Faith seeking understanding, machine-checked. Write the arguments of faith in formal logic; Theos verifies every step.</div>
    </div>
    <div class="formula">&#9633;&#8707;x. G(x)<small>godel.theo: ok</small></div>
  </div></div>`;
}

// ---------------------------------------------------------------- .theos byte map

function chunks(bytes) {
  const out = [];
  let off = 24;
  while (off < bytes.length) {
    const tag = bytes.toString('latin1', off, off + 4);
    const len = bytes.readUInt32LE(off + 4);
    out.push({ tag, off, len, pad: (4 - (len % 4)) % 4 });
    off += 12 + len + ((4 - (len % 4)) % 4);
  }
  return out;
}

function formatHtml(t, bytes) {
  const colors = { META: '#b5527a', STRS: '#a8842a', FORM: '#1f4e9c', AXIM: '#b3261e', DEFN: '#5b6b85', NODE: '#2e7d5b', EDGE: '#6a4c93', DIAL: '#c2571a', PROF: '#3b7fa6' };
  const region = new Array(bytes.length);
  for (let i = 0; i < 8; i++) region[i] = { c: t.ink, k: 'sig' };
  for (let i = 8; i < 24; i++) region[i] = { c: t.soft, k: 'hdr' };
  const list = chunks(bytes);
  for (const ch of list) {
    for (let i = ch.off; i < ch.off + 12; i++) region[i] = { c: colors[ch.tag], k: 'head' };
    for (let i = ch.off + 12; i < ch.off + 12 + ch.len; i++) region[i] = { c: colors[ch.tag], k: 'body' };
    for (let i = ch.off + 12 + ch.len; i < ch.off + 12 + ch.len + ch.pad; i++) region[i] = { c: t.rule, k: 'pad' };
  }
  const cells = [...bytes].map((b, i) => {
    const r = region[i];
    const hex = b.toString(16).toUpperCase().padStart(2, '0');
    const style = r.k === 'head' || r.k === 'sig' ? `background:${r.c};color:#fff` : r.k === 'pad' ? `color:${t.soft};border-color:${t.rule}` : `color:${r.c};border-color:${r.c}`;
    return `<span class="b ${r.k}" style="${style}">${hex}</span>`;
  });
  const rows = [];
  for (let i = 0; i < cells.length; i += 16) rows.push(`<div class="row"><span class="off">${i.toString(16).toUpperCase().padStart(4, '0')}</span>${cells.slice(i, i + 16).join('')}</div>`);
  const legend = [
    `<li><span class="sw" style="background:${t.ink}"></span><b>Signature</b> <code>89 THEO 0D 0A 1A</code></li>`,
    `<li><span class="sw" style="background:${t.soft}"></span><b>Header</b> version, chunk count, size, CRC-32</li>`,
    ...list.map(ch => `<li><span class="sw" style="background:${colors[ch.tag]}"></span><b>${ch.tag}</b> ${{ META: 'title, author, imports', STRS: 'deduplicated strings', FORM: 'formula DAG, children first', AXIM: 'axioms, scripture resolved', DEFN: 'definitions', NODE: 'premises and conclusions', EDGE: 'from and grounded_by edges', DIAL: 'objections and replies', PROF: 'Fitch proofs' }[ch.tag]} <span class="n">${ch.len} B</span></li>`),
  ].join('');
  return `<!doctype html><meta charset="utf-8">${FONTS}<style>
  body { margin: 0; }
  #frame { display: inline-flex; gap: 40px; padding: 32px 36px; background: ${t.page}; color: ${t.ink}; font: 16px/1.4 "Alegreya Sans", sans-serif; }
  h3 { font: 30px/1 "IM Fell English", serif; margin: 0 0 4px; }
  .sub { color: ${t.soft}; margin: 0 0 18px; }
  .row { display: flex; gap: 3px; margin-bottom: 3px; font: 13px/1 "JetBrains Mono", monospace; }
  .off { width: 44px; color: ${t.soft}; padding-top: 5px; }
  .b { width: 25px; height: 21px; display: inline-flex; align-items: center; justify-content: center; border-radius: 3px; border: 1px solid transparent; box-sizing: border-box; }
  .body { background: ${t.surface}; }
  .pad { border-style: dashed; }
  ul { list-style: none; padding: 0; margin: 74px 0 0; }
  li { margin: 0 0 9px; white-space: nowrap; }
  .sw { display: inline-block; width: 14px; height: 14px; border-radius: 3px; vertical-align: -2px; margin-right: 8px; }
  code { font: 13px "JetBrains Mono", monospace; color: ${t.soft}; }
  .n { color: ${t.soft}; font-size: 14px; }
  .key { margin-top: 18px; color: ${t.soft}; font-size: 14px; white-space: normal; max-width: 300px; }
  </style>
  <div id="frame">
    <div><h3>Anatomy of a .theos file</h3><p class="sub">tests/golden/binary/tiny.theo, all ${bytes.length} bytes</p>${rows.join('')}</div>
    <div><ul>${legend}</ul><p class="key">Filled cells: chunk tag, length and CRC-32. Outlined: payload. Dashed: padding to 4 bytes.</p></div>
  </div>`;
}

// ---------------------------------------------------------------- terminal

const SCENES = [
  { cmd: 'theosc check godel.theo', hold: 1400 },
  { comment: '# T1 says a positive property is *possibly* exemplified. Claim more:' },
  { cmd: "sed 's/24. <>exists/24. exists/' godel.theo > overreach.theo", hold: 300 },
  { cmd: 'theosc check overreach.theo', hold: 3200 },
  { cmd: 'theosc check affirming_the_consequent.theo', hold: 3200 },
  { cmd: 'theosc build godel.theo && theosc inspect godel.theos --chunks | head -15', hold: 4000 },
];

function colorize(line) {
  const e = esc(line);
  if (/^\s*\^~*$/.test(line)) return `<span class="err">${e}</span>`;
  if (/: error: /.test(line)) return e.replace(/^(.*?:\d+:\d+:) error: (.*)$/, '<span class="dim">$1</span> <span class="err b">error:</span> $2');
  if (/: ok$/.test(line)) return e.replace(/ok$/, '<span class="ok b">ok</span>');
  if (/\s(ok)$/.test(line)) return e.replace(/ok$/, '<span class="ok">ok</span>');
  if (/^\s+tag\s+offset/.test(line) || /^\.theos v/.test(line)) return `<span class="hl">${e}</span>`;
  if (/^\s+[0-9A-F]{8}\s/.test(line)) return `<span class="dim">${e}</span>`;
  return e;
}

function terminalHtml(t) {
  return `<!doctype html><meta charset="utf-8">${FONTS}<style>
  body { margin: 0; background: transparent; }
  #frame { width: 1100px; padding: 0; background: ${t.page}; border-radius: 12px; overflow: hidden; border: 1px solid ${t.rule}; }
  .chrome { display: flex; align-items: center; gap: 8px; padding: 12px 16px; background: ${t.surface}; border-bottom: 1px solid ${t.rule}; font: 14px "Alegreya Sans", sans-serif; color: ${t.soft}; }
  .dot { width: 12px; height: 12px; border-radius: 50%; } .title { flex: 1; text-align: center; margin-right: 52px; }
  pre { margin: 0; padding: 18px 22px; height: ${22 * 22}px; overflow: hidden; font: 14.5px/22px "JetBrains Mono", monospace; color: ${t.ink}; white-space: pre; display: flex; flex-direction: column; justify-content: flex-end; }
  .p { color: ${t.lapis}; font-weight: 700; } .err { color: ${t.rubric}; } .ok { color: #7fc8a9; } .b { font-weight: 700; }
  .dim { color: ${t.soft}; } .hl { color: ${t.gold}; } .c { color: ${t.soft}; font-style: italic; }
  .cur { background: ${t.ink}; color: ${t.page}; }
  </style>
  <div id="frame"><div class="chrome"><span class="dot" style="background:${t.rubric}"></span><span class="dot" style="background:${t.gold}"></span><span class="dot" style="background:${t.lapis}"></span><span class="title">theosc</span></div><pre id="screen"></pre></div>`;
}

async function terminal(page) {
  const work = path.join(TMP, 'term');
  fs.mkdirSync(work);
  fs.copyFileSync(path.join(ROOT, 'library/arguments/godel.theo'), path.join(work, 'godel.theo'));
  fs.copyFileSync(path.join(ROOT, 'examples/affirming_the_consequent.theo'), path.join(work, 'affirming_the_consequent.theo'));
  const env = { ...process.env, PATH: `${path.dirname(THEOSC)}:${process.env.PATH}` };

  await page.setViewportSize({ width: 1140, height: 700 });
  await page.setContent(terminalHtml(THEMES.dark), { waitUntil: 'networkidle' });
  await page.evaluate(() => document.fonts.ready);

  const done = [];      // rendered lines so far
  const frames = [];
  const frame = async (lines, ms) => {
    const file = path.join(TMP, `t${String(frames.length).padStart(4, '0')}.png`);
    await page.evaluate(html => { document.getElementById('screen').innerHTML = html; }, lines.slice(-22).map(l => `<div>${l || ' '}</div>`).join(''));
    await page.locator('#frame').screenshot({ path: file });
    frames.push({ file, ms });
  };
  const prompt = '<span class="p">❯</span> ';
  await frame([prompt + '<span class="cur"> </span>'], 700);
  for (const s of SCENES) {
    if (s.comment) {
      done.push(prompt + `<span class="c">${esc(s.comment)}</span>`);
      await frame([...done, prompt + '<span class="cur"> </span>'], 1800);
      continue;
    }
    for (let i = 2; i <= s.cmd.length + 1; i += 2) {
      await frame([...done, prompt + esc(s.cmd.slice(0, i)) + '<span class="cur"> </span>'], 45);
    }
    await frame([...done, prompt + esc(s.cmd)], 350);
    let out = '';
    try { out = execSync(`${s.cmd} 2>&1`, { cwd: work, env, encoding: 'utf8', shell: '/bin/sh' }); } catch (err) { out = err.stdout; }
    done.push(prompt + esc(s.cmd), ...out.replace(/\n$/, '').split('\n').filter(l => out !== '').map(colorize));
    await frame([...done, prompt + '<span class="cur"> </span>'], s.hold);
  }
  frames.at(-1).ms = 4500;
  gif(frames, 'terminal.gif', 1100);
}

// ---------------------------------------------------------------- playground

async function playground(browser) {
  const site = path.join(TMP, 'site');
  execFileSync(path.join(ROOT, 'web/build-site.sh'), [THEOSC, site], { stdio: 'ignore' });
  const types = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.wasm': 'application/wasm' };
  const server = http.createServer((req, res) => {
    const file = path.join(site, decodeURIComponent(req.url.split('?')[0]));
    if (!file.startsWith(site) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) { res.writeHead(404); res.end(); return; }
    res.writeHead(200, { 'content-type': types[path.extname(file)] ?? 'application/octet-stream' });
    fs.createReadStream(file).pipe(res);
  });
  await new Promise(r => server.listen(0, r));
  const page = await browser.newPage({ viewport: { width: 1280, height: 720 }, deviceScaleFactor: 1.5 });
  await page.goto(`http://localhost:${server.address().port}/index.html`);
  await page.waitForSelector('#graph svg', { timeout: 30000 });
  await page.evaluate(() => document.fonts.ready);
  await sleep(500);

  // Screenshot continuously while the script drives the page; each frame lasts until the next.
  const frames = [];
  let recording = true;
  const recorder = (async () => {
    while (recording) {
      const file = path.join(TMP, `p${String(frames.length).padStart(4, '0')}.png`);
      const t = Date.now();
      await page.screenshot({ path: file });
      frames.push({ file, t });
    }
  })();

  const replace = async (line, from, to) => {
    await page.evaluate(([line, from]) => {
      const cm = document.querySelector('.CodeMirror').CodeMirror;
      const ch = cm.getLine(line).lastIndexOf(from);
      cm.focus();
      cm.setSelection({ line, ch }, { line, ch: ch + from.length });
    }, [line, from]);
    await sleep(350);
    await page.keyboard.type(to, { delay: 110 });
    await sleep(500);
  };

  await sleep(1800);
  await page.selectOption('#examples', 'examples/affirming_the_consequent.theo');
  await page.waitForSelector('#verdict.fails');
  await sleep(3200);
  // Turn affirming the consequent into modus ponens, one edit at a time
  await replace(13, 'The street is wet.', 'It rains.');
  await replace(18, 'It rains.', 'The street is wet.');
  await replace(14, 'Wet', 'Rain');
  await replace(19, 'Rain', 'Wet');
  await page.waitForFunction(() => document.getElementById('verdict').textContent === 'The argument is valid.');
  await sleep(4000);
  recording = false;
  await recorder;
  server.close();

  const timed = frames.map((f, i) => ({ file: f.file, ms: i + 1 < frames.length ? frames[i + 1].t - f.t : 2500 }));
  gif(timed, 'playground.gif', 1200);
}

// ---------------------------------------------------------------- main

(async () => {
  const which = process.argv.slice(2);
  const want = name => which.length === 0 || which.includes(name);
  const proxy = process.env.HTTPS_PROXY ? { server: process.env.HTTPS_PROXY, bypass: 'localhost,127.0.0.1' } : undefined;
  const browser = await chromium.launch({ proxy });
  try {
    const page = await browser.newPage({ deviceScaleFactor: 2, viewport: { width: 1400, height: 1000 } });
    if (want('banner')) {
      for (const [name, t] of Object.entries(THEMES)) await shoot(page, bannerHtml(t), `banner-${name}.png`);
    }
    if (want('format')) {
      const file = path.join(TMP, 'tiny.theos');
      execFileSync(THEOSC, ['build', 'tests/golden/binary/tiny.theo', '-o', file], { cwd: ROOT });  // relative path, as in docs/spec/theos-v2.md
      const bytes = fs.readFileSync(file);
      for (const [name, t] of Object.entries(THEMES)) await shoot(page, formatHtml(t, bytes), `theos-format-${name}.png`);
    }
    if (want('terminal')) await terminal(await browser.newPage({ deviceScaleFactor: 1.5 }));
    if (want('playground')) await playground(browser);
  } finally {
    await browser.close();
    fs.rmSync(TMP, { recursive: true, force: true });
  }
})().catch(err => { console.error(err); process.exit(1); });
