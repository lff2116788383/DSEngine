# DSEngine 跨平台抽象方案

> 状态：**Phase 4 已完成（Android 准备）**  
> 日期：2025-05-19（规划）→ 2026-05-19（Phase 1 实施）→ 2026-05-19（Phase 2 实施）→ 2026-05-19（Phase 3 实施）→ 2026-05-19（Phase 4 实施）  
> 前置完成：RHI 三后端统一 + Shader 生成架构 + Reflection 驱动绑定

---

## 一、现状分析

### 1.1 已跨平台的部分

| 模块 | 状态 | 说明 |
|------|------|------|
| RHI 层 | ✅ | `RhiDevice` 纯虚接口，OpenGL/Vulkan/DX11 三后端对称 |
| Shader 系统 | ✅ | SPIR-V 统一源码 → GLSL 430 + HLSL SM5.0 + DXBC + SPIR-V binary |
| Shader Reflection | ✅ | 36 个 `*_reflect.gen.h` 编译期反射数据，三后端消费 |
| 动态库加载 | ✅ | `DynamicLibrary` 已有 Win32/macOS/Linux 三路径 |
| 时间系统 | ✅ | `Time` 类使用 `std::chrono::steady_clock` |
| 输入数据层 | ✅ | `Input` 类是纯数据，不依赖平台 API |
| 线程/同步 | ✅ | `std::thread` / `std::mutex` / `std::condition_variable` |
| 文件系统 | ⚠️ | `std::filesystem`（桌面可用，Android APK 内资源不可用） |

### 1.2 平台耦合点

| 优先级 | 模块 | 问题 | 影响文件 |
|--------|------|------|----------|
| **P0** | 窗口/应用层 | `engine_app.cpp` 直接调用 69 处 GLFW API | `engine_app.h/cpp` |
| **P0** | Vulkan Surface | `vulkan_context.cpp` 只有 `#ifdef _WIN32` 的 Win32 surface | `vulkan_context.cpp` |
| **P1** | GL Loader | `gladLoadGL(glfwGetProcAddress)` 硬编码 | `engine_app.cpp` |
| **P1** | 原生窗口句柄 | `glfwGetWin32Window()` + `#ifdef _WIN32` | `engine_app.cpp` |
| **P2** | 定时器精度 | `timeBeginPeriod/timeEndPeriod` Win32 only | `engine_app.cpp`（2 行） |
| **P2** | 音频 | FMOD Studio 硬编码 | `CMakeLists.txt` 条件编译已隔离 |

### 1.3 GLFW 依赖范围（仅 3 个文件）

- `engine/runtime/engine_app.cpp` — 69 处（全部核心逻辑）
- `engine/runtime/engine_app.h` — 2 处（注释 + `void* glfw_window_`）
- `engine/scripting/lua/bindings/lua_binding_core.cpp` — 1 处（仅注释）

**渲染层 `engine/render/` 下零 GLFW 引用。**

---

## 二、目标平台矩阵

| 平台 | 窗口系统 | 图形 API | 资产 I/O | 状态 |
|------|---------|----------|---------|------|
| Windows | GLFW | OpenGL 4.3 / Vulkan / D3D11 | std::filesystem | ✅ 当前已支持 |
| Linux | GLFW | OpenGL 4.3 / Vulkan | std::filesystem | Phase 1 后可支持 |
| macOS | GLFW | OpenGL 4.1 (deprecated) | std::filesystem | Phase 1 后部分支持 |
| Android | ANativeWindow | OpenGL ES 3.1+ / Vulkan | AAssetManager | Phase 4 |
| iOS | UIWindow | Metal (未来) | NSBundle | 远期 |

---

## 三、分层架构设计

```
┌──────────────────────────────────────────────────────┐
│                    apps / examples                    │
├──────────────────────────────────────────────────────┤
│                  engine/runtime                       │
│   EngineInstance (Init/Tick/Shutdown/Run)             │
│   FramePipeline, RuntimeContext, RuntimeServices      │
├──────────────┬───────────────────────────────────────┤
│  engine/     │  engine/render/                        │
│  platform/   │  RhiDevice → GL / Vulkan / DX11       │
│              │  ShaderManager, DrawExecutor, etc.     │
│  PlatformApp │  (不依赖 platform/)                    │
│  GlfwApp     │                                        │
│  AndroidApp  │                                        │
├──────────────┴───────────────────────────────────────┤
│              engine/core, base, ecs, assets, etc.     │
├──────────────────────────────────────────────────────┤
│                     depends/                          │
│   glfw, glad, glm, entt, stb, lua, fmod, etc.        │
└──────────────────────────────────────────────────────┘
```

**关键约束**：`engine/render/` 不依赖 `engine/platform/`，保持 RHI 层独立。

---

## 四、Phase 1 — PlatformApp 接口 + GlfwApp 实现

### 4.1 新增文件

#### `engine/platform/platform_app.h` — 纯虚接口

```cpp
#ifndef DSE_PLATFORM_APP_H
#define DSE_PLATFORM_APP_H

#include <memory>
#include <string>
#include <cstdint>

namespace dse::platform {

struct WindowConfig {
    int width = 800;
    int height = 600;
    std::string title = "DSEngine";
    bool no_graphics_api = false;  // D3D11/Vulkan：不创建 GL context
    bool gl_fallback_33 = true;    // GL 4.3 失败时降级到 3.3
};

class PlatformApp {
public:
    virtual ~PlatformApp() = default;

    // --- 生命周期 ---
    virtual bool Init(const WindowConfig& config) = 0;
    virtual void Shutdown() = 0;

    // --- 主循环驱动 ---
    virtual bool ShouldClose() const = 0;
    virtual void PollEvents() = 0;
    virtual void SwapBuffers() = 0;        // GL swap；非 GL no-op
    virtual double GetTime() const = 0;

    // --- 窗口信息 ---
    virtual void GetFramebufferSize(int& w, int& h) const = 0;
    virtual void SetWindowTitle(const std::string& title) = 0;
    virtual void RequestClose() = 0;

    // --- 平台桥接 ---
    virtual void* GetNativeWindowHandle() const = 0;  // HWND / X11 Window / ANativeWindow*
    virtual bool HasGLContext() const = 0;
    virtual bool LoadGLFunctions() = 0;

    // --- Vulkan Surface（避免 vulkan.h 头文件依赖，用 uint64_t 传递） ---
    virtual uint64_t CreateVulkanSurface(void* vk_instance) = 0;

    // --- 输入回调 ---
    using KeyCallback = void(*)(int key, int action);
    using MouseButtonCallback = void(*)(int button, int action);
    using ScrollCallback = void(*)(float yoffset);
    using CursorPosCallback = void(*)(float x, float y);
    virtual void SetInputCallbacks(KeyCallback, MouseButtonCallback,
                                   ScrollCallback, CursorPosCallback) = 0;

    // --- 编辑器外部窗口注入 ---
    virtual bool AttachExternal(void* existing_window) = 0;
};

/// 创建当前平台的默认 PlatformApp 实现
std::unique_ptr<PlatformApp> CreateDefaultPlatformApp();

} // namespace dse::platform

#endif // DSE_PLATFORM_APP_H
```

#### `engine/platform/glfw/glfw_app.h` + `glfw_app.cpp` — GLFW 实现

- 封装全部 69 处 GLFW 调用
- `Init()`: `glfwInit → glfwWindowHint → glfwCreateWindow → (可选)glfwMakeContextCurrent`
- GL 4.3 失败自动降级 3.3（搬移已有逻辑）
- `LoadGLFunctions()`: `gladLoadGL(glfwGetProcAddress)`
- `CreateVulkanSurface()`: `glfwCreateWindowSurface()`（替代 vulkan_context.cpp 的平台 ifdef）
- `AttachExternal()`: 接受已存在的 `GLFWwindow*`（编辑器模式）
- `SwapBuffers()`: 内部检查 `glfwGetCurrentContext() != nullptr`
- `GetNativeWindowHandle()`: `#ifdef _WIN32 glfwGetWin32Window()`
- `GetTime()`: `glfwGetTime()`

### 4.2 改造文件

#### `engine/runtime/engine_app.h`

```diff
- void* glfw_window_ = nullptr;
+ std::unique_ptr<dse::platform::PlatformApp> platform_;
```

- 前向声明 `namespace dse::platform { class PlatformApp; }`
- 删除 GLFW 相关注释

#### `engine/runtime/engine_app.cpp`

- 删除 `#include <GLFW/glfw3.h>` 和 `#include <glad/gl.h>`
- 删除 `#include <GLFW/glfw3native.h>`
- 新增 `#include "engine/platform/platform_app.h"`
- 所有 `glfwXxx()` 调用替换为 `platform_->Xxx()`
- `Run()` 主循环改为：
  ```cpp
  while (!platform_->ShouldClose()) {
      const double frame_start = platform_->GetTime();
      platform_->PollEvents();
      int w, h;
      platform_->GetFramebufferSize(w, h);
      Screen::set_width_height(w, h);
      Tick();
      platform_->SwapBuffers();
      // ... 帧率限制 ...
  }
  ```
- 输入回调改为：
  ```cpp
  platform_->SetInputCallbacks(
      [](int key, int action) { Input::RecordKey(key, action); },
      [](int btn, int action) { Input::RecordKey(btn, action); },
      [](float y) { Input::RecordMouseScroll(y); },
      [](float x, float y) { Input::RecordMousePosition(x, y); }
  );
  ```
- `timeBeginPeriod/timeEndPeriod` 保留 2 行 `#ifdef _WIN32`（不值得抽象）

#### `engine/render/rhi/vulkan/vulkan_context.h/cpp`

- `CreateSurface(void* window_handle)` 改为接收已创建的 `VkSurfaceKHR`：
  ```cpp
  // 旧：bool CreateSurface(void* window_handle);
  // 新：
  void SetSurface(VkSurfaceKHR surface) { surface_ = surface; }
  ```
- 删除 `vulkan_context.cpp` 中的 `#ifdef _WIN32 / vkCreateWin32SurfaceKHR` 平台代码
- `Init()` 参数调整：不再接收 `window_handle`，改为在 Init 之前由外部设置 surface

#### `CMakeLists.txt`

- 将 `engine/platform/platform_app.h`、`engine/platform/glfw/glfw_app.h/cpp` 加入 `dse_engine` 目标
- GLFW 链接从 `engine_app.cpp` 级别移到 `glfw_app.cpp` 级别（实际不变，只是逻辑归属更清晰）

### 4.3 验证标准

1. `dse_engine` RelWithDebInfo 编译零错误零链接错误
2. `dse_gtest_unit_tests` Debug 1483 测试全通过
3. `engine/render/` 下无 GLFW/glfw 引用（保持现状）
4. `engine/runtime/engine_app.cpp` 中无 GLFW 直接引用（目标）
5. `engine/platform/glfw/` 是唯一引用 GLFW 的位置
6. 运行 KF_Framework demo 或任意 3D demo 渲染结果无回归

---

## 五、Phase 2 — 资产虚拟文件系统

### 目标
抽象文件 I/O，为 Android APK 资源读取铺路。

### 新增文件

```
engine/assets/
├── file_system.h                ← 虚拟文件系统接口
│   ├── ReadFile(path) → vector<uint8_t>
│   ├── ReadTextFile(path) → string
│   ├── Exists(path) → bool
│   ├── ListDirectory(path) → vector<string>
│   └── GetBasePath() → string
├── native_file_system.h/cpp     ← std::filesystem 实现（桌面）
└── (未来) android_asset_fs.h/cpp ← AAssetManager 实现
```

### 改造
- `AssetManager` 内部持有 `FileSystem*`，所有 `fopen/ifstream/std::filesystem` 调用改为委托
- `ServiceLocator` 注册 `FileSystem` 服务
- Lua `io.open` 不受影响（Lua 脚本在桌面仍用原生 I/O）

---

## 六、Phase 3 — Shader ESSL 310 输出 + GL/GLES 统一

### 目标
shader_compiler 新增 OpenGL ES 3.1 输出，OpenGL 后端支持 GLES 模式。

### 改造

#### shader_compiler
```cpp
// 新增函数
static std::string CrossCompileToESSL310(const std::vector<uint32_t>& spirv, EShLanguage stage) {
    spirv_cross::CompilerGLSL compiler(spirv);
    spirv_cross::CompilerGLSL::Options options;
    options.version = 310;
    options.es = true;  // ← 关键区别
    options.vulkan_semantics = false;
    compiler.set_common_options(options);
    return compiler.compile();
}
```

#### *.gen.h
每个 gen.h 新增 `k*_essl310` 字符串常量。

#### GL Loader 抽象
```
engine/render/rhi/opengl/
├── gl_loader.h
│   #ifdef __ANDROID__
│       #include <GLES3/gl31.h>
│       #include <GLES3/gl3ext.h>
│   #else
│       #include <glad/gl.h>
│   #endif
```

#### OpenGL 后端
- `gl_rhi_device.cpp` 中 `#include <glad/gl.h>` 改为 `#include "gl_loader.h"`
- Shader 选择：`#ifdef __ANDROID__` 用 `essl310`，否则用 `glsl430`
- SSBO 在 GLES 3.1+ 可用，API 相同

---

## 七、Phase 4 — Android NDK 构建 + AndroidApp 实现

### 新增文件

```
engine/platform/android/
├── android_app.h/cpp            ← ANativeActivity 集成
│   ├── Init(): 接收 ANativeWindow*
│   ├── PollEvents(): ALooper_pollAll
│   ├── CreateVulkanSurface(): VkAndroidSurfaceCreateInfoKHR
│   ├── GetNativeWindowHandle(): ANativeWindow*
│   └── LoadGLFunctions(): EGL + GLES（或 Vulkan only）
└── android_asset_fs.h/cpp       ← AAssetManager 文件系统实现
```

### CMake 配置
```cmake
if(ANDROID)
    # 不链接 GLFW，不链接 glad
    # 链接 EGL, GLESv3, android, log
    target_sources(dse_engine PRIVATE
        engine/platform/android/android_app.cpp
        engine/platform/android/android_asset_fs.cpp
    )
else()
    target_sources(dse_engine PRIVATE
        engine/platform/glfw/glfw_app.cpp
    )
endif()
```

### 触摸输入桥接
- Android touch event → `Input::RecordMousePosition(x, y)` + `Input::RecordKey(MOUSE_BUTTON_LEFT, action)`
- 多点触控映射为 gamepad axes（可选）

---

## 八、实施优先级与时间估算

| Phase | 内容 | 预估工作量 | 前置条件 |
|-------|------|-----------|---------|
| **Phase 1** | PlatformApp + GlfwApp + EngineInstance 解耦 | ~300 行新增 + ~200 行改写 | 无 | ✅ **已完成** |
| **Phase 2** | 虚拟文件系统 | ~200 行新增 + ~150 行改写 | Phase 1 | ✅ **已完成** |
| **Phase 3** | ESSL 310 + GL Loader 抽象 | ~100 行新增 + ~50 行改写 | Phase 1 | ✅ **已完成** |
| **Phase 4** | Android NDK 构建 + AndroidApp | ~400 行新增 | Phase 1+2+3 | ✅ **已完成（核心准备）** |

**Phase 1 已完成**，桌面端 GLFW 依赖已全部封装进 `engine/platform/glfw/`。  
**Phase 2 已完成**，`FileSystem` 接口 + `NativeFileSystem` 桌面实现已建立，`AssetManager::LoadFileToMemory` 通过 `FileSystem*` 委托读取，`ServiceLocator` 注册 `FileSystem` 服务。  
**Phase 3 已完成**，`shader_compiler` 新增 ESSL 310 交叉编译，所有 44 个 gen.h 已包含 `k*_essl310` 常量；新建 `engine/render/rhi/opengl/gl_loader.h` 统一 GL/GLES include；6 个 OpenGL 后端源文件替换为 `gl_loader.h`。  
**Phase 4 已完成**，Android 代码层完整交付：

基础设施（上一阶段）：
- `VulkanContext::CreateSurface` 补 `VK_KHR_ANDROID_SURFACE_EXTENSION_NAME` + `VkAndroidSurfaceCreateInfoKHR` 分支
- `gl_rhi_device.cpp` 补 `#ifdef __ANDROID__` 直接绑定 GLES 3.1 原生函数，跳过 `wglGetProcAddress`
- `gl_loader.h` 补 `GLAD_API_PTR` 空定义，保证 GLES 下函数指针类型兼容
- `CMakeLists.txt` 新增 Android 块：自动禁用 PhysX/D3D11/GTest/Editor，跳过 GLFW/glad/imgui_impl_glfw 编译，链接 `EGL GLESv3 android log`

平台实现（本阶段）：
- `engine/platform/android/android_app.h/.cpp`：`AndroidApp` 实现 `PlatformApp` 全部接口
  - EGL 初始化（GLES 3.0 context + ANativeWindow surface）
  - `AInputQueue` touch/key 事件泵送 → 统一回调接口
  - Vulkan Surface：`VkAndroidSurfaceCreateInfoKHR`（`DSE_ENABLE_VULKAN` 条件编译）
  - `CreateDefaultPlatformApp()` 返回 `AndroidApp`，与 `GlfwApp` 工厂函数互斥
- `engine/assets/android_asset_fs.h/.cpp`：`AndroidAssetFileSystem` 实现 `FileSystem` 全部接口
  - `AAssetManager` 读取 APK `assets/` 目录，`AASSET_MODE_BUFFER` 零拷贝读取
- `CMakeLists.txt`：非 Android 构建过滤 `android/*.cpp` + `android_asset_fs.cpp`
- 桌面端编译验证：290 目标文件，零错误

剩余工作（需 Android Studio 环境）：
- `apps/android_host/`：Android Studio 项目（`build.gradle` + JNI 桥接 + CMakePresets）

---

## 九、技术债检查清单

| 项目 | Phase 1 后状态 | 最终状态 |
|------|---------------|---------|
| GLFW 耦合 | ✅ 解决 | ✅ |
| Vulkan Surface 平台代码 | ✅ 解决 | ✅ |
| 编辑器模式窗口所有权 | ✅ AttachExternal | ✅ |
| SwapBuffers 后端条件 | ✅ 封入 PlatformApp | ✅ |
| GLES 兼容 | ❌ 未做 | ✅ Phase 3 |
| 资产 I/O 抽象 | ❌ 未做 | ✅ Phase 2 |
| Android 支持 | ❌ 未做 | ✅ Phase 4 |
| FMOD 音频 | ⚡ 条件编译已隔离 | ⚡ 保持 |
| PhysX 物理 | ⚡ DSE_ENABLE_PHYSX 已隔离 | ⚡ 保持 |
| `timeBeginPeriod` | ⚡ 保留 2 行 ifdef | ⚡ 保持 |

---

## 十、相关文件索引

### 需改造的文件（Phase 1）
- `engine/runtime/engine_app.h` — EngineInstance 类定义
- `engine/runtime/engine_app.cpp` — 69 处 GLFW 调用
- `engine/render/rhi/vulkan/vulkan_context.h` — CreateSurface 声明
- `engine/render/rhi/vulkan/vulkan_context.cpp` — CreateSurface Win32 实现
- `CMakeLists.txt` — 构建配置

### 参考文件（不改但需理解）
- `engine/runtime/runtime_context.h` — RuntimeContext（含 native_window_handle）
- `engine/render/rhi/rhi_device.h` — InitDevice(void*) 接口
- `engine/render/rhi/rhi_factory.h` — CreateRhiDevice / ResolveRhiBackendFromEnv
- `engine/render/rhi/dx11/dx11_context.h/cpp` — DX11 Init(void* hwnd)
- `engine/render/rhi/vulkan/vulkan_rhi_device.cpp` — InitDevice 调用链
- `engine/platform/screen.h` — 已有平台层
- `engine/input/input.h` — 输入回调目标
- `engine/core/dynamic_library.cpp` — 已有跨平台范例
