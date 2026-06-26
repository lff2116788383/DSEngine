// Headless WebGPU self-test harness runner for dse_web_host (DSE_WEBGPU_SELFTEST=ON build).
//
// The engine, when compiled with -DDSE_WEBGPU_SELFTEST=ON, instantiates an
// offline WebGpuSelfTestHarness that — once per session, on the first idle
// frame — records a battery of isolated offscreen self-tests (compute, GPU
// skinning, Hi-Z, mega-VAO, indirect draw, CSM / point-light cube shadow,
// deferred, HDR, IBL, WBOIT, …), reads their results back asynchronously and
// logs "...自检 PASS" / "...自检 FAIL" for each.  These exercise exactly the
// web-only hot paths that the single-scene golden-image regression does NOT
// cover, so this runner boots that build under Dawn + SwiftShader Vulkan
// (software, deterministic, no host GPU) and fails CI if any self-test logs
// FAIL or if the expected PASS lines never appear.
//
// Usage:
//   node selftest_harness.mjs [--bin DIR] [--frames N] [--timeout MS]
//
// Env:
//   PUPPETEER_EXECUTABLE_PATH  use an existing Chrome/Chromium instead of the
//                              puppeteer-bundled one (local dev).
//   DSE_DUMP_LOGS=1            print every captured page log line.

import http from 'node:http';
import { createReadStream, existsSync, statSync } from 'node:fs';
import { dirname, extname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import puppeteer from 'puppeteer';

const __dirname = dirname(fileURLToPath(import.meta.url));

function parseArgs(argv) {
  const out = {
    bin: resolve(__dirname, '..', '..', 'bin'),
    frames: 240,        // self-tests record on first idle frame, async readbacks settle within a few frames
    timeout: 120000,
  };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--bin') out.bin = resolve(argv[++i]);
    else if (a === '--frames') out.frames = parseInt(argv[++i], 10);
    else if (a === '--timeout') out.timeout = parseInt(argv[++i], 10);
    else throw new Error(`Unknown argument: ${a}`);
  }
  return out;
}

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.wasm': 'application/wasm',
  '.data': 'application/octet-stream',
  '.json': 'application/json',
  '.png': 'image/png',
};

function startServer(rootDir) {
  return new Promise((resolvePromise) => {
    const server = http.createServer((req, res) => {
      const urlPath = decodeURIComponent((req.url || '/').split('?')[0]);
      const rel = urlPath === '/' ? '/index.html' : urlPath;
      const filePath = join(rootDir, rel);
      if (!filePath.startsWith(rootDir) || !existsSync(filePath) || !statSync(filePath).isFile()) {
        res.writeHead(404);
        res.end('not found');
        return;
      }
      res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
      res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
      res.setHeader('Content-Type', MIME[extname(filePath)] || 'application/octet-stream');
      createReadStream(filePath).pipe(res);
    });
    server.listen(0, '127.0.0.1', () => resolvePromise(server));
  });
}

// A self-test result line looks like:  "...WebGPU[<id>] <desc>自检 PASS：..." or
// "...WebGPU[<id>] <desc>自检 FAIL...".  Extract the bracketed id + verdict.
const RESULT_RE = /WebGPU\[([^\]]+)\][^\n]*?自检\s*(PASS|FAIL)/;

async function main() {
  const opts = parseArgs(process.argv);

  const indexHtml = join(opts.bin, 'index.html');
  if (!existsSync(indexHtml)) {
    console.error(`[selftest] FAIL: ${indexHtml} not found — build the SELFTEST harness first ` +
      `(cmake -DDSE_WEBGPU_SELFTEST=ON ... && cmake --build ...).`);
    process.exit(2);
  }

  const server = await startServer(opts.bin);
  const port = server.address().port;
  const url = `http://127.0.0.1:${port}/index.html?backend=webgpu`;
  console.log(`[selftest] serving ${opts.bin} at ${url}`);

  const launchArgs = [
    '--headless=new',
    '--no-sandbox',
    '--disable-dev-shm-usage',
    '--disable-setuid-sandbox',
    '--enable-unsafe-swiftshader',
    '--ignore-gpu-blocklist',
    '--window-size=1280,720',
    '--enable-unsafe-webgpu',
    '--enable-features=Vulkan',
    '--use-vulkan=swiftshader',
  ];

  const browser = await puppeteer.launch({
    headless: 'new',
    executablePath: process.env.PUPPETEER_EXECUTABLE_PATH || undefined,
    args: launchArgs,
  });

  let exitCode = 0;
  try {
    const page = await browser.newPage();
    await page.setViewport({ width: 1280, height: 720, deviceScaleFactor: 1 });
    const logs = [];
    page.on('console', (m) => logs.push(m.text()));
    page.on('pageerror', (e) => logs.push(`[page:error] ${e.message}`));

    await page.goto(url, { waitUntil: 'load', timeout: opts.timeout });
    await page.waitForSelector('canvas#canvas', { timeout: opts.timeout });

    // Drive enough frames for the harness to record + async-read-back every test.
    await page.waitForFunction(
      (n) => (window.__dseFrames || 0) >= n,
      { timeout: opts.timeout, polling: 100 },
      opts.frames,
    );
    const frames = await page.evaluate(() => window.__dseFrames || 0);
    console.log(`[selftest] engine rendered ${frames} frames`);

    // Give async map callbacks a final moment to flush their PASS/FAIL logs.
    await new Promise((r) => setTimeout(r, 1500));

    if (process.env.DSE_DUMP_LOGS) console.log('PAGELOGS>>>\n' + logs.join('\n') + '\n<<<PAGELOGS');

    // Tally self-test verdicts.
    const results = new Map(); // id -> 'PASS' | 'FAIL'
    for (const line of logs) {
      const m = line.match(RESULT_RE);
      if (m) {
        const id = m[1], verdict = m[2];
        // A FAIL must stick even if a later line for the same id says otherwise.
        if (results.get(id) !== 'FAIL') results.set(id, verdict);
      }
    }

    const passed = [...results.entries()].filter(([, v]) => v === 'PASS').map(([id]) => id);
    const failed = [...results.entries()].filter(([, v]) => v === 'FAIL').map(([id]) => id);

    console.log(`[selftest] self-tests reported: ${results.size} ` +
      `(PASS=${passed.length}, FAIL=${failed.length})`);
    console.log(`[selftest] PASS ids: ${passed.sort().join(', ') || '(none)'}`);

    // Hard gate 1: no self-test may FAIL.
    if (failed.length) {
      console.error(`[selftest] FAIL: ${failed.length} self-test(s) reported FAIL: ${failed.sort().join(', ')}`);
      for (const line of logs) {
        if (/自检\s*FAIL/.test(line) || /\[T5-6\]/.test(line)) console.error(`  > ${line}`);
      }
      exitCode = 1;
    }

    // Hard gate 2: the harness must actually have run (guards against a silent
    // no-op build / WebGPU device that never came up).
    if (results.size === 0) {
      console.error('[selftest] FAIL: no self-test result lines captured — the harness never ran. ' +
        'Page logs:\n  ' + logs.join('\n  '));
      exitCode = 1;
    }

    // Hard gate 3: the new point-light cube true-shadow occlusion test must be present and pass.
    if (!results.has('T5-6')) {
      console.error('[selftest] FAIL: point-light cube shadow self-test [T5-6] never reported a result.');
      exitCode = 1;
    } else if (results.get('T5-6') === 'PASS') {
      console.log('[selftest] PASS: point-light cube true-shadow occlusion self-test [T5-6] verified.');
    }

    if (exitCode === 0) console.log(`[selftest] PASS: all ${passed.length} WebGPU self-tests verified.`);
  } catch (err) {
    console.error(`[selftest] FAIL: ${err && err.stack ? err.stack : err}`);
    exitCode = 1;
  } finally {
    await browser.close();
    server.close();
  }
  process.exit(exitCode);
}

main();
