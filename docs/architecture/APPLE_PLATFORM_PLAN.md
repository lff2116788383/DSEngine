# Apple 平台后端实现方案（macOS / iOS）

> 状态：实验性（`DSE_ENABLE_APPLE_PLATFORM` 编译开关隔离）
> 最后更新：2026-07-01

---

## 1. 现状分析

### 1.1 已有平台后端

| 平台 | 实现文件 | 窗口系统 | RHI |
|------|---------|---------|-----|
| Windows/Linux | `engine/platform/glfw/glfw_app.cpp` (386行) | GLFW 3 | OpenGL / Vulkan / DX11 |
| Android | `engine/platform/android/android_app.cpp` (333行) | ANativeActivity + EGL | OpenGL ES 3 / Vulkan |
| Web | `engine/platform/web/web_app.cpp` (255行) | Emscripten + GLFW port | WebGPU / WebGL2 |

### 1.2 已有 RHI 后端

| 后端 | 代码量 | Compute | SSBO | Indirect Draw | GPU Timer |
|------|--------|---------|------|--------------|-----------|
| OpenGL 4.3 | ~5600行 | Yes | Yes | Yes | Yes |
| Vulkan | ~10300行 | Yes | Yes | Yes | Yes |
| DX11 | ~5960行 | Yes | Yes | Yes | Yes |
| WebGPU | ~10185行 | Yes | Yes | Yes | Yes |

### 1.3 Apple 平台现状

- `__APPLE__` 在整个引擎中仅出现 **1 处**：`dynamic_library.cpp` 的 `.dylib` 加载
- `vulkan_context.cpp` 的 `CreateSurface()` 仅有 Win32 和 Android 分支，`#else` 直接返回 `false`
- `GetRequiredExtensions()` 仅添加 Win32/Android surface 扩展
- 无任何 Objective-C/Objective-C++ 文件
- 无 Metal、UIKit、AppKit 代码

---

## 2. 核心策略：Vulkan + MoltenVK

### 2.1 为什么选择 MoltenVK 而非原生 Metal RHI

| 维度 | MoltenVK 方案 | Metal RHI 方案 |
|------|-------------|---------------|
| 代码量 | ~2100行（平台层 + Vulkan 适配） | ~8000行（完整第 5 个 RHI 后端） |
| 工期 | 4-5 周 | 8-12 周 |
| 维护成本 | 低（复用 Vulkan RHI 10300 行） | 高（新增后端，独立维护） |
| 性能 | 翻译层开销 5-15% | 原生性能 |
| 功能覆盖 | Vulkan 1.1 子集（compute/SSBO/indirect draw 完整） | Metal 3 全部特性 |
| 行业验证 | Dota 2 / Baldur's Gate 3 / No Man's Sky | UE5 / Unity |

**结论**：当前阶段选择 MoltenVK，性价比最高。

### 2.2 MoltenVK 关键能力映射

| DSE 使用的 Vulkan 特性 | MoltenVK 支持 | 备注 |
|----------------------|-------------|------|
| Compute Shader | 完整 | VG pipeline 正常 |
| SSBO (Storage Buffer) | 完整 | GPU-driven rendering 正常 |
| Indirect Draw | 完整 | Meshlet/VG indirect draw 正常 |
| Push Constants | 完整 | CommandBuffer::PushConstants 正常 |
| Multiple Render Targets | 完整 | GBuffer MRT 正常 |
| Hi-Z 遮挡剔除 | 完整 | R32F mip chain + compute 正常 |
| Swapchain | 完整 | 自动映射到 CAMetalLayer |
| Pipeline Cache | 完整 | 自动映射到 Metal Binary Archives |
| GPU Timer (Timestamp Query) | 完整 | 映射到 MTLCounterSampleBuffer |
| 64-bit Atomics | 部分设备 | SW rasterizer 已有 32-bit fallback |
| Geometry Shader | 不支持 | DSE 未使用，无影响 |
| Multi-draw Indirect Count | iOS 不支持 | 已有 multi-draw indirect fallback |

### 2.3 已有基础设施复用

| 组件 | 文件 | 复用方式 |
|------|------|---------|
| SPIRV-Cross | `depends/spirv-cross/` 含 `spirv_msl.hpp/cpp` | MoltenVK 运行时自动 SPIR-V→MSL；离线预编译可选 |
| Vulkan RHI | `engine/render/rhi/vulkan/` 10300行 | 零改动复用 |
| GLSL Shader | `engine/render/shaders/src/` 全部 | GLSL→SPIR-V→(MoltenVK)→MSL 自动链路 |
| glslang | `depends/glslang/` | GLSL→SPIR-V 编译器，已集成 |
| 着色器编译器工具 | `dse_shader_compiler` | 已支持 GLSL→SPIR-V→GLSL330→HLSL 交叉编译 |

---

## 3. 平滑升级路径：MoltenVK → Metal RHI

### 3.1 为什么可以平滑升级

DSE 的 RHI 抽象层设计天然支持多后端并存：

```
RhiDevice (纯虚基类, 520行)
├── IRhiCompute        (compute shader 扩展, 136行)
├── IRhiStorageBuffer  (SSBO 扩展, 69行)
├── IRhiGpuDriven      (indirect draw / mega buffer / Hi-Z, 183行)
└── IRhiGpuTimer       (GPU 时间戳, 80行)

已有实现：
├── GlRhiDevice        (OpenGL)
├── VulkanRhiDevice    (Vulkan)  ← MoltenVK 阶段直接用这个
├── Dx11RhiDevice      (DX11)
└── WebGpuRhiDevice    (WebGPU)

将来可选：
└── MetalRhiDevice     (Metal)  ← 升级时新增
```

**关键点**：`RhiDevice` 的 ~39 个纯虚函数 + 4 个扩展接口定义了完整的后端契约。
上层渲染代码（`frame_pipeline.cpp`、`builtin_passes.cpp`、`mesh_renderer.cpp` 等）
全部通过 `RhiDevice*` 指针调用，**不引用任何具体后端类型**。

### 3.2 升级步骤（当需要时）

```
阶段 A（当前）: Vulkan RHI + MoltenVK → Metal
    上层代码 → RhiDevice* → VulkanRhiDevice → Vulkan API → MoltenVK → Metal

阶段 B（升级）: 新增 MetalRhiDevice，与 VulkanRhiDevice 并存
    上层代码 → RhiDevice* → MetalRhiDevice → Metal API（原生）
```

升级时需要做的：

| 步骤 | 改动 | 影响范围 |
|------|------|---------|
| 1 | 新建 `engine/render/rhi/metal/` 目录 | 新文件，不影响现有代码 |
| 2 | 实现 `MetalRhiDevice : public RhiDevice` | 实现 ~39 个纯虚函数 |
| 3 | `rhi_types.h` 的 `RhiBackend` 枚举加 `Metal = 4` | 1行 |
| 4 | `rhi_factory.h/cpp` 加 Metal 分支 | ~10行 |
| 5 | `CMakeLists.txt` 加 `DSE_ENABLE_METAL` option | ~20行 |
| 6 | `spirv_cross_embedded.cpp` 加 `#include <spirv_msl.cpp>` 用于离线转译 | 1行 |

**上层零改动** — `frame_pipeline.cpp`、`builtin_passes.cpp`、所有 render pass、
VG pipeline、编辑器 — 全部通过 `RhiDevice*` 接口调用，不感知后端切换。

### 3.3 升级触发条件

以下场景出现时，才值得投入 Metal RHI：

| 场景 | 原因 |
|------|------|
| 需要 Metal 3 Mesh Shader | MoltenVK 不支持 `VK_EXT_mesh_shader` |
| 需要 Hardware Ray Tracing | MoltenVK 不支持 `VK_KHR_ray_tracing_pipeline` |
| 需要 MetalFX Upscaling | Apple 专有技术，无 Vulkan 等价 |
| MoltenVK 性能瓶颈实测 > 20% | 翻译层开销超过可接受阈值 |
| Apple App Store 政策变化 | 目前允许 MoltenVK，但未来不确定 |

**当前均不触发**。MoltenVK 方案不会产生技术债，因为升级路径完全是"新增"而非"重写"。

### 3.4 平台层复用

MoltenVK 阶段创建的平台代码在升级到 Metal RHI 后**完全复用**：

| 组件 | MoltenVK 阶段 | Metal RHI 阶段 | 是否复用 |
|------|-------------|---------------|---------|
| `ios_app.mm` (UIKit 生命周期) | 提供 CAMetalLayer 给 MoltenVK | 提供 CAMetalLayer 给 Metal | 完全复用 |
| `ios_input.mm` (触屏输入) | 转发到 PlatformApp 回调 | 同上 | 完全复用 |
| `ios_file_system.mm` (文件系统) | NSBundle + Documents | 同上 | 完全复用 |
| `ios_audio_session.mm` (音频会话) | AVAudioSession 配置 | 同上 | 完全复用 |
| `cmake/ios.toolchain.cmake` | iOS 交叉编译 | 同上 | 完全复用 |
| `glfw_app.cpp` macOS 适配 | GLFW + MoltenVK surface | GLFW + Metal surface | 完全复用 |
| `vulkan_context.cpp` Apple 分支 | 创建 MoltenVK surface | 不再使用（走 Metal） | 仅 RHI 层切换 |

**升级路径零废弃代码**：平台层 ~1100 行全部保留，仅 Vulkan RHI 层内的 Apple surface 代码
（~80 行）在切换到 Metal 后不再使用（但仍可作为 fallback 保留）。

---

## 4. 编译隔离方案

### 4.1 CMake 开关

```cmake
# CMakeLists.txt
option(DSE_ENABLE_APPLE_PLATFORM
    "Enable Apple platform support (macOS/iOS via MoltenVK, experimental)" OFF)
```

### 4.2 隔离规则

| 层级 | 隔离方式 | 示例 |
|------|---------|------|
| CMake | `if(DSE_ENABLE_APPLE_PLATFORM)` | 链接 framework、包含 .mm 文件 |
| C++ 源码 | `#ifdef DSE_ENABLE_APPLE_PLATFORM` | Vulkan surface Apple 分支 |
| Objective-C++ | 文件级隔离 | `ios_app.mm` 仅在 `IOS AND DSE_ENABLE_APPLE_PLATFORM` 时编译 |
| 运行时 | 平台层自动选择 | `CreateDefaultPlatformApp()` 按 `__APPLE__` 编译期决议 |

### 4.3 开关关闭时的保证

- 零编译：所有 `.mm` 文件被 CMake 过滤，`#ifdef` 内代码不编译
- 零链接：不链接 Cocoa / Metal / QuartzCore / IOKit 等 Apple framework
- 零运行时开销：无任何 Apple 相关代码路径被执行
- 现有测试不受影响：2783 个测试保持全部通过

---

## 5. 实现方案

### 5.1 第 1 层：macOS 支持（GLFW + MoltenVK）

macOS 上 GLFW 已原生支持，改动最小。

#### 5.1.1 VulkanContext 加 Apple 支持

**文件**：`engine/render/rhi/vulkan/vulkan_context.cpp`

```cpp
// 头文件区域新增（约第 27 行后）
#ifdef DSE_ENABLE_APPLE_PLATFORM
  #if defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IOS
      #define VK_USE_PLATFORM_IOS_MVK
      #include <vulkan/vulkan_ios.h>
    #else
      #define VK_USE_PLATFORM_MACOS_MVK
      #include <vulkan/vulkan_macos.h>
    #endif
  #endif
#endif

// GetRequiredExtensions() 新增 Apple 分支
#ifdef DSE_ENABLE_APPLE_PLATFORM
  #if defined(__APPLE__)
    #if TARGET_OS_IOS
      extensions.push_back("VK_MVK_ios_surface");
    #else
      extensions.push_back("VK_MVK_macos_surface");
    #endif
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  #endif
#endif

// CreateInstance() 新增 portability 标志
#ifdef DSE_ENABLE_APPLE_PLATFORM
  #if defined(__APPLE__)
    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  #endif
#endif

// CreateSurface() 新增 Apple 分支（iOS 专用；macOS 由 GLFW 处理）
#ifdef DSE_ENABLE_APPLE_PLATFORM
  #if defined(__APPLE__) && TARGET_OS_IOS
    VkIOSSurfaceCreateInfoMVK create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
    create_info.pView = window_handle; // CAMetalLayer*
    VkResult result = vkCreateIOSSurfaceMVK(instance_, &create_info,
                                            nullptr, &surface_);
    if (result != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create iOS surface: {}",
                        static_cast<int>(result));
        return false;
    }
  #endif
#endif

// kDeviceExtensions 新增 portability subset
#ifdef DSE_ENABLE_APPLE_PLATFORM
  #if defined(__APPLE__)
    "VK_KHR_portability_subset",
  #endif
#endif
```

**改动量**：~80 行

#### 5.1.2 GlfwApp 加 macOS 原生窗口句柄

**文件**：`engine/platform/glfw/glfw_app.cpp`

```cpp
// 头文件区域新增
#ifdef DSE_ENABLE_APPLE_PLATFORM
  #if defined(__APPLE__) && !TARGET_OS_IOS
    #define GLFW_EXPOSE_NATIVE_COCOA
    #include <GLFW/glfw3native.h>
  #endif
#endif

// GetNativeWindowHandle() 加 macOS 分支
#ifdef DSE_ENABLE_APPLE_PLATFORM
  #if defined(__APPLE__) && !TARGET_OS_IOS
    return window_ ? (void*)glfwGetCocoaWindow(window_) : nullptr;
  #endif
#endif

// CreateVulkanSurface() — GLFW 的 glfwCreateWindowSurface() 已原生支持
// MoltenVK，无需额外代码。现有实现直接可用。
```

**改动量**：~20 行

**关键设计决策**：macOS 上 `CreateVulkanSurface()` 不手写 `vkCreateMacOSSurfaceMVK`，
而是复用 GLFW 的 `glfwCreateWindowSurface()`——它在检测到 MoltenVK 时自动创建正确的
macOS Vulkan surface，消除平台判断冗余。

#### 5.1.3 CMakeLists.txt Apple 链接

```cmake
# macOS framework 链接
if(DSE_ENABLE_APPLE_PLATFORM AND APPLE AND NOT IOS)
    find_library(COCOA_LIB Cocoa)
    find_library(IOKIT_LIB IOKit)
    find_library(QUARTZCORE_LIB QuartzCore)
    target_link_libraries(dse_engine PRIVATE
        ${COCOA_LIB} ${IOKIT_LIB} ${QUARTZCORE_LIB})

    # MoltenVK: 引擎运行时通过 Vulkan Loader (libvulkan) 加载 MoltenVK ICD
    # 用户需安装 Vulkan SDK for macOS 或将 libMoltenVK.dylib 放入应用 bundle
    target_compile_definitions(dse_engine PRIVATE DSE_ENABLE_APPLE_PLATFORM)
endif()
```

**改动量**：~20 行

#### 5.1.4 macOS 总改动量

| 文件 | 类型 | 行数 |
|------|------|------|
| `vulkan_context.cpp` | 修改 | +80 |
| `glfw_app.cpp` | 修改 | +20 |
| `CMakeLists.txt` | 修改 | +20 |
| **合计** | | **~120 行** |

---

### 5.2 第 2 层：iOS 支持（UIKit + MoltenVK）

iOS 不能使用 GLFW，需新增 `UIKitApp`（`PlatformApp` 子类）。

#### 5.2.1 目录结构

```
engine/platform/ios/
├── ios_app.h              UIKitApp 类声明
├── ios_app.mm             UIApplicationDelegate + MTKView + MoltenVK surface
├── ios_input.mm           触屏事件 → PlatformApp::TouchCallback
├── ios_file_system.h      iOS 文件路径工具
├── ios_file_system.mm     NSBundle 资源路径 + Documents 可写路径
└── ios_audio_session.mm   AVAudioSession 配置（miniaudio CoreAudio 前置）
```

#### 5.2.2 UIKitApp 类设计

**文件**：`engine/platform/ios/ios_app.h`

```objc
// UIKitApp : PlatformApp — iOS 平台实现
// 持有 UIWindow + DSEViewController（含 MTKView）
// MTKView.layer 为 CAMetalLayer，供 MoltenVK 创建 VkSurfaceKHR

class UIKitApp final : public PlatformApp {
public:
    ~UIKitApp() override;

    bool Init(const WindowConfig& config) override;
    void Shutdown() override;

    bool ShouldClose() const override;
    void PollEvents() override;
    void SwapBuffers() override;         // MoltenVK swapchain 处理，no-op
    double GetTime() const override;     // CACurrentMediaTime()

    void GetFramebufferSize(int& w, int& h) const override;
    void SetWindowTitle(const std::string& title) override; // no-op
    void RequestClose() override;

    void* GetNativeWindowHandle() const override; // CAMetalLayer*
    bool HasGLContext() const override;            // false（Vulkan 模式）
    bool LoadGLFunctions() override;               // false
    uint64_t CreateVulkanSurface(void* vk_instance) override;

    void SetInputCallbacks(KeyCallback, MouseButtonCallback,
                           ScrollCallback, CursorPosCallback) override;
    void SetTouchCallback(TouchCallback cb) override;
    bool AttachExternal(void* existing_window) override;

private:
    void* ui_window_ = nullptr;      // UIWindow* (ARC bridge)
    void* view_controller_ = nullptr; // DSEViewController*
    void* metal_view_ = nullptr;      // MTKView*
    TouchCallback touch_cb_ = nullptr;
    bool should_close_ = false;
};
```

**行数**：~80 行

#### 5.2.3 UIKitApp 实现

**文件**：`engine/platform/ios/ios_app.mm`

核心逻辑：

```objc
// DSEViewController — 持有 MTKView，转发触屏事件
@interface DSEViewController : UIViewController
@property (nonatomic, strong) MTKView* metalView;
@end

@implementation DSEViewController
- (void)viewDidLoad {
    [super viewDidLoad];
    self.metalView = [[MTKView alloc] initWithFrame:self.view.bounds];
    // MoltenVK 要求 CAMetalLayer，MTKView 默认 layer 即 CAMetalLayer
    self.metalView.autoresizingMask = UIViewAutoresizingFlexibleWidth |
                                     UIViewAutoresizingFlexibleHeight;
    self.metalView.paused = YES;         // 引擎自驱帧循环
    self.metalView.enableSetNeedsDisplay = NO;
    [self.view addSubview:self.metalView];
}
@end

// UIKitApp::Init() — 创建 UIWindow + DSEViewController
bool UIKitApp::Init(const WindowConfig& config) {
    UIWindow* window = [[UIWindow alloc] initWithFrame:
        [[UIScreen mainScreen] bounds]];
    DSEViewController* vc = [[DSEViewController alloc] init];
    window.rootViewController = vc;
    [window makeKeyAndVisible];

    ui_window_ = (__bridge_retained void*)window;
    view_controller_ = (__bridge void*)vc;
    metal_view_ = (__bridge void*)vc.metalView;
    return true;
}

// UIKitApp::CreateVulkanSurface() — MoltenVK iOS surface
uint64_t UIKitApp::CreateVulkanSurface(void* vk_instance) {
    VkIOSSurfaceCreateInfoMVK info{};
    info.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
    info.pView = (__bridge void*)((__bridge MTKView*)metal_view_).layer;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    vkCreateIOSSurfaceMVK((VkInstance)vk_instance, &info, nullptr, &surface);
    return (uint64_t)surface;
}

// UIKitApp::GetNativeWindowHandle() — 返回 CAMetalLayer*
void* UIKitApp::GetNativeWindowHandle() const {
    MTKView* view = (__bridge MTKView*)metal_view_;
    return (__bridge void*)view.layer;
}

// GetTime() — 高精度时间
double UIKitApp::GetTime() const {
    return CACurrentMediaTime();
}
```

**行数**：~500 行

#### 5.2.4 触屏输入

**文件**：`engine/platform/ios/ios_input.mm`

```objc
// DSETouchView — UIView 子类，接收触屏事件
@interface DSETouchView : UIView
@property (nonatomic, assign) dse::platform::PlatformApp::TouchCallback callback;
@end

@implementation DSETouchView
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    for (UITouch* t in touches) {
        CGPoint p = [t locationInView:self];
        NSUInteger fid = (NSUInteger)(__bridge void*)t;
        if (self.callback)
            self.callback((int)fid, (float)p.x, (float)p.y, 1); // Began
    }
}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    // phase = 2 (Moved)
}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    // phase = 4 (Ended)
}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    // phase = 5 (Cancelled)
}
@end
```

TouchPhase 映射与 `PlatformApp::TouchCallback` 对齐（与 Android 后端一致）：
```
1 = Began,  2 = Moved,  3 = Stationary,  4 = Ended,  5 = Cancelled
```

**行数**：~150 行

#### 5.2.5 文件系统

**文件**：`engine/platform/ios/ios_file_system.h/mm`

```objc
namespace dse::platform::ios {
    // 只读资源路径（app bundle 内）
    std::string GetBundleResourcePath();    // NSBundle.mainBundle.resourcePath

    // 可写路径（Documents 目录）
    std::string GetDocumentsPath();         // NSSearchPathForDirectoriesInDomains

    // 可写路径（Caches 目录，系统可能清理）
    std::string GetCachesPath();
}
```

引擎的 `AssetManager` 在 iOS 上将：
- 只读资产从 bundle resource path 加载
- 用户存档/配置写入 Documents path
- 流式缓存写入 Caches path

**行数**：~100 行

#### 5.2.6 音频会话

**文件**：`engine/platform/ios/ios_audio_session.mm`

```objc
// iOS 要求在使用音频前配置 AVAudioSession
// miniaudio 的 ma_context_init 会自动使用 CoreAudio 后端，
// 但需要提前设置 session category

void dse::platform::ios::ConfigureAudioSession() {
    AVAudioSession* session = [AVAudioSession sharedInstance];
    NSError* error = nil;
    [session setCategory:AVAudioSessionCategoryAmbient error:&error];
    [session setActive:YES error:&error];
}
```

**行数**：~50 行

#### 5.2.7 CMake iOS 构建

**文件**：`cmake/ios.toolchain.cmake`

```cmake
set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_OSX_ARCHITECTURES arm64)
set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0)

# Xcode 自动签名（开发阶段）
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer")
set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "" CACHE STRING "Apple Developer Team ID")
```

**CMakeLists.txt 追加**：

```cmake
if(DSE_ENABLE_APPLE_PLATFORM AND IOS)
    # iOS 专有：禁用桌面功能
    set(DSE_ENABLE_PHYSX OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_D3D11 OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_EDITOR OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_LAUNCHER OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_GTESTS OFF CACHE BOOL "" FORCE)

    # 排除桌面/Android 平台文件
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/platform/glfw/.*\\.cpp$")
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/platform/android/.*\\.cpp$")

    # 包含 iOS 平台文件
    file(GLOB ios_platform_mm "engine/platform/ios/*.mm")
    list(APPEND engine_cpp ${ios_platform_mm})

    # iOS framework 链接
    find_library(UIKIT_LIB UIKit)
    find_library(METAL_LIB Metal)
    find_library(METALKIT_LIB MetalKit)
    find_library(QUARTZCORE_LIB QuartzCore)
    find_library(AVFOUNDATION_LIB AVFoundation)
    target_link_libraries(dse_engine PRIVATE
        ${UIKIT_LIB} ${METAL_LIB} ${METALKIT_LIB}
        ${QUARTZCORE_LIB} ${AVFOUNDATION_LIB})

    target_compile_definitions(dse_engine PRIVATE DSE_ENABLE_APPLE_PLATFORM)
endif()
```

**改动量**：~50 行

#### 5.2.8 iOS 总改动量

| 文件 | 类型 | 行数 |
|------|------|------|
| `ios_app.h` | 新增 | ~80 |
| `ios_app.mm` | 新增 | ~500 |
| `ios_input.mm` | 新增 | ~150 |
| `ios_file_system.h/mm` | 新增 | ~100 |
| `ios_audio_session.mm` | 新增 | ~50 |
| `cmake/ios.toolchain.cmake` | 新增 | ~30 |
| `CMakeLists.txt` (iOS 部分) | 修改 | +50 |
| **合计** | | **~960 行** |

---

### 5.3 第 3 层：共用适配

#### 5.3.1 Splash Screen

**文件**：`engine/platform/splash_screen.cpp`

```cpp
#ifdef DSE_ENABLE_APPLE_PLATFORM
  #if defined(__APPLE__)
    // macOS/iOS: splash screen 由 LaunchScreen.storyboard 处理
    // Show() 返回 false，引擎跳过自绘 splash
  #endif
#endif
```

**改动量**：~10 行

#### 5.3.2 Dynamic Library

已有 `__APPLE__` 支持（`.dylib` 后缀 + `dlopen`），无需改动。

#### 5.3.3 Shader 兼容性

- MoltenVK 运行时自动将 SPIR-V 转译为 MSL，**无需手动转换**
- 引擎现有 GLSL→SPIR-V 管线（glslang）完全可用
- VG 的 5 个 compute shader 在 SPIR-V 层通过 MoltenVK 直接映射到 Metal compute
- 可选优化：离线预编译 SPIR-V→MSL（减少首次加载转译时间），
  利用已集成的 `spirv_cross_embedded.cpp` 加 `#include <spirv_msl.cpp>`

---

## 6. VG 技术债修复（顺带）

上一轮 VG GPU wiring 中引入的 `void* vg_renderer` 类型擦除需修复。

### 6.1 问题

```cpp
// render_pass_context.h (当前)
void* vg_renderer = nullptr;  // VirtualGeometryRenderer* 类型擦除
```

`void*` 丢失类型信息，编译器无法检查错误转型。

### 6.2 修复

```cpp
// render_pass_context.h (修复后)
#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY
namespace dse::render::vg { class VirtualGeometryRenderer; }
// ...
    dse::render::vg::VirtualGeometryRenderer* vg_renderer = nullptr;
#endif
```

同步修改 `virtual_geometry_pass.cpp` 中的 `GetRenderer()` 方法，
移除 `reinterpret_cast<VirtualGeometryRenderer*>(ctx_.vg_renderer)` 转型。

**改动量**：~10 行

---

## 7. 文件变更总览

| 文件 | 改动类型 | 行数 | 隔离方式 |
|------|---------|------|---------|
| `vulkan_context.cpp` | 修改 | +80 | `#ifdef DSE_ENABLE_APPLE_PLATFORM` |
| `glfw_app.cpp` | 修改 | +20 | `#ifdef DSE_ENABLE_APPLE_PLATFORM` |
| `CMakeLists.txt` | 修改 | +70 | `if(DSE_ENABLE_APPLE_PLATFORM)` |
| `splash_screen.cpp` | 修改 | +10 | `#ifdef DSE_ENABLE_APPLE_PLATFORM` |
| `render_pass_context.h` | 修改 | +5 | `#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY` |
| `virtual_geometry_pass.cpp` | 修改 | +5 | `#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY` |
| `ios_app.h` | 新增 | ~80 | 文件级（仅 iOS 构建编译） |
| `ios_app.mm` | 新增 | ~500 | 文件级 |
| `ios_input.mm` | 新增 | ~150 | 文件级 |
| `ios_file_system.h/mm` | 新增 | ~100 | 文件级 |
| `ios_audio_session.mm` | 新增 | ~50 | 文件级 |
| `cmake/ios.toolchain.cmake` | 新增 | ~30 | 文件级 |
| **合计** | | **~1100 行** | |

---

## 8. 工期估计

| 阶段 | 内容 | 前置依赖 | 工期 |
|------|------|---------|------|
| 1 | macOS (GLFW + MoltenVK) | macOS + Vulkan SDK | 3-5 天 |
| 2 | iOS (UIKitApp + MoltenVK) | Xcode + iOS 模拟器 | 2-3 周 |
| 3 | 验证 + 性能基准 | Apple Developer 账号 | 1 周 |
| **合计** | | | **4-5 周** |

### 8.1 前置依赖

| 依赖 | 用途 | 必须/可选 |
|------|------|----------|
| macOS 开发机 | 编译 Objective-C++、运行 Xcode | 必须 |
| Vulkan SDK for macOS | 包含 MoltenVK + Vulkan Loader | 必须 |
| Xcode 15+ | iOS 构建 + 模拟器 | iOS 必须 |
| Apple Developer Program | 真机部署 | 真机测试必须，模拟器不需要 |

### 8.2 可在 Windows 上预完成的工作

| 工作 | 说明 |
|------|------|
| CMake 条件编译逻辑 | `#ifdef` + `if()` 在 Windows 上可编译验证（OFF 时不影响） |
| iOS 平台层头文件 | `.h` 文件的类声明可在 Windows 上编写 |
| 文档 | 本文档 |
| VG 技术债修复 | `void*` → 强类型指针，Windows 上可立即验证 |

---

## 9. 技术债评估

| 项 | 当前状态 | 本方案处理 |
|---|---------|----------|
| `void* vg_renderer` 类型擦除 | 已存在 | 本次修复为强类型前向声明指针 |
| MoltenVK 翻译层开销 | 预期 5-15% | 接受；升级路径已规划（第 3 节） |
| Metal 3 特性不可达 | 当前不需要 | 接受；升级路径已规划 |
| 64-bit atomics 兼容性 | VG SW rasterizer 已有 32-bit fallback | 无新债务 |
| App Store 合规性 | MoltenVK 使用公开 Metal API | 已有上架先例（Dota 2 等） |

**结论**：本方案零新增技术债。`void* vg_renderer` 是唯一已有债务，本次顺带修复。
MoltenVK 选型不构成技术债，因为：
1. 升级到 Metal RHI 是纯增量操作（第 3.2 节）
2. 平台层代码 100% 复用（第 3.4 节）
3. 上层渲染代码零改动（RhiDevice 抽象层隔离）

---

## 10. 构建命令参考

### 10.1 macOS 构建

```bash
# 安装 Vulkan SDK (包含 MoltenVK)
# https://vulkan.lunarg.com/sdk/home#mac

cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DDSE_ENABLE_VULKAN=ON \
    -DDSE_ENABLE_APPLE_PLATFORM=ON \
    -DDSE_ENABLE_D3D11=OFF

cmake --build build
```

### 10.2 iOS 构建

```bash
cmake -B build-ios -G Xcode \
    -DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.cmake \
    -DDSE_ENABLE_VULKAN=ON \
    -DDSE_ENABLE_APPLE_PLATFORM=ON \
    -DDSE_ENABLE_D3D11=OFF \
    -DDSE_BUILD_EDITOR=OFF

cmake --build build-ios --config Debug
# 或在 Xcode 中打开 build-ios/DSEngine.xcodeproj
```

### 10.3 Windows 构建（验证隔离）

```bash
# 开关 OFF — 确保零影响
cmake --preset windows-x64-debug
cmake --build out/build/windows-x64-debug
ctest --test-dir out/build/windows-x64-debug --output-on-failure
```
