/**
 * @file native_main.cpp
 * @brief DSEngine Android 游戏宿主入口（NativeActivity + native_app_glue）
 *
 * 仅 __ANDROID__ 下编译。通过 native_app_glue 的 android_main 接管
 * ANativeActivity 生命周期，把系统的 ANativeWindow / AAssetManager 注入引擎
 * （SetAndroidPendingContext），随后以完整 EngineInstance 运行游戏：
 *
 *   1. 首帧 Surface 就绪后，把 APK assets/ 内的打包产物
 *      （game.dsmanifest + game.dpak / game.bun / launch.cfg）提取到应用
 *      内部存储（internalDataPath），并 chdir 到该目录——引擎的
 *      NativeFileSystem / MountPak / MountBundle 即可像桌面端一样工作；
 *   2. 读取 game.dsmanifest 得到入口 Lua 脚本与窗口标题；
 *   3. EngineInstance::Init() + 逐帧 RunOneFrame()，生命周期（前后台/
 *      Surface 丢失重建）与触摸输入经 GetCurrentAndroidApp() 转发。
 *
 * 编辑器 Build Game（Android 平台）与 scripts/export_android_apk.ps1
 * 产出的 APK 均使用本宿主（android.app.lib_name = dse_android_host）。
 */

#ifdef __ANDROID__

#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <android/log.h>

#include <memory>
#include <string>
#include <fstream>
#include <vector>
#include <unistd.h>

#include "engine/platform/android/android_app.h"
#include "engine/runtime/engine_app.h"
#include "engine/runtime/app_manifest.h"

#define DSE_HOST_TAG "DSEngineHost"
#define HLOGI(...) __android_log_print(ANDROID_LOG_INFO,  DSE_HOST_TAG, __VA_ARGS__)
#define HLOGE(...) __android_log_print(ANDROID_LOG_ERROR, DSE_HOST_TAG, __VA_ARGS__)

using dse::platform::AndroidApp;
using dse::platform::GetCurrentAndroidApp;
using dse::platform::SetAndroidPendingContext;

namespace {

struct HostState {
    std::unique_ptr<dse::runtime::EngineInstance> engine;
    bool engine_ready = false;
    bool window_ready = false;
};

// 把 APK assets/<name> 提取到 <dest_dir>/<name>（已存在且大小一致则跳过）。
bool ExtractAsset(AAssetManager* mgr, const char* name, const std::string& dest_dir) {
    AAsset* asset = AAssetManager_open(mgr, name, AASSET_MODE_STREAMING);
    if (!asset) return false;

    const off64_t size = AAsset_getLength64(asset);
    const std::string dest = dest_dir + "/" + name;

    // 已提取且大小一致 → 跳过（避免每次冷启动重复拷贝大 pak）。
    {
        std::ifstream existing(dest, std::ios::binary | std::ios::ate);
        if (existing.is_open() && existing.tellg() == static_cast<std::streamoff>(size)) {
            AAsset_close(asset);
            HLOGI("asset %s already extracted (%lld bytes)", name, static_cast<long long>(size));
            return true;
        }
    }

    std::ofstream out(dest, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        AAsset_close(asset);
        HLOGE("cannot open %s for write", dest.c_str());
        return false;
    }
    std::vector<char> buf(256 * 1024);
    int n = 0;
    while ((n = AAsset_read(asset, buf.data(), buf.size())) > 0) {
        out.write(buf.data(), n);
    }
    AAsset_close(asset);
    HLOGI("extracted asset %s (%lld bytes)", name, static_cast<long long>(size));
    return true;
}

// launch.cfg（key=value 行）：加密 .bun 构建时由编辑器写入 bundle/key/script。
void ParseLaunchCfg(const std::string& path, std::string& bundle, std::string& key,
                    std::string& script) {
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq);
        const std::string v = line.substr(eq + 1);
        if (k == "bundle")      bundle = v;
        else if (k == "key")    key = v;
        else if (k == "script") script = v;
    }
}

bool StartEngine(struct android_app* a, HostState* hs) {
    AAssetManager* assets = a->activity ? a->activity->assetManager : nullptr;
    const char* internal = a->activity ? a->activity->internalDataPath : nullptr;
    if (!assets || !internal) {
        HLOGE("missing assetManager/internalDataPath");
        return false;
    }

    // 1) 提取打包产物到内部存储并切换工作目录
    const std::string data_dir(internal);
    const bool has_manifest = ExtractAsset(assets, "game.dsmanifest", data_dir);
    const bool has_pak      = ExtractAsset(assets, "game.dpak", data_dir);
    const bool has_bun      = ExtractAsset(assets, "game.bun", data_dir);
    const bool has_cfg      = ExtractAsset(assets, "launch.cfg", data_dir);
    if (chdir(data_dir.c_str()) != 0) {
        HLOGE("chdir(%s) failed", data_dir.c_str());
        return false;
    }

    // 2) 组装引擎启动配置
    SetAndroidPendingContext(a->window, assets);

    dse::runtime::EngineRunConfig cfg;
    cfg.window_title = "DSEngine Game";
    cfg.business_mode = BusinessMode::Lua;
    cfg.startup_lua_script_path = "scripts/main.lua";

    if (has_manifest) {
        dse::runtime::AppManifest manifest;
        if (dse::runtime::LoadAppManifest(data_dir + "/game.dsmanifest", manifest)) {
            if (manifest.has_window_title) cfg.window_title = manifest.window_title;
            if (manifest.has_entry_script) cfg.startup_lua_script_path = manifest.entry_script;
        }
    }
    if (has_cfg) {
        std::string bundle, key, script;
        ParseLaunchCfg(data_dir + "/launch.cfg", bundle, key, script);
        if (!bundle.empty()) cfg.asset_bundle_path = bundle;
        if (!key.empty())    cfg.asset_bundle_key = key;
        if (!script.empty()) cfg.startup_lua_script_path = script;
    } else if (has_bun) {
        cfg.asset_bundle_path = "game.bun";
    }
    if (has_pak) cfg.asset_pak_path = "game.dpak";

    // 窗口尺寸取实际 Surface（Screen 由 Init 内 GetFramebufferSize 再校准）
    cfg.window_width  = ANativeWindow_getWidth(a->window);
    cfg.window_height = ANativeWindow_getHeight(a->window);

    HLOGI("starting engine: script=%s pak=%d bun=%d size=%dx%d",
          cfg.startup_lua_script_path.c_str(), (int)has_pak, (int)has_bun,
          cfg.window_width, cfg.window_height);

    // 3) 启动引擎（Init 内经 CreateDefaultPlatformApp 吸收注入的窗口/资产）
    hs->engine = std::make_unique<dse::runtime::EngineInstance>(cfg);
    if (!hs->engine->Init()) {
        HLOGE("EngineInstance::Init failed");
        hs->engine.reset();
        return false;
    }
    hs->engine_ready = true;
    HLOGI("engine initialized");
    return true;
}

void HandleCmd(struct android_app* a, int32_t cmd) {
    auto* hs = static_cast<HostState*>(a->userData);
    AndroidApp* app = GetCurrentAndroidApp();
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            hs->window_ready = (a->window != nullptr);
            if (!hs->engine_ready) {
                if (a->window && !StartEngine(a, hs)) {
                    HLOGE("engine start failed; exiting");
                    ANativeActivity_finish(a->activity);
                }
            } else if (app && a->window) {
                app->OnSurfaceRegained(a->window);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            hs->window_ready = false;
            if (app) app->OnSurfaceLost();
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
            if (app && a->window) {
                app->OnSurfaceChanged(ANativeWindow_getWidth(a->window),
                                      ANativeWindow_getHeight(a->window));
            }
            break;
        case APP_CMD_PAUSE:
            if (app) app->OnPause();
            break;
        case APP_CMD_RESUME:
            if (app) app->OnResume();
            break;
        default:
            break;
    }
}

int32_t HandleInput(struct android_app* /*a*/, AInputEvent* event) {
    AndroidApp* app = GetCurrentAndroidApp();
    return app ? app->HandleInputEvent(event) : 0;
}

} // namespace

extern "C" void android_main(struct android_app* state) {
    HostState hs;
    state->userData = &hs;
    state->onAppCmd = HandleCmd;
    state->onInputEvent = HandleInput;

    HLOGI("android_main entered");

    while (true) {
        AndroidApp* app = GetCurrentAndroidApp();
        const bool running = hs.engine_ready && app && app->IsRenderable();

        int events = 0;
        struct android_poll_source* source = nullptr;
        // 渲染中不阻塞(timeout 0)，暂停/无 Surface 时阻塞等事件(timeout -1)
        const int timeout = running ? 0 : -1;
        while (ALooper_pollOnce(timeout, nullptr, &events,
                               reinterpret_cast<void**>(&source)) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) {
                HLOGI("destroyRequested; shutting down engine");
                if (hs.engine) {
                    hs.engine->Shutdown();
                    hs.engine.reset();
                }
                return;
            }
        }

        if (running && hs.engine) {
            if (!hs.engine->RunOneFrame()) {
                HLOGI("engine requested exit");
                hs.engine->Shutdown();
                hs.engine.reset();
                hs.engine_ready = false;
                ANativeActivity_finish(state->activity);
            }
        }
    }
}

#endif // __ANDROID__
