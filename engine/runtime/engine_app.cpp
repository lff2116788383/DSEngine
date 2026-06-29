/**
 * @file engine_app.cpp
 * @brief 引擎应用外壳，负责运行时生命周期与服务装配。
 */

#include "engine/runtime/engine_app.h"
#include "engine/platform/platform_app.h"
#include "engine/assets/native_file_system.h"
#ifdef DSE_ENABLE_LUA
#include "engine/scripting/lua/lua_runtime.h"
#endif
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/render/rhi/rhi_factory.h"
#include "engine/render/rhi/ubo_types.h"
#ifdef _WIN32
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif
#include <iostream>
#include <thread>
#include "engine/base/debug.h"
#include "engine/diagnostics/crash_handler.h"
#include "engine/core/job_system.h"
#include "engine/core/memory/memory.h"
#include "engine/core/service_locator.h"
#include "engine/core/event_bus.h"
#include "engine/render/font/font_service.h"
#include <utility>
#include "engine/base/time.h"
#include "engine/base/time_context.h"
#include "engine/platform/screen.h"
#include "engine/input/input.h"
#include "engine/input/key_code.h"
#include "engine/input/touch.h"
#include <cstdlib>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <string>
#include "engine/assets/localization_manager.h"

#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif
#include <stb_image_write.h>

namespace dse::runtime {
namespace {

// 桌面端用鼠标左键模拟单点触摸，便于在无触屏设备上验证 Touch 抽象层。
bool g_mouse_touch_down = false;

std::string ReadNonEmptyEnv(const char* name) {
    if (const char* value = std::getenv(name)) {
        if (value[0] != '\0') {
            return value;
        }
    }
    return {};
}

bool IsStartupSceneRegressionDisabled() {
    // 默认跳过场景回归测试（生产/demo 场景不需要）。
    // 设置 DSE_ENABLE_STARTUP_SCENE_REGRESSION=1 显式启用。
    const char* env = std::getenv("DSE_ENABLE_STARTUP_SCENE_REGRESSION");
    bool enabled = env && env[0] != '\0' && std::string(env) != "0";
    return !enabled;
}

std::string RuntimeOutputPathInBin(const char* filename) {
    std::filesystem::path current_path = std::filesystem::current_path();
    if (current_path.filename() == "bin") {
        return (current_path / filename).string();
    }
    return (current_path / "bin" / filename).string();
}

void FlipImageRowsRgba8(std::vector<unsigned char>& pixels, int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }

    const std::size_t row_bytes = static_cast<std::size_t>(width) * 4u;
    std::vector<unsigned char> row(row_bytes, 0u);
    for (int y = 0; y < height / 2; ++y) {
        auto* top = pixels.data() + static_cast<std::size_t>(y) * row_bytes;
        auto* bottom = pixels.data() + static_cast<std::size_t>(height - 1 - y) * row_bytes;
        std::copy(top, top + row_bytes, row.begin());
        std::copy(bottom, bottom + row_bytes, top);
        std::copy(row.begin(), row.end(), bottom);
    }
}

// 解析 splash logo 路径：优先 exe 旁，其次向上 data/icon/dse_icon.png。
std::string ResolveSplashImagePath() {
    std::filesystem::path exe_dir;
#if defined(_WIN32)
    wchar_t module_buf[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, module_buf, MAX_PATH)) {
        exe_dir = std::filesystem::path(module_buf).parent_path();
    }
#endif
    const char* candidates[] = {
        "data/icon/dse_icon.png",
        "../data/icon/dse_icon.png",
        "../../data/icon/dse_icon.png",
    };
    for (const char* rel : candidates) {
        std::filesystem::path p = exe_dir.empty() ? std::filesystem::path(rel) : exe_dir / rel;
        std::error_code ec;
        if (std::filesystem::exists(p, ec)) {
            return p.string();
        }
    }
    return {};
}

// 是否启用启动 splash：自动化/截图/无头与 DSE_SPLASH=0 时关闭。
bool ShouldShowSplash() {
    if (const char* v = std::getenv("DSE_SPLASH")) {
        if (std::string(v) == "0") return false;
    }
    if (!ReadNonEmptyEnv("DSE_MAX_FRAMES").empty()) return false;
    if (!ReadNonEmptyEnv("DSE_SCREENSHOT_PATH").empty()) return false;
    if (!ReadNonEmptyEnv("DSE_SCREENSHOT_FRAME").empty()) return false;
    return true;
}

bool CaptureRuntimeScreenshot(FramePipeline& pipeline) {
    const std::string screenshot_path = ReadNonEmptyEnv("DSE_SCREENSHOT_PATH");
    if (screenshot_path.empty()) {
        return true;
    }

    const std::string target = ReadNonEmptyEnv("DSE_SCREENSHOT_TARGET");
    RenderTargetReadback readback{};
    if (target == "main")
        readback = pipeline.ReadMainColorRgba8WithSize();
    else if (target == "bloom")
        readback = pipeline.ReadBloomMip0Rgba8WithSize();
    else if (target == "bloom_extract")
        readback = pipeline.ReadBloomExtractRgba8WithSize();
    else {
        readback = pipeline.ReadSceneColorRgba8WithSize();
        if (readback.pixels.empty()) {
            readback = pipeline.ReadMainColorRgba8WithSize();
        }
    }
    if (readback.pixels.empty() || readback.width <= 0 || readback.height <= 0) {
        std::cerr << "Failed to capture screenshot pixels"
                  << " w=" << readback.width << " h=" << readback.height
                  << " pixels=" << readback.pixels.size()
                  << " target=" << (target.empty() ? "scene" : target)
                  << " screen=" << Screen::width() << "x" << Screen::height() << "\n";
        // 回退到默认帧缓冲回读
        return false;
    }

    const std::size_t expected_size = static_cast<std::size_t>(readback.width) * static_cast<std::size_t>(readback.height) * 4u;
    if (readback.pixels.size() != expected_size) {
        std::cerr << "Unexpected screenshot pixel buffer size\n";
        return false;
    }

    if (pipeline.NeedsReadbackYFlip()) {
        FlipImageRowsRgba8(readback.pixels, readback.width, readback.height);
    }
    std::filesystem::create_directories(std::filesystem::path(screenshot_path).parent_path());
    if (stbi_write_png(screenshot_path.c_str(), readback.width, readback.height, 4, readback.pixels.data(), readback.width * 4) == 0) {
        std::cerr << "Failed to write screenshot png: " << screenshot_path << "\n";
        return false;
    }

    std::cout << "DSE_SCREENSHOT_WRITTEN path=" << screenshot_path << " target=" << (target.empty() ? "scene" : target)
              << " size=" << readback.width << "x" << readback.height << std::endl;
    return true;
}
}



EngineInstance::EngineInstance(const EngineRunConfig& config)
    : config_(config)
    , services_(config.services) {
    // 最早期初始化内存子系统（幂等），早于任何子系统分配。
    core::Memory::Init();

    if (services_.world == nullptr) {
        default_world_ = std::make_unique<World>();
        services_.world = default_world_.get();
    }
    if (services_.asset_manager == nullptr) {
        default_asset_manager_ = std::make_unique<AssetManager>();
        services_.asset_manager = default_asset_manager_.get();
    }
    if (services_.job_system == nullptr) {
        default_job_system_ = std::make_unique<core::JobSystem>();
        services_.job_system = default_job_system_.get();
    }

    pipeline_ = std::make_unique<FramePipeline>();
}

EngineInstance::~EngineInstance() {
    Shutdown();
}

bool EngineInstance::RunStartupSceneRegressionChecks() {
    if (IsStartupSceneRegressionDisabled()) {
        DEBUG_LOG_INFO("EngineInstance init: startup scene regression checks skipped by env");
        return true;
    }

    DEBUG_LOG_INFO("EngineInstance init: startup scene regression begin");
    const bool scene_round_trip_ok = ::scene::RunSceneRoundTripRegressionSample(RuntimeOutputPathInBin("scene_roundtrip_regression.json"));
    DEBUG_LOG_INFO("Scene round-trip regression: {}", scene_round_trip_ok ? "PASSED" : "FAILED");
    const bool scene_backward_compat_ok = ::scene::RunSceneBackwardCompatibilityRegressionSample(RuntimeOutputPathInBin("scene_backward_compat_regression.json"));
    DEBUG_LOG_INFO("Scene backward-compat regression: {}", scene_backward_compat_ok ? "PASSED" : "FAILED");
    return scene_round_trip_ok && scene_backward_compat_ok;
}

void EngineInstance::RegisterRuntimeServices() {
    auto pipeline_shared = std::shared_ptr<FramePipeline>(pipeline_.get(), [](FramePipeline*) {});
    service_locator().Register<FramePipeline, FramePipeline>(pipeline_shared);

    if (services_.world) {
        auto world_shared = std::shared_ptr<World>(services_.world, [](World*) {});
        service_locator().Register<World, World>(world_shared);
    }

    event_bus_ = std::make_shared<core::EventBus>(&service_locator());
    service_locator().Register<core::EventBus, core::EventBus>(event_bus_);

    if (services_.job_system) {
        auto job_system_shared = std::shared_ptr<core::JobSystem>(services_.job_system, [](core::JobSystem*) {});
        service_locator().Register<core::JobSystem, core::JobSystem>(job_system_shared);
    }

    scene_manager_ = std::make_shared<scene::SceneManager>();
    scene_manager_->SetWorld(services_.world);
    scene_manager_->SetAssetManager(services_.asset_manager);
    scene_manager_->SetEventBus(event_bus_.get());
    scene_manager_->SetJobSystem(services_.job_system);
    service_locator().Register<scene::SceneManager, scene::SceneManager>(scene_manager_);

    localization_manager_ = std::make_shared<dse::assets::LocalizationManager>();
    service_locator().Register<dse::assets::LocalizationManager, dse::assets::LocalizationManager>(localization_manager_);

    font_service_ = std::make_shared<dse::render::FontService>();
    if (pipeline_) {
        auto* rhi = pipeline_->GetRhiDevice();
        if (rhi) {
            font_service_->SetTextureCallbacks(
                [rhi](int w, int h, const unsigned char* data, bool linear) {
                    // 字形图集用 ClampToEdge：避免相邻字形在 linear 采样下边缘出血(bleeding)。
                    TextureSamplerDesc sampler;
                    sampler.filter = linear ? TextureFilter::Linear : TextureFilter::Nearest;
                    sampler.wrap = TextureWrap::ClampToEdge;
                    return rhi->CreateTexture2D(w, h, data, sampler);
                },
                [rhi](unsigned int handle) {
                    rhi->DeleteTexture(handle);
                });
        }
    }
    service_locator().Register<dse::render::FontService, dse::render::FontService>(font_service_);

    service_locator().BridgeTo<FramePipeline>(core::ServiceLocator::Instance());
    service_locator().BridgeTo<World>(core::ServiceLocator::Instance());
    service_locator().BridgeTo<core::EventBus>(core::ServiceLocator::Instance());
    service_locator().BridgeTo<core::JobSystem>(core::ServiceLocator::Instance());
    service_locator().BridgeTo<scene::SceneManager>(core::ServiceLocator::Instance());
    service_locator().BridgeTo<dse::assets::LocalizationManager>(core::ServiceLocator::Instance());
    service_locator().BridgeTo<dse::render::FontService>(core::ServiceLocator::Instance());
}

void EngineInstance::ResetRuntimeServices() {
    service_locator().Reset<core::JobSystem>();
    service_locator().Reset<core::EventBus>();
    service_locator().Reset<FramePipeline>();
    service_locator().Reset<World>();
    service_locator().Reset<scene::SceneManager>();
    service_locator().Reset<dse::assets::LocalizationManager>();
    service_locator().Reset<dse::render::FontService>();
    service_locator().Reset<dse::assets::FileSystem>();
    if (font_service_) { font_service_->Shutdown(); font_service_.reset(); }
    localization_manager_.reset();
    if (scene_manager_) { scene_manager_.reset(); }
    event_bus_.reset();

    core::ServiceLocator::Instance().Reset<core::JobSystem>();
    core::ServiceLocator::Instance().Reset<FramePipeline>();
    core::ServiceLocator::Instance().Reset<World>();
    core::ServiceLocator::Instance().Reset<core::EventBus>();
    core::ServiceLocator::Instance().Reset<scene::SceneManager>();
}

void EngineInstance::CleanupOnInitFailure() {
    splash_.Finish();
    ResetRuntimeServices();
    pipeline_->Shutdown();
    if (services_.job_system) {
        services_.job_system->Shutdown();
    }
    Debug::ShutDown();
    if (!config_.enable_editor && platform_) platform_->Shutdown();
}

bool EngineInstance::Init() {

    if (is_initialized_) return true;

    // 如果未启用编辑器模式，则初始化系统环境
    const auto rhi_backend = dse::render::ValidateRhiBackend(dse::render::ResolveRhiBackendFromEnv());
    // WebGPU(Web)：画布只能持有单一上下文，GL context 会占用 #canvas 使 getContext('webgpu')
    // 失败，故 WebGPU 后端同 D3D11/Vulkan 一样不创建 GL context（不影响 A 阶段 WebGL2 回退）。
    const bool needs_gl_context = (rhi_backend != RhiBackend::D3D11 &&
                                   rhi_backend != RhiBackend::Vulkan &&
                                   rhi_backend != RhiBackend::WebGPU);

    platform_ = dse::platform::CreateDefaultPlatformApp();

    if (!config_.enable_editor) {
        // 启动 splash：进程一启动即弹出 logo，盖住窗口创建→首帧之间的空白期，带淡入淡出。
        const bool splash_on = ShouldShowSplash();
        if (std::getenv("DSE_SPLASH_DEBUG")) {
            std::cerr << "[splash] splash_on=" << (int)splash_on
                      << " img='" << ResolveSplashImagePath() << "'" << std::endl;
        }
        if (splash_on) {
            // 优先用打包/工程提供的品牌化 splash 配置；未给出的字段回退到引擎默认。
            dse::platform::SplashConfig splash_cfg = config_.use_splash_config ? config_.splash
                                                                              : dse::platform::SplashConfig{};
            if (splash_cfg.image_path.empty()) splash_cfg.image_path = ResolveSplashImagePath();
            if (splash_cfg.app_name.empty()) splash_cfg.app_name = "DSEngine";
            if (splash_cfg.initial_status.empty()) splash_cfg.initial_status = "正在启动…";
            dse::platform::ApplySplashEnvOverrides(splash_cfg);
            splash_.Show(splash_cfg);
        }

        dse::platform::WindowConfig win_cfg;
        win_cfg.width  = config_.window_width;
        win_cfg.height = config_.window_height;
        win_cfg.title  = config_.window_title;
        win_cfg.no_graphics_api = !needs_gl_context;
        win_cfg.gl_fallback_33  = true;
        win_cfg.start_hidden    = splash_on;  // 首帧后再 Show()

        splash_.SetStatus("正在创建窗口…");
        if (!platform_->Init(win_cfg)) {
            splash_.Finish();
            return false;
        }

        if (needs_gl_context) {
            if (!platform_->LoadGLFunctions()) {
                splash_.Finish();
                platform_->Shutdown();
                return false;
            }
        }

        int framebuffer_width = config_.window_width;
        int framebuffer_height = config_.window_height;
        platform_->GetFramebufferSize(framebuffer_width, framebuffer_height);
        Screen::set_width_height(framebuffer_width, framebuffer_height);

        // 注入平台窗口句柄（D3D11/Vulkan 需要在 FramePipeline::Init 中使用）
        if (!needs_gl_context) {
            void* native_handle = platform_->GetNativeWindowHandle();
            if (native_handle) {
                pipeline_->SetNativeWindowHandle(native_handle);
            }
        }
    } else {
        // 编辑器模式：外部窗口已由编辑器创建
        platform_->AttachExternal(nullptr);  // 无特定窗口，仅用于 LoadGLFunctions
        platform_->LoadGLFunctions();

        Screen::set_width_height(config_.window_width, config_.window_height);
    }

    if (!config_.enable_editor) {
        platform_->SetInputCallbacks(
            [](int key, int action) { Input::RecordKey(static_cast<unsigned short>(key), static_cast<unsigned short>(action)); },
            [](int btn, int action) {
                Input::RecordKey(static_cast<unsigned short>(btn), static_cast<unsigned short>(action));
                // 桌面：鼠标左键 → 单点触摸（finger 0）模拟，便于验证 Touch 抽象层。
                if (btn == MOUSE_BUTTON_LEFT) {
                    const glm::vec2 p = Input::mousePosition();
                    if (action == KEY_ACTION_UP) {
                        g_mouse_touch_down = false;
                        dse::input::Touch::RecordTouch(0, p.x, p.y, dse::input::TouchPhase::Ended);
                    } else {
                        g_mouse_touch_down = true;
                        dse::input::Touch::RecordTouch(0, p.x, p.y, dse::input::TouchPhase::Began);
                    }
                }
            },
            [](float y) { Input::RecordMouseScroll(y); },
            [](float x, float y) {
                Input::RecordMousePosition(x, y);
                if (g_mouse_touch_down) {
                    dse::input::Touch::RecordTouch(0, x, y, dse::input::TouchPhase::Moved);
                }
            }
        );

        // 手柄数据源：GLFW 每帧 PollGamepads 后经此回调喂入 Input。
        platform_->SetGamepadCallbacks(
            [](int /*gamepad_id*/, int button, int action) {
                Input::RecordKey(static_cast<unsigned short>(button), static_cast<unsigned short>(action));
            },
            [](int gamepad_id, int axis, float value) { Input::RecordGamepadAxis(gamepad_id, axis, value); },
            [](int gamepad_id, bool connected) { Input::SetGamepadConnected(gamepad_id, connected); }
        );

        // 触摸数据源（Android 触屏等）：平台层喂入触点事件。
        platform_->SetTouchCallback(
            [](int finger_id, float x, float y, int phase) {
                dse::input::Touch::RecordTouch(finger_id, x, y, static_cast<dse::input::TouchPhase>(phase));
            }
        );

        pipeline_->SetWindowTitleSetter([this](const std::string& title) {
            platform_->SetWindowTitle(title);
        });
    } else {
        pipeline_->SetWindowTitleSetter([](const std::string&) {
            // Do nothing — 编辑器模式保持编辑器窗口标题不受干扰
        });
    }

    Debug::Init();

    // 进程级崩溃报告器（默认启用；设环境变量 DSE_CRASH_HANDLER=0 可关闭）。
    // 纯本地：崩溃时在 dump 目录落地可读 .txt 报告，Windows 另写 minidump .dmp。
    // 上传保持关闭——由上层按需注册 UploadCallback（自建端点复用 dse.http，或接
    // Sentry/BugSplat 等 SaaS），引擎不内置任何后端，保证“零服务器即可用”。
    {
        const char* disable = std::getenv("DSE_CRASH_HANDLER");
        if (!(disable && std::string(disable) == "0")) {
            dse::diagnostics::CrashHandlerConfig crash_cfg;
            crash_cfg.app_name = "DSEngine";
            if (const char* dir = std::getenv("DSE_CRASH_DIR")) {
                if (dir[0] != '\0') crash_cfg.dump_dir = dir;
            }
            dse::diagnostics::CrashReporter::Instance().Install(crash_cfg);
            dse::diagnostics::CrashReporter::Instance().AddBreadcrumb("engine init");
        }
    }

    if (services_.job_system) {
        services_.job_system->Init();
    }

    // 先登记 EngineInstance 级服务容器，再桥接到兼容全局入口。
    RegisterRuntimeServices();

    // 初始化期间保持窗口消息泵送，防止 Windows 标记"未响应"
    if (platform_) platform_->PollEvents();

    if (platform_) {
        pipeline_->SetInitKeepAlive([this]() {
            platform_->PollEvents();
        });
    }
    pipeline_->EnableEditorMode(config_.enable_editor);
    pipeline_->SetWorld(services_.world);
    pipeline_->SetAssetManager(services_.asset_manager);
    if (services_.asset_manager) {
        services_.asset_manager->SetEventBus(event_bus_.get());
        services_.asset_manager->SetJobSystem(services_.job_system);
        if (!services_.asset_manager->GetFileSystem()) {
            default_file_system_ = std::make_unique<dse::assets::NativeFileSystem>();
            services_.asset_manager->SetFileSystem(default_file_system_.get());
            auto fs_shared = std::shared_ptr<dse::assets::FileSystem>(default_file_system_.get(), [](dse::assets::FileSystem*) {});
            service_locator().Register<dse::assets::FileSystem, dse::assets::FileSystem>(fs_shared);
        }

        // 挂载离线打包产物：.dpak 归档 + 加密/明文 .bun 资源包。
        // 挂载后 LoadFileToMemory 与 Lua VFS searcher 即可直接从中读取，无需磁盘松散文件。
        if (!config_.asset_pak_path.empty()) {
            if (services_.asset_manager->MountPak(config_.asset_pak_path)) {
                std::cout << "Mounted pak: " << config_.asset_pak_path << std::endl;
            } else {
                std::cerr << "Failed to mount pak: " << config_.asset_pak_path << std::endl;
            }
        }
        if (!config_.asset_bundle_path.empty()) {
            if (services_.asset_manager->MountBundle(config_.asset_bundle_path, config_.asset_bundle_key)) {
                std::cout << "Mounted bundle: " << config_.asset_bundle_path << std::endl;
            } else {
                std::cerr << "Failed to mount bundle: " << config_.asset_bundle_path << std::endl;
            }
        }
    }
    pipeline_->SetBusinessMode(config_.business_mode);
    if (platform_) {
        pipeline_->SetQuitCallback([this]() {
            platform_->RequestClose();
        });
    }
    pipeline_->SetTargetFpsCallbacks(
        [this](float fps) { target_fps_ = fps; },
        [this]() { return target_fps_; }
    );
    
#ifdef DSE_ENABLE_LUA
    if (config_.business_mode == BusinessMode::Lua && !config_.startup_lua_script_path.empty()) {
        SetStartupLuaScriptPath(config_.startup_lua_script_path);
    }
#endif
    
    std::cout << "Business mode: " << (config_.business_mode == BusinessMode::Lua ? "lua" : "cpp") << std::endl;

    if (platform_) platform_->PollEvents();

    // Phase 2: 注入渲染线程 context 管理回调
    if (platform_) {
        auto* p = platform_.get();
        pipeline_->SetRenderContextCallbacks(
            [p]() { p->MakeContextCurrent(); },
            [p]() { p->ReleaseContext(); },
            [p]() { p->SwapBuffers(); }
        );
    }

    splash_.SetStatus("正在初始化渲染管线…");
    if (!pipeline_->Init()) {
        std::cerr << "Failed to initialize FramePipeline\n";
        CleanupOnInitFailure();
        return false;
    }
    pipeline_->SetInitKeepAlive(nullptr);
    splash_.SetStatus("正在加载场景…");

    if (!RunStartupSceneRegressionChecks()) {
        std::cerr << "Startup scene regression checks failed\n";
        CleanupOnInitFailure();
        return false;
    }

    is_initialized_ = true;
    return true;
}


void EngineInstance::Tick() {
    if (!is_initialized_) return;

    Time::Update();
    // 先对真实 dt 做首帧/卡顿钳制（Vulkan 初始化比 OpenGL 慢，首帧 dt 可能数秒）
    float unscaled_dt = Time::delta_time();
    if (unscaled_dt > 0.1f) unscaled_dt = 0.1f;
    // 再乘全局时间缩放：scale=0 → scaled_dt=0 → gameplay 与物理累加器同时冻结
    const float time_scale = Time::time_scale();
    const float scaled_dt = unscaled_dt * time_scale;

    // Clamp accumulator to prevent spiral-of-death when dt is very large
    // (e.g. after a loading stall or breakpoint). Allow at most 10 fixed steps per frame.
    constexpr float kMaxAccumulator = 0.2f; // 10 * 0.02s
    // 物理固定步长累加器吃缩放后 dt：scale=0 时不再累加 → 物理冻结，无穿透
    accumulator_ += scaled_dt;
    if (accumulator_ > kMaxAccumulator) {
        accumulator_ = kMaxAccumulator;
    }
    pipeline_->FlushPhysicsEvents();
    while (accumulator_ >= fixed_time_step_) {
        pipeline_->FixedUpdate(fixed_time_step_);
        accumulator_ -= fixed_time_step_;
    }

    pipeline_->Update(dse::FrameUpdateContext{
        dse::TimeContext{scaled_dt, unscaled_dt, time_scale}, frame_index_++});
    pipeline_->Render();
    Input::Update();
    dse::input::Touch::Update();
}

void EngineInstance::Shutdown() {
    if (!is_initialized_) return;

    splash_.Finish();
    pipeline_->SetWindowTitleSetter(nullptr);
    pipeline_->Shutdown();
    if (services_.job_system) {
        services_.job_system->Shutdown();
    }

    if (services_.asset_manager) {
        services_.asset_manager->SetEventBus(nullptr);
        services_.asset_manager->SetJobSystem(nullptr);
        services_.asset_manager->SetFileSystem(nullptr);
    }

    // 清理实例级/兼容级 ServiceLocator 中的服务引用（不销毁 World 本身，由 EngineInstance 管理）
    ResetRuntimeServices();

    // FramePipeline and the default runtime services own objects whose destructors may touch
    // engine/runtime state. Destroy owned instances while platform and the debug logger are still
    // alive instead of deferring to EngineInstance member destruction after RunEngine() returns.
    pipeline_.reset();
    if (default_world_) {
        default_world_->Clear();
        // EnTT keeps component storage pools allocated after clear(); one of the late pool
        // destructors is currently crashing during process teardown after runtime shutdown.
        // The world is empty at this point and the process is exiting, so intentionally detach
        // the default world to avoid a late registry destructor re-entering released state.
        (void)default_world_.release();
        services_.world = nullptr;
    }
    default_asset_manager_.reset();
    default_job_system_.reset();
    default_file_system_.reset();

    Debug::ShutDown();

    if (!config_.enable_editor && platform_) {
        platform_->Shutdown();
    }
    platform_.reset();

    is_initialized_ = false;
}

bool EngineInstance::RunOneFrame() {
    if (!platform_) return false;

    if (!loop_started_) {
        loop_started_ = true;
        loop_max_frames_ = 0;
        if (const char* env_max_frames = std::getenv("DSE_MAX_FRAMES")) {
            loop_max_frames_ = (std::max)(0, std::atoi(env_max_frames));
        }
        loop_screenshot_frame_ = 0;
        if (const char* env_ss_frame = std::getenv("DSE_SCREENSHOT_FRAME")) {
            loop_screenshot_frame_ = (std::max)(0, std::atoi(env_ss_frame));
        }
        loop_screenshot_taken_ = false;
        loop_frame_counter_ = 0;
        loop_prev_fb_width_  = Screen::width();
        loop_prev_fb_height_ = Screen::height();
    }

    if (platform_->ShouldClose()) return false;

    const double frame_start = platform_->GetTime();
    platform_->PollEvents();
    platform_->PollGamepads();

    int width = 0;
    int height = 0;
    platform_->GetFramebufferSize(width, height);

    if (width > 0 && height > 0 &&
        (width != loop_prev_fb_width_ || height != loop_prev_fb_height_)) {
        pipeline_->OnWindowResize(width, height);
        loop_prev_fb_width_  = width;
        loop_prev_fb_height_ = height;
    }

    Tick();

    // Present / SwapBuffers 在 render 计时之外执行
    // Phase 2: 渲染线程活跃时由渲染线程负责 SwapBuffers
    if (platform_->HasGLContext() && !pipeline_->IsRenderThreadActive()) {
        platform_->SwapBuffers();
    } else {
        // DX11/Vulkan: Present 交换链（EndFrame 不再包含 Present）
        pipeline_->PresentFrame();
    }

    // 首帧已上屏：显示主窗口并淡出 splash（满足最短显示时长后才真正消失）。
    if (!first_frame_shown_) {
        first_frame_shown_ = true;
        platform_->Show();
        splash_.Finish();
    }
    if (target_fps_ > 0.0f) {
        const double target_frame_time = 1.0 / static_cast<double>(target_fps_);
        double remaining = target_frame_time - (platform_->GetTime() - frame_start);
        // sleep 大部分等待时间（保留 1.5ms 给 spin-wait 以补偿 OS 调度抖动）
        if (remaining > 0.0015) {
            std::this_thread::sleep_for(
                std::chrono::microseconds(static_cast<int>((remaining - 0.0015) * 1e6)));
        }
        // spin-wait 精确到目标时刻
        while ((platform_->GetTime() - frame_start) < target_frame_time) {
            // busy wait
        }
    }
    loop_frame_counter_ += 1;
    if (loop_screenshot_frame_ > 0 && loop_frame_counter_ == loop_screenshot_frame_ && !loop_screenshot_taken_) {
        loop_screenshot_taken_ = CaptureRuntimeScreenshot(*pipeline_);
    }
    if (loop_max_frames_ > 0 && loop_frame_counter_ >= loop_max_frames_) {
        std::cout << "DSE_MAX_FRAMES reached: " << loop_frame_counter_ << std::endl;
        return false;
    }
    return true;
}

int EngineInstance::Run() {
    if (!Init()) return -1;

    if (!platform_) {
        std::cerr << "No platform app available\n";
        return -1;
    }

#ifdef _WIN32
    // 提升 Windows 定时器分辨率至 1ms（默认 ~15ms），改善 sleep 精度
    timeBeginPeriod(1);
#endif

    while (RunOneFrame()) {
        // 桌面端阻塞主循环；Web 端改由 emscripten_set_main_loop 逐帧驱动 RunOneFrame()。
    }

#ifdef _WIN32
    timeEndPeriod(1);
#endif

    const bool screenshot_ok = loop_screenshot_taken_ || CaptureRuntimeScreenshot(*pipeline_);

    Shutdown();
    return screenshot_ok ? 0 : -2;
}

int RunEngine(const EngineRunConfig& config) {
    std::cout << "Starting DSEngine Runtime..." << std::endl;
    EngineInstance instance(config);
    return instance.Run();
}
}
