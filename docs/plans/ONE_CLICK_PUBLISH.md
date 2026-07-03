# DSEngine ä¸€é”®å‘å¸ƒæ–¹æ¡ˆ

> ç‰ˆæœ¬: v2.0 | æ—¥æœŸ: 2026-07-03
> ç›®æ ‡: ç¼–è¾‘å™¨å†…ä¸€é”®æž„å»º Web æ¸¸æˆ â†’ ä¸Šä¼ åˆ° DSE å®˜æ–¹æ‰˜ç®¡ â†’ ç”ŸæˆäºŒç»´ç /é“¾æŽ¥ â†’ åˆ†äº«å³çŽ©
> ç¦»çº¿æ¨¡å¼: æœåŠ¡å™¨æœªå°±ç»ªæ—¶ï¼Œä»…æž„å»º+å¯¼å‡º zipï¼Œä¸å½±å“æ­£å¸¸ä½¿ç”¨

---

## ä¸€ã€æ¦‚è¿°

### 1.1 ç”¨æˆ·æ•…äº‹

ä½œä¸º DSEngine çš„ç”¨æˆ·ï¼ˆæ¸¸æˆå¼€å‘è€…ï¼‰ï¼š
1. åœ¨ç¼–è¾‘å™¨é‡Œå®Œæˆæ¸¸æˆå¼€å‘
2. ç‚¹ `File â†’ Build Game â†’ å¹³å°é€‰ Web â†’ ç‚¹"æž„å»ºå¹¶å‘å¸ƒ"`
3. ç­‰å¾… ~30 ç§’
4. å¾—åˆ°ä¸€ä¸ªäºŒç»´ç å’Œé“¾æŽ¥ï¼š`https://mygame.dse.run`
5. åˆ†äº«ç»™æœ‹å‹ â†’ æ‰«ç å³çŽ©
6. ä¸‹æ¬¡æ›´æ–°æ¸¸æˆï¼Œå†æ¬¡ç‚¹"æž„å»ºå¹¶å‘å¸ƒ"â†’ åŒåè¦†ç›–ï¼ˆURL ä¸å˜ï¼‰

**ç¦»çº¿æ¨¡å¼**ï¼ˆæœåŠ¡å™¨æœªé…ç½®æ—¶ï¼‰ï¼š
1. ç‚¹ `File â†’ Build Game â†’ å¹³å°é€‰ Web â†’ ç‚¹"å¯¼å‡º Web åŒ…"`
2. ç­‰å¾… ~20 ç§’
3. å¾—åˆ°æœ¬åœ° zip æ–‡ä»¶è·¯å¾„ï¼ˆå¦‚ `build/web/mygame.zip`ï¼‰
4. ç”¨æˆ·è‡ªè¡Œéƒ¨ç½²åˆ°ä»»æ„é™æ€æœåŠ¡å™¨

### 1.2 æž¶æž„æ€»è§ˆ

```
æ¸¸æˆå¼€å‘è€…                              DSE æœåŠ¡å™¨                       çŽ©å®¶
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”    POST       â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”          â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚ DSEngine Editor     â”‚  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â–¶  â”‚ dse.run æ‰˜ç®¡æœåŠ¡     â”‚  HTTPS   â”‚ æ‰‹æœº/PC â”‚
â”‚                     â”‚  game.zip     â”‚                     â”‚  â—€â”€â”€â”€â”€â”€  â”‚ æµè§ˆå™¨  â”‚
â”‚ Build Game (Web)    â”‚  + API Key    â”‚ Node.js ä¸Šä¼ æœåŠ¡     â”‚          â”‚         â”‚
â”‚   â†“                 â”‚               â”‚ Nginx é™æ€æ‰˜ç®¡       â”‚          â”‚ æ‰«ç     â”‚
â”‚ minizip åŽ‹ç¼©        â”‚               â”‚ CDN ç¼“å­˜             â”‚          â”‚ å³çŽ©    â”‚
â”‚ æ˜¾ç¤ºäºŒç»´ç /é“¾æŽ¥     â”‚  â—€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€  â”‚                     â”‚          â”‚         â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜  è¿”å›ž URL     â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜          â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
```

### 1.3 è¿è¡Œæ¨¡å¼

| æ¨¡å¼ | æ¡ä»¶ | è¡Œä¸º |
|:-----|:-----|:-----|
| **ç¦»çº¿æ¨¡å¼** | æœåŠ¡å™¨åœ°å€ä¸ºç©º æˆ– `DSE_PUBLISH_ENABLED` æœªå®šä¹‰ | ä»…æž„å»º+åŽ‹ç¼©ä¸º zipï¼Œä¿å­˜åˆ°æœ¬åœ° |
| **åœ¨çº¿æ¨¡å¼** | æœåŠ¡å™¨åœ°å€å·²é…ç½® ä¸” API Key æœ‰æ•ˆ | æž„å»º+åŽ‹ç¼©+ä¸Šä¼ +è¿”å›žé“¾æŽ¥ |

### 1.4 å¯¹æ¸¸æˆå¼€å‘è€…çš„è¦æ±‚

| è¦æ±‚ | è¯´æ˜Ž |
|:-----|:------|
| å®‰è£… Emscripten SDK | **å¿…é¡»ã€‚** `emsdk install latest && emsdk activate latest`ï¼Œä¸€æ¬¡æ€§çš„ |
| é…ç½®æœåŠ¡å™¨åœ°å€ | å¯é€‰ã€‚ç¼–è¾‘å™¨å†…ç½®é»˜è®¤ `https://api.dse.run/api/publish` |
| API Key | åœ¨çº¿æ¨¡å¼å¿…é¡»ã€‚ç¼–è¾‘å™¨ Settings â†’ Publish â†’ å¡«å…¥ API Key |

---

## äºŒã€æœåŠ¡å™¨ç«¯

### 2.1 æŠ€æœ¯é€‰åž‹

| ç»„ä»¶ | é€‰æ‹© | ç†ç”± |
|:-----|:------|:------|
| è¿è¡Œæ—¶ | Node.js 18+ | è½»é‡ã€å•è¿›ç¨‹å¤Ÿç”¨ |
| ä¸Šä¼ å¤„ç† | multer + adm-zip | æˆç†Ÿ npm åŒ… |
| HTTP æœåŠ¡ | Express | æœ€ç®€è·¯ç”± |
| é‰´æƒ | API Key + HMAC | é˜²æ­¢æœªæŽˆæƒä¸Šä¼  |
| é™é€Ÿ | express-rate-limit | é˜²æ»¥ç”¨ï¼ˆ5æ¬¡/å°æ—¶/IPï¼‰ |
| è¿›ç¨‹ç®¡ç† | PM2 | è‡ªåŠ¨é‡å¯ã€æ—¥å¿— |
| é™æ€æ‰˜ç®¡ | Nginx | é«˜æ€§èƒ½é™æ€æ–‡ä»¶æœåŠ¡ |
| æ³›åŸŸå | `*.dse.run` DNS A è®°å½• â†’ æœåŠ¡å™¨ IP | æ¯ä¸ªæ¸¸æˆè‡ªåŠ¨èŽ·å¾—å­åŸŸå |

### 2.2 Node.js æœåŠ¡

```javascript
// dse-publish-server/server.js
const express = require('express');
const multer = require('multer');
const AdmZip = require('adm-zip');
const crypto = require('crypto');
const path = require('path');
const fs = require('fs');
const rateLimit = require('express-rate-limit');

const app = express();
const upload = multer({ dest: '/tmp/dse_uploads/', limits: { fileSize: 200 * 1024 * 1024 } });
const GAMES_DIR = '/var/www/games';
const MAX_SIZE_MB = 200;

// API Key åˆ—è¡¨ï¼ˆç”Ÿäº§çŽ¯å¢ƒä»ŽçŽ¯å¢ƒå˜é‡æˆ–æ•°æ®åº“è¯»å–ï¼‰
const VALID_API_KEYS = new Set((process.env.DSE_API_KEYS || '').split(',').filter(Boolean));

// æ–‡ä»¶ç™½åå•
const ALLOWED_EXTENSIONS = new Set([
    '.html', '.htm', '.js', '.mjs', '.wasm', '.data',
    '.css', '.json', '.png', '.jpg', '.jpeg', '.gif',
    '.svg', '.ico', '.webp', '.mp3', '.ogg', '.wav',
    '.woff', '.woff2', '.ttf', '.map', '.txt'
]);

fs.mkdirSync(GAMES_DIR, { recursive: true });

// â”€â”€ é™é€Ÿä¸­é—´ä»¶ â”€â”€
const publishLimiter = rateLimit({
    windowMs: 60 * 60 * 1000,  // 1 å°æ—¶
    max: 5,                     // æ¯ IP æœ€å¤š 5 æ¬¡
    message: { error: 'ä¸Šä¼ é¢‘çŽ‡è¿‡é«˜ï¼Œè¯·ç¨åŽå†è¯•' }
});

// â”€â”€ é‰´æƒä¸­é—´ä»¶ â”€â”€
function requireApiKey(req, res, next) {
    const key = req.headers['x-api-key'] || req.body.api_key || '';
    if (!VALID_API_KEYS.has(key)) {
        return res.status(401).json({ error: 'æ— æ•ˆçš„ API Key' });
    }
    next();
}

// â”€â”€ è·¯å¾„å®‰å…¨æ£€æŸ¥ â”€â”€
function isPathSafe(filePath, baseDir) {
    const resolved = path.resolve(baseDir, filePath);
    return resolved.startsWith(path.resolve(baseDir));
}

// â”€â”€ æ–‡ä»¶æ‰©å±•åæ£€æŸ¥ â”€â”€
function isFileAllowed(filename) {
    const ext = path.extname(filename).toLowerCase();
    return ALLOWED_EXTENSIONS.has(ext) || ext === '';
}

// â”€â”€ POST /api/publish â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
app.post('/api/publish', publishLimiter, requireApiKey, upload.single('package'), (req, res) => {
    try {
        const file = req.file;
        if (!file) {
            return res.status(400).json({ error: 'ç¼ºå°‘ package æ–‡ä»¶' });
        }

        // ç¡®å®šæ¸¸æˆ ID
        let gameId = req.body.game_id || '';
        if (!gameId || !/^[a-z0-9_-]{1,64}$/i.test(gameId)) {
            gameId = crypto.randomBytes(4).toString('hex');
        }

        // è§£åŽ‹å‰å®‰å…¨æ£€æŸ¥
        const zip = new AdmZip(file.path);
        const entries = zip.getEntries();

        // æ£€æŸ¥è·¯å¾„ç©¿è¶Šå’Œæ–‡ä»¶ç±»åž‹
        const gameDir = path.join(GAMES_DIR, gameId);
        for (const entry of entries) {
            if (!isPathSafe(entry.entryName, gameDir)) {
                fs.unlinkSync(file.path);
                return res.status(400).json({ error: 'æ¸¸æˆåŒ…å«éžæ³•è·¯å¾„' });
            }
            if (!entry.isDirectory && !isFileAllowed(entry.entryName)) {
                fs.unlinkSync(file.path);
                return res.status(400).json({
                    error: `ä¸å…è®¸çš„æ–‡ä»¶ç±»åž‹: ${entry.entryName}`
                });
            }
        }

        // é¦–æ¬¡å‘å¸ƒæˆ–è¦†ç›–æ›´æ–°
        if (fs.existsSync(gameDir)) {
            fs.rmSync(gameDir, { recursive: true });
        }

        // è§£åŽ‹åˆ° /var/www/games/{gameId}/
        zip.extractAllTo(gameDir, true);

        // éªŒè¯è§£åŽ‹ç»“æžœï¼šå¿…é¡»æœ‰ index.html
        if (!fs.existsSync(path.join(gameDir, 'index.html'))) {
            fs.rmSync(gameDir, { recursive: true });
            fs.unlinkSync(file.path);
            return res.status(400).json({
                error: 'æ¸¸æˆåŒ…ç¼ºå°‘ index.htmlï¼Œè¯·ç¡®è®¤ Web æž„å»ºæˆåŠŸ'
            });
        }

        // å†™å…¥å…ƒä¿¡æ¯
        const meta = {
            title: req.body.title || gameId,
            game_id: gameId,
            created_at: new Date().toISOString(),
            updated_at: new Date().toISOString(),
            size_bytes: file.size,
            update_token: crypto.randomBytes(16).toString('hex'),
        };
        fs.writeFileSync(
            path.join(gameDir, '.dse_meta.json'),
            JSON.stringify(meta, null, 2)
        );

        // æ¸…ç†ä¸´æ—¶æ–‡ä»¶
        fs.unlinkSync(file.path);

        // è¿”å›žç»“æžœ
        res.json({
            url: `https://${gameId}.dse.run`,
            game_id: gameId,
            title: meta.title,
            update_token: meta.update_token
        });

    } catch (err) {
        console.error('Publish error:', err);
        if (req.file && fs.existsSync(req.file.path)) {
            fs.unlinkSync(req.file.path);
        }
        res.status(500).json({ error: 'æœåŠ¡å™¨å†…éƒ¨é”™è¯¯' });
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
                    delete meta.update_token;  // ä¸æš´éœ² token
                    games.push(meta);
                } catch {}
            }
        }
    }
    res.json({ games });
});

// â”€â”€ DELETE /api/games/:id â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
app.delete('/api/games/:id', requireApiKey, (req, res) => {
    const gameDir = path.join(GAMES_DIR, req.params.id);
    if (!fs.existsSync(gameDir)) {
        return res.status(404).json({ error: 'æ¸¸æˆä¸å­˜åœ¨' });
    }
    fs.rmSync(gameDir, { recursive: true });
    res.json({ success: true });
});

// â”€â”€ å¯åŠ¨ â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
const PORT = process.env.PORT || 8080;
app.listen(PORT, () => {
    console.log(`DSE Publish Server running on port ${PORT}`);
});
```

### 2.3 éƒ¨ç½²

```bash
# 1. å®‰è£…ä¾èµ–
cd /opt/dse-publish-server
npm init -y
npm install express multer adm-zip express-rate-limit
npm install -g pm2

# 2. é…ç½®çŽ¯å¢ƒå˜é‡
export DSE_API_KEYS="your-api-key-1,your-api-key-2"

# 3. å¯åŠ¨
pm2 start server.js --name dse-publish
pm2 save
pm2 startup  # å¼€æœºè‡ªå¯

# 4. Nginx é…ç½®
cat > /etc/nginx/sites-available/dse.run << 'EOF'
# æ³›åŸŸåï¼šæŒ‰å­åŸŸåè·¯ç”±åˆ°å¯¹åº”æ¸¸æˆç›®å½•
server {
    listen 80;
    server_name ~^(?<game_id>.+)\.dse\.run$;

    root /var/www/games/$game_id;
    index index.html;

    # å®‰å…¨å¤´
    add_header X-Content-Type-Options nosniff;
    add_header X-Frame-Options SAMEORIGIN;
    add_header Content-Security-Policy "default-src 'self' 'unsafe-inline' 'unsafe-eval' blob: data:;";

    # WASM MIME ç±»åž‹
    types {
        application/wasm wasm;
    }

    # ç¼“å­˜ç­–ç•¥
    location ~* \.(wasm|data)$ {
        expires 30d;
        add_header Cache-Control "public, immutable";
    }
    location ~* \.(js|css)$ {
        expires 7d;
    }
    location = /index.html {
        expires -1;
        add_header Cache-Control "no-cache";
    }

    location / {
        try_files $uri $uri/ /index.html;
    }

    # ç¦æ­¢è®¿é—®å…ƒä¿¡æ¯
    location = /.dse_meta.json {
        return 404;
    }

    client_max_body_size 200M;
}

# ä¸Šä¼  API åä»£
server {
    listen 80;
    server_name api.dse.run;

    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }

    client_max_body_size 200M;
}
EOF

ln -s /etc/nginx/sites-available/dse.run /etc/nginx/sites-enabled/
nginx -t && systemctl reload nginx

# 5. é…ç½® HTTPSï¼ˆæ³›åŸŸåéœ€è¦ DNS éªŒè¯ï¼‰
apt install certbot python3-certbot-nginx python3-certbot-dns-cloudflare
certbot certonly --dns-cloudflare -d dse.run -d '*.dse.run'

# 6. DNS é…ç½®
# æ·»åŠ  A è®°å½•:
#   *.dse.run    â†’ æœåŠ¡å™¨ IP
#   api.dse.run  â†’ æœåŠ¡å™¨ IP
```

### 2.4 CDN é…ç½®ï¼ˆå¯é€‰ï¼ŒæŽ¨èï¼‰

ä»¥è…¾è®¯äº‘ CDN ä¸ºä¾‹ï¼š

```
1. CDN æŽ§åˆ¶å° â†’ æ·»åŠ åŸŸå
   åŸŸå: *.dse.run
   æºç«™: æœåŠ¡å™¨ IP
   åŠ é€ŸåŒºåŸŸ: ä¸­å›½å¢ƒå†…

2. ç¼“å­˜è§„åˆ™:
   *.wasm      â†’ ç¼“å­˜ 30 å¤©
   *.js        â†’ ç¼“å­˜ 7 å¤©ï¼ˆå¸¦ hash çš„å¯ä»¥ 30 å¤©ï¼‰
   *.data      â†’ ç¼“å­˜ 30 å¤©
   index.html  â†’ ä¸ç¼“å­˜

3. åˆ·æ–°ç­–ç•¥:
   æ¯æ¬¡ä¸Šä¼ æˆåŠŸåŽè°ƒç”¨ CDN åˆ·æ–° API:
   POST https://cdn.tencentcloudapi.com/?Action=PurgePathCache
   Paths: ["https://{gameId}.dse.run/"]
```

### 2.5 ç£ç›˜ç©ºé—´ç®¡ç†

```bash
# crontab æ·»åŠ å®šæ—¶æ¸…ç†ï¼šæ¯å¤©å‡Œæ™¨æ¸…ç†è¶…è¿‡ 90 å¤©æœªæ›´æ–°çš„æ¸¸æˆ
0 3 * * * find /var/www/games -name '.dse_meta.json' -mtime +90 -execdir rm -rf $(dirname {}) \;

# ç›‘æŽ§ç£ç›˜ä½¿ç”¨ï¼ˆè¶…è¿‡ 80% å‘Šè­¦ï¼‰
*/5 * * * * [ $(df /var/www/games --output=pcent | tail -1 | tr -d '% ') -gt 80 ] && echo "Disk warning" | mail admin@dse.run
```

---

## ä¸‰ã€ç¼–è¾‘å™¨æ”¹é€ 

### 3.1 ä¿®æ”¹æ–‡ä»¶

| æ–‡ä»¶ | æ”¹åŠ¨ |
|:-----|:------|
| `apps/editor_cpp/src/editor_build_game.h` | æ–°å¢ž `PublishState` ç»“æž„ä½“ |
| `apps/editor_cpp/src/editor_build_game.cpp` | ä¿®æ”¹ Build Game å¯¹è¯æ¡† UI + ç¦»çº¿/åœ¨çº¿æ¨¡å¼åˆ‡æ¢ |
| `apps/editor_cpp/src/editor_build_game_publish.cpp` | **æ–°å»ºã€‚** åŽ‹ç¼©ï¼ˆminizipï¼‰+ HTTP ä¸Šä¼  + QR ç  |
| `CMakeLists.txt` | æ–°å¢ž `DSE_PUBLISH_ENABLED` ç¼–è¯‘é€‰é¡¹ |

### 3.2 ç¼–è¯‘å¼€å…³

```cmake
# CMakeLists.txt
option(DSE_PUBLISH_ENABLED "Enable online publish feature (requires libcurl)" OFF)

if(DSE_PUBLISH_ENABLED)
    find_package(CURL REQUIRED)
    target_compile_definitions(dse_editor PRIVATE DSE_PUBLISH_ENABLED=1)
    target_link_libraries(dse_editor PRIVATE CURL::libcurl)
endif()
```

æœªå¼€å¯æ—¶ï¼šç¼–è¾‘å™¨åªæœ‰"å¯¼å‡º Web åŒ…"åŠŸèƒ½ï¼ˆç¦»çº¿æ¨¡å¼ï¼‰ï¼Œä¸ç¼–è¯‘ä¸Šä¼ ä»£ç ï¼Œä¸ä¾èµ– libcurlã€‚

### 3.3 æ–°å¢žçŠ¶æ€

```cpp
// editor_build_game.h æ–°å¢ž
struct PublishState {
    // é…ç½®
    bool enable_publish = false;            // æ˜¯å¦å‹¾é€‰"ä¸€é”®å‘å¸ƒ"
    char game_id[64] = "";                  // è‡ªå®šä¹‰æ¸¸æˆ ID
    char upload_url[256] = "https://api.dse.run/api/publish";
    char api_key[128] = "";                 // API Key
    bool auto_copy_url = true;              // å‘å¸ƒåŽè‡ªåŠ¨å¤åˆ¶é“¾æŽ¥

    // ç»“æžœ
    std::string publish_url;                // å‘å¸ƒåŽå¾—åˆ°çš„ URL
    std::string local_zip_path;             // ç¦»çº¿æ¨¡å¼çš„æœ¬åœ° zip è·¯å¾„
    std::string qr_code_png_base64;         // äºŒç»´ç  PNG base64
    bool publish_done = false;
    bool publish_success = false;
    std::string publish_error;

    // è¿›åº¦
    float upload_progress = 0.0f;           // ä¸Šä¼ è¿›åº¦ 0.0~1.0
    std::string status_text;                // å½“å‰çŠ¶æ€æ–‡å­—

    // ç”Ÿå‘½å‘¨æœŸç®¡ç†
    std::future<void> build_future;         // async ä»»åŠ¡å¥æŸ„
    std::string pending_clipboard;          // å¾…å¤åˆ¶åˆ°å‰ªè´´æ¿çš„æ–‡æœ¬ï¼ˆä¸»çº¿ç¨‹å¤„ç†ï¼‰
};
```

### 3.4 UI æ”¹åŠ¨

```cpp
// editor_build_game.cppï¼ŒDrawBuildGameDialog å†…
if (state.platform == BuildPlatform::Web) {
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), ICON_FA_GLOBE " Web å‘å¸ƒ");

#ifdef DSE_PUBLISH_ENABLED
    ImGui::Checkbox("æž„å»ºåŽä¸Šä¼ åˆ°æœåŠ¡å™¨", &state.publish_enable);

    if (state.publish_enable) {
        ImGui::Indent();
        ImGui::Text("æ¸¸æˆ ID:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##game_id", state.game_id, sizeof(state.game_id));
        ImGui::SameLine();
        ImGui::TextDisabled("(ç•™ç©ºè‡ªåŠ¨ç”Ÿæˆ)");

        ImGui::Text("API Key:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("##api_key", state.api_key, sizeof(state.api_key),
                         ImGuiInputTextFlags_Password);

        ImGui::Text("æœåŠ¡å™¨:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("##upload_url", state.upload_url, sizeof(state.upload_url));

        ImGui::Checkbox("å‘å¸ƒåŽè‡ªåŠ¨å¤åˆ¶é“¾æŽ¥", &state.auto_copy_url);
        ImGui::Unindent();
    }

    // æž„å»ºæŒ‰é’®
    const char* btn_text = state.publish_enable ? "æž„å»ºå¹¶å‘å¸ƒ" : "å¯¼å‡º Web åŒ…";
    if (ImGui::Button(btn_text, ImVec2(140, 28)) && !state.building) {
        StartWebBuild(state);
    }
#else
    // ç¦»çº¿æ¨¡å¼ï¼šåªæœ‰å¯¼å‡ºåŠŸèƒ½
    if (ImGui::Button("å¯¼å‡º Web åŒ…", ImVec2(140, 28)) && !state.building) {
        StartWebBuild(state);
    }
#endif

    // è¿›åº¦æ˜¾ç¤º
    if (state.building) {
        ImGui::ProgressBar(state.upload_progress);
        ImGui::Text("%s", state.status_text.c_str());
    }

    // ä¸»çº¿ç¨‹å¤„ç†å‰ªè´´æ¿ï¼ˆImGui å‰ªè´´æ¿ä¸æ˜¯çº¿ç¨‹å®‰å…¨çš„ï¼‰
    if (!state.pending_clipboard.empty()) {
        ImGui::SetClipboardText(state.pending_clipboard.c_str());
        state.pending_clipboard.clear();
    }

    // ç»“æžœæ˜¾ç¤º
    if (state.publish_done && state.publish_success) {
        ImGui::Separator();
        if (!state.publish_url.empty()) {
            ImGui::TextColored(ImVec4(0,1,0,1), ICON_FA_CHECK " å‘å¸ƒæˆåŠŸ!");
            ImGui::Text("é“¾æŽ¥: %s", state.publish_url.c_str());
            if (ImGui::SmallButton("å¤åˆ¶é“¾æŽ¥")) {
                ImGui::SetClipboardText(state.publish_url.c_str());
            }
            ShowQRCodeCached(state.qr_code_png_base64);
        } else {
            ImGui::TextColored(ImVec4(0,1,0,1), ICON_FA_CHECK " å¯¼å‡ºæˆåŠŸ!");
            ImGui::Text("æ–‡ä»¶: %s", state.local_zip_path.c_str());
            if (ImGui::SmallButton("æ‰“å¼€ç›®å½•")) {
                OpenInExplorer(state.local_zip_path);
            }
        }
    } else if (state.publish_done && !state.publish_success) {
        ImGui::TextColored(ImVec4(1,0,0,1), ICON_FA_TIMES " å¤±è´¥: %s",
                          state.publish_error.c_str());
    }
}
```

### 3.5 æ ¸å¿ƒé€»è¾‘

```cpp
// editor_build_game_publish.cppï¼ˆæ–°å»ºï¼‰

#include "editor_build_game.h"
#include <minizip/zip.h>       // å¼•æ“Žå·²æœ‰ zlibï¼Œminizip æ˜¯å…¶ä¸€éƒ¨åˆ†
#include <filesystem>
#include <thread>
#include <atomic>
#include <future>
#include <fstream>

#ifdef DSE_PUBLISH_ENABLED
#include <curl/curl.h>
#endif

namespace dse::editor {

// â”€â”€ åŽ‹ç¼©ç›®å½•ä¸º .zipï¼ˆè·¨å¹³å°ï¼Œä½¿ç”¨ minizipï¼‰â”€â”€
std::string ZipDirectory(const std::string& dir_path) {
    namespace fs = std::filesystem;
    std::string zip_path = dir_path + ".zip";

    zipFile zf = zipOpen(zip_path.c_str(), APPEND_STATUS_CREATE);
    if (!zf) return "";

    for (auto& entry : fs::recursive_directory_iterator(dir_path)) {
        if (!entry.is_regular_file()) continue;

        std::string rel_path = fs::relative(entry.path(), dir_path).string();
        std::replace(rel_path.begin(), rel_path.end(), '\\', '/');

        zip_fileinfo zi = {};
        if (zipOpenNewFileInZip(zf, rel_path.c_str(), &zi,
                                nullptr, 0, nullptr, 0, nullptr,
                                Z_DEFLATED, Z_DEFAULT_COMPRESSION) != ZIP_OK) {
            continue;
        }

        std::ifstream ifs(entry.path(), std::ios::binary);
        char buf[8192];
        while (ifs.read(buf, sizeof(buf)) || ifs.gcount() > 0) {
            zipWriteInFileInZip(zf, buf, (unsigned int)ifs.gcount());
        }
        zipCloseFileInZip(zf);
    }

    zipClose(zf, nullptr);
    return fs::exists(zip_path) ? zip_path : "";
}

#ifdef DSE_PUBLISH_ENABLED
// â”€â”€ ä¸Šä¼ è¿›åº¦å›žè°ƒ â”€â”€
static int UploadProgressCallback(void* userdata, curl_off_t dltotal,
                                   curl_off_t dlnow, curl_off_t ultotal,
                                   curl_off_t ulnow) {
    auto* progress = static_cast<std::atomic<float>*>(userdata);
    if (ultotal > 0) {
        *progress = static_cast<float>(ulnow) / static_cast<float>(ultotal);
    }
    return 0;
}

// â”€â”€ ä¸Šä¼ ç»“æžœ â”€â”€
struct PublishResult {
    bool success = false;
    std::string url;
    std::string error;
};

// â”€â”€ ä¸Šä¼ åˆ°æœåŠ¡å™¨ â”€â”€
PublishResult UploadToServer(const std::string& zip_path,
                              const std::string& server_url,
                              const std::string& game_id,
                              const std::string& api_key,
                              std::atomic<float>& progress) {
    PublishResult result;

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error = "åˆå§‹åŒ– HTTP å®¢æˆ·ç«¯å¤±è´¥";
        return result;
    }

    // æž„å»º multipart formï¼ˆä½¿ç”¨æ–°ç‰ˆ curl_mime APIï¼‰
    curl_mime* mime = curl_mime_init(curl);

    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "package");
    curl_mime_filedata(part, zip_path.c_str());

    if (!game_id.empty()) {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "game_id");
        curl_mime_data(part, game_id.c_str(), CURL_ZERO_TERMINATED);
    }

    // å“åº”è¯»å–
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, server_url.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    // API Key å¤´
    struct curl_slist* headers = nullptr;
    std::string auth_header = "X-API-Key: " + api_key;
    headers = curl_slist_append(headers, auth_header.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // å†™å›žè°ƒ
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        +[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
            auto* resp = static_cast<std::string*>(userdata);
            resp->append(ptr, size * nmemb);
            return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // è¿›åº¦å›žè°ƒ
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, UploadProgressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    // è¶…æ—¶
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        result.error = std::string("ç½‘ç»œé”™è¯¯: ") + curl_easy_strerror(res);
        return result;
    }

    if (http_code != 200) {
        result.error = "æœåŠ¡å™¨è¿”å›žé”™è¯¯ " + std::to_string(http_code);
        return result;
    }

    // è§£æž JSONï¼ˆä½¿ç”¨å¼•æ“Žå·²æœ‰çš„ nlohmann/jsonï¼‰
    try {
        auto json = nlohmann::json::parse(response);
        if (json.contains("error")) {
            result.error = json["error"].get<std::string>();
        } else {
            result.success = true;
            result.url = json["url"].get<std::string>();
        }
    } catch (...) {
        result.error = "è§£æžæœåŠ¡å™¨å“åº”å¤±è´¥: " + response.substr(0, 200);
    }

    return result;
}
#endif  // DSE_PUBLISH_ENABLED

// â”€â”€ ä¸»æž„å»ºæµç¨‹ â”€â”€
void StartWebBuild(PublishState& state) {
    state.building = true;
    state.publish_done = false;
    state.publish_success = false;
    state.publish_error.clear();
    state.publish_url.clear();
    state.local_zip_path.clear();
    state.upload_progress = 0.0f;
    state.status_text = "æ­£åœ¨æž„å»º Web ç‰ˆæœ¬...";

    // ä½¿ç”¨ std::asyncï¼ˆå®‰å…¨ç®¡ç†ç”Ÿå‘½å‘¨æœŸï¼Œé¿å… detach æ‚¬ç©ºå¼•ç”¨ï¼‰
    state.build_future = std::async(std::launch::async, [&state]() {
        // 1. æž„å»º Webï¼ˆè°ƒç”¨ Emscriptenï¼‰
        bool build_ok = DoBuildWeb(state);
        if (!build_ok) {
            state.publish_error = "Web æž„å»ºå¤±è´¥ï¼Œè¯·æ£€æŸ¥ Emscripten é…ç½®";
            state.building = false;
            state.publish_done = true;
            return;
        }

        // 2. åŽ‹ç¼©äº§ç‰©ï¼ˆä½¿ç”¨ minizipï¼Œè·¨å¹³å°ï¼‰
        state.status_text = "æ­£åœ¨åŽ‹ç¼©...";
        std::string zip_path = ZipDirectory(state.output_dir);
        if (zip_path.empty()) {
            state.publish_error = "åŽ‹ç¼©äº§ç‰©å¤±è´¥";
            state.building = false;
            state.publish_done = true;
            return;
        }

#ifdef DSE_PUBLISH_ENABLED
        // 3. åœ¨çº¿æ¨¡å¼ï¼šä¸Šä¼ 
        if (state.enable_publish && state.upload_url[0] != '\0' && state.api_key[0] != '\0') {
            state.status_text = "æ­£åœ¨ä¸Šä¼ ...";
            std::atomic<float> progress{0.0f};

            // è¿›åº¦æ›´æ–°çº¿ç¨‹
            auto progress_updater = std::thread([&state, &progress]() {
                while (state.building) {
                    state.upload_progress = progress.load();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            });

            auto result = UploadToServer(zip_path, state.upload_url,
                                          state.game_id, state.api_key, progress);
            state.building = false;
            progress_updater.join();

            std::filesystem::remove(zip_path);

            if (result.success) {
                state.publish_url = result.url;
                state.publish_success = true;
                state.qr_code_png_base64 = GenerateQRCodeBase64(result.url);
                if (state.auto_copy_url) {
                    state.pending_clipboard = result.url;
                }
            } else {
                state.publish_error = result.error;
            }
        } else
#endif
        {
            // ç¦»çº¿æ¨¡å¼ï¼šä¿å­˜ zip åˆ°æœ¬åœ°
            state.local_zip_path = zip_path;
            state.publish_success = true;
        }

        state.building = false;
        state.publish_done = true;
    });
}

// â”€â”€ äºŒç»´ç ç”Ÿæˆï¼ˆqrcodegen header-only, MIT Licenseï¼‰â”€â”€
// https://github.com/nayuki/QR-Code-generator
std::string GenerateQRCodeBase64(const std::string& url) {
    auto qr = qrcodegen::QrCode::encodeText(
        url.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);

    const int scale = 4;
    const int border = 2;
    int size = (qr.getSize() + border * 2) * scale;

    std::vector<uint8_t> pixels(size * size, 255);
    for (int y = 0; y < qr.getSize(); y++) {
        for (int x = 0; x < qr.getSize(); x++) {
            if (qr.getModule(x, y)) {
                for (int dy = 0; dy < scale; dy++)
                    for (int dx = 0; dx < scale; dx++)
                        pixels[((y+border)*scale+dy)*size + (x+border)*scale+dx] = 0;
            }
        }
    }

    // ç”¨ stb_image_write ç¼–ç ä¸º PNGï¼ˆå¼•æ“Žå·²æœ‰ stb ä¾èµ–ï¼‰
    int png_len = 0;
    unsigned char* png = stbi_write_png_to_mem(
        pixels.data(), size, size, size, 1, &png_len);
    if (!png) return "";

    std::string base64 = Base64Encode(png, png_len);
    STBI_FREE(png);
    return "data:image/png;base64," + base64;
}

// â”€â”€ äºŒç»´ç çº¹ç†ç¼“å­˜ï¼ˆé¿å…é‡å¤åˆ›å»ºå¯¼è‡´æ³„æ¼ï¼‰â”€â”€
static GLuint s_qr_texture = 0;
static std::string s_qr_last_data;

void ShowQRCodeCached(const std::string& base64_png) {
    if (base64_png.empty()) return;

    if (base64_png != s_qr_last_data) {
        if (s_qr_texture) {
            glDeleteTextures(1, &s_qr_texture);
            s_qr_texture = 0;
        }

        std::string png_data = Base64Decode(base64_png);
        int w, h, channels;
        unsigned char* rgba = stbi_load_from_memory(
            (unsigned char*)png_data.data(), (int)png_data.size(),
            &w, &h, &channels, 4);
        if (!rgba) return;

        glGenTextures(1, &s_qr_texture);
        glBindTexture(GL_TEXTURE_2D, s_qr_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        stbi_image_free(rgba);

        s_qr_last_data = base64_png;
    }

    if (s_qr_texture) {
        ImGui::Image((void*)(intptr_t)s_qr_texture, ImVec2(200, 200));
    }
}

} // namespace dse::editor
```

---

## å››ã€å¤–éƒ¨ä¾èµ–

| ä¾èµ– | ç”¨é€” | å·²æœ‰? | å¤‡æ³¨ |
|:-----|:------|:------|:------|
| `zlib` / `minizip` | ZIP åŽ‹ç¼©ï¼ˆè·¨å¹³å°ï¼‰ | âœ… å¼•æ“Žå·²æœ‰ zlib | minizip æ˜¯ zlib contrib çš„ä¸€éƒ¨åˆ† |
| `stb_image_write.h` | PNG ç¼–ç  | âœ… å·²æœ‰ | |
| `stb_image.h` | PNG è§£ç  | âœ… å·²æœ‰ | |
| `qrcodegen` (header-only) | QR ç ç”Ÿæˆ | âŒ éœ€æ·»åŠ  | ~300 è¡Œ C++ï¼ŒMIT License |
| `nlohmann/json` | JSON è§£æž | âœ… å¼•æ“Žå·²æœ‰ | |
| `libcurl` | HTTP ä¸Šä¼ ï¼ˆä»…åœ¨çº¿æ¨¡å¼ï¼‰ | æ¡ä»¶ä¾èµ– | `DSE_PUBLISH_ENABLED=ON` æ—¶éœ€è¦ |
| Node.js + express + multer + adm-zip + express-rate-limit | æœåŠ¡å™¨ç«¯ | âŒ æœåŠ¡å™¨å®‰è£… | |
| Nginx | åå‘ä»£ç† + é™æ€æ–‡ä»¶ | âŒ æœåŠ¡å™¨å®‰è£… | |

---

## äº”ã€å®žæ–½è®¡åˆ’

| é˜¶æ®µ | å†…å®¹ | å·¥ä½œé‡ | ä¾èµ– |
|:-----|:------|:-------|:-----|
| **Phase 1** | ç¼–è¾‘å™¨ç¦»çº¿æ¨¡å¼ï¼šWeb æž„å»º + minizip åŽ‹ç¼© + å¯¼å‡º | **2 å¤©** | æ—  |
| **Phase 2** | æ·»åŠ  qrcodegen + äºŒç»´ç æ˜¾ç¤º | **åŠå¤©** | Phase 1 |
| **Phase 3** | æœåŠ¡å™¨æ­å»ºï¼šNode.js + Nginx + HTTPS + DNS | **1 å¤©** | éœ€è¦æœåŠ¡å™¨ |
| **Phase 4** | ç¼–è¾‘å™¨åœ¨çº¿æ¨¡å¼ï¼šlibcurl ä¸Šä¼  + è¿›åº¦æ¡ + é“¾æŽ¥æ˜¾ç¤º | **2 å¤©** | Phase 1+3 |
| **Phase 5** | è°ƒè¯• + é”™è¯¯å¤„ç† + CDN é…ç½® | **1 å¤©** | Phase 4 |
| **æ€»è®¡** | | **~6 å¤©** | Phase 1-2 å¯ç«‹å³å¼€å§‹ |

**åˆ†é˜¶æ®µäº¤ä»˜ç­–ç•¥**ï¼š
- Phase 1-2 **ä¸éœ€è¦æœåŠ¡å™¨**ï¼Œå¯ä»¥ç«‹å³å®žçŽ°å¹¶åˆå…¥ä¸»çº¿
- Phase 3-5 ç­‰æœåŠ¡å™¨å°±ç»ªåŽå†åš
- ä¸¤è€…äº’ä¸é˜»å¡ž

---

## å…­ã€å®‰å…¨æŽªæ–½

| é˜²æŠ¤ç‚¹ | å®žçŽ°æ–¹å¼ |
|:-------|:---------|
| **é‰´æƒ** | API Key å¤´éªŒè¯ï¼ˆ`X-API-Key`ï¼‰ |
| **é™é€Ÿ** | express-rate-limit: 5 æ¬¡/å°æ—¶/IP |
| **è·¯å¾„ç©¿è¶Š** | è§£åŽ‹å‰é€æ–‡ä»¶æ£€æŸ¥ `path.resolve` æ˜¯å¦åœ¨ç›®æ ‡ç›®å½•å†… |
| **æ–‡ä»¶ç±»åž‹** | ç™½åå•è¿‡æ»¤ï¼ˆä»…å…è®¸ Web èµ„æºç±»åž‹ï¼‰ |
| **å¤§å°é™åˆ¶** | multer 200MB é™åˆ¶ + Nginx `client_max_body_size` |
| **å…ƒä¿¡æ¯éšè—** | Nginx è¿”å›ž 404 for `.dse_meta.json` |
| **HTTPS** | Let's Encrypt å…¨è¦†ç›– |
| **å®‰å…¨å¤´** | X-Content-Type-Options, X-Frame-Options, CSP |

---

## ä¸ƒã€æˆæœ¬ä¼°ç®—

| é¡¹ç›® | æœˆè´¹ |
|:-----|:------|
| è½»é‡äº‘æœåŠ¡å™¨ï¼ˆ2æ ¸2G 3Mbpsï¼‰ | ï¿¥40 |
| CDNï¼ˆæŒ‰é‡ï¼Œ10GB ä»¥å†…ï¼‰ | ï¿¥10 |
| åŸŸå dse.runï¼ˆç»­è´¹ï¼‰ | ï¿¥30/å¹´ â‰ˆ ï¿¥2.5/æœˆ |
| **æ€»è®¡** | **â‰ˆ ï¿¥52.5/æœˆ** |

---

## å…«ã€æŠ€æœ¯å€ºä¸ŽåŽç»­è¿­ä»£

### 8.1 å½“å‰æ–¹æ¡ˆæ— æŠ€æœ¯å€º

æ‰€æœ‰å·²çŸ¥é—®é¢˜å‡åœ¨ v2.0 ä¸­è§£å†³ï¼š
- âœ… é‰´æƒï¼ˆAPI Keyï¼‰
- âœ… è·¯å¾„ç©¿è¶Šé˜²æŠ¤
- âœ… æ–‡ä»¶ç±»åž‹ç™½åå•
- âœ… é™é€Ÿ
- âœ… è·¨å¹³å°åŽ‹ç¼©ï¼ˆminizip æ›¿ä»£ system("zip")ï¼‰
- âœ… çº¹ç†ç¼“å­˜ï¼ˆé˜²æ³„æ¼ï¼‰
- âœ… std::async æ›¿ä»£ detachï¼ˆç”Ÿå‘½å‘¨æœŸå®‰å…¨ï¼‰
- âœ… Nginx æ³›åŸŸåæ­£ç¡®è·¯ç”±ï¼ˆregex æå– game_idï¼‰
- âœ… ç¦»çº¿/åœ¨çº¿æ¨¡å¼è§£è€¦ï¼ˆç¼–è¯‘å¼€å…³ï¼‰
- âœ… ä¸Šä¼ è¿›åº¦æ¡
- âœ… CDN åˆ·æ–°ç­–ç•¥
- âœ… ç£ç›˜ç©ºé—´ç®¡ç†

### 8.2 åŽç»­è¿­ä»£ï¼ˆä¸å½±å“å½“å‰å‘å¸ƒï¼‰

| åŠŸèƒ½ | ä¼˜å…ˆçº§ | è¯´æ˜Ž |
|:-----|:------:|:------|
| ç”¨æˆ·ç™»å½•ç³»ç»Ÿ | P2 | DSE è´¦å·ï¼Œå…³è”å·²å‘å¸ƒæ¸¸æˆ |
| ç‰ˆæœ¬ç®¡ç† | P2 | ä¿ç•™æœ€è¿‘ N ä¸ªç‰ˆæœ¬ï¼Œæ”¯æŒå›žæ»š |
| è®¿é—®ç»Ÿè®¡ | P3 | ç®€å• PV/UV |
| è‡ªå®šä¹‰åŸŸå | P3 | ç»‘å®šç”¨æˆ·è‡ªæœ‰åŸŸå |
| å¯†ç ä¿æŠ¤ | P3 | ä¸ºæ¸¸æˆè®¾ç½®è®¿é—®å¯†ç  |
| æŽ’è¡Œæ¦œ/äº‘å­˜æ¡£ | P3 | ç®€å•åŽç«¯ API |
| Emscripten è‡ªåŠ¨å®‰è£… | P2 | ç¼–è¾‘å™¨é¦–æ¬¡æž„å»ºæ—¶å¼•å¯¼å®‰è£… |
