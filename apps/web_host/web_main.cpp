/**
 * @file web_main.cpp
 * @brief Web (Emscripten) 宿主入口 — 用 emscripten_set_main_loop 逐帧驱动引擎。
 *
 * 桌面端 EngineInstance::Run() 内部是阻塞式 while 主循环，浏览器不允许阻塞主线程，
 * 故这里改为：Init() 后把 EngineInstance::RunOneFrame() 注册为 requestAnimationFrame
 * 回调。EngineInstance 放在静态指针上以跨越异步回调存活。
 *
 * 资源通过 `--preload-file` 打包进 MEMFS（见 CMakeLists）；存档可走 IDBFS。
 */

#include "engine/runtime/engine_app.h"
#include "engine/runtime/runtime_context.h"

#include <emscripten/emscripten.h>

#include <cstdlib>
#include <memory>

namespace {

std::unique_ptr<dse::runtime::EngineInstance> g_instance;

void WebFrame() {
    if (!g_instance) return;
    if (!g_instance->RunOneFrame()) {
        emscripten_cancel_main_loop();
        g_instance->Shutdown();
        g_instance.reset();
    }
}

} // namespace

int main(int /*argc*/, char** /*argv*/) {
    // Web 默认选用最小前向管线，避免在 WebGL2 上运行完整延迟着色 + HDR 后处理链
    // （其多 RT ping-pong 与 Compute 在 WebGL2 上不成立）。overwrite=0：保留用户
    // 通过环境变量的显式覆盖。
    //
    // DSE_ENABLE_3D（由 DSE_WEB_ENABLE_3D 开启，M5）：默认载入 3D forward 场景；
    // 否则维持已验证的 2D-first MVP。两条路径都用各自的最小非 Compute profile。
#ifdef DSE_ENABLE_3D
    setenv("DSE_RENDER_PIPELINE_PROFILE", "forward_3d", /*overwrite=*/0);
    const char* startup_lua = "data/main3d.lua";
#else
    setenv("DSE_RENDER_PIPELINE_PROFILE", "forward_2d", /*overwrite=*/0);
    const char* startup_lua = "data/main.lua";
#endif

    dse::runtime::EngineRunConfig cfg;
    cfg.window_width  = 1280;
    cfg.window_height = 720;
    cfg.window_title  = "DSEngine Web";
    cfg.business_mode = BusinessMode::Lua;  // BusinessMode 在全局命名空间（runtime_context.h）
    cfg.enable_editor = false;
    cfg.startup_lua_script_path = startup_lua;

    g_instance = std::make_unique<dse::runtime::EngineInstance>(cfg);
    if (!g_instance->Init()) {
        return -1;
    }

    // fps=0 → 跟随浏览器 requestAnimationFrame；simulate_infinite_loop=1 → main 不返回，
    // 保持运行时对象存活。
    emscripten_set_main_loop(WebFrame, 0, /*simulate_infinite_loop=*/1);
    return 0;
}
