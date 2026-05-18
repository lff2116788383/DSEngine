#!/usr/bin/env node
/**
 * AI Texture Generator Plugin for DSEngine.
 *
 * Generates PBR textures using OpenAI DALL-E 3 API and imports them into the
 * engine via the Control Server. Demonstrates:
 *   - WebSocket JSON-RPC connection to Control Server
 *   - Calling external AI APIs
 *   - Importing generated assets into the engine
 *   - Long-running plugin with heartbeat
 *
 * Usage:
 *   node index.js --prompt "seamless rust metal texture" --output textures/rust.png
 *   node index.js --interactive
 *
 * Environment:
 *   OPENAI_API_KEY - Required for DALL-E 3 API access
 */

const WebSocket = require('ws');
const fs = require('fs');
const path = require('path');

const CONTROL_SERVER_URL = 'ws://127.0.0.1:9527';

// ─── DSEngine Client ────────────────────────────────────────────────────────

class DSEngineClient {
    constructor(url = CONTROL_SERVER_URL) {
        this.url = url;
        this.ws = null;
        this._id = 0;
        this._pending = new Map();
    }

    connect() {
        return new Promise((resolve, reject) => {
            this.ws = new WebSocket(this.url);

            this.ws.on('open', () => {
                console.error('[AITexGen] Connected to Control Server.');
                resolve();
            });

            this.ws.on('error', (err) => {
                reject(new Error(`Connection failed: ${err.message}`));
            });

            this.ws.on('message', (data) => {
                const response = JSON.parse(data.toString());
                const pending = this._pending.get(response.id);
                if (pending) {
                    this._pending.delete(response.id);
                    if (response.error) {
                        pending.reject(new Error(JSON.stringify(response.error)));
                    } else {
                        pending.resolve(response.result);
                    }
                }
            });

            this.ws.on('close', () => {
                console.error('[AITexGen] Disconnected.');
            });
        });
    }

    call(method, params = {}) {
        return new Promise((resolve, reject) => {
            this._id++;
            const id = this._id;
            const request = { jsonrpc: '2.0', id, method, params };

            this._pending.set(id, { resolve, reject });
            this.ws.send(JSON.stringify(request));

            // Timeout after 30s
            setTimeout(() => {
                if (this._pending.has(id)) {
                    this._pending.delete(id);
                    reject(new Error(`Request ${method} timed out`));
                }
            }, 30000);
        });
    }

    close() {
        if (this.ws) {
            this.ws.close();
        }
    }
}

// ─── Texture Generation ─────────────────────────────────────────────────────

async function generateTexture(prompt, outputPath, options = {}) {
    const apiKey = process.env.OPENAI_API_KEY;
    if (!apiKey) {
        throw new Error('OPENAI_API_KEY environment variable not set');
    }

    const OpenAI = require('openai');
    const openai = new OpenAI({ apiKey });

    const size = options.size || '1024x1024';
    const quality = options.quality || 'standard';
    const style = options.style || 'natural';

    console.error(`[AITexGen] Generating: "${prompt}" (${size}, ${quality})`);

    const response = await openai.images.generate({
        model: 'dall-e-3',
        prompt: prompt,
        n: 1,
        size: size,
        quality: quality,
        style: style,
        response_format: 'url'
    });

    const imageUrl = response.data[0].url;
    console.error(`[AITexGen] Downloading from DALL-E...`);

    // Download the image
    const fetchResp = await fetch(imageUrl);
    if (!fetchResp.ok) {
        throw new Error(`Failed to download image: ${fetchResp.status}`);
    }

    const buffer = Buffer.from(await fetchResp.arrayBuffer());

    // Ensure output directory exists
    const dir = path.dirname(outputPath);
    if (dir) {
        fs.mkdirSync(dir, { recursive: true });
    }

    fs.writeFileSync(outputPath, buffer);
    console.error(`[AITexGen] Saved: ${outputPath} (${buffer.length} bytes)`);

    return { filePath: outputPath, size: buffer.length };
}

// ─── Main ───────────────────────────────────────────────────────────────────

async function main() {
    const args = parseArgs(process.argv.slice(2));

    if (args.help) {
        printUsage();
        process.exit(0);
    }

    const client = new DSEngineClient();

    try {
        await client.connect();

        // Verify connection
        const pong = await client.call('dsengine_ping');
        console.error(`[AITexGen] Server: ${JSON.stringify(pong)}`);

        if (args.interactive) {
            // Interactive mode: keep alive, wait for commands via stdin
            console.error('[AITexGen] Interactive mode. Type prompts or Ctrl+C to exit.');
            await runInteractive(client, args);
        } else if (args.prompt) {
            // One-shot mode: generate a single texture
            const outputPath = args.output || 'data/textures/ai_generated.png';
            const result = await generateTexture(args.prompt, outputPath, {
                size: args.size,
                quality: args.quality,
                style: args.style
            });

            // Import into engine
            const relativePath = outputPath.startsWith('data/')
                ? outputPath.slice(5)
                : outputPath;

            const importResult = await client.call('dsengine_asset_import', {
                path: relativePath,
                type: 'texture'
            });

            console.error(`[AITexGen] Imported: ${JSON.stringify(importResult)}`);
            console.error(`[AITexGen] Done. Texture ready at: ${relativePath}`);
        } else {
            console.error('[AITexGen] No --prompt or --interactive specified.');
            printUsage();
            process.exit(1);
        }

    } catch (err) {
        console.error(`[AITexGen] Error: ${err.message}`);
        process.exit(1);
    } finally {
        client.close();
    }
}

async function runInteractive(client, args) {
    const readline = require('readline');
    const rl = readline.createInterface({ input: process.stdin, output: process.stderr });

    // Heartbeat
    const heartbeat = setInterval(async () => {
        try {
            await client.call('dsengine_ping');
        } catch {
            console.error('[AITexGen] Lost connection.');
            clearInterval(heartbeat);
            process.exit(1);
        }
    }, 15000);

    rl.on('line', async (line) => {
        const prompt = line.trim();
        if (!prompt) return;

        const filename = `ai_${Date.now()}.png`;
        const outputPath = `data/textures/${filename}`;

        try {
            await generateTexture(prompt, outputPath, {
                size: args.size || '1024x1024',
                quality: args.quality || 'standard',
                style: args.style || 'natural'
            });

            await client.call('dsengine_asset_import', {
                path: `textures/${filename}`,
                type: 'texture'
            });

            console.error(`[AITexGen] Ready: textures/${filename}`);
        } catch (err) {
            console.error(`[AITexGen] Generation failed: ${err.message}`);
        }
    });

    rl.on('close', () => {
        clearInterval(heartbeat);
        process.exit(0);
    });
}

// ─── Argument parsing ───────────────────────────────────────────────────────

function parseArgs(argv) {
    const args = {};
    for (let i = 0; i < argv.length; i++) {
        switch (argv[i]) {
            case '--prompt': args.prompt = argv[++i]; break;
            case '--output': args.output = argv[++i]; break;
            case '--size': args.size = argv[++i]; break;
            case '--quality': args.quality = argv[++i]; break;
            case '--style': args.style = argv[++i]; break;
            case '--interactive': args.interactive = true; break;
            case '--help': case '-h': args.help = true; break;
        }
    }
    return args;
}

function printUsage() {
    console.error(`
AI Texture Generator Plugin for DSEngine

Usage:
  node index.js --prompt "texture description" [--output path] [options]
  node index.js --interactive

Options:
  --prompt TEXT      Texture description for DALL-E 3
  --output PATH     Output file path (default: data/textures/ai_generated.png)
  --size SIZE       Image size: 1024x1024, 512x512, 1792x1024 (default: 1024x1024)
  --quality Q       Quality: standard, hd (default: standard)
  --style S         Style: natural, vivid (default: natural)
  --interactive     Keep running, read prompts from stdin
  --help            Show this help

Environment:
  OPENAI_API_KEY    Required for DALL-E 3 API access
`);
}

main();
