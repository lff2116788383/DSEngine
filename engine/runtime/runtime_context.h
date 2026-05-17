#ifndef DSE_RUNTIME_CONTEXT_H
#define DSE_RUNTIME_CONTEXT_H

#include <functional>
#include <memory>
#include <string>

#include "engine/render/rhi/rhi_device.h"

class World;
class AssetManager;

enum class BusinessMode {
    Lua = 0,
    Cpp = 1
};

namespace dse::runtime {

struct RuntimeContext {
    World* world = nullptr;
    AssetManager* asset_manager = nullptr;
    std::unique_ptr<RhiDevice> rhi_device;
    std::function<void(const std::string&)> window_title_setter;
    BusinessMode business_mode = BusinessMode::Lua;
    bool editor_mode = false;
    /// 平台原生窗口句柄（Win32 HWND），D3D11/Vulkan 后端初始化时需要
    void* native_window_handle = nullptr;
    /// 音频系统指针（避免头文件依赖，实际为 dse::gameplay2d::AudioSystem*）
    void* audio_system = nullptr;
    /// 退出应用回调（由 EngineInstance 注入）
    std::function<void()> quit_app;
    /// 设置/获取目标帧率回调
    std::function<void(float)> set_target_fps;
    std::function<float()> get_target_fps;
};

} // namespace dse::runtime

#endif
