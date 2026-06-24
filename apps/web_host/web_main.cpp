/**
 * @file web_main.cpp
 * @brief Web (Emscripten) 宿主入口 — 用 emscripten_set_main_loop 逐帧驱动引擎。
 *
 * 桌面端 EngineInstance::Run() 内部是阻塞式 while 主循环，浏览器不允许阻塞主线程，
 * 故这里改为：Init() 后把 EngineInstance::RunOneFrame() 注册为 requestAnimationFrame
 * 回调。EngineInstance 放在静态指针上以跨越异步回调存活。
 *
 * 资源通过 `--preload-file` 打包进 MEMFS（见 CMakeLists）；存档可走 IDBFS。
 *
 * 运行期路径选择（A5 打磨）：渲染剖面/启动脚本不再由编译期宏写死，而是由 shell.html
 * 依据 URL 参数（?mode=2d|3d、?profile=NAME、?lua=PATH）和 WebGL2 能力探测算好后，
 * 通过 Module.arguments 以命令行参数传入；编译期宏仅决定缺省值。这样同一份产物可在
 * 运行时按 URL 或浏览器能力自动选 2D / 3D 路径并允许显式覆盖。
 */

#include "engine/runtime/engine_app.h"
#include "engine/runtime/runtime_context.h"

#include <emscripten/emscripten.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

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

// 解析 "--key=value" 形式参数；命中则返回 value，否则返回 nullptr。
const char* MatchArg(const char* arg, std::string_view key) {
    const std::string_view a(arg);
    if (a.size() > key.size() && a.compare(0, key.size(), key) == 0 &&
        a[key.size()] == '=') {
        return arg + key.size() + 1;
    }
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    // 编译期缺省：DSE_ENABLE_3D（由 DSE_WEB_ENABLE_3D 开启，M5）默认 3D forward 场景；
    // 否则维持已验证的 2D-first MVP。两条路径都用各自的最小非 Compute profile。
#ifdef DSE_ENABLE_3D
    std::string_view mode = "3d";
#else
    std::string_view mode = "2d";
#endif
    const char* profile_override = nullptr;  // 显式 --profile= 覆盖
    const char* lua_override = nullptr;       // 显式 --lua= 覆盖

    for (int i = 1; i < argc && argv[i]; ++i) {
        if (const char* v = MatchArg(argv[i], "--mode")) {
            if (std::strcmp(v, "2d") == 0 || std::strcmp(v, "3d") == 0) mode = v;
        } else if (const char* p = MatchArg(argv[i], "--profile")) {
            profile_override = p;
        } else if (const char* l = MatchArg(argv[i], "--lua")) {
            lua_override = l;
        }
    }

    const bool is_3d = (mode == "3d");
    const char* default_profile = is_3d ? "forward_3d" : "forward_2d";
    const char* default_lua = is_3d ? "data/main3d.lua" : "data/main.lua";
    const char* startup_lua = lua_override ? lua_override : default_lua;

    // overwrite=1：运行期解析已是单一真相源（命令行 > 缺省）。引擎深处仍按
    // getenv("DSE_RENDER_PIPELINE_PROFILE") 解析剖面，故这里写入进程环境。
    setenv("DSE_RENDER_PIPELINE_PROFILE",
           profile_override ? profile_override : default_profile, /*overwrite=*/1);

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
