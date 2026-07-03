// dse-publish-server/server.js
// DSEngine One-Click Publish Server v2.0
// Security: API Key auth, rate limiting, path traversal protection, file type whitelist

const express = require('express');
const multer = require('multer');
const AdmZip = require('adm-zip');
const crypto = require('crypto');
const path = require('path');
const fs = require('fs');
const rateLimit = require('express-rate-limit');

const app = express();
const GAMES_DIR = process.env.GAMES_DIR || '/var/www/games';
const MAX_SIZE_MB = 200;

// API Key list (production: from env var or database)
const VALID_API_KEYS = new Set(
    (process.env.DSE_API_KEYS || '').split(',').filter(Boolean)
);

// File extension whitelist (Web resources only)
const ALLOWED_EXTENSIONS = new Set([
    '.html', '.htm', '.js', '.mjs', '.wasm', '.data',
    '.css', '.json', '.png', '.jpg', '.jpeg', '.gif',
    '.svg', '.ico', '.webp', '.mp3', '.ogg', '.wav',
    '.woff', '.woff2', '.ttf', '.map', '.txt'
]);

// Ensure games directory exists
fs.mkdirSync(GAMES_DIR, { recursive: true });

// Upload storage config
const upload = multer({
    dest: '/tmp/dse_uploads/',
    limits: { fileSize: MAX_SIZE_MB * 1024 * 1024 }
});

// â”€â”€ Rate limiting middleware â”€â”€
const publishLimiter = rateLimit({
    windowMs: 60 * 60 * 1000,  // 1 hour window
    max: 5,                     // max 5 uploads per IP per hour
    message: { error: 'Rate limit exceeded. Please try again later.' },
    standardHeaders: true,
    legacyHeaders: false,
});

// â”€â”€ Authentication middleware â”€â”€
function requireApiKey(req, res, next) {
    const key = req.headers['x-api-key'] || '';
    if (!VALID_API_KEYS.size) {
        // No keys configured = development mode (warn but allow)
        console.warn('WARNING: No API keys configured (DSE_API_KEYS empty). All requests accepted.');
        return next();
    }
    if (!VALID_API_KEYS.has(key)) {
        return res.status(401).json({ error: 'Invalid API Key' });
    }
    next();
}

// â”€â”€ Path safety check â”€â”€
function isPathSafe(filePath, baseDir) {
    const resolved = path.resolve(baseDir, filePath);
    return resolved.startsWith(path.resolve(baseDir) + path.sep) ||
           resolved === path.resolve(baseDir);
}

// â”€â”€ File extension check â”€â”€
function isFileAllowed(filename) {
    const ext = path.extname(filename).toLowerCase();
    // Allow files with no extension (e.g. LICENSE) or whitelisted extensions
    return ext === '' || ALLOWED_EXTENSIONS.has(ext);
}

// â”€â”€ POST /api/publish â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
app.post('/api/publish', publishLimiter, requireApiKey, upload.single('package'), (req, res) => {
    try {
        const file = req.file;
        if (!file) {
            return res.status(400).json({ error: 'Missing package file' });
        }

        // Determine game ID
        let gameId = req.body.game_id || '';
        if (!gameId || !/^[a-z0-9_-]{1,64}$/i.test(gameId)) {
            gameId = crypto.randomBytes(4).toString('hex');
        }

        const gameDir = path.join(GAMES_DIR, gameId);

        // Pre-extraction security checks
        const zip = new AdmZip(file.path);
        const entries = zip.getEntries();

        for (const entry of entries) {
            // Path traversal check
            if (!isPathSafe(entry.entryName, gameDir)) {
                fs.unlinkSync(file.path);
                return res.status(400).json({
                    error: `Illegal path detected: ${entry.entryName}`
                });
            }
            // File type check (skip directories)
            if (!entry.isDirectory && !isFileAllowed(entry.entryName)) {
                fs.unlinkSync(file.path);
                return res.status(400).json({
                    error: `Disallowed file type: ${entry.entryName}`
                });
            }
        }

        // Remove old version if exists (overwrite update)
        if (fs.existsSync(gameDir)) {
            fs.rmSync(gameDir, { recursive: true });
        }

        // Extract to /var/www/games/{gameId}/
        zip.extractAllTo(gameDir, true);

        // Validate: must contain index.html
        if (!fs.existsSync(path.join(gameDir, 'index.html'))) {
            fs.rmSync(gameDir, { recursive: true });
            fs.unlinkSync(file.path);
            return res.status(400).json({
                error: 'Package missing index.html. Ensure Web build succeeded.'
            });
        }

        // Write metadata
        const meta = {
            title: req.body.title || gameId,
            game_id: gameId,
            created_at: new Date().toISOString(),
            updated_at: new Date().toISOString(),
            size_bytes: file.size,
        };
        fs.writeFileSync(
            path.join(gameDir, '.dse_meta.json'),
            JSON.stringify(meta, null, 2)
        );

        // Clean up temp upload
        fs.unlinkSync(file.path);

        // Respond with URL
        const baseHost = process.env.DSE_HOST || 'dse.run';
        res.json({
            url: `https://${gameId}.${baseHost}`,
            game_id: gameId,
            title: meta.title,
        });

    } catch (err) {
        console.error('Publish error:', err);
        if (req.file && fs.existsSync(req.file.path)) {
            fs.unlinkSync(req.file.path);
        }
        res.status(500).json({ error: 'Internal server error' });
    }
});

// â”€â”€ GET /api/games â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
app.get('/api/games', requireApiKey, (req, res) => {
    const games = [];
    if (fs.existsSync(GAMES_DIR)) {
        for (const entry of fs.readdirSync(GAMES_DIR)) {
            const metaPath = path.join(GAMES_DIR, entry, '.dse_meta.json');
            if (fs.existsSync(metaPath)) {
                try {
                    const meta = JSON.parse(fs.readFileSync(metaPath, 'utf-8'));
                    games.push(meta);
                } catch { /* skip corrupted meta */ }
            }
        }
    }
    res.json({ games });
});

// â”€â”€ DELETE /api/games/:id â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
app.delete('/api/games/:id', requireApiKey, (req, res) => {
    const id = req.params.id;
    if (!/^[a-z0-9_-]{1,64}$/i.test(id)) {
        return res.status(400).json({ error: 'Invalid game ID' });
    }
    const gameDir = path.join(GAMES_DIR, id);
    if (!fs.existsSync(gameDir)) {
        return res.status(404).json({ error: 'Game not found' });
    }
    fs.rmSync(gameDir, { recursive: true });
    res.json({ success: true });
});

// â”€â”€ Health check â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
app.get('/api/health', (req, res) => {
    res.json({ status: 'ok', version: '2.0.0' });
});

// â”€â”€ Start server â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
const PORT = process.env.PORT || 8080;
app.listen(PORT, () => {
    console.log(`DSE Publish Server v2.0 running on port ${PORT}`);
    console.log(`Games directory: ${GAMES_DIR}`);
    console.log(`API keys configured: ${VALID_API_KEYS.size}`);
});
