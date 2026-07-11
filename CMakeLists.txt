cmake_minimum_required(VERSION 3.17)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0175)
    cmake_policy(SET CMP0175 OLD)
endif()
project(DSEngine VERSION 0.1.0 LANGUAGES C CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# 预发布标签（SemVer pre-release）。首个 SDK 测试版为 v0.1.0-alpha。
# 正式发布时置空字符串即可（DSE_VERSION_STRING 退化为纯 "x.y.z"）。
# package_sdk.ps1 会解析此行用于命名发行包，保持格式为：set(DSEngine_VERSION_PRERELEASE "...")
set(DSEngine_VERSION_PRERELEASE "alpha")
if(DSEngine_VERSION_PRERELEASE STREQUAL "")
    set(DSEngine_VERSION_STRING "${PROJECT_VERSION}")
else()
    set(DSEngine_VERSION_STRING "${PROJECT_VERSION}-${DSEngine_VERSION_PRERELEASE}")
endif()

# 生成版本头文件 dse_version.h（从 .in 模板）
configure_file(
    ${CMAKE_SOURCE_DIR}/engine/dse_version.h.in
    ${CMAKE_BINARY_DIR}/generated/engine/dse_version.h
    @ONLY
)
# 让源码编译时能 #include "engine/dse_version.h"
include_directories(${CMAKE_BINARY_DIR}/generated)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-D GLM_FORCE_SWIZZLE)#GLM开启重排，随意转换。 vec2 v2 = v4.xz();
add_definitions(-D GLM_ENABLE_EXPERIMENTAL)

# 注意：当前工程会把 vcpkg 的整个 lib 目录以通配方式拼进 MSVC 链接命令，
# 其中如果存在 lua.lib，就会与源码内置 Lua 重复链接。
# 这里显式忽略 Lua 默认库名，作为针对该项目链接命令的最小修复。
if(MSVC)
    add_link_options(/NODEFAULTLIB:lua.lib /NODEFAULTLIB:lua5.1.lib /NODEFAULTLIB:lua51.lib /NODEFAULTLIB:lua53.lib /NODEFAULTLIB:lua54.lib)
endif()

option(DSE_ENABLE_PHYSX "Enable PhysX integration" OFF)
option(DSE_ENABLE_JOLT "Enable Jolt Physics integration" ON)
if(DSE_ENABLE_PHYSX AND DSE_ENABLE_JOLT)
    message(FATAL_ERROR "DSE_ENABLE_PHYSX and DSE_ENABLE_JOLT are mutually exclusive. Please enable only one 3D physics backend.")
endif()
option(DSE_ENABLE_2D "Enable 2D runtime path" ON)
option(DSE_ENABLE_3D "Enable 3D runtime path" ON)
# Web/Emscripten 默认是 2D-first MVP（见下方 if(EMSCRIPTEN)）；打开此开关后
# Web 构建保留 3D 前向路径（无 Compute/SSBO，依靠 UBO 版 PBR 着色器 + 能力门控）。
option(DSE_WEB_ENABLE_3D "Keep the 3D forward path on Web/Emscripten builds (M5 best-effort)" OFF)
# 多线程里程碑（可选）：打开后 Web 构建启用 Emscripten pthreads（-pthread + 共享内存 +
# 线程池），JobSystem 按 __EMSCRIPTEN_PTHREADS__ 起真实工作线程（默认 OFF 时 web 仍单线程
# 同步执行，现有 web-release-3d 构建零改动）。运行需宿主发 COOP/COEP 跨源隔离头以启用
# SharedArrayBuffer（harness server 已发；静态部署见 docs §7.4）。
option(DSE_WEB_ENABLE_THREADS "Enable Emscripten pthreads (SharedArrayBuffer + worker pool) on Web builds" OFF)
# Spine（2D 骨骼动画运行时）与 Assimp（FBX/OBJ 等模型导入）默认关闭以缩小 SDK 包体；
# 需要时按需开启：-DDSE_ENABLE_SPINE=ON / -DDSE_ENABLE_ASSIMP=ON。
# 关闭 Spine 时 SpineRendererComponent 与 dse.spine.* 绑定仍在（纯数据/ECS），仅不编译 spine-cpp 运行时与 SpineSystem。
# 关闭 Assimp 时 glTF 导入（tinygltf）不受影响，仅 FBX/OBJ 等 Assimp 格式不可用。
# 编辑器构建默认开启 Assimp 以支持 FBX/OBJ/Blender 等格式导入；
# 纯运行时/SDK 构建可 -DDSE_ENABLE_ASSIMP=OFF 缩小包体。
# v1 必备功能：默认开启（Spine/Net/HTTP/C#）。Web/Emscripten 交叉构建里由下方 if(EMSCRIPTEN)
# 按平台能力强制关闭不适用者（Net/HTTP/C# 无对应 Web 后端）。
option(DSE_ENABLE_SPINE "Enable Spine runtime path (2D skeletal animation)" ON)
option(DSE_ENABLE_ASSIMP "Build with Assimp model importer (FBX/OBJ/...); off keeps glTF-only and shrinks the package" ON)
if(ANDROID)
    option(DSE_ENABLE_VULKAN "Enable Vulkan RHI backend" OFF)
else()
    option(DSE_ENABLE_VULKAN "Enable Vulkan RHI backend" ON)
endif()
if(WIN32)
    option(DSE_ENABLE_D3D11 "Enable Direct3D 11 backend" ON)
else()
    option(DSE_ENABLE_D3D11 "Enable Direct3D 11 backend" OFF)
endif()
# WebGPU(Dawn/Emscripten) 后端：阶段 B，桌面级 parity。仅 Web/Emscripten 默认开启
# （见下方 if(EMSCRIPTEN) 强制 ON）；桌面暂无原生 Dawn 集成，默认 OFF。
option(DSE_ENABLE_WEBGPU "Enable WebGPU RHI backend (Web/Emscripten via Dawn)" OFF)
option(DSE_WEBGPU_SELFTEST "Compile WebGPU bring-up self-test harness (diagnostic, off in shipping builds)" OFF)
option(DSE_BUILD_EDITOR "Build editor via npm/node-gyp" OFF)
option(DSE_BUILD_LAUNCHER "Build launcher target" OFF)
option(DSE_BUILD_GTESTS "Build GoogleTest based test targets" ON)
option(DSE_ENABLE_PROFILER "Include EasyProfiler instrumentation" OFF)
option(DSE_ENABLE_LUA "Enable Lua scripting runtime" ON)
option(DSE_ENABLE_NAVMESH "Enable NavMesh/pathfinding (Recast/Detour)" ON)
option(DSE_ENABLE_NET "Enable networking layer (GameNetworkingSockets backend)" ON)
option(DSE_ENABLE_HTTP "Enable async HTTP(S) client (IXWebSocket + OpenSSL)" ON)
option(DSE_ENABLE_PCH "Enable precompiled headers for faster compilation" ON)
option(DSE_ENABLE_UNITY_BUILD "Enable CMake unity (jumbo) build for faster compilation" OFF)
option(DSE_ENABLE_CSHARP "Enable C# scripting via .NET 8 CoreCLR (hostfxr)" ON)
option(DSE_ENABLE_VIRTUAL_GEOMETRY "Enable experimental virtual geometry (Nanite-style) pipeline" OFF)
# Apple 平台后端（macOS/iOS via MoltenVK）：实验性，默认 OFF。
# macOS: GLFW + MoltenVK（复用桌面 Vulkan RHI）
# iOS: UIKit + MTKView + MoltenVK（新增 UIKitApp 平台层）
option(DSE_ENABLE_APPLE_PLATFORM
    "Enable Apple platform support (macOS/iOS via MoltenVK, experimental)" OFF)

# HarmonyOS / OpenHarmony 平台后端：实验性，默认 OFF。
# Vulkan (VK_OHOS_surface) + GLES3 (EGL on OHOS) 双 RHI 后端
option(DSE_ENABLE_HARMONY_PLATFORM
    "Enable HarmonyOS/OpenHarmony platform support (experimental)" OFF)

# --- 内存子系统 (engine/core/memory) ---
option(DSE_ENABLE_MEM_TRACKING "Enable per-tag memory tracking + leak reporting (Debug config only)" ON)
option(DSE_MEM_TRACK_POINTERS "Register every allocation by pointer for precise leak location (slower)" OFF)
set(DSE_MEM_BACKEND "system" CACHE STRING "General-heap backend: system | mimalloc (mimalloc requires the dependency, default system = zero new deps)")
set_property(CACHE DSE_MEM_BACKEND PROPERTY STRINGS system mimalloc)

# --- Android NDK 交叉编译默认值 ---
if(ANDROID)
    set(DSE_ENABLE_PHYSX OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_D3D11 OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_PROFILER OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_EDITOR OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_LAUNCHER OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_GTESTS OFF CACHE BOOL "" FORCE)
    # v1 桌面/Web 必备功能在移动端无对应后端（.NET8/GNS/OpenSSL），强制关闭以免破坏交叉构建。
    set(DSE_ENABLE_NET OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_HTTP OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_CSHARP OFF CACHE BOOL "" FORCE)
    add_definitions(-D __ANDROID__)
    message(STATUS "Android NDK build: PhysX/D3D11/EasyProfiler/GTest/Net/HTTP/C# disabled")
endif()

# --- iOS (Apple) 交叉编译默认值 ---
if(DSE_ENABLE_APPLE_PLATFORM AND IOS)
    set(DSE_ENABLE_PHYSX OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_D3D11 OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_PROFILER OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_EDITOR OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_LAUNCHER OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_GTESTS OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_NET OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_HTTP OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_CSHARP OFF CACHE BOOL "" FORCE)
    message(STATUS "iOS build: PhysX/D3D11/EasyProfiler/Editor/Launcher/GTest/Net/HTTP/C# disabled")
endif()

# --- OHOS (HarmonyOS) 交叉编译默认值 ---
if(DSE_ENABLE_HARMONY_PLATFORM AND OHOS)
    set(DSE_ENABLE_PHYSX OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_D3D11 OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_PROFILER OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_EDITOR OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_LAUNCHER OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_GTESTS OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_NET OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_HTTP OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_CSHARP OFF CACHE BOOL "" FORCE)
    message(STATUS "OHOS build: PhysX/D3D11/EasyProfiler/Editor/Launcher/GTest/Net/HTTP/C# disabled")
endif()

# --- Emscripten (Web/WASM) 交叉编译默认值 ---
# Web MVP：WebGL2(=GLES3.0，无 Compute)；关闭桌面/移动专属与重型后端，先把 2D 跑通。
# 高级特性按 RHI 能力标志在运行时自动门控，核心渲染代码不出现平台宏。
if(EMSCRIPTEN)
    set(DSE_ENABLE_VULKAN  OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_D3D11   OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_PHYSX   OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_JOLT    OFF CACHE BOOL "" FORCE)
    # 物理后端在 Web 无 pthreads，始终关闭；3D 渲染前向路径由 DSE_WEB_ENABLE_3D 控制。
    if(NOT DSE_WEB_ENABLE_3D)
        set(DSE_ENABLE_3D  OFF CACHE BOOL "" FORCE)
        set(DSE_ENABLE_WEBGPU OFF CACHE BOOL "" FORCE)
    else()
        # 阶段 B：Web 3D 默认编入 WebGPU 后端（设备初始化失败/不可用时由工厂回退 WebGL2）。
        set(DSE_ENABLE_WEBGPU ON CACHE BOOL "" FORCE)
    endif()
    set(DSE_ENABLE_NAVMESH OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_NET     OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_HTTP    OFF CACHE BOOL "" FORCE)
    # C# 依赖 .NET 8 CoreCLR/hostfxr，Web/wasm 无对应运行时，强制关闭。
    set(DSE_ENABLE_CSHARP  OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_PROFILER OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_PCH     OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_EDITOR   OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_LAUNCHER OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_GTESTS   OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_SHARED   OFF CACHE BOOL "" FORCE)
    # GLFW3 由 Emscripten 端口提供（-sUSE_GLFW=3）；GLES3 头文件随工具链。编译期亦需该开关。
    add_compile_options(-sUSE_GLFW=3)
    # 多线程里程碑：pthreads 采用共享内存模型，**所有** TU 必须以 -pthread 编译（否则 ABI 不一致）。
    # 故在此全局加入编译选项；链接侧 flag 见 apps/web_host。默认 OFF 时不加，现有构建零改动。
    if(DSE_WEB_ENABLE_THREADS)
        add_compile_options(-pthread)
        message(STATUS "Emscripten build: pthreads ENABLED (DSE_WEB_ENABLE_THREADS=ON) — SharedArrayBuffer + worker pool; host must serve COOP/COEP")
    endif()
    if(DSE_ENABLE_3D)
        message(STATUS "Emscripten build: 3D forward path KEPT (DSE_WEB_ENABLE_3D=ON); physics/Vulkan/D3D11/NavMesh/Net/HTTP disabled (WebGL2, no Compute)")
    else()
        message(STATUS "Emscripten build: Vulkan/D3D11/PhysX/Jolt/3D/NavMesh/Net/HTTP/Editor/Launcher/GTest disabled (WebGL2 2D MVP)")
    endif()
endif()


# --- in-tree 依赖 submodule 完整性检查 ---
# 本工程所有第三方依赖以 git submodule 形式内置于 depends/，克隆后必须先执行：
#     git submodule update --init --recursive
# 这里用代表性 sentinel 文件检测；缺失时给出明确指引，而非晦涩的下游编译/配置报错。
set(_DSE_SUBMODULE_SENTINELS
    "depends/glm/glm/glm.hpp"
    "depends/imgui/imgui.h"
    "depends/assimp/CMakeLists.txt"
    "depends/glfw-3.3-3.4/CMakeLists.txt"
    "depends/JoltPhysics/Build/CMakeLists.txt"
    "depends/box2d-2.4.1/CMakeLists.txt"
    "depends/spdlog/CMakeLists.txt"
    "depends/entt-3.13.0/CMakeLists.txt"
)
if(DSE_ENABLE_VULKAN)
    list(APPEND _DSE_SUBMODULE_SENTINELS
        "depends/Vulkan-Headers/include/vulkan/vulkan.h"
        "depends/glslang/CMakeLists.txt"
        "depends/spirv-cross/CMakeLists.txt")
endif()
if(DSE_MEM_BACKEND STREQUAL "mimalloc")
    list(APPEND _DSE_SUBMODULE_SENTINELS "depends/mimalloc/CMakeLists.txt")
endif()
set(_DSE_MISSING_SUBMODULES "")
foreach(_dse_s IN LISTS _DSE_SUBMODULE_SENTINELS)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${_dse_s}")
        list(APPEND _DSE_MISSING_SUBMODULES "${_dse_s}")
    endif()
endforeach()
if(_DSE_MISSING_SUBMODULES)
    string(REPLACE ";" "\n    - " _DSE_MISSING_PRETTY "${_DSE_MISSING_SUBMODULES}")
    message(FATAL_ERROR
        "检测到依赖 submodule 缺失（depends/ 下内置第三方库未初始化）：\n"
        "    - ${_DSE_MISSING_PRETTY}\n"
        "\n"
        "本工程所有依赖以 git submodule 内置于 depends/。请在仓库根目录执行：\n"
        "    git submodule update --init --recursive\n"
        "\n"
        "完成后重新配置（VS：删除 build/ 目录后重新生成；命令行：重跑 cmake --preset）。")
endif()

if(DSE_ENABLE_NAVMESH)
    add_definitions(-D DSE_ENABLE_NAVMESH)
endif()
if(DSE_ENABLE_2D)
    add_definitions(-D DSE_ENABLE_2D)
endif()
if(DSE_ENABLE_3D)
    add_definitions(-D DSE_ENABLE_3D)
endif()

# Vulkan 源码集成
if(DSE_ENABLE_VULKAN)
    include(cmake/CMakeLists.txt.vulkan)
endif()

# glslang 源码集成（shader 离线编译工具链）
include(cmake/CMakeLists.txt.glslang)

# SPIRV-Cross 源码集成（shader 交叉编译工具）
include(cmake/CMakeLists.txt.spirv_cross)

# 注意：DSE_ENABLE_PHYSX 宏定义移至 Physx cmake include 之后，
# 因为 DSE_HAS_PHYSX_LIBS 在 cmake/CMakeLists.txt.Physx 中才被设置。

# 设置输出目录为 bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)

# 本项目主要依赖仓库内置 depends 源码，不希望 MSVC 的 vcpkg 自动把
# installed/<triplet>/lib/*.lib 注入到每个 target 的链接命令中。
# 注意：vcpkg 的 MSBuild 集成要求使用字符串 "false" 才会命中其条件判断。
set(VcpkgAutoLink "false" CACHE STRING "Disable MSBuild vcpkg auto-link wildcard injection" FORCE)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "MSVC runtime library" FORCE)
    set(CMAKE_VS_GLOBALS
        "VcpkgApplocalDeps=false"
        "VcpkgAutoLink=false"
    )
    add_compile_options(/bigobj)
    add_compile_options(/MP)   # 多线程编译：始终启用，不依赖构建脚本传参
    add_compile_options(/wd4819)#忽略保存为unicode警告
    add_compile_options(/FS) # 修复多线程编译 PDB 写入冲突 C1041
    add_compile_options("$<$<CONFIG:Debug>:/Z7>")
    add_compile_options("$<$<CONFIG:RelWithDebInfo>:/Z7>")
    add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
    add_compile_definitions(_CRT_STDIO_ISO_WIDE_SPECIFIERS=1)
    add_compile_definitions(_CRT_NONSTDC_NO_WARNINGS=1)
    add_compile_definitions(_CRT_DECLARE_NONSTDC_NAMES=1)
endif ()
if(ANDROID OR EMSCRIPTEN OR (DSE_ENABLE_APPLE_PLATFORM AND IOS))
    # Android: 使用 AndroidApp；Emscripten: GLFW3 由 -sUSE_GLFW=3 端口提供；
    # iOS: 使用 UIKitApp（无 GLFW）。均不在 in-tree 编 GLFW。
    set(glfw_sources "")
else()
    include(cmake/CMakeLists.txt.glfw)
endif()
include(cmake/CMakeLists.txt.FreeType)
if(DSE_ENABLE_PROFILER)
    include(cmake/CMakeLists.txt.easy_profile_core)
else()
    set(easy_profiler_core_source "")
    add_definitions(-D EASY_PROFILER_DISABLE)
endif()
if(DSE_ENABLE_LUA)
    include(cmake/CMakeLists.txt.lua_script)
    add_definitions(-D DSE_ENABLE_LUA)
else()
    set(lua_src "")
endif()
if(DSE_ENABLE_PHYSX)
    include(cmake/CMakeLists.txt.Physx)
endif()

# DSE_ENABLE_PHYSX 宏需在 Physx cmake include 之后定义，
# 因为 DSE_HAS_PHYSX_LIBS 在 cmake/CMakeLists.txt.Physx 中才被设置
if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
    add_definitions(-D DSE_ENABLE_PHYSX)
endif()

# Jolt Physics (跨平台替代 PhysX)
include(cmake/CMakeLists.txt.Jolt)
if(DSE_ENABLE_JOLT AND DSE_HAS_JOLT)
    add_definitions(-D DSE_ENABLE_JOLT)
endif()

# 网络层 (GameNetworkingSockets)。默认 OFF：OFF 路径完全不引入网络依赖，
# 现有三端构建零影响。ON 时由 cmake/CMakeLists.txt.gns 负责按平台分流加密后端
# (桌面 libsodium / 移动 OpenSSL) 并接入 protobuf + GNS 静态库。
if(DSE_ENABLE_NET)
    include(cmake/CMakeLists.txt.gns)
    if(DSE_HAS_NET)
        add_definitions(-D DSE_NET_ENABLED=1)
    endif()
endif()

# 通用异步 HTTP(S) 客户端 (IXWebSocket + OpenSSL)。默认 OFF：OFF 路径完全不引入
# IXWebSocket/OpenSSL 依赖，现有构建零影响。ON 时由 cmake/CMakeLists.txt.http
# 编出 dse_http 静态库并定义 DSE_ENABLE_HTTP（供 engine/http 与 Lua 绑定启用）。
if(DSE_ENABLE_HTTP)
    include(cmake/CMakeLists.txt.http)
    if(DSE_HAS_HTTP)
        add_definitions(-D DSE_ENABLE_HTTP=1)
    endif()
endif()

#头文件目录（全局，合并为一处避免重复）
include_directories(
    "${CMAKE_SOURCE_DIR}"
    "engine"
    "modules"
    "samples"
    "depends"
    "depends/spdlog/include"
    "depends/glm"
    "depends/glm_ext"
    "depends/rapidjson/include"
    "depends/eventpp/include"
    "depends/tinygltf"
)
set(DSE_HAS_ASSIMP OFF)
# Assimp 默认关闭（DSE_ENABLE_ASSIMP=OFF）以缩小包体；Emscripten 2D MVP 始终不编译该重型库。
if(DSE_ENABLE_ASSIMP AND EXISTS "${CMAKE_SOURCE_DIR}/depends/assimp/CMakeLists.txt" AND NOT EMSCRIPTEN)
    include_directories("depends/assimp/include")
    set(DSE_HAS_ASSIMP ON)
endif()

# Use local depends instead of FetchContent
set(ENTT_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ENTT_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(depends/entt-3.13.0)

set(DSE_PREV_CMAKE_WARN_DEPRECATED ${CMAKE_WARN_DEPRECATED})
set(DSE_PREV_CMAKE_SUPPRESS_DEVELOPER_WARNINGS ${CMAKE_SUPPRESS_DEVELOPER_WARNINGS})
set(CMAKE_WARN_DEPRECATED OFF)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1)

set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(depends/box2d-2.4.1)
# 强制 box2d 使用 DLL CRT，与 dse_engine 的 /MDd 一致
if(MSVC)
    set_property(TARGET box2d PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Recast & Detour 静态库（仅在 NavMesh 启用时编译）
if(DSE_ENABLE_NAVMESH)
    include(cmake/CMakeLists.txt.recastnavigation)
endif()
if(DSE_HAS_ASSIMP)
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
    set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "Disable Assimp install/export for local dependency build" FORCE)
    set(ASSIMP_INSTALL_PDB OFF CACHE BOOL "Disable assimp PDB install to avoid missing .pdb install failure" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES
        ${CMAKE_C_STANDARD_INCLUDE_DIRECTORIES}
        ${CMAKE_SOURCE_DIR}/depends/lua
    )
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/depends/assimp/generated")
    add_subdirectory(depends/assimp)
    # 强制 assimp 使用 DLL CRT，与 dse_engine 的 /MDd 一致
    if(MSVC)
        set_property(TARGET assimp PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
endif()


set(CMAKE_WARN_DEPRECATED ${DSE_PREV_CMAKE_WARN_DEPRECATED})
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ${DSE_PREV_CMAKE_SUPPRESS_DEVELOPER_WARNINGS})

# 仅追加上方未覆盖的第三方库路径
include_directories(
    ${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src
    ${CMAKE_SOURCE_DIR}/depends/box2d-2.4.1/include
    ${CMAKE_SOURCE_DIR}/depends/spine-runtimes/spine-cpp/spine-cpp/include
    ${CMAKE_SOURCE_DIR}/depends/tiny-AES-c
    ${CMAKE_SOURCE_DIR}/depends/imgui
    ${CMAKE_SOURCE_DIR}/depends/imgui/backends
)
set(BOX2D_ROOT "${CMAKE_SOURCE_DIR}/depends/box2d-2.4.1")

# ── Engine source collection (extracted to cmake/engine_sources.cmake) ─
include(cmake/engine_sources.cmake)

option(DSE_BUILD_SHARED "Build dse_engine as shared library (DLL). Requires DSE_EXPORT on public APIs." OFF)

# --- LTCG / IPO（Release 链接期代码生成，缩小二进制 + 跨翻译单元优化）---
# 仅对「静态」配置启用：DSE_BUILD_SHARED=ON 时 /GL 与导出 .def 生成不兼容
# （cmake -E __create_def 读不了 /GL 产出的 .obj 符号表），故共享库构建保持关闭。
# 通过 dse_enable_lto() 按目标精确开启，避免波及第三方依赖（assimp/Jolt 等）。
option(DSE_ENABLE_LTO "Enable Link-Time Code Generation (/GL+/LTCG) on Release for first-party targets" ON)
set(DSE_IPO_SUPPORTED OFF)
if(DSE_ENABLE_LTO AND NOT DSE_BUILD_SHARED)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _dse_ipo_ok OUTPUT _dse_ipo_msg LANGUAGES CXX)
    if(_dse_ipo_ok)
        set(DSE_IPO_SUPPORTED ON)
        message(STATUS "LTCG/IPO enabled for first-party Release builds")
    else()
        message(STATUS "LTCG/IPO unsupported, skipping: ${_dse_ipo_msg}")
    endif()
endif()
# 对单个目标在 Release / RelWithDebInfo 配置启用 IPO（Debug 不启用，保留快编译 + 可调试）。
function(dse_enable_lto target)
    if(DSE_IPO_SUPPORTED)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    endif()
endfunction()

set(DSE_ENGINE_ALL_SOURCES
    ${glfw_sources}
    ${freetype_src}
    ${easy_profiler_core_source}
    ${lua_src}
    ${spine_cpp}
    ${tiny_aes_c}
    ${bundle_cpp}
    ${imgui_src}
    ${engine_cpp}
)
if(DSE_BUILD_SHARED)
    add_library(dse_engine SHARED ${DSE_ENGINE_ALL_SOURCES})
else()
    add_library(dse_engine STATIC ${DSE_ENGINE_ALL_SOURCES})
endif()
target_link_libraries(dse_engine PRIVATE box2d)
# 崩溃报告子系统（engine/diagnostics/crash_handler.cpp）：Windows 上需 DbgHelp
# 提供 MiniDumpWriteDump / StackWalk64 / Sym* 符号化。
if(WIN32)
    target_link_libraries(dse_engine PRIVATE Dbghelp)
endif()
if(DSE_HAS_ASSIMP)
    target_link_libraries(dse_engine PRIVATE assimp)
endif()
if(DSE_ENABLE_NAVMESH)
    target_link_libraries(dse_engine PRIVATE recast detour detour_tilecache)
endif()
if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
    target_include_directories(dse_engine PRIVATE ${PHYSX_INCLUDE_DIR} ${PXSHARED_INCLUDE_DIR})
    target_link_directories(dse_engine PRIVATE
        $<$<CONFIG:Debug>:${PHYSX_DLL_DIR_DEBUG}>
        $<$<NOT:$<CONFIG:Debug>>:${PHYSX_DLL_DIR_RELEASE}>
    )
    # PHYSX_LIBS 由 cmake/CMakeLists.txt.Physx 计算，已包含 release→debug import lib 回退
    target_link_libraries(dse_engine PRIVATE
        ${PHYSX_LIBS}
        $<$<CONFIG:Debug>:${PHYSX_LIB_DIR_DEBUG}/PhysXExtensions_static_64.lib>
        $<$<NOT:$<CONFIG:Debug>>:${PHYSX_LIB_DIR_RELEASE}/PhysXExtensions_static_64.lib>
    )
endif()
if(DSE_ENABLE_JOLT AND DSE_HAS_JOLT)
    target_link_libraries(dse_engine PRIVATE Jolt)
endif()
# --- 异步 HTTP 客户端 ---
if(DSE_ENABLE_HTTP AND DSE_HAS_HTTP)
    target_link_libraries(dse_engine PRIVATE dse_http)
    # 无头验证 Lua 绑定 dse.http（裸 lua_State + 本地回环服务器），不依赖窗口/RHI。
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/http/http_lua_smoke.cpp")
        add_executable(dse_http_lua_smoke "${CMAKE_SOURCE_DIR}/tests/http/http_lua_smoke.cpp")
        target_link_libraries(dse_http_lua_smoke PRIVATE dse_engine dse_http)
        target_include_directories(dse_http_lua_smoke PRIVATE
            "${CMAKE_SOURCE_DIR}"
            "${CMAKE_SOURCE_DIR}/depends/IXWebSocket"
            "${CMAKE_SOURCE_DIR}/depends/lua")
        target_compile_features(dse_http_lua_smoke PRIVATE cxx_std_17)
        if(MSVC)
            set_property(TARGET dse_http_lua_smoke PROPERTY
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
            target_compile_options(dse_http_lua_smoke PRIVATE /utf-8)
        endif()
    endif()
endif()
# --- 游戏网络传输 (GameNetworkingSockets) Lua 绑定 ---
if(DSE_ENABLE_NET AND DSE_HAS_NET)
    # dse.net 绑定(engine/scripting/.../lua_binding_net.cpp)随 dse_engine 编译，
    # 调用 dse_net C ABI；故把 dse_net 链进 dse_engine（静态库依赖透传给可执行）。
    target_link_libraries(dse_engine PRIVATE dse_net)
    # dse_net 单独成静态库、且不链接 dse_engine，故不会继承 dse_engine 的 PUBLIC
    # DSE_BUILD_STATIC。静态配置下须显式给它定义，否则其复制层 TU 里包含 engine/core
    # 头文件时 DSE_EXPORT 展开为 __declspec(dllimport)，引用 __imp_Memory::* 无法解析。
    if(NOT DSE_BUILD_SHARED)
        target_compile_definitions(dse_net PUBLIC DSE_BUILD_STATIC)
    endif()
    # cmake/CMakeLists.txt.gns 中创建的 net 测试可执行仅链接了 dse_net；其中触及
    # 复制层(engine/net/replication)/位级序列化的目标会引用 engine/core 的 Memory
    # 符号（AllocAligned/Free，定义在 dse_engine）。dse_engine 在此处才定义，故在此
    # 补链，避免 LNK2019/LNK1120。对不引用这些符号的目标补链亦无害（静态库按需拉取）。
    foreach(_dse_net_test dse_net_smoke dse_net_capi_smoke dse_repl_smoke dse_net_full_test)
        if(TARGET ${_dse_net_test})
            target_link_libraries(${_dse_net_test} PRIVATE dse_engine)
        endif()
    endforeach()
    # 无头验证 Lua 绑定 dse.net（裸 lua_State + 本地 UDP 回环），不依赖窗口/RHI。
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/net/net_lua_smoke.cpp")
        add_executable(dse_net_lua_smoke "${CMAKE_SOURCE_DIR}/tests/net/net_lua_smoke.cpp")
        target_link_libraries(dse_net_lua_smoke PRIVATE dse_engine dse_net)
        target_include_directories(dse_net_lua_smoke PRIVATE
            "${CMAKE_SOURCE_DIR}"
            "${CMAKE_SOURCE_DIR}/depends/lua")
        target_compile_features(dse_net_lua_smoke PRIVATE cxx_std_17)
        if(MSVC)
            set_property(TARGET dse_net_lua_smoke PROPERTY
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
            target_compile_options(dse_net_lua_smoke PRIVATE /utf-8)
        endif()
    endif()
endif()
# --- 自描述二进制序列化 (dse.serialize) Lua 绑定 ---
# 绑定 engine/scripting/.../lua_binding_serialize.cpp 随 dse_engine 编译（无外部依赖、
# 不受 NET/HTTP 开关影响）。此处仅在启用 Lua 时构建其无头往返 smoke。
if(DSE_ENABLE_LUA AND EXISTS "${CMAKE_SOURCE_DIR}/tests/serialize/serialize_smoke.cpp")
    add_executable(dse_serialize_smoke "${CMAKE_SOURCE_DIR}/tests/serialize/serialize_smoke.cpp")
    target_link_libraries(dse_serialize_smoke PRIVATE dse_engine)
    target_include_directories(dse_serialize_smoke PRIVATE
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/depends/lua")
    target_compile_features(dse_serialize_smoke PRIVATE cxx_std_17)
    if(MSVC)
        set_property(TARGET dse_serialize_smoke PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        target_compile_options(dse_serialize_smoke PRIVATE /utf-8)
    endif()
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/blueprint/blueprint_smoke.cpp")
    add_executable(dse_blueprint_smoke "${CMAKE_SOURCE_DIR}/tests/blueprint/blueprint_smoke.cpp")
    target_link_libraries(dse_blueprint_smoke PRIVATE dse_engine)
    target_include_directories(dse_blueprint_smoke PRIVATE "${CMAKE_SOURCE_DIR}")
    target_compile_features(dse_blueprint_smoke PRIVATE cxx_std_17)
    if(MSVC)
        set_property(TARGET dse_blueprint_smoke PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        target_compile_options(dse_blueprint_smoke PRIVATE /utf-8)
    endif()
endif()
# --- D3D11 RHI 后端 ---
if(DSE_ENABLE_D3D11 AND WIN32)
    add_definitions(-D DSE_ENABLE_D3D11)
    target_link_libraries(dse_engine PRIVATE d3d11 dxgi d3dcompiler dxguid)
elseif(DSE_ENABLE_D3D11 AND NOT WIN32)
    message(WARNING "DSE_ENABLE_D3D11=ON but platform is not Windows. D3D11 backend will be disabled.")
    set(DSE_ENABLE_D3D11 OFF)
endif()
# --- WebGPU RHI 后端 (Emscripten/Dawn) ---
if(DSE_ENABLE_WEBGPU AND EMSCRIPTEN)
    add_definitions(-D DSE_ENABLE_WEBGPU)
    # -sUSE_WEBGPU=1 的编译/链接 flag 由 apps/web_host 在链接阶段统一加入。
    message(STATUS "WebGPU RHI backend enabled (Emscripten)")
    if(DSE_WEBGPU_SELFTEST)
        add_definitions(-D DSE_WEBGPU_SELFTEST)
        message(STATUS "WebGPU self-test harness enabled (diagnostic build)")
    endif()
elseif(DSE_ENABLE_WEBGPU AND NOT EMSCRIPTEN)
    message(WARNING "DSE_ENABLE_WEBGPU=ON but platform is not Emscripten. WebGPU backend will be disabled (no native Dawn integration yet).")
    set(DSE_ENABLE_WEBGPU OFF)
endif()
# --- Vulkan RHI 后端 ---
if(DSE_ENABLE_VULKAN AND DSE_HAS_VULKAN)
    add_definitions(-D DSE_ENABLE_VULKAN)

    # Vulkan 头文件 + Loader 链接（源码构建或系统 SDK）
    if(DSE_VULKAN_HEADERS_FROM_SOURCE)
        target_link_libraries(dse_engine PRIVATE Vulkan::Headers)
    endif()
    if(DSE_VULKAN_LOADER_FROM_SOURCE)
        target_link_libraries(dse_engine PRIVATE Vulkan::Loader)
    elseif(DSE_VULKAN_FROM_SYSTEM)
        target_include_directories(dse_engine PRIVATE ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(dse_engine PRIVATE Vulkan::Vulkan)
    endif()

    # glslang 运行时编译库（GLSL→SPIR-V）
    if(DSE_HAS_GLSLANG)
        add_definitions(-D DSE_HAS_GLSLANG)
        target_include_directories(dse_engine PRIVATE
            ${DSE_GLSLANG_INCLUDE_DIR}
            ${DSE_GLSLANG_INCLUDE_DIR}/glslang/Include
            ${DSE_GLSLANG_INCLUDE_DIR}/SPIRV
        )
        target_link_libraries(dse_engine PRIVATE glslang SPIRV MachineIndependent GenericCodeGen glslang-default-resource-limits)
    else()
        message(WARNING "DSE_ENABLE_VULKAN=ON but glslang not available. "
                        "Vulkan shader compilation will be disabled at runtime.")
    endif()
    # SPIRV-Cross 运行时反射（GPU-driven shader 自动发现 descriptor bindings）
    # 注意：不链接 spirv-cross 库以避免 WINDOWS_EXPORT_ALL_SYMBOLS 符号数超限（LNK1189）。
    # 改为在 spirv_cross_embedded.cpp 中直接 include 需要的 spirv-cross 源文件。
    if(DSE_HAS_SPIRV_CROSS)
        target_compile_definitions(dse_engine PRIVATE DSE_HAS_SPIRV_CROSS)
        target_include_directories(dse_engine PRIVATE ${DSE_SPIRV_CROSS_INCLUDE_DIR})
    endif()
elseif(DSE_ENABLE_VULKAN AND NOT DSE_HAS_VULKAN)
    message(WARNING "DSE_ENABLE_VULKAN=ON but Vulkan headers/loader not available. "
                    "Vulkan RHI backend will be disabled.")
    set(DSE_ENABLE_VULKAN OFF)
endif()
set_target_properties(dse_engine PROPERTIES OUTPUT_NAME "DSEngine")
if(DSE_BUILD_SHARED)
    target_compile_definitions(dse_engine PRIVATE DSE_ENGINE_EXPORTS)
else()
    target_compile_definitions(dse_engine PUBLIC DSE_BUILD_STATIC)
endif()

# Spine 运行时启用标记：PUBLIC 以便头文件（gameplay_2d_module.h）与消费端口径一致。
if(DSE_ENABLE_SPINE)
    target_compile_definitions(dse_engine PUBLIC DSE_ENABLE_SPINE)
endif()

# Virtual Geometry (Nanite-style) 实验性管线：OFF by default, 完全隔离
# 源文件由 engine/*.cpp glob 自动收集，NOT DSE_ENABLE_VIRTUAL_GEOMETRY 时排除（见上方 list FILTER）。
if(DSE_ENABLE_VIRTUAL_GEOMETRY)
    target_compile_definitions(dse_engine PUBLIC DSE_ENABLE_VIRTUAL_GEOMETRY)
    message(STATUS "Virtual Geometry (experimental): ENABLED")
endif()

# 内存追踪：在 Debug 与 RelWithDebInfo 配置生效（带调试信息的诊断构建），
# Release 保持零开销。PUBLIC 以便测试/消费端口径一致。
if(DSE_ENABLE_MEM_TRACKING)
    target_compile_definitions(dse_engine PUBLIC
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:DSE_ENABLE_MEM_TRACKING=1>)
endif()
if(DSE_MEM_TRACK_POINTERS)
    target_compile_definitions(dse_engine PUBLIC DSE_MEM_TRACK_POINTERS=1)
endif()

# 通用堆后端：默认 system（零新增依赖）。mimalloc 为可选高性能后端，
# 按仓库惯例以 git submodule 内置于 depends/mimalloc，源码 add_subdirectory 编入。
# 仅当显式选择 mimalloc 时才编入 + 链接 + 定义 DSE_MEM_USE_MIMALLOC；
# 否则 mimalloc_allocator.cpp 编译为空翻译单元，对默认构建零影响。
if(DSE_MEM_BACKEND STREQUAL "mimalloc")
    # 禁用 mimalloc 自带的测试/动态库/全局 override，仅编静态库。
    set(MI_BUILD_TESTS  OFF CACHE BOOL "" FORCE)
    set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(MI_BUILD_OBJECT OFF CACHE BOOL "" FORCE)
    set(MI_OVERRIDE     OFF CACHE BOOL "" FORCE)
    add_subdirectory(depends/mimalloc EXCLUDE_FROM_ALL)
    if(TARGET mimalloc-static)
        target_link_libraries(dse_engine PRIVATE mimalloc-static)
    else()
        target_link_libraries(dse_engine PRIVATE mimalloc)
    endif()
    # include 目录随静态库目标传播；显式补一份以防不同版本未导出。
    target_include_directories(dse_engine PRIVATE ${CMAKE_SOURCE_DIR}/depends/mimalloc/include)
    target_compile_definitions(dse_engine PRIVATE DSE_MEM_USE_MIMALLOC=1)
    message(STATUS "Memory backend: mimalloc (depends/mimalloc submodule)")
else()
    message(STATUS "Memory backend: system (default)")
endif()

if(MSVC)
    target_compile_options(dse_engine PRIVATE /utf-8 /wd5033 /wd4834)
endif()

# --- C# 脚本运行时 (.NET 8 CoreCLR via hostfxr) ---
if(DSE_ENABLE_CSHARP)
    target_compile_definitions(dse_engine PRIVATE DSE_CSHARP=1)
    # nethost: 提供 get_hostfxr_path() 用于定位 hostfxr.dll
    # .NET SDK 安装后 nethost 头文件和库位于
    #   packs/Microsoft.NETCore.App.Host.win-x64/<版本>/runtimes/win-x64/native/
    # 注意版本号子目录（如 8.0.28）：find_path/find_library 不会递归进版本目录，
    # 故先 GLOB 出所有版本目录作为搜索根，再在其 native 子目录里定位。
    file(GLOB _dse_nethost_dirs
        "$ENV{ProgramFiles}/dotnet/packs/Microsoft.NETCore.App.Host.win-x64/*/runtimes/win-x64/native"
    )
    find_path(NETHOST_INCLUDE_DIR nethost.h
        PATHS ${_dse_nethost_dirs}
              "$ENV{ProgramFiles}/dotnet/packs/Microsoft.NETCore.App.Host.win-x64"
        PATH_SUFFIXES "runtimes/win-x64/native"
    )
    # 用 import 库 nethost.lib（对应 nethost.dll）。注意：静态库 libnethost.lib 由
    # 静态 Release CRT 编出，与本工程的 /MDd 动态调试 CRT 不兼容（LNK2038），故不用它。
    find_library(NETHOST_LIBRARY nethost
        PATHS ${_dse_nethost_dirs}
              "$ENV{ProgramFiles}/dotnet/packs/Microsoft.NETCore.App.Host.win-x64"
        PATH_SUFFIXES "runtimes/win-x64/native"
    )
    if(NETHOST_INCLUDE_DIR AND NETHOST_LIBRARY)
        target_include_directories(dse_engine PRIVATE ${NETHOST_INCLUDE_DIR})
        target_link_libraries(dse_engine PRIVATE ${NETHOST_LIBRARY})
        # nethost.lib 是 import 库，运行时需 nethost.dll 在可执行同目录；拷到输出目录，
        # 否则装载期报 0xC0000135 (STATUS_DLL_NOT_FOUND)。
        if(EXISTS "${NETHOST_INCLUDE_DIR}/nethost.dll")
            file(COPY "${NETHOST_INCLUDE_DIR}/nethost.dll"
                 DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        endif()
        message(STATUS "C# scripting: .NET 8 hostfxr (nethost found at ${NETHOST_INCLUDE_DIR})")
    else()
        message(STATUS "C# scripting: nethost not found, will search at runtime")
        # 回退: CSharpHost 在运行时通过 PATH 搜索 hostfxr
        target_compile_definitions(dse_engine PRIVATE DSE_CSHARP_NO_NETHOST=1)
    endif()
    # C# 项目构建 (requires dotnet CLI)
    # dotnet 可能不在 PATH 上（如用 dotnet-install.ps1 装到 Program Files\dotnet）；
    # 显式查找可执行，找不到再回退裸 "dotnet"（依赖 PATH）。
    find_program(DOTNET_EXECUTABLE dotnet
        PATHS "$ENV{ProgramFiles}/dotnet" "$ENV{ProgramW6432}/dotnet"
    )
    if(NOT DOTNET_EXECUTABLE)
        set(DOTNET_EXECUTABLE dotnet)
        message(STATUS "C# scripting: dotnet CLI not found in Program Files, relying on PATH")
    else()
        message(STATUS "C# scripting: dotnet CLI at ${DOTNET_EXECUTABLE}")
    endif()
    # 注意：vcvars64.bat 会设置环境变量 Platform=x64，msbuild 会据此尝试用
    # "Debug|x64" 求解方案配置，但本 .sln 仅定义 "Any CPU"，导致 MSB4126。
    # 故显式指定 -p:Platform="Any CPU"。
    add_custom_target(dse_csharp_build
        COMMAND "${DOTNET_EXECUTABLE}" build "${CMAKE_SOURCE_DIR}/GameScripts/DSEngine.sln"
                -c $<IF:$<CONFIG:Debug>,Debug,Release>
                "-p:Platform=Any CPU"
                -o "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/managed/"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/GameScripts"
        COMMENT "Building C# scripts -> managed/*.dll"
    )

    # C# 运行时端到端无头冒烟：通过 CSharpHost 实际启动 CoreCLR、加载托管程序集、
    # 驱动一帧回调 + 热重载。依赖 dse_csharp_build 先产出 managed/*.dll + runtimeconfig。
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/csharp/csharp_host_smoke.cpp")
        add_executable(dse_csharp_smoke "${CMAKE_SOURCE_DIR}/tests/csharp/csharp_host_smoke.cpp")
        target_link_libraries(dse_csharp_smoke PRIVATE dse_engine)
        target_include_directories(dse_csharp_smoke PRIVATE "${CMAKE_SOURCE_DIR}")
        target_compile_features(dse_csharp_smoke PRIVATE cxx_std_17)
        target_compile_definitions(dse_csharp_smoke PRIVATE
            DSE_CS_MANAGED_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/managed")
        add_dependencies(dse_csharp_smoke dse_csharp_build)
        if(MSVC)
            set_property(TARGET dse_csharp_smoke PROPERTY
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
            target_compile_options(dse_csharp_smoke PRIVATE /utf-8)
        endif()
    endif()
endif()

target_include_directories(dse_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/render/shaders/generated
)
if(NOT ANDROID AND NOT EMSCRIPTEN AND NOT (DSE_ENABLE_APPLE_PLATFORM AND IOS) AND NOT (DSE_ENABLE_HARMONY_PLATFORM AND OHOS))
    # glad 仅桌面端使用；Android/Web/OHOS 用静态链接的 GLES3 符号；iOS 用 MoltenVK。
    target_include_directories(dse_engine PRIVATE ${CMAKE_SOURCE_DIR}/depends/glad/include)
endif()

# --- Android 链接库 ---
if(ANDROID)
    target_link_libraries(dse_engine PRIVATE EGL GLESv3 android log)
endif()

# --- Linux 桌面链接库 (X11 窗口 + GL + dl + pthread) ---
# 注意：Emscripten 也满足 UNIX AND NOT APPLE AND NOT ANDROID，但 GL/audio/线程由
# emscripten 端口（-lGL/-lal/-lhtml5）提供，且 WASM 无 X11，故必须排除。
if(UNIX AND NOT APPLE AND NOT ANDROID AND NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
    target_link_libraries(dse_engine PRIVATE
        X11 Xrandr Xinerama Xcursor Xi
        GL ${CMAKE_DL_LIBS} Threads::Threads rt m)
endif()

# --- Apple 平台链接库 (macOS / iOS) ---
if(DSE_ENABLE_APPLE_PLATFORM AND APPLE)
    if(IOS)
        # iOS: UIKit + Metal + MetalKit + QuartzCore + AVFoundation
        find_library(UIKIT_LIB UIKit)
        find_library(METAL_LIB Metal)
        find_library(METALKIT_LIB MetalKit)
        find_library(QUARTZCORE_LIB QuartzCore)
        find_library(AVFOUNDATION_LIB AVFoundation)
        find_library(FOUNDATION_LIB Foundation)
        target_link_libraries(dse_engine PRIVATE
            ${UIKIT_LIB} ${METAL_LIB} ${METALKIT_LIB}
            ${QUARTZCORE_LIB} ${AVFOUNDATION_LIB} ${FOUNDATION_LIB})
        target_compile_definitions(dse_engine PRIVATE DSE_ENABLE_APPLE_PLATFORM)
        message(STATUS "Apple iOS build: UIKit/Metal/MetalKit/QuartzCore/AVFoundation linked")
    else()
        # macOS: Cocoa + IOKit + QuartzCore（GLFW 复用桌面窗口，MoltenVK 通过 Vulkan Loader 加载）
        find_library(COCOA_LIB Cocoa)
        find_library(IOKIT_LIB IOKit)
        find_library(QUARTZCORE_LIB QuartzCore)
        target_link_libraries(dse_engine PRIVATE
            ${COCOA_LIB} ${IOKIT_LIB} ${QUARTZCORE_LIB})
        target_compile_definitions(dse_engine PRIVATE DSE_ENABLE_APPLE_PLATFORM)
        message(STATUS "Apple macOS build: Cocoa/IOKit/QuartzCore linked (MoltenVK via Vulkan Loader)")
    endif()
endif()

# --- OHOS (HarmonyOS) 链接库 ---
if(DSE_ENABLE_HARMONY_PLATFORM AND OHOS)
    target_link_libraries(dse_engine PRIVATE
        ace_napi          # NAPI 运行时
        ace_ndk.z         # XComponent NDK
        native_window     # OHNativeWindow
        hilog_ndk.z       # 日志 (OH_LOG_Print)
        EGL GLESv3        # GLES3（兼容路径）
        rawfile.z         # rawfile 资产访问
        native_media_core # OHAudio
    )
    target_compile_definitions(dse_engine PRIVATE DSE_ENABLE_HARMONY_PLATFORM)
    message(STATUS "HarmonyOS build: NAPI/XComponent/OHNativeWindow/rawfile/OHAudio linked")
endif()

if(DSE_ENABLE_LUA)
    target_include_directories(dse_engine PRIVATE
        ${CMAKE_SOURCE_DIR}/depends/lua
    )
endif()

if(EXISTS "${BOX2D_ROOT}/include")
    target_include_directories(dse_engine PRIVATE "${BOX2D_ROOT}/include")
endif()
if(EXISTS "${BOX2D_ROOT}/src")
    target_include_directories(dse_engine PRIVATE "${BOX2D_ROOT}/src")
endif()

function(dse_apply_config_postfix target_name)
    set_target_properties(${target_name} PROPERTIES
        DEBUG_POSTFIX "_debug"
        RELEASE_POSTFIX "_release"
        RELWITHDEBINFO_POSTFIX "_relwithdebinfo"
        MINSIZEREL_POSTFIX "_minsizerel"
    )
endfunction()

dse_apply_config_postfix(dse_engine)
# IMPORT_SUFFIX / PREFIX 由 dse_apply_config_postfix 的 DEBUG_POSTFIX 等属性自动处理，
# 不再硬编码为 _debug.lib，否则 Release 配置也会链接到 _debug.lib。

# --- 预编译头（PCH）：加速常用 STL / GLM / EnTT 头文件解析 ---
if(DSE_ENABLE_PCH)
    target_precompile_headers(dse_engine PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/engine/dse_pch.h>"
    )
endif()

# --- Unity Build（可选）：合并翻译单元减少编译器启动 / 头文件重复解析开销 ---
if(DSE_ENABLE_UNITY_BUILD)
    set_target_properties(dse_engine PROPERTIES
        UNITY_BUILD ON
        UNITY_BUILD_BATCH_SIZE 16
    )
endif()

if (MSVC)
    set_property(TARGET dse_engine PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    # LTCG (/GL+/LTCG)：静态库配置（默认 DSE_BUILD_SHARED=OFF）下启用，Release 缩小二进制。
    # 共享库配置不可用——/GL 产出的 .obj 无标准 COFF 符号表，cmake -E __create_def 读不了，
    # 故 dse_enable_lto() 已用 NOT DSE_BUILD_SHARED 守卫（见上方定义）。
    dse_enable_lto(dse_engine)

    # Workaround: CMake 4.3 VS generator 在非 Debug 配置下可能不生成 objects.txt，
    # 导致 WINDOWS_EXPORT_ALL_SYMBOLS 的 __create_def PreLinkEvent 失败。
    # 此自定义 PRE_LINK 命令在 __create_def 之前运行，确保 objects.txt 存在。
    add_custom_command(TARGET dse_engine PRE_LINK
        COMMAND ${CMAKE_COMMAND} -E remove -f
            ${CMAKE_BINARY_DIR}/dse_engine.dir/$<CONFIG>/exports.def
        COMMAND ${CMAKE_COMMAND}
            -DOBJ_DIR=${CMAKE_BINARY_DIR}/dse_engine.dir/$<CONFIG>
            -P ${CMAKE_SOURCE_DIR}/cmake/write_objects_txt.cmake
        COMMENT "Generating objects.txt for WINDOWS_EXPORT_ALL_SYMBOLS"
        VERBATIM
    )
else()
    add_definitions(-D _DEBUG)
endif ()

# DSE_Gameplay3D 已静态编入 dse_engine，无需独立 install 规则。

add_custom_target(dse_copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/samples
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/samples
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/script
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/script
    COMMENT "Copying runtime assets to bin/"
)
add_dependencies(dse_copy_assets dse_engine)

function(dse_copy_runtime_assets target_name)
    add_dependencies(${target_name} dse_copy_assets)
endfunction()

function(dse_copy_runtime_dlls target_name)
    add_dependencies(${target_name} dse_copy_assets)
endfunction()

# Web(Emscripten) 交叉编译：只构建 dse_engine + web_host，跳过桌面 apps / 工具 / 测试 / 打包。
if(EMSCRIPTEN)
    add_subdirectory(apps/web_host)
    return()
endif()

# 包含运行时宿主入口
add_subdirectory(apps/runtime)

# 编辑器核心门面层（命令/查询，无 UI 框架依赖）。无条件构建，
# 以便编辑器 app 与 gtest 集成测试（通常在 editor 未构建的配置下运行）都能链接。
add_subdirectory(apps/editor_cpp/core)

if(DSE_BUILD_EDITOR)
    add_subdirectory(apps/editor_cpp)
endif()

# Standalone game runtime (no editor UI)
add_subdirectory(apps/standalone)

# 工具链：dse CLI（headless：建项目 / 打包加密 / 完整 build）
add_subdirectory(apps/tools/dse_cli)

# 对随 SDK/工具包发行的可执行文件启用 LTCG（与静态 dse_engine 的 /GL 配合做跨模块优化）。
dse_enable_lto(dse_standalone)
dse_enable_lto(dse_cli)

# 工具链：AssetBuilder
add_executable(AssetBuilder 
    apps/tools/asset_builder/main.cpp
    engine/assets/compiler/importer.cpp
    engine/assets/texture_compressor.cpp
    engine/assets/dtex.cpp
    engine/assets/ktx2_parser.cpp
    engine/mesh/mesh_decimator.cpp
    engine/render/gi/lightmap_baker.cpp
    # lightmap_baker 经 JobSystem 并行烘焙，独立工具需自带其实现及核心依赖
    engine/core/job_system.cpp
    engine/base/debug.cpp
    engine/core/memory/linear_allocator.cpp
    engine/core/memory/memory.cpp
    engine/core/memory/allocator.cpp
    engine/core/memory/system_allocator.cpp
    engine/core/memory/frame_allocator.cpp
    engine/core/memory/memory_tracker.cpp
)
target_include_directories(AssetBuilder PRIVATE ${CMAKE_SOURCE_DIR} depends/tinygltf depends/glm)
# 与静态 dse_engine 消费端一致：DSE_EXPORT 展开为空，避免 dllimport 未解析符号
target_compile_definitions(AssetBuilder PRIVATE DSE_BUILD_STATIC)
if(DSE_HAS_ASSIMP)
    target_link_libraries(AssetBuilder PRIVATE assimp)
    target_compile_definitions(AssetBuilder PRIVATE DSE_HAS_ASSIMP=1)
endif()
if(MSVC)
    set_property(TARGET AssetBuilder PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# 工具链：ShaderCompiler (GLSL 450 → SPIR-V / GLSL 330 / HLSL)
if(DSE_HAS_GLSLANG AND DSE_HAS_SPIRV_CROSS)
    # 工具链构建延后到下方：交叉编译时改用 host 预构建的 dse_shader_compiler。

    # --- Shader 离线编译：构建时自动执行 ---
    set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/engine/render/shaders/src")
    set(SHADER_GEN_DIR "${CMAKE_SOURCE_DIR}/engine/render/shaders/generated")

    file(GLOB SHADER_SOURCE_FILES
        "${SHADER_SRC_DIR}/*.vert"
        "${SHADER_SRC_DIR}/*.frag"
        "${SHADER_SRC_DIR}/*.comp"
    )

    # 生成的 embed 头文件列表（用于依赖跟踪）
    set(GENERATED_SHADER_HEADERS "")
    foreach(_shader ${SHADER_SOURCE_FILES})
        get_filename_component(_name ${_shader} NAME_WE)
        get_filename_component(_ext ${_shader} LAST_EXT)
        string(SUBSTRING ${_ext} 1 -1 _ext_clean)
        list(APPEND GENERATED_SHADER_HEADERS "${SHADER_GEN_DIR}/embed/${_name}_${_ext_clean}.gen.h")
    endforeach()
    # @VARIANTS: GPU_DRIVEN 变体（pbr.vert → pbr_gpu_driven_vert.gen.h）
    list(APPEND GENERATED_SHADER_HEADERS
        "${SHADER_GEN_DIR}/embed/pbr_gpu_driven_vert.gen.h"
        "${SHADER_GEN_DIR}/embed/pbr_gpu_driven_vert_reflect.gen.h"
    )
    # compute skinning shader（skinning.comp → skinning_comp.gen.h）
    list(APPEND GENERATED_SHADER_HEADERS
        "${SHADER_GEN_DIR}/embed/skinning_comp.gen.h"
        "${SHADER_GEN_DIR}/embed/skinning_comp_reflect.gen.h"
    )
    # skinning.comp 已被上面的 glob 收集（skinning_comp.gen.h），显式追加会重复；
    # MSBuild 静默去重，Ninja 会因 dupbuild 报错——统一去重以跨生成器安全。
    list(REMOVE_DUPLICATES GENERATED_SHADER_HEADERS)

    # dse_shader_compiler 是 host 工具：
    #  - 原生构建：直接构建该 target 并使用；
    #  - 交叉编译(Android/iOS)：clang 产物无法在 host 运行，改用 host 预构建可执行文件
    #    （先在桌面平台构建一次生成 bin/dse_shader_compiler[.exe]，或 -DDSE_HOST_SHADER_COMPILER=<path>）。
    if(CMAKE_CROSSCOMPILING)
        if(NOT DSE_HOST_SHADER_COMPILER)
            find_program(DSE_HOST_SHADER_COMPILER
                NAMES dse_shader_compiler dse_shader_compiler.exe
                PATHS "${CMAKE_SOURCE_DIR}/bin" NO_DEFAULT_PATH)
        endif()
        if(NOT DSE_HOST_SHADER_COMPILER)
            message(FATAL_ERROR
                "交叉编译需要 host 版 dse_shader_compiler。请先在桌面平台(Windows/Linux)"
                "构建一次以生成 bin/dse_shader_compiler，或通过 -DDSE_HOST_SHADER_COMPILER=<path> 指定。")
        endif()
        set(_DSE_SHADER_COMPILER_CMD "${DSE_HOST_SHADER_COMPILER}")
        set(_DSE_SHADER_COMPILER_DEP "")
        message(STATUS "Cross-compiling: using host shader compiler ${DSE_HOST_SHADER_COMPILER}")
    else()
        add_subdirectory(tools/shader_compiler)
        set(_DSE_SHADER_COMPILER_CMD dse_shader_compiler)
        set(_DSE_SHADER_COMPILER_DEP dse_shader_compiler)
    endif()

    add_custom_command(
        OUTPUT ${GENERATED_SHADER_HEADERS}
        COMMAND ${_DSE_SHADER_COMPILER_CMD}
            --input-dir "${SHADER_SRC_DIR}"
            --output-dir "${SHADER_GEN_DIR}"
            --target all --embed
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_SOURCE_DIR}/bin/data/shader_cache"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_SOURCE_DIR}/data/shader_cache"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_SOURCE_DIR}/examples/KF_Framework/data/shader_cache"
        DEPENDS ${_DSE_SHADER_COMPILER_DEP} ${SHADER_SOURCE_FILES}
        COMMENT "Compiling shaders (GLSL 450 -> SPIR-V / GLSL 430 / HLSL) and clearing runtime shader cache"
        VERBATIM
    )

    add_custom_target(dse_compile_shaders DEPENDS ${GENERATED_SHADER_HEADERS})
    add_dependencies(dse_engine dse_compile_shaders)
    target_include_directories(dse_engine PRIVATE "${SHADER_GEN_DIR}")
    # 将生成的 embed 头文件注册为 dse_engine 的源，使 MSVC 能跟踪
    # #include 依赖——gen.h 变更时自动重编译包含它的 .cpp
    set_source_files_properties(${GENERATED_SHADER_HEADERS} PROPERTIES
        GENERATED TRUE
        HEADER_FILE_ONLY TRUE)
    target_sources(dse_engine PRIVATE ${GENERATED_SHADER_HEADERS})
endif()

# 工具链：DSSL Compiler (.dssl → GLSL 450)
add_subdirectory(tools/dssl_compiler)

# ---------------------------------------------------------------------------
# PUBLIC interface include directories（供下游 target_link_libraries 自动继承）
# BUILD_INTERFACE  : 源码编译时使用的路径
# INSTALL_INTERFACE: SDK 安装后消费者使用的路径
# ---------------------------------------------------------------------------
target_include_directories(dse_engine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/engine>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/modules>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/depends/glm>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/depends/glm_ext>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/depends/box2d-2.4.1/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/DSEngine>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/engine>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/modules>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/glm>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/glm_ext>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/entt/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/box2d/include>
)

# Install rules (extracted to cmake/engine_install.cmake)
include(cmake/engine_install.cmake)

enable_testing()

# ── net/http 回环冒烟测试接入 ctest ───────────────────────────────────────────
# 这些是独立可执行（127.0.0.1 回环，退出码 0=通过），仅在对应模块启用时存在
# （-DDSE_ENABLE_HTTP=ON / -DDSE_ENABLE_NET=ON）。注册进 ctest 后，opt-in 构建
# 可在默认 ctest 运行中自动自检；不影响未启用这两个模块的默认构建/CI。
if(TARGET dse_blueprint_smoke)
    add_test(NAME dse_blueprint_smoke COMMAND dse_blueprint_smoke)
    set_tests_properties(dse_blueprint_smoke PROPERTIES
        LABELS "smoke;blueprint"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 30)
endif()

foreach(_smoke_target
        dse_http_smoke
        dse_http_lua_smoke
        dse_net_smoke
        dse_net_capi_smoke
        dse_net_lua_smoke
        dse_repl_smoke)
    if(TARGET ${_smoke_target})
        add_test(NAME ${_smoke_target} COMMAND ${_smoke_target})
        set_tests_properties(${_smoke_target} PROPERTIES
            LABELS "smoke;net_http"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            TIMEOUT 60)
    endif()
endforeach()

# C# 运行时冒烟（-DDSE_ENABLE_CSHARP=ON 时存在）。首次加载 CoreCLR 略慢，超时给宽些。
if(TARGET dse_csharp_smoke)
    add_test(NAME dse_csharp_smoke COMMAND dse_csharp_smoke)
    set_tests_properties(dse_csharp_smoke PROPERTIES
        LABELS "smoke;csharp"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 120)
endif()

if(DSE_BUILD_GTESTS)
    set(DSE_LOCAL_GTEST_DIR "${CMAKE_SOURCE_DIR}/depends/googletest-1.17.0")
    if(NOT EXISTS "${DSE_LOCAL_GTEST_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "DSE_BUILD_GTESTS=ON 需要本地 GoogleTest 源码。请将 googletest-1.17.0.zip 解压到 ${DSE_LOCAL_GTEST_DIR}"
        )
    endif()

    # 禁止 googletest 自身占据全局 include / install，避免与引擎构建冲突
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    # dse_engine 是 DLL；GoogleTest 和测试 exe 使用 DLL CRT，避免 Debug CRT 堆链表跨模块不一致。
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory("${DSE_LOCAL_GTEST_DIR}" "${CMAKE_BINARY_DIR}/third_party/googletest")

    add_subdirectory(tests/gtest)
endif()


if(DSE_BUILD_LAUNCHER)
    if(EXISTS "${CMAKE_SOURCE_DIR}/apps/launcher/CMakeLists.txt")
        add_subdirectory(apps/launcher)
    else()
        add_custom_target(dse_launcher ALL
            COMMAND ${CMAKE_COMMAND} -E echo "launcher_not_implemented"
        )
    endif()
endif()

# ── S1.5 Codegen Target ─────────────────────────────────────────────────────
# 用法: cmake --build build_vs2022 --target dse_codegen
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(
        NAME dse_feature_ledger
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/audit/verify_feature_ledger.py
    )
    set_tests_properties(dse_feature_ledger PROPERTIES LABELS "audit;editor")

    add_test(
        NAME dse_feature_ledger_negative
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/audit/test_verify_feature_ledger.py
    )
    set_tests_properties(dse_feature_ledger_negative PROPERTIES LABELS "audit;editor")

    add_custom_target(dse_codegen
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/codegen/codegen.py
                --defs  ${CMAKE_SOURCE_DIR}/tools/codegen/binding_defs.json
                --out   ${CMAKE_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running DSEngine binding codegen..."
        VERBATIM
    )
endif()

# ── Android 原生宿主 (NativeActivity host) ──────────────────────────────────
# 产物 libdse_android_host.so：通过 native_app_glue 提供的 ANativeActivity_onCreate
# 作为入口，链接 dse_engine（静态库时自包含），供打包 APK 使用。
# 构建: cmake --build <android_build_dir> --target dse_android_host
if(ANDROID)
    set(_DSE_GLUE_DIR "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
    if(NOT EXISTS "${_DSE_GLUE_DIR}/android_native_app_glue.c")
        set(_DSE_GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")
    endif()
    if(NOT EXISTS "${_DSE_GLUE_DIR}/android_native_app_glue.c")
        message(FATAL_ERROR "找不到 native_app_glue 源（${_DSE_GLUE_DIR}）")
    endif()
    add_library(dse_android_host SHARED
        "${CMAKE_SOURCE_DIR}/apps/android_host/native_main.cpp"
        "${_DSE_GLUE_DIR}/android_native_app_glue.c")
    target_include_directories(dse_android_host PRIVATE
        "${CMAKE_SOURCE_DIR}" "${_DSE_GLUE_DIR}")
    target_link_libraries(dse_android_host PRIVATE dse_engine EGL GLESv3 android log)
    # NativeActivity 经 dlsym 取入口；强制保留 glue 提供的 ANativeActivity_onCreate
    target_link_options(dse_android_host PRIVATE "-u" "ANativeActivity_onCreate")
    # 固定输出名(去掉 Debug 后缀)，使 APK manifest 的 android.app.lib_name 稳定为 dse_android_host
    set_target_properties(dse_android_host PROPERTIES OUTPUT_NAME "dse_android_host" DEBUG_POSTFIX "")
endif()
