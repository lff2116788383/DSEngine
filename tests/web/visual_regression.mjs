// Headless WebGL2 visual-regression harness for dse_web_host (WEB_3D_BACKEND A4).
//
// Serves the emscripten artifacts (bin/index.html + .js + .wasm + .data) over a
// local HTTP server, boots them in headless Chrome with the SwiftShader software
// WebGL2 backend (no host GPU required — works on hosted CI runners), waits until
// the engine has rendered a number of frames (via the window.__dseFrames hook in
// shell.html), screenshots the canvas, and runs two layers of checks:
//
//   1. Non-triviality guard (always on, baseline-free): the rendered frame must
//      contain real content — multiple distinct colors and a non-trivial fraction
//      of non-background pixels. This catches a dead/black/single-color WebGL2
//      path (context creation failure, shader compile failure, blank scene…)
//      without needing a committed golden image. This is the hard CI assertion.
//
//   2. Golden-image diff (optional, strict): if a baseline PNG exists it is diffed
//      against the capture with pixelmatch and fails when the mismatch ratio
//      exceeds --threshold. A missing baseline is non-fatal (the capture is saved
//      to artifacts for blessing); run with --update to (re)write baselines.
//
// Usage:
//   node visual_regression.mjs [--bin DIR] [--name NAME] [--frames N]
//                              [--threshold R] [--timeout MS] [--update]
//
// Env:
//   PUPPETEER_EXECUTABLE_PATH  use an existing Chrome/Chromium instead of the
//                              puppeteer-bundled one (local dev).

import http from 'node:http';
import { createReadStream, existsSync, mkdirSync, readFileSync, statSync, writeFileSync } from 'node:fs';
import { dirname, extname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { PNG } from 'pngjs';
import pixelmatch from 'pixelmatch';
import puppeteer from 'puppeteer';

const __dirname = dirname(fileURLToPath(import.meta.url));

function parseArgs(argv) {
  const out = {
    bin: resolve(__dirname, '..', '..', 'bin'),
    name: 'web3d',
    frames: 60,
    threshold: 0.02, // per-pixel color distance for pixelmatch (0..1)
    maxMismatchRatio: 0.02, // fail if >2% of pixels differ from baseline
    timeout: 60000,
    update: false,
    query: '', // appended to index.html URL, e.g. "mode=2d" (A5 path override)
  };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--update') out.update = true;
    else if (a === '--bin') out.bin = resolve(argv[++i]);
    else if (a === '--name') out.name = argv[++i];
    else if (a === '--frames') out.frames = parseInt(argv[++i], 10);
    else if (a === '--threshold') out.threshold = parseFloat(argv[++i]);
    else if (a === '--max-mismatch') out.maxMismatchRatio = parseFloat(argv[++i]);
    else if (a === '--timeout') out.timeout = parseInt(argv[++i], 10);
    else if (a === '--query') out.query = argv[++i].replace(/^\?/, '');
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
      // Cross-origin isolation headers (harmless single-thread; required once
      // SharedArrayBuffer/pthreads land — see WEB_3D_BACKEND multithread phase).
      res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
      res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
      res.setHeader('Content-Type', MIME[extname(filePath)] || 'application/octet-stream');
      createReadStream(filePath).pipe(res);
    });
    server.listen(0, '127.0.0.1', () => resolvePromise(server));
  });
}

// Frame-content analysis: returns distinct quantized colors and the fraction of
// pixels that differ from the dominant (background) color.
function analyzeFrame(png) {
  const { data, width, height } = png;
  const counts = new Map();
  const total = width * height;
  for (let i = 0; i < data.length; i += 4) {
    // Quantize to 5 bits/channel to ignore dithering/AA noise.
    const r = data[i] >> 3, g = data[i + 1] >> 3, b = data[i + 2] >> 3;
    const key = (r << 10) | (g << 5) | b;
    counts.set(key, (counts.get(key) || 0) + 1);
  }
  let dominant = 0, dominantCount = 0;
  for (const [key, c] of counts) {
    if (c > dominantCount) { dominantCount = c; dominant = key; }
  }
  return {
    distinctColors: counts.size,
    nonBackgroundRatio: 1 - dominantCount / total,
    dominantColor: dominant,
    width,
    height,
  };
}

async function main() {
  const opts = parseArgs(process.argv);
  const artifactsDir = join(__dirname, 'artifacts');
  const baselineDir = join(__dirname, 'baseline');
  mkdirSync(artifactsDir, { recursive: true });
  mkdirSync(baselineDir, { recursive: true });

  const indexHtml = join(opts.bin, 'index.html');
  if (!existsSync(indexHtml)) {
    console.error(`[visual] FAIL: ${indexHtml} not found — build dse_web_host first ` +
      `(cmake --build --preset web-release-3d).`);
    process.exit(2);
  }

  const server = await startServer(opts.bin);
  const port = server.address().port;
  const url = `http://127.0.0.1:${port}/index.html${opts.query ? '?' + opts.query : ''}`;
  console.log(`[visual] serving ${opts.bin} at ${url}`);

  const launchArgs = [
    '--headless=new',
    '--no-sandbox',
    '--disable-dev-shm-usage',
    '--disable-setuid-sandbox',
    // Software WebGL2 via ANGLE/SwiftShader — deterministic, GPU-free.
    '--use-gl=angle',
    '--use-angle=swiftshader',
    '--enable-unsafe-swiftshader',
    '--ignore-gpu-blocklist',
    '--window-size=1280,720',
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
    page.on('console', (m) => logs.push(`[page:${m.type()}] ${m.text()}`));
    page.on('pageerror', (e) => logs.push(`[page:error] ${e.message}`));

    await page.goto(url, { waitUntil: 'load', timeout: opts.timeout });
    await page.waitForSelector('canvas#canvas', { timeout: opts.timeout });

    // Wait until the engine has rendered enough frames (rAF hook in shell.html).
    await page.waitForFunction(
      (n) => (window.__dseFrames || 0) >= n,
      { timeout: opts.timeout, polling: 100 },
      opts.frames,
    );
    const frames = await page.evaluate(() => window.__dseFrames || 0);
    console.log(`[visual] engine rendered ${frames} frames`);

    // Capture the canvas pixels from inside the engine's rAF tick (see shell.html
    // hook): toDataURL there returns a valid frame without preserveDrawingBuffer,
    // whereas an element screenshot grabs the already-cleared default framebuffer.
    const dataUrl = await page.evaluate(async (timeoutMs) => {
      window.__dseLastFrame = null;
      window.__dseCaptureRequested = true;
      const start = performance.now();
      while (window.__dseLastFrame === null) {
        if (performance.now() - start > timeoutMs) return 'ERR:capture-timeout';
        await new Promise((r) => setTimeout(r, 16));
      }
      return window.__dseLastFrame;
    }, opts.timeout);
    if (typeof dataUrl !== 'string' || !dataUrl.startsWith('data:image/png;base64,')) {
      throw new Error(`canvas capture failed: ${dataUrl}`);
    }
    const shotBuf = Buffer.from(dataUrl.slice('data:image/png;base64,'.length), 'base64');
    const actualPath = join(artifactsDir, `${opts.name}.actual.png`);
    writeFileSync(actualPath, shotBuf);
    console.log(`[visual] captured ${actualPath}`);

    const actualPng = PNG.sync.read(shotBuf);
    const stats = analyzeFrame(actualPng);
    console.log(`[visual] frame: ${stats.width}x${stats.height}, distinctColors=` +
      `${stats.distinctColors}, nonBackgroundRatio=${stats.nonBackgroundRatio.toFixed(4)}`);

    // Layer 1: non-triviality guard (baseline-free hard assertion).
    const MIN_DISTINCT_COLORS = 8;
    const MIN_NON_BG_RATIO = 0.01;
    if (stats.distinctColors < MIN_DISTINCT_COLORS || stats.nonBackgroundRatio < MIN_NON_BG_RATIO) {
      console.error(`[visual] FAIL: rendered frame is trivial (distinctColors=` +
        `${stats.distinctColors} < ${MIN_DISTINCT_COLORS} or nonBackgroundRatio=` +
        `${stats.nonBackgroundRatio.toFixed(4)} < ${MIN_NON_BG_RATIO}). ` +
        `The WebGL2 path likely failed to render. Page logs:\n  ${logs.join('\n  ')}`);
      exitCode = 1;
    } else {
      console.log('[visual] PASS: non-triviality guard (WebGL2 frame has real content)');
    }

    // Layer 2: golden-image diff (optional/strict).
    const baselinePath = join(baselineDir, `${opts.name}.png`);
    if (opts.update || !existsSync(baselinePath)) {
      writeFileSync(baselinePath, shotBuf);
      console.log(opts.update
        ? `[visual] baseline updated: ${baselinePath}`
        : `[visual] no baseline — saved capture as new baseline: ${baselinePath} ` +
          `(commit to enable strict diffing).`);
    } else {
      const baselinePng = PNG.sync.read(readFileSync(baselinePath));
      if (baselinePng.width !== actualPng.width || baselinePng.height !== actualPng.height) {
        console.error(`[visual] FAIL: baseline size ${baselinePng.width}x${baselinePng.height} ` +
          `!= capture ${actualPng.width}x${actualPng.height}`);
        exitCode = 1;
      } else {
        const diff = new PNG({ width: actualPng.width, height: actualPng.height });
        const mismatched = pixelmatch(
          baselinePng.data, actualPng.data, diff.data,
          actualPng.width, actualPng.height, { threshold: opts.threshold },
        );
        const ratio = mismatched / (actualPng.width * actualPng.height);
        const diffPath = join(artifactsDir, `${opts.name}.diff.png`);
        writeFileSync(diffPath, PNG.sync.write(diff));
        console.log(`[visual] golden diff: ${mismatched} px (${(ratio * 100).toFixed(3)}%) ` +
          `vs max ${(opts.maxMismatchRatio * 100).toFixed(3)}% -> ${diffPath}`);
        if (ratio > opts.maxMismatchRatio) {
          console.error(`[visual] FAIL: golden-image mismatch ${(ratio * 100).toFixed(3)}% ` +
            `exceeds ${(opts.maxMismatchRatio * 100).toFixed(3)}%`);
          exitCode = 1;
        } else {
          console.log('[visual] PASS: golden-image diff within tolerance');
        }
      }
    }
  } catch (err) {
    console.error(`[visual] FAIL: ${err && err.stack ? err.stack : err}`);
    exitCode = 1;
  } finally {
    await browser.close();
    server.close();
  }
  process.exit(exitCode);
}

main();
