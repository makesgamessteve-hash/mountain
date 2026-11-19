// Mountain - web build smoke test (headless Chromium via Playwright).
// Loads the WebAssembly build, checks that frames render on desktop, that a
// mobile/touch device gets the touch controls, and that the Enter VR button
// appears when WebXR immersive-vr is supported (mocked). MIT License.
// Usage: node tests/web/smoke.cjs <dir with mountain.html> [screenshot dir]
const { chromium, devices } = require('playwright');
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');

(async () => {

const root = path.resolve(process.argv[2] || 'build/web/bin');
const outDir = path.resolve(process.argv[3] || 'build/web-smoke');
fs.mkdirSync(outDir, { recursive: true });
const types = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.data': 'application/octet-stream' };
const server = http.createServer((req, res) => {
  const p = path.join(root, decodeURIComponent(req.url.split('?')[0]).replace(/^\/+/, '') || 'mountain.html');
  if (!p.startsWith(root) || !fs.existsSync(p)) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { 'Content-Type': types[path.extname(p)] || 'application/octet-stream' });
  fs.createReadStream(p).pipe(res);
});
await new Promise(r => server.listen(0, r));
const url = `http://127.0.0.1:${server.address().port}/mountain.html`;
const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
let failures = 0;
const check = (cond, msg) => { console.log((cond ? 'ok   ' : 'FAIL ') + msg); if (!cond) failures++; };

async function run(name, contextOptions, initScript) {
  const ctx = await browser.newContext(contextOptions);
  if (initScript) await ctx.addInitScript(initScript);
  const page = await ctx.newPage();
  const logs = [];
  page.on('console', m => logs.push(m.text()));
  page.on('pageerror', e => logs.push('pageerror: ' + e.message));
  await page.goto(url);
  try {
    await page.waitForFunction(() => window.mtnReady === true, null, { timeout: 120000 });
  } catch (e) {
    console.log(logs.join('\n'));
    throw e;
  }
  await page.waitForTimeout(4000);
  const file = path.join(outDir, `${name}.png`);
  await page.screenshot({ path: file });
  const errors = logs.filter(l => /pageerror|script\]|Aborted|RuntimeError/.test(l));
  check(errors.length === 0, `${name}: no script or runtime errors ${errors.slice(0, 3).join(' | ')}`);
  return { ctx, page, logs };
}

// desktop
{
  const { ctx, page } = await run('desktop', { viewport: { width: 1280, height: 720 } });
  const pixels = await page.evaluate(() => {
    const c = document.getElementById('canvas');
    return { w: c.width, h: c.height };
  });
  check(pixels.w >= 640 && pixels.h >= 360, `desktop canvas fills the window (${pixels.w}x${pixels.h})`);
  const vrHidden = await page.evaluate(() => getComputedStyle(document.getElementById('vr-button')).display === 'none');
  check(vrHidden, 'desktop without WebXR: no Enter VR button');
  await page.keyboard.down('w');
  await page.waitForTimeout(1500);
  await page.keyboard.up('w');
  await page.screenshot({ path: path.join(outDir, 'desktop_walked.png') });
  await ctx.close();
}

// mobile phone with touch
{
  const { ctx, page } = await run('mobile', { ...devices['Pixel 7'] });
  await page.touchscreen.tap(100, 300);
  await page.waitForTimeout(1500);
  await page.screenshot({ path: path.join(outDir, 'mobile_touch.png') });
  await ctx.close();
}

// WebXR supported (mocked): the Enter VR button must appear
{
  const mock = () => {
    Object.defineProperty(navigator, 'xr', { value: { isSessionSupported: (m) => Promise.resolve(m === 'immersive-vr'),
      requestSession: () => Promise.reject(new Error('mock')) }, configurable: true });
  };
  const { ctx, page } = await run('webxr', { viewport: { width: 1024, height: 600 } }, mock);
  const shown = await page.evaluate(() => getComputedStyle(document.getElementById('vr-button')).display !== 'none');
  check(shown, 'WebXR supported: Enter VR button shown');
  await ctx.close();
}

await browser.close();
server.close();
console.log(failures ? `${failures} web smoke check(s) failed` : 'web smoke test passed');
process.exit(failures ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
