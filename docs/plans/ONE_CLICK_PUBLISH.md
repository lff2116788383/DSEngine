# DSEngine 一键发布方案

> 版本: v1.0 | 日期: 2026-07-03
> 目标: 编辑器内一键构建 Web 游戏 → 上传到 DSE 官方托管 → 生成二维码/链接 → 分享即玩

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

### 1.2 架构总览

```
游戏开发者                              DSE 服务器                       玩家
┌─────────────────────┐    POST       ┌─────────────────────┐          ┌─────────┐
│ DSEngine Editor     │  ──────────▶  │ dse.run 托管服务     │  HTTPS   │ 手机/PC │
│                     │  game.zip     │                     │  ◀─────  │ 浏览器  │
│ Build Game (Web)    │               │ Node.js 上传服务     │          │         │
│   ↓                 │               │ Nginx 静态托管       │          │ 扫码    │
│ 自动压缩为 .zip     │               │ CDN 缓存             │          │ 即玩    │
│ 显示二维码/链接     │  ◀──────────  │                     │          │         │
└─────────────────────┘  返回 URL     └─────────────────────┘          └─────────┘
```

### 1.3 对游戏开发者的要求

| 要求 | 说明 |
|:-----|:------|
| 注册 DSE 账号 | 可选（用于管理已发布的游戏列表） |
| 安装 Emscripten SDK | **必须。** `emsdk install latest && emsdk activate latest`，一次性的 |
| 配置服务器地址 | 编辑器内置默认 `https://dse.run/api/publish`，可选自定义 |
| 登录/Token | 暂无要求（后续版本可加） |

---

## 二、服务器端

### 2.1 技术选型

| 组件 | 选择 | 理由 |
|:-----|:------|:------|
| 运行时 | Node.js 18+ | 轻量、DSE 生态已有 Node.js（VS Code 扩展）、单进程 |
| 上传处理 | multer + adm-zip | 成熟 npm 包，处理文件上传和解压 |
| HTTP 服务 | Express | 最简路由 |
| 进程管理 | PM2 | 自动重启、日志、在线更新 |
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

const app = express();
const upload = multer({ dest: '/tmp/dse_uploads/' });
const GAMES_DIR = '/var/www/games';
const MAX_SIZE_MB = 200;

// 确保游戏目录存在
fs.mkdirSync(GAMES_DIR, { recursive: true });

// ── POST /api/publish ─────────────────────────────────────────────
// 编辑器上传游戏包
// Body: multipart/form-data
//   - package: .zip 文件（游戏 Web 产物）
//   - game_id: 可选，自定义 ID（为空则自动生成）
//   - title:   可选，游戏标题
//   - token:   可选，更新已有游戏时的鉴权（后续版本）
app.post('/api/publish', upload.single('package'), (req, res) => {
    try {
        const file = req.file;
        if (!file) {
            return res.status(400).json({ error: '缺少 package 文件' });
        }

        // 大小限制
        if (file.size > MAX_SIZE_MB * 1024 * 1024) {
            fs.unlinkSync(file.path);
            return res.status(413).json({
                error: `文件过大（最大 ${MAX_SIZE_MB}MB）`
            });
        }

        // 确定游戏 ID
        let gameId = req.body.game_id || '';
        if (!gameId || !/^[a-z0-9_-]{1,64}$/i.test(gameId)) {
            gameId = crypto.randomBytes(4).toString('hex');
        }

        // 首次发布或覆盖更新
        const gameDir = path.join(GAMES_DIR, gameId);
        if (fs.existsSync(gameDir)) {
            // 可选：检查 token 鉴权（后续版本）
            // 目前允许覆盖（方便快速迭代）
            fs.rmSync(gameDir, { recursive: true });
        }

        // 解压到 /var/www/games/{gameId}/
        const zip = new AdmZip(file.path);
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
            token: crypto.randomBytes(16).toString('hex'),  // 供后续更新用
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
            admin_url: `https://dse.run/admin/${gameId}?token=${meta.token}`
        });

    } catch (err) {
        console.error('Publish error:', err);
        res.status(500).json({ error: '服务器内部错误' });
    }
});

// ── GET /api/games ────────────────────────────────────────────────
// 获取已发布游戏列表（管理员用）
app.get('/api/games', (req, res) => {
    const games = [];
    if (fs.existsSync(GAMES_DIR)) {
        for (const entry of fs.readdirSync(GAMES_DIR)) {
            const metaPath = path.join(GAMES_DIR, entry, '.dse_meta.json');
            if (fs.existsSync(metaPath)) {
                try {
                    const meta = JSON.parse(fs.readFileSync(metaPath, 'utf-8'));
                    games.push(meta);
                } catch {}
            }
        }
    }
    res.json({ games });
});

// ── GET /api/games/:id ────────────────────────────────────────────
// 获取单个游戏信息
app.get('/api/games/:id', (req, res) => {
    const metaPath = path.join(GAMES_DIR, req.params.id, '.dse_meta.json');
    if (!fs.existsSync(metaPath)) {
        return res.status(404).json({ error: '游戏不存在' });
    }
    const meta = JSON.parse(fs.readFileSync(metaPath, 'utf-8'));
    res.json(meta);
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
npm install express multer adm-zip
npm install -g pm2

# 2. 启动
pm2 start server.js --name dse-publish
pm2 save
pm2 startup  # 开机自启

# 3. Nginx 配置
cat > /etc/nginx/sites-available/dse.run << 'EOF'
# 泛域名：所有 *.dse.run 指向游戏目录
server {
    listen 80;
    server_name *.dse.run;
    
    root /var/www/games;
    index index.html;
    
    location / {
        try_files $uri $uri/ /index.html;
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
    }
    
    client_max_body_size 200M;
}

# 官网（可选，与游戏托管合并）
server {
    listen 80;
    server_name dsengine.com www.dsengine.com;
    root /var/www/dsengine-site;
    index index.html;
}
EOF

# 4. 配置 HTTPS（自动 Let's Encrypt）
apt install certbot python3-certbot-nginx
certbot --nginx -d dse.run -d *.dse.run -d dsengine.com

# 5. DNS 配置
# 添加 A 记录:
#   *.dse.run    → 服务器 IP
#   api.dse.run  → 服务器 IP
#   dsengine.com → 服务器 IP
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
   *.js        → 缓存 7 天
   *.data      → 缓存 30 天
   index.html  → 不缓存（或 TTL 60 秒）

3. HTTPS 配置:
   上传自有证书 或 让 CDN 托管
```

CDN 启用后，游戏产物的下载流量全部走 CDN 节点，源站服务器几乎不消耗带宽。

---

## 三、编辑器改造

### 3.1 修改文件

| 文件 | 改动 |
|:-----|:------|
| `apps/editor_cpp/src/editor_build_game.h` | 新增 `PublishState` 结构体 |
| `apps/editor_cpp/src/editor_build_game.cpp` | 修改 Build Game 对话框 UI + 新增发布逻辑 |
| `apps/editor_cpp/src/editor_build_game_publish.cpp` | **新建。** 上传、压缩、QR 码等独立逻辑 |

### 3.2 新增状态

```cpp
// editor_build_game.h 新增
struct PublishState {
    bool enable_publish = false;            // 是否勾选"一键发布"
    char game_id[64] = "";                  // 自定义游戏 ID
    char upload_url[256] = "https://api.dse.run/api/publish";  // 上传服务器
    bool auto_copy_url = true;              // 发布后自动复制链接
    
    std::string publish_url;                // 发布后得到的 URL
    std::string admin_url;                  // 管理页 URL
    std::string qr_code_png_base64;         // 二维码 PNG base64
    bool publish_done = false;
    bool publish_success = false;
    std::string publish_error;
};
```

### 3.3 UI 改动

在现有 Build Game 对话框中，Web 平台下增加发布区域：

```cpp
// editor_build_game.cpp，DrawBuildGameDialog 内
// 位置: 在 Build 按钮之后、log 区域之前

if (state.platform == BuildPlatform::Web) {
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), ICON_FA_CLOUD_UPLOAD " 一键发布");
    
    ImGui::Checkbox("构建后自动发布到 Web", &state.publish_enable);
    
    if (state.publish_enable) {
        ImGui::Indent();
        
        // 游戏 ID
        ImGui::Text("游戏 ID:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##game_id", state.game_id, sizeof(state.game_id));
        ImGui::SameLine();
        ImGui::TextDisabled("(可选，留空自动生成)");
        
        // 服务器地址
        ImGui::Text("服务器:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("##upload_url", state.upload_url, sizeof(state.upload_url));
        
        // 自动复制链接
        ImGui::Checkbox("发布后自动复制链接", &state.auto_copy_url);
        
        ImGui::Unindent();
    }
}

// ... 在原有的 Build 按钮旁边 ...
if (state.publish_enable) {
    ImGui::SameLine();
    if (ImGui::Button("构建并发布", ImVec2(120, 24)) && !state.building) {
        state.building = true;
        state.build_done = false;
        state.publish_done = false;
        state.publish_success = false;
        state.publish_error.clear();
        state.publish_url.clear();
        state.qr_code_png_base64.clear();
        
        std::thread([&state]() {
            // 1. 构建
            DoBuild(state);
            if (!state.build_success) {
                state.publish_error = "构建失败";
                state.building = false;
                state.publish_done = true;
                return;
            }
            
            // 2. 压缩产物
            std::string zip_path = ZipOutputDir(state);
            if (zip_path.empty()) {
                state.publish_error = "压缩产物失败";
                state.building = false;
                state.publish_done = true;
                return;
            }
            
            // 3. 上传到服务器
            auto result = UploadToServer(zip_path, state.upload_url, state.game_id);
            std::remove(zip_path.c_str());  // 清理临时 zip
            
            if (!result.success) {
                state.publish_error = result.error;
                state.publish_success = false;
            } else {
                state.publish_url = result.url;
                state.admin_url = result.admin_url;
                state.publish_success = true;
                
                // 4. 生成二维码
                state.qr_code_png_base64 = GenerateQRCodeBase64(result.url);
                
                // 5. 自动复制链接
                if (state.auto_copy_url) {
                    ImGui::SetClipboardText(result.url.c_str());
                }
            }
            
            state.building = false;
            state.publish_done = true;
        }).detach();
    }
}

// 发布结果显示
if (state.publish_done) {
    if (state.publish_success) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0, 1, 0, 1), ICON_FA_CHECK_CIRCLE " 发布成功!");
        ImGui::Text("链接: %s", state.publish_url.c_str());
        if (ImGui::SmallButton("复制链接")) {
            ImGui::SetClipboardText(state.publish_url.c_str());
        }
        
        // 二维码
        if (!state.qr_code_png_base64.empty()) {
            ImGui::Text("扫码即玩:");
            // 解码 base64 → 显示纹理
            // 使用 ImGui::Image() 显示
        }
    } else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), ICON_FA_TIMES_CIRCLE " 发布失败: %s",
                          state.publish_error.c_str());
    }
}
```

### 3.4 新增逻辑文件

```cpp
// editor_build_game_publish.cpp（新建，~150 行）

#include "editor_build_game_publish.h"
#include <curl/curl.h>         // 或使用引擎已有的 HttpClient
#include <fstream>
#include <filesystem>
#include <sstream>
#include <zlib.h>              // minizip 或仅用系统 zip

namespace dse::editor {

struct PublishResult {
    bool success;
    std::string url;
    std::string admin_url;
    std::string error;
};

// ── 压缩产物目录为 .zip ──
std::string ZipOutputDir(const char* output_dir) {
    namespace fs = std::filesystem;
    
    std::string zip_path = std::string(output_dir) + ".zip";
    
    // 用 minizip 或 系统 zip 命令打包
    // 这里使用系统 zip 命令作为示例
    std::string cmd = std::string("cd ") + output_dir
        + " && zip -r \"" + zip_path + "\" .";
    
    int ret = std::system(cmd.c_str());
    if (ret != 0 || !fs::exists(zip_path)) {
        return "";
    }
    return zip_path;
}

// ── 上传到服务器 ──
PublishResult UploadToServer(const std::string& zip_path,
                              const std::string& server_url,
                              const std::string& game_id) {
    PublishResult result = {false};
    
    // 使用 libcurl（跨平台，引擎已有依赖）
    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error = "初始化 HTTP 客户端失败";
        return result;
    }
    
    struct curl_httppost* form = nullptr;
    struct curl_httppost* last = nullptr;
    
    curl_formadd(&form, &last,
                 CURLFORM_COPYNAME, "package",
                 CURLFORM_FILE, zip_path.c_str(),
                 CURLFORM_END);
    
    if (!game_id.empty()) {
        curl_formadd(&form, &last,
                     CURLFORM_COPYNAME, "game_id",
                     CURLFORM_COPYCONTENTS, game_id.c_str(),
                     CURLFORM_END);
    }
    
    // 响应读取
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, server_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, form);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                     +[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
                         auto* resp = static_cast<std::string*>(userdata);
                         resp->append(ptr, size * nmemb);
                         return size * nmemb;
                     });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);  // 120 秒超时
    
    CURLcode res = curl_easy_perform(curl);
    curl_formfree(form);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        result.error = curl_easy_strerror(res);
        return result;
    }
    
    // 解析 JSON 响应
    try {
        auto json = nlohmann::json::parse(response);
        result.success = true;
        result.url = json["url"];
        result.admin_url = json.value("admin_url", "");
    } catch (...) {
        result.error = "解析服务器响应失败";
    }
    
    return result;
}

// ── 生成二维码（QR Code Generator，C++ header-only）──
std::string GenerateQRCodeBase64(const std::string& url) {
    // 使用 https://github.com/Project-Phoenix/qrcodegen
    // qrcodegen 是 ~300 行 C++ header-only
    // 生成 QR 码矩阵 → 编码为 PNG → 转 base64
    
    // 1. 创建 QR 码
    qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(
        url.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
    
    // 2. 按模块大小缩放（每模块 4 像素，留白 2 模块）
    const int scale = 4;
    const int border = 2;
    int size = (qr.getSize() + border * 2) * scale;
    
    // 3. 生成灰度像素
    std::vector<uint8_t> pixels(size * size, 255);
    for (int y = 0; y < qr.getSize(); y++) {
        for (int x = 0; x < qr.getSize(); x++) {
            if (qr.getModule(x, y)) {
                int py = (y + border) * scale;
                int px = (x + border) * scale;
                for (int dy = 0; dy < scale; dy++)
                    for (int dx = 0; dx < scale; dx++)
                        pixels[(py + dy) * size + (px + dx)] = 0;
            }
        }
    }
    
    // 4. 编码为 PNG 内存块（用 stb_image_write.h，引擎已有 stb 依赖）
    int png_len = 0;
    unsigned char* png = stbi_write_png_to_mem(
        pixels.data(), size, size, size, 1, &png_len);
    
    if (!png) return "";
    
    // 5. 转 base64
    std::string base64 = Base64Encode(png, png_len);
    STBI_FREE(png);
    
    return "data:image/png;base64," + base64;
}

} // namespace dse::editor
```

### 3.5 二维码渲染

编辑器使用 ImGui 显示二维码：

```cpp
// 在 ImGui 中显示 base64 编码的二维码
void ShowQRCode(const std::string& base64_png) {
    // 1. 解码 base64 为原始 PNG 数据
    std::string png_data = Base64Decode(base64_png);
    
    // 2. 用 stb_image 解码为 RGBA
    int w, h, channels;
    unsigned char* rgba = stbi_load_from_memory(
        (unsigned char*)png_data.data(), png_data.size(),
        &w, &h, &channels, 4);
    
    if (!rgba) return;
    
    // 3. 创建 OpenGL 纹理
    GLuint tex_id;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    stbi_image_free(rgba);
    
    // 4. ImGui 显示
    ImGui::Image((void*)(intptr_t)tex_id, ImVec2(256, 256));
    
    // 5. 释放纹理（或缓存起来重复使用）
}
```

---

## 四、外部依赖

| 依赖 | 用途 | 已有? |
|:-----|:------|:------|
| `libcurl` | HTTP 上传 | ✅ 已在引擎中（也可用现有 HttpClient） |
| `stb_image_write.h` | PNG 编码 | ✅ 已作为 stb 依赖存在 |
| `stb_image.h` | PNG 解码 | ✅ 已有 |
| `qrcodegen` (header-only) | QR 码生成 | ❌ 需添加（~300 行 C++，MIT License） |
| Node.js + express + multer + adm-zip | 服务器端 | ❌ 需安装（服务器端） |
| Nginx | 反向代理 + 静态文件 | ❌ 需安装（服务器端） |
| certbot | HTTPS 证书 | ❌ 需安装（服务器端，可选） |

---

## 五、实施计划

| 阶段 | 内容 | 工作量 |
|:-----|:------|:-------|
| **Phase 1** | 服务器端搭建：Node.js 上传服务 + Nginx + HTTPS + DNS | **半天** |
| **Phase 2** | 编辑器手动验证：Web 构建成功 → 手动上传到服务器 → 手动发布链接 | **0 天**（现有功能） |
| **Phase 3** | 编辑器集成：压缩产物 + HTTP 上传 + 显示链接 + 二维码 | **3-5 天** |
| **Phase 4** | 调试 + 错误处理 + 边缘情况 | **1-2 天** |
| **总计** | | **1-2 周（截止周末前可上线 Phase 1+2）** |

---

## 六、安全与扩展

### 6.1 当前安全模型

- 上传接口无鉴权（方便快速迭代）
- 每个游戏有一个 `admin_token`，后续可用于管理
- HTTPS 全覆盖

### 6.2 后续扩展

| 功能 | 说明 |
|:-----|:------|
| **用户登录** | DSE 账号系统，发布关联用户 |
| **版本管理** | 同一游戏保留最近 N 个版本 |
| **访问统计** | 简单的 PV/UV 统计 |
| **自定义域名** | 支持绑定用户自有域名 |
| **删除游戏** | 管理页面删除已发布游戏 |
| **密码保护** | 可为游戏设置访问密码 |
| **排行榜/云存档** | 简单后端支持，供游戏内调用 |

---

## 七、成本估算

| 项目 | 月费 |
|:-----|:------|
| 轻量云服务器（2核2G 3Mbps） | ￥40 |
| CDN（按量，10GB 以内） | ￥10 |
| 域名 dse.run（续费） | ￥30/年 ≈ ￥2.5/月 |
| **总计** | **≈ ￥52.5/月** |
