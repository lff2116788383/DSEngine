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
    dse::runtime::EngineRunConfig cfg;
    cfg.window_width  = 1280;
    cfg.window_height = 720;
    cfg.window_title  = "DSEngine Web";
    cfg.business_mode = BusinessMode::Lua;  // BusinessMode 在全局命名空间（runtime_context.h）
    cfg.enable_editor = false;
    cfg.startup_lua_script_path = "data/main.lua";

    g_instance = std::make_unique<dse::runtime::EngineInstance>(cfg);
    if (!g_instance->Init()) {
        return -1;
    }

    // fps=0 → 跟随浏览器 requestAnimationFrame；simulate_infinite_loop=1 → main 不返回，
    // 保持运行时对象存活。
    emscripten_set_main_loop(WebFrame, 0, /*simulate_infinite_loop=*/1);
    return 0;
}
