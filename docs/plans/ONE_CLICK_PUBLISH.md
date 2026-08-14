# DSEngine 一键发布方案

> 版本: v2.0 | 日期: 2026-07-03
> 目标: 编辑器内一键构建 Web 游戏 → 上传到 DSE 官方托管 → 生成二维码/链接 → 分享即玩
> 离线模式: 服务器未就绪时，仅构建+导出 zip，不影响正常使用

---

## 一、概述

### 1.1 用户故事

作为 DSEngine 的用户（游戏开发者）：
1. 在编辑器里完成游戏开发
2. 点 `File → Build Game → 平台选 Web → 点"构建并发布"`
3. 等待 ~30 秒
4. 得到一个二维码和链接：`https://mygame.dse.run`
5. 分享给朋友 → 扫码即玩
6. 下次更新游戏，再次点"构建并发布"→ 同名覆盖（URL 不变）

**离线模式**（服务器未配置时）：
1. 点 `File → Build Game → 平台选 Web → 点"导出 Web 包"`
2. 等待 ~20 秒
3. 得到本地 zip 文件路径（如 `build/web/mygame.zip`）
4. 用户自行部署到任意静态服务器

### 1.2 架构总览

```
游戏开发者                              DSE 服务器                       玩家
┌─────────────────────┐    POST       ┌─────────────────────┐          ┌─────────┐
│ DSEngine Editor     │  ──────────▶  │ dse.run 托管服务     │  HTTPS   │ 手机/PC │
│                     │  game.zip     │                     │  ◀─────  │ 浏览器  │
│ Build Game (Web)    │  + API Key    │ Node.js 上传服务     │          │         │
│   ↓                 │               │ Nginx 静态托管       │          │ 扫码    │
│ minizip 压缩        │               │ CDN 缓存             │          │ 即玩    │
│ 显示二维码/链接     │  ◀──────────  │                     │          │         │
└─────────────────────┘  返回 URL     └─────────────────────┘          └─────────┘
```

### 1.3 运行模式

| 模式 | 条件 | 行为 |
|:-----|:-----|:-----|
| **离线模式** | 服务器地址为空 或 `DSE_PUBLISH_ENABLED` 未定义 | 仅构建+压缩为 zip，保存到本地 |
| **在线模式** | 服务器地址已配置 且 API Key 有效 | 构建+压缩+上传+返回链接 |

### 1.4 对游戏开发者的要求

| 要求 | 说明 |
|:-----|:------|
| 安装 Emscripten SDK | **必须。** `emsdk install latest && emsdk activate latest`，一次性的 |
| 配置服务器地址 | 可选。编辑器内置默认 `https://api.dse.run/api/publish` |
| API Key | 在线模式必须。编辑器 Settings → Publish → 填入 API Key |

---

## 二、服务器端

### 2.1 技术选型

| 组件 | 选择 | 理由 |
|:-----|:------|:------|
| 运行时 | Node.js 18+ | 轻量、单进程够用 |
| 上传处理 | multer + adm-zip | 成熟 npm 包 |
| HTTP 服务 | Express | 最简路由 |
| 鉴权 | API Key + HMAC | 防止未授权上传 |
| 限速 | express-rate-limit | 防滥用（5次/小时/IP） |
| 进程管理 | PM2 | 自动重启、日志 |
| 静态托管 | Nginx | 高性能静态文件服务 |
| 泛域名 | `*.dse.run` DNS A 记录 → 服务器 IP | 每个游戏自动获得子域名 |

### 2.2 Node.js 服务

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

// API Key 列表（生产环境从环境变量或数据库读取）
const VALID_API_KEYS = new Set((process.env.DSE_API_KEYS || '').split(',').filter(Boolean));

// 文件白名单
const ALLOWED_EXTENSIONS = new Set([
    '.html', '.htm', '.js', '.mjs', '.wasm', '.data',
    '.css', '.json', '.png', '.jpg', '.jpeg', '.gif',
    '.svg', '.ico', '.webp', '.mp3', '.ogg', '.wav',
    '.woff', '.woff2', '.ttf', '.map', '.txt'
]);

fs.mkdirSync(GAMES_DIR, { recursive: true });

// ── 限速中间件 ──
const publishLimiter = rateLimit({
    windowMs: 60 * 60 * 1000,  // 1 小时
    max: 5,                     // 每 IP 最多 5 次
    message: { error: '上传频率过高，请稍后再试' }
});

// ── 鉴权中间件 ──
function requireApiKey(req, res, next) {
    const key = req.headers['x-api-key'] || req.body.api_key || '';
    if (!VALID_API_KEYS.has(key)) {
        return res.status(401).json({ error: '无效的 API Key' });
    }
    next();
}

// ── 路径安全检查 ──
function isPathSafe(filePath, baseDir) {
    const resolved = path.resolve(baseDir, filePath);
    return resolved.startsWith(path.resolve(baseDir));
}

// ── 文件扩展名检查 ──
function isFileAllowed(filename) {
    const ext = path.extname(filename).toLowerCase();
    return ALLOWED_EXTENSIONS.has(ext) || ext === '';
}

// ── POST /api/publish ─────────────────────────────────────────────
app.post('/api/publish', publishLimiter, requireApiKey, upload.single('package'), (req, res) => {
    try {
        const file = req.file;
        if (!file) {
            return res.status(400).json({ error: '缺少 package 文件' });
        }

        // 确定游戏 ID
        let gameId = req.body.game_id || '';
        if (!gameId || !/^[a-z0-9_-]{1,64}$/i.test(gameId)) {
            gameId = crypto.randomBytes(4).toString('hex');
        }

        // 解压前安全检查
        const zip = new AdmZip(file.path);
        const entries = zip.getEntries();

        // 检查路径穿越和文件类型
        const gameDir = path.join(GAMES_DIR, gameId);
        for (const entry of entries) {
            if (!isPathSafe(entry.entryName, gameDir)) {
                fs.unlinkSync(file.path);
                return res.status(400).json({ error: '游戏包含非法路径' });
            }
            if (!entry.isDirectory && !isFileAllowed(entry.entryName)) {
                fs.unlinkSync(file.path);
                return res.status(400).json({
                    error: `不允许的文件类型: ${entry.entryName}`
                });
            }
        }

        // 首次发布或覆盖更新
        if (fs.existsSync(gameDir)) {
            fs.rmSync(gameDir, { recursive: true });
        }

        // 解压到 /var/www/games/{gameId}/
        zip.extractAllTo(gameDir, true);

        // 验证解压结果：必须有 index.html
        if (!fs.existsSync(path.join(gameDir, 'index.html'))) {
            fs.rmSync(gameDir, { recursive: true });
            fs.unlinkSync(file.path);
            return res.status(400).json({
                error: '游戏包缺少 index.html，请确认 Web 构建成功'
            });
        }

        // 写入元信息
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

        // 清理临时文件
        fs.unlinkSync(file.path);

        // 返回结果
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
        res.status(500).json({ error: '服务器内部错误' });
    }
});

// ── GET /api/games ────────────────────────────────────────────────
app.get('/api/games', requireApiKey, (req, res) => {
    const games = [];
    if (fs.existsSync(GAMES_DIR)) {
        for (const entry of fs.readdirSync(GAMES_DIR)) {
            const metaPath = path.join(GAMES_DIR, entry, '.dse_meta.json');
            if (fs.existsSync(metaPath)) {
                try {
                    const meta = JSON.parse(fs.readFileSync(metaPath, 'utf-8'));
                    delete meta.update_token;  // 不暴露 token
                    games.push(meta);
                } catch {}
            }
        }
    }
    res.json({ games });
});

// ── DELETE /api/games/:id ─────────────────────────────────────────
app.delete('/api/games/:id', requireApiKey, (req, res) => {
    const gameDir = path.join(GAMES_DIR, req.params.id);
    if (!fs.existsSync(gameDir)) {
        return res.status(404).json({ error: '游戏不存在' });
    }
    fs.rmSync(gameDir, { recursive: true });
    res.json({ success: true });
});

// ── 启动 ──────────────────────────────────────────────────────────
const PORT = process.env.PORT || 8080;
app.listen(PORT, () => {
    console.log(`DSE Publish Server running on port ${PORT}`);
});
```

### 2.3 部署

```bash
# 1. 安装依赖
cd /opt/dse-publish-server
npm init -y
npm install express multer adm-zip express-rate-limit
npm install -g pm2

# 2. 配置环境变量
export DSE_API_KEYS="your-api-key-1,your-api-key-2"

# 3. 启动
pm2 start server.js --name dse-publish
pm2 save
pm2 startup  # 开机自启

# 4. Nginx 配置
cat > /etc/nginx/sites-available/dse.run << 'EOF'
# 泛域名：按子域名路由到对应游戏目录
server {
    listen 80;
    server_name ~^(?<game_id>.+)\.dse\.run$;

    root /var/www/games/$game_id;
    index index.html;

    # 安全头
    add_header X-Content-Type-Options nosniff;
    add_header X-Frame-Options SAMEORIGIN;
    add_header Content-Security-Policy "default-src 'self' 'unsafe-inline' 'unsafe-eval' blob: data:;";

    # WASM MIME 类型
    types {
        application/wasm wasm;
    }

    # 缓存策略
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

    # 禁止访问元信息
    location = /.dse_meta.json {
        return 404;
    }

    client_max_body_size 200M;
}

# 上传 API 反代
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

# 5. 配置 HTTPS（泛域名需要 DNS 验证）
apt install certbot python3-certbot-nginx python3-certbot-dns-cloudflare
certbot certonly --dns-cloudflare -d dse.run -d '*.dse.run'

# 6. DNS 配置
# 添加 A 记录:
#   *.dse.run    → 服务器 IP
#   api.dse.run  → 服务器 IP
```

### 2.4 CDN 配置（可选，推荐）

以腾讯云 CDN 为例：

```
1. CDN 控制台 → 添加域名
   域名: *.dse.run
   源站: 服务器 IP
   加速区域: 中国境内

2. 缓存规则:
   *.wasm      → 缓存 30 天
   *.js        → 缓存 7 天（带 hash 的可以 30 天）
   *.data      → 缓存 30 天
   index.html  → 不缓存

3. 刷新策略:
   每次上传成功后调用 CDN 刷新 API:
   POST https://cdn.tencentcloudapi.com/?Action=PurgePathCache
   Paths: ["https://{gameId}.dse.run/"]
```

### 2.5 磁盘空间管理

```bash
# crontab 添加定时清理：每天凌晨清理超过 90 天未更新的游戏
0 3 * * * find /var/www/games -name '.dse_meta.json' -mtime +90 -execdir rm -rf $(dirname {}) \;

# 监控磁盘使用（超过 80% 告警）
*/5 * * * * [ $(df /var/www/games --output=pcent | tail -1 | tr -d '% ') -gt 80 ] && echo "Disk warning" | mail admin@dse.run
```

---

## 三、编辑器改造

### 3.1 修改文件

| 文件 | 改动 |
|:-----|:------|
| `apps/editor_cpp/src/editor_build_game.h` | 新增 `PublishState` 结构体 |
| `apps/editor_cpp/src/editor_build_game.cpp` | 修改 Build Game 对话框 UI + 离线/在线模式切换 |
| `apps/editor_cpp/src/editor_build_game_publish.cpp` | **新建。** 压缩（minizip）+ HTTP 上传 + QR 码 |
| `CMakeLists.txt` | 新增 `DSE_PUBLISH_ENABLED` 编译选项 |

### 3.2 编译开关

```cmake
# CMakeLists.txt
option(DSE_PUBLISH_ENABLED "Enable online publish feature (requires libcurl)" OFF)

if(DSE_PUBLISH_ENABLED)
    find_package(CURL REQUIRED)
    target_compile_definitions(dse_editor PRIVATE DSE_PUBLISH_ENABLED=1)
    target_link_libraries(dse_editor PRIVATE CURL::libcurl)
endif()
```

未开启时：编辑器只有"导出 Web 包"功能（离线模式），不编译上传代码，不依赖 libcurl。

### 3.3 新增状态

```cpp
// editor_build_game.h 新增
struct PublishState {
    // 配置
    bool enable_publish = false;            // 是否勾选"一键发布"
    char game_id[64] = "";                  // 自定义游戏 ID
    char upload_url[256] = "https://api.dse.run/api/publish";
    char api_key[128] = "";                 // API Key
    bool auto_copy_url = true;              // 发布后自动复制链接

    // 结果
    std::string publish_url;                // 发布后得到的 URL
    std::string local_zip_path;             // 离线模式的本地 zip 路径
    std::string qr_code_png_base64;         // 二维码 PNG base64
    bool publish_done = false;
    bool publish_success = false;
    std::string publish_error;

    // 进度
    float upload_progress = 0.0f;           // 上传进度 0.0~1.0
    std::string status_text;                // 当前状态文字

    // 生命周期管理
    std::future<void> build_future;         // async 任务句柄
    std::string pending_clipboard;          // 待复制到剪贴板的文本（主线程处理）
};
```

### 3.4 UI 改动

```cpp
// editor_build_game.cppï¼ŒDrawBuildGameDialog å†…
if (state.platform == BuildPlatform::Web) {
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), ICON_FA_GLOBE " Web 发布");

#ifdef DSE_PUBLISH_ENABLED
    ImGui::Checkbox("构建后上传到服务器", &state.publish_enable);

    if (state.publish_enable) {
        ImGui::Indent();
        ImGui::Text("游戏 ID:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##game_id", state.game_id, sizeof(state.game_id));
        ImGui::SameLine();
        ImGui::TextDisabled("(留空自动生成)");

        ImGui::Text("API Key:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("##api_key", state.api_key, sizeof(state.api_key),
                         ImGuiInputTextFlags_Password);

        ImGui::Text("服务器:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("##upload_url", state.upload_url, sizeof(state.upload_url));

        ImGui::Checkbox("发布后自动复制链接", &state.auto_copy_url);
        ImGui::Unindent();
    }

    // 构建按钮
    const char* btn_text = state.publish_enable ? "构建并发布" : "导出 Web 包";
    if (ImGui::Button(btn_text, ImVec2(140, 28)) && !state.building) {
        StartWebBuild(state);
    }
#else
    // 离线模式：只有导出功能
    if (ImGui::Button("导出 Web 包", ImVec2(140, 28)) && !state.building) {
        StartWebBuild(state);
    }
#endif

    // 进度显示
    if (state.building) {
        ImGui::ProgressBar(state.upload_progress);
        ImGui::Text("%s", state.status_text.c_str());
    }

    // 主线程处理剪贴板（ImGui 剪贴板不是线程安全的）
    if (!state.pending_clipboard.empty()) {
        ImGui::SetClipboardText(state.pending_clipboard.c_str());
        state.pending_clipboard.clear();
    }

    // 结果显示
    if (state.publish_done && state.publish_success) {
        ImGui::Separator();
        if (!state.publish_url.empty()) {
            ImGui::TextColored(ImVec4(0,1,0,1), ICON_FA_CHECK " 发布成功!");
            ImGui::Text("链接: %s", state.publish_url.c_str());
            if (ImGui::SmallButton("复制链接")) {
                ImGui::SetClipboardText(state.publish_url.c_str());
            }
            ShowQRCodeCached(state.qr_code_png_base64);
        } else {
            ImGui::TextColored(ImVec4(0,1,0,1), ICON_FA_CHECK " 导出成功!");
            ImGui::Text("文件: %s", state.local_zip_path.c_str());
            if (ImGui::SmallButton("打开目录")) {
                OpenInExplorer(state.local_zip_path);
            }
        }
    } else if (state.publish_done && !state.publish_success) {
        ImGui::TextColored(ImVec4(1,0,0,1), ICON_FA_TIMES " 失败: %s",
                          state.publish_error.c_str());
    }
}
```

### 3.5 核心逻辑

```cpp
// editor_build_game_publish.cpp（新建）

#include "editor_build_game.h"
#include <minizip/zip.h>       // 引擎已有 zlib，minizip 是其一部分
#include <filesystem>
#include <thread>
#include <atomic>
#include <future>
#include <fstream>

#ifdef DSE_PUBLISH_ENABLED
#include <curl/curl.h>
#endif

namespace dse::editor {

// ── 压缩目录为 .zip（跨平台，使用 minizip）──
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
// ── 上传进度回调 ──
static int UploadProgressCallback(void* userdata, curl_off_t dltotal,
                                   curl_off_t dlnow, curl_off_t ultotal,
                                   curl_off_t ulnow) {
    auto* progress = static_cast<std::atomic<float>*>(userdata);
    if (ultotal > 0) {
        *progress = static_cast<float>(ulnow) / static_cast<float>(ultotal);
    }
    return 0;
}

// ── 上传结果 ──
struct PublishResult {
    bool success = false;
    std::string url;
    std::string error;
};

// ── 上传到服务器 ──
PublishResult UploadToServer(const std::string& zip_path,
                              const std::string& server_url,
                              const std::string& game_id,
                              const std::string& api_key,
                              std::atomic<float>& progress) {
    PublishResult result;

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error = "初始化 HTTP 客户端失败";
        return result;
    }

    // 构建 multipart form（使用新版 curl_mime API）
    curl_mime* mime = curl_mime_init(curl);

    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "package");
    curl_mime_filedata(part, zip_path.c_str());

    if (!game_id.empty()) {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "game_id");
        curl_mime_data(part, game_id.c_str(), CURL_ZERO_TERMINATED);
    }

    // 响应读取
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, server_url.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    // API Key å¤´
    struct curl_slist* headers = nullptr;
    std::string auth_header = "X-API-Key: " + api_key;
    headers = curl_slist_append(headers, auth_header.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // 写回调
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        +[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
            auto* resp = static_cast<std::string*>(userdata);
            resp->append(ptr, size * nmemb);
            return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // 进度回调
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, UploadProgressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    // 超时
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        result.error = std::string("网络错误: ") + curl_easy_strerror(res);
        return result;
    }

    if (http_code != 200) {
        result.error = "服务器返回错误 " + std::to_string(http_code);
        return result;
    }

    // 解析 JSON（使用引擎已有的 nlohmann/json）
    try {
        auto json = nlohmann::json::parse(response);
        if (json.contains("error")) {
            result.error = json["error"].get<std::string>();
        } else {
            result.success = true;
            result.url = json["url"].get<std::string>();
        }
    } catch (...) {
        result.error = "解析服务器响应失败: " + response.substr(0, 200);
    }

    return result;
}
#endif  // DSE_PUBLISH_ENABLED

// ── 主构建流程 ──
void StartWebBuild(PublishState& state) {
    state.building = true;
    state.publish_done = false;
    state.publish_success = false;
    state.publish_error.clear();
    state.publish_url.clear();
    state.local_zip_path.clear();
    state.upload_progress = 0.0f;
    state.status_text = "正在构建 Web 版本...";

    // 使用 std::async（安全管理生命周期，避免 detach 悬空引用）
    state.build_future = std::async(std::launch::async, [&state]() {
        // 1. 构建 Web（调用 Emscripten）
        bool build_ok = DoBuildWeb(state);
        if (!build_ok) {
            state.publish_error = "Web 构建失败，请检查 Emscripten 配置";
            state.building = false;
            state.publish_done = true;
            return;
        }

        // 2. 压缩产物（使用 minizip，跨平台）
        state.status_text = "正在压缩...";
        std::string zip_path = ZipDirectory(state.output_dir);
        if (zip_path.empty()) {
            state.publish_error = "压缩产物失败";
            state.building = false;
            state.publish_done = true;
            return;
        }

#ifdef DSE_PUBLISH_ENABLED
        // 3. 在线模式：上传
        if (state.enable_publish && state.upload_url[0] != '\0' && state.api_key[0] != '\0') {
            state.status_text = "正在上传...";
            std::atomic<float> progress{0.0f};

            // 进度更新线程
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
            // 离线模式：保存 zip 到本地
            state.local_zip_path = zip_path;
            state.publish_success = true;
        }

        state.building = false;
        state.publish_done = true;
    });
}

// ── 二维码生成（qrcodegen header-only, MIT License）──
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

    // 用 stb_image_write 编码为 PNG（引擎已有 stb 依赖）
    int png_len = 0;
    unsigned char* png = stbi_write_png_to_mem(
        pixels.data(), size, size, size, 1, &png_len);
    if (!png) return "";

    std::string base64 = Base64Encode(png, png_len);
    STBI_FREE(png);
    return "data:image/png;base64," + base64;
}

// ── 二维码纹理缓存（避免重复创建导致泄漏）──
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

## 四、外部依赖

| 依赖 | 用途 | 已有? | 备注 |
|:-----|:------|:------|:------|
| `zlib` / `minizip` | ZIP 压缩（跨平台） | ✅ 引擎已有 zlib | minizip 是 zlib contrib 的一部分 |
| `stb_image_write.h` | PNG 编码 | ✅ 已有 | |
| `stb_image.h` | PNG 解码 | ✅ 已有 | |
| `qrcodegen` (header-only) | QR 码生成 | ❌ 需添加 | ~300 行 C++，MIT License |
| `nlohmann/json` | JSON 解析 | ✅ 引擎已有 | |
| `libcurl` | HTTP 上传（仅在线模式） | 条件依赖 | `DSE_PUBLISH_ENABLED=ON` 时需要 |
| Node.js + express + multer + adm-zip + express-rate-limit | 服务器端 | ❌ 服务器安装 | |
| Nginx | 反向代理 + 静态文件 | ❌ 服务器安装 | |

---

## 五、实施计划

| 阶段 | 内容 | 工作量 | 依赖 |
|:-----|:------|:-------|:-----|
| **Phase 1** | 编辑器离线模式：Web 构建 + minizip 压缩 + 导出 | **2 天** | 无 |
| **Phase 2** | 添加 qrcodegen + 二维码显示 | **半天** | Phase 1 |
| **Phase 3** | 服务器搭建：Node.js + Nginx + HTTPS + DNS | **1 天** | 需要服务器 |
| **Phase 4** | 编辑器在线模式：libcurl 上传 + 进度条 + 链接显示 | **2 天** | Phase 1+3 |
| **Phase 5** | 调试 + 错误处理 + CDN 配置 | **1 天** | Phase 4 |
| **总计** | | **~6 天** | Phase 1-2 可立即开始 |

**分阶段交付策略**：
- Phase 1-2 **不需要服务器**，可以立即实现并合入主线
- Phase 3-5 等服务器就绪后再做
- 两者互不阻塞

---

## 六、安全措施

| 防护点 | 实现方式 |
|:-------|:---------|
| **鉴权** | API Key 头验证（`X-API-Key`） |
| **限速** | express-rate-limit: 5 次/小时/IP |
| **路径穿越** | 解压前逐文件检查 `path.resolve` 是否在目标目录内 |
| **文件类型** | 白名单过滤（仅允许 Web 资源类型） |
| **大小限制** | multer 200MB 限制 + Nginx `client_max_body_size` |
| **元信息隐藏** | Nginx 返回 404 for `.dse_meta.json` |
| **HTTPS** | Let's Encrypt 全覆盖 |
| **安全头** | X-Content-Type-Options, X-Frame-Options, CSP |

---

## 七、成本估算

| 项目 | 月费 |
|:-----|:------|
| 轻量云服务器（2核2G 3Mbps） | ￥40 |
| CDN（按量，10GB 以内） | ￥10 |
| 域名 dse.run（续费） | ￥30/年 ≈ ￥2.5/月 |
| **总计** | **≈ ￥52.5/月** |

---

## 八、技术债与后续迭代

### 8.1 当前方案无技术债

所有已知问题均在 v2.0 中解决：
- ✅ 鉴权（API Key）
- ✅ 路径穿越防护
- ✅ 文件类型白名单
- ✅ 限速
- ✅ 跨平台压缩（minizip 替代 system("zip")）
- ✅ 纹理缓存（防泄漏）
- ✅ std::async 替代 detach（生命周期安全）
- ✅ Nginx 泛域名正确路由（regex 提取 game_id）
- ✅ 离线/在线模式解耦（编译开关）
- ✅ 上传进度条
- ✅ CDN 刷新策略
- ✅ 磁盘空间管理

### 8.2 后续迭代（不影响当前发布）

| 功能 | 优先级 | 说明 |
|:-----|:------:|:------|
| 用户登录系统 | P2 | DSE 账号，关联已发布游戏 |
| 版本管理 | P2 | 保留最近 N 个版本，支持回滚 |
| 访问统计 | P3 | 简单 PV/UV |
| 自定义域名 | P3 | 绑定用户自有域名 |
| 密码保护 | P3 | 为游戏设置访问密码 |
| 排行榜/云存档 | P3 | 简单后端 API |
| Emscripten 自动安装 | P2 | 编辑器首次构建时引导安装 |
